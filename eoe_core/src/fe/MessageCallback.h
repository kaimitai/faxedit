#ifndef FE_MESSAGE_CALLBACK_H
#define FE_MESSAGE_CALLBACK_H

#include "Message.h"
#include <functional>

// pulls in the message type and defines an alias for message callbacks
// and adds a generic helper for sending messages
namespace fe {
	using MessageCallback = std::function<void(const Message&)>;

	inline void send_message(const MessageCallback& p_callback, const Message& p_message) {
		if (p_callback)
			p_callback(p_message);
	}
}

#endif
