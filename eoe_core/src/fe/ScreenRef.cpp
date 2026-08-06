#include "ScreenRef.h"
#include <format>

std::string fe::ScreenRef::to_string(bool p_incl_destination) const {
	std::string out;

	if (src_world && src_screen)
		out = std::format("World {}, Screen {} -> ", src_world.value(), src_screen.value());
	else
		out = "[GLOBAL] -> ";

	switch (type) {
	case Type::Scroll: out += "Scroll"; break;
	case Type::TransitionSameWorld: out += "Sameworld Transition"; break;
	case Type::TransitionOtherWorld: out += "Otherworld Transition"; break;
	case Type::DoorSameWorld: out += "Sameworld Door"; break;
	case Type::DoorBuilding: out += "Building Door"; break;
	case Type::DoorHack: out += "Hack Door"; break;
	case Type::PushBlock: out += "Push-Block"; break;
	case Type::Spawn: out += "Spawn"; break;
	case Type::StageNext: out += "Next Stage"; break;
	case Type::StagePrev: out += "Prev Stage"; break;
	case Type::StageStart: out += "Start Screen"; break;
	case Type::Hardcoded: out += "Hardcoded"; break;
	default: out += "Unknown"; break;
	}

	if (p_incl_destination)
		out += std::format(" -> World {}, Screen {}", dst_world, dst_screen);

	return out;
}

bool fe::ScreenRef::operator<(const ScreenRef& rhs) const {
	if (dst_world != rhs.dst_world)
		return dst_world < rhs.dst_world;

	if (dst_screen != rhs.dst_screen)
		return dst_screen < rhs.dst_screen;

	if (type != rhs.type)
		return type < rhs.type;

	if (src_world != rhs.src_world)
		return src_world < rhs.src_world;

	return src_screen < rhs.src_screen;
}
