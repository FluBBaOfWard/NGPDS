#include <nds.h>
#include "Sound.h"

/* A launch-only ramp prevents the initial full-scale DC step in the legacy
 * stream. Leave the PSG/DAC renderer and all later samples unchanged. */
#define STARTUP_FADE_FRAMES 32768u
static unsigned startupFramesLeft = STARTUP_FADE_FRAMES;

/* Sound.s exports ARM entry points without interworking metadata. */
#ifdef __arm__
__attribute__((target("arm")))
#endif
mm_word soundRenderStartup(mm_word length, mm_addr dest, mm_stream_formats format) {
	mm_word filled = soundRender(length, dest, format);
	unsigned frames = filled < startupFramesLeft ? filled : startupFramesLeft;
	if (frames) {
		s16 *samples = dest;
		for (unsigned i = 0; i < frames; i++) {
			int gain = (int)(STARTUP_FADE_FRAMES - startupFramesLeft);
			samples[2 * i] = (s32)samples[2 * i] * gain >> 15;
			samples[2 * i + 1] = (s32)samples[2 * i + 1] * gain >> 15;
			startupFramesLeft--;
		}
	}
	return filled;
}
