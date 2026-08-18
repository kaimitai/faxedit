#ifndef FE_MESSAGE_H
#define FE_MESSAGE_H

#include <string>

namespace fe {

	enum class MsgType { Info, Success, Warning, Error };

	struct Message {
		std::string text;
		MsgType type{ MsgType::Info };
	};

}

#endif
