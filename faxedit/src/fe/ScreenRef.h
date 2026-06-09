#ifndef FE_SCREENREF_H
#define FE_SCREENREF_H

#include <cstddef>
#include <string>
#include <optional>

namespace fe {

    struct ScreenRef {
        std::optional<std::size_t> src_world;
        std::optional<std::size_t> src_screen;

        std::size_t dst_world;
        std::size_t dst_screen;

        enum class Type : unsigned char {
            Scroll,
            TransitionSameWorld,
            TransitionOtherWorld,
            DoorSameWorld,
            DoorBuilding,
            PushBlock,
            Spawn,
            StageNext,
            StagePrev,
            StageStart,
            Hardcoded
        } type;

        bool operator<(const ScreenRef& rhs) const;
        std::string to_string(bool p_incl_destination = false) const;
    };

}

#endif
