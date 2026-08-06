#include "MetatileRef.h"
#include <format>

std::string fe::MetatileRef::to_string(bool p_incl_mt_no) const {
	std::string out;

	// source
	if (src_screen.has_value())
		out = std::format("Screen {} -> ", src_screen.value());
	else
		out = "[GLOBAL] -> ";

	// type
	switch (type) {
	case Type::ScreenTilemap: out += "Tilemap"; break;
	case Type::MattockAnimation: out += "Mattock Animation"; break;
	case Type::PushBlockDrawBlock: out += "Push-Block Draw"; break;
	case Type::PushBlockSource: out += "Push-Block Source"; break;
	case Type::PushBlockTarget: out += "Push-Block Target"; break;
	default: out += "Unknown"; break;
	}

	// destination
	if (p_incl_mt_no)
		out += std::format(" -> Metatile {}", metatile);

	return out;
}
