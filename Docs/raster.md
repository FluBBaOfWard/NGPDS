# Scanline state and sparse OBJ composition

Record backdrop/palette, background offsets and sprite register/attribute state at the existing visible-line boundary. Unchanged sprite frames retain native OAM; changed rows are binned by source Y, composed into sparse 32x32 8bpp OBJ blocks, and uploaded with aligned word stores. Preserve the three sprite priorities, transparency and window clipping. The atlas extends E into unused F/G banks; background A-D and existing palette DMA keep their resources. No new DMA channel or HBlank interrupt.

Exact input caching, packed-word merging and per-block revision/content comparisons skip repeated composition and unchanged VRAM uploads. ARM/O3 remains enabled without a diagnostic macro. There is no game ID, fixed frame or image signature condition, and no audio-synthesis/clock modification. Position/attribute raster behavior is covered; arbitrary within-line character-pattern rewriting and every possible cartridge are not claimed.

The optimized baseline has historical user NDSi/R4 NTR experience. Current automation preserves that baseline: 623 presentation-aligned frames per Japanese/translated version; reference core matches 620/623 exactly, with three existing one-line differences of up to 156 pixels. Emulator-cycle timing is not hardware timing. New file-persistence changes are separate and are not given an invented hardware pass.

Portable independent pixel oracle:

```sh
clang -O1 -g -fsanitize=address,undefined -Itests/host_include tests/test_sprite_raster.c -o /tmp/ngpds-raster
/tmp/ngpds-raster
```

80 deterministic randomized scenes exercise chains, flips, wrapping, palettes, transparent pens, clipping, priorities and mid-frame attributes; cache-hit/invalidation checks are independent of a game screenshot. Only synthetic fixtures belong in a public PR. K2GE/CPU/PSG and Shared dependency gitlinks remain exactly as pinned by current NGPDS main; the adapters belong to NGPDS.
