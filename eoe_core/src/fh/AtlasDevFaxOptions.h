#ifndef FH_ATLASDEV_FAXOPTIONS_H
#define FH_ATLASDEV_FAXOPTIONS_H

#include "GeneralHack.h"

#include <string>
#include <vector>

namespace fe { class Config; }
namespace klib { class Asm6502; }

namespace fh {
	// the resident part's addresses, for the menu. state is the 8 bytes of
	// row bits, bit i&7 of byte i>>3 for row i; without a save it is the flag
	// block itself and a row's bit is its flag. rows_table holds per row
	// { flag offset, flag mask, state offset, state mask }, and a row's bit
	// is read through the offset at bit_column and the mask after it
	struct FaxOptionsResident {
		word apply, toggle, defaults, state, rows_table, row_count;
		byte bit_column;
	};

	struct FaxOptionsRow {
		std::string label;	// up to 19 characters
		byte flag;			// 0 to 247
	};

	// one row per listed AtlasDev hack with flag=n, in list order; throws on
	// KillSwitch, no rows, more than 64, a label over 19, or a shared flag
	std::vector<FaxOptionsRow> faxoptions_rows(const std::vector<GeneralHack>& p_hacks);
	std::string faxoptions_label(GeneralHackLib p_type);

	// the menu the pause hook opens, with its cursor, row and top in the three
	// zero page bytes from p_state_zp; throws on a label the font cannot show
	void emit_faxoptions_menu(klib::Asm6502& p_code, const std::vector<FaxOptionsRow>& p_rows,
		const FaxOptionsResident& p_resident, byte p_state_zp, const fe::Config& p_config);
}

#endif
