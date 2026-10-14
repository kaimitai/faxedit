#ifndef FH_ATLASDEV_VERTICAL_SCROLL_H
#define FH_ATLASDEV_VERTICAL_SCROLL_H

#include "common/klib/Asm6502.h"
#include <vector>

namespace fh {
	// AtlasDevVerticalScroll keeps its 11 bytes of ram at $04e2-$04ec, the
	// same bytes AtlasDevDayNightCycle, AtlasDevInfectedTint,
	// AtlasDevTimeOfDay and AtlasDevEnemyHud use. each side refuses the
	// other; this is the check they call.
	bool vertical_scroll_installed(const std::vector<byte>& p_rom);
}

#endif
