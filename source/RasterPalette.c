#include <nds.h>
#include <stdint.h>
#include <string.h>

/* The two free ARM9 HBlank DMA channels replay sparse color-palette changes.
 * Frames exceeding that bandwidth retain the upstream whole-frame palette.
 * All targets are derived from K2GE palette writes, never from game content. */
#define VISIBLE_LINES 152
#define DISPLAY_LINES 192
#define PALETTE_COLORS (0x180 / 2)
#define STREAMS 2

extern const uint16_t MAPPED_RGB[];
extern const uint16_t EMUPALBUFF[];

typedef struct {
    unsigned color, lastLine, value;
    uint16_t colors[VISIBLE_LINES];
} PaletteChanges;
typedef struct {
    uint32_t destination;
    uint16_t colors[DISPLAY_LINES + 2];
} PaletteStream;

static uint16_t initialPalette[PALETTE_COLORS] __attribute__((aligned(4)));
static uint8_t lineFlags[VISIBLE_LINES];
static uint32_t seenColors[PALETTE_COLORS / 32];
static PaletteChanges changes[STREAMS];
static PaletteStream streams[2][STREAMS] __attribute__((aligned(32)));
static unsigned changeCount, uniqueCount, capturedLines, displayBank, pendingBank;

uint32_t rasterPaletteSupportedFrames;
uint32_t rasterPaletteOverBudgetFrames;
uint32_t rasterPaletteUnsupportedModeFrames;
uint32_t rasterPaletteMaxEntries;

void rasterPaletteCapture(const void *palette, unsigned line, unsigned control, unsigned mode) {
    if (line >= VISIBLE_LINES) return;
    if (!line) {
        memcpy(initialPalette, palette, sizeof(initialPalette));
        memset(seenColors, 0, sizeof(seenColors));
        changeCount = uniqueCount = 0;
    }
    lineFlags[line] = (control & 0x80) | (mode ? 1 : 0);
    capturedLines = line + 1;
}

/* Record the resulting value of an actually changed K2GE palette write.
 * Two byte writes to one color on the same scanline resolve to its final value. */
void rasterPaletteWrite(unsigned color, unsigned value, unsigned line) {
    if (!line || line >= VISIBLE_LINES || color >= PALETTE_COLORS || !(color & 3)) return;
    value &= 0xFFF;
    uint32_t bit = 1u << (color & 31);
    if (!(seenColors[color / 32] & bit)) {
        if (value == (initialPalette[color] & 0xFFF)) return;
        seenColors[color / 32] |= bit;
        uniqueCount++;
        if (changeCount >= STREAMS) return;
        PaletteChanges *c = &changes[changeCount++];
        c->color = color;
        c->lastLine = 0;
        c->value = initialPalette[color] & 0xFFF;
    }
    for (unsigned i = 0; i < changeCount; i++) {
        PaletteChanges *c = &changes[i];
        if (c->color != color) continue;
        while (c->lastLine < line) c->colors[c->lastLine++] = c->value;
        c->value = value;
        break;
    }
}

/* Observe the NGPDS memory-mapped byte/word write before K2GE stores it.
 * The palette register addresses below are hardware offsets, not ROM offsets. */
void rasterPaletteMemoryWrite(unsigned offsetAndWidth, unsigned value,
                              unsigned line, const uint8_t *palette) {
    unsigned offset = offsetAndWidth & 0xFFFF;
    unsigned color = offset / 2;
    if (offsetAndWidth & 0x10000) {
        if (!(offset & 1)) {
            rasterPaletteWrite(color, value, line);
        } else {
            unsigned first = palette[offset - 1] | ((value & 0xFF) << 8);
            rasterPaletteWrite(color, first, line);
            if (color + 1 < PALETTE_COLORS) {
                unsigned second = ((value >> 8) & 0xFF) | (palette[offset + 2] << 8);
                rasterPaletteWrite(color + 1, second, line);
            }
        }
    } else {
        unsigned raw = palette[color * 2] | (palette[color * 2 + 1] << 8);
        if (offset & 1) raw = (raw & 0xFF) | ((value & 0xFF) << 8);
        else raw = (raw & 0xFF00) | (value & 0xFF);
        rasterPaletteWrite(color, raw, line);
    }
}

static unsigned hardwareOffset(unsigned color) {
    unsigned group = color / 4, pen = color & 3;
    if (group < 16) return 0x200 + group * 32 + pen * 2; /* OBJ */
    if (group < 32) return (group - 16) * 32 + pen * 2; /* plane 1 */
    return (group - 32) * 32 + (pen + 4) * 2;          /* plane 2 */
}

void rasterPaletteFinish(void) {
    unsigned bank = displayBank ^ 1;
    PaletteStream *next = streams[bank];
    next[0].destination = next[1].destination = 0;
    if (uniqueCount > rasterPaletteMaxEntries) rasterPaletteMaxEntries = uniqueCount;
    if (capturedLines != VISIBLE_LINES) goto publish;
    for (unsigned y = 0; y < VISIBLE_LINES; y++) {
        if ((lineFlags[y] & 1) || lineFlags[y] != lineFlags[0]) {
            rasterPaletteUnsupportedModeFrames++;
            goto publish;
        }
    }
    if (uniqueCount > STREAMS) {
        rasterPaletteOverBudgetFrames++;
        goto publish;
    }
    for (unsigned i = 0; i < changeCount; i++) {
        PaletteChanges *c = &changes[i];
        while (c->lastLine < VISIBLE_LINES) c->colors[c->lastLine++] = c->value;
        unsigned offset = hardwareOffset(c->color);
        uint16_t final = EMUPALBUFF[offset / 2];
        for (unsigned y = 0; y < DISPLAY_LINES + 2; y++) next[i].colors[y] = final;
        for (unsigned y = 0; y < VISIBLE_LINES; y++) {
            unsigned raw = c->colors[y];
            if (lineFlags[y] & 0x80) raw ^= 0xFFF;
            next[i].colors[y + (DISPLAY_LINES - VISIBLE_LINES) / 2] = MAPPED_RGB[raw];
        }
        next[i].destination = 0x05000000 + offset;
    }
    if (changeCount) rasterPaletteSupportedFrames++;
publish:
    /* ARM9 writes are cached; HBlank DMA reads main RAM directly. */
    DC_FlushRange(next, sizeof(streams[bank]));
    __asm__ volatile ("" ::: "memory");
    pendingBank = bank;
}

void rasterPalettePublish(void) { displayBank = pendingBank; }

void rasterPaletteVBlank(void) {
    const PaletteStream *current = streams[displayBank];
    for (unsigned i = 0; i < STREAMS; i++) {
        if (!current[i].destination) continue;
        volatile uint32_t *dma = (volatile uint32_t *)(0x040000C8 + i * 12);
        *(volatile uint16_t *)current[i].destination = current[i].colors[0];
        dma[0] = (uint32_t)&current[i].colors[1];
        dma[1] = current[i].destination;
        dma[2] = 0x92400001;
    }
}
