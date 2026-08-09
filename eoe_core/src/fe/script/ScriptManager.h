#ifndef FE_SCRIPTMANAGER_H
#define FE_SCRIPTMANAGER_H

#include <functional>
#include <string>
#include "fe/Config.h"

using byte = unsigned char;

namespace fe::script {

	using MessageCallback = std::function<void(const std::string&)>;

	void message(const MessageCallback& p_callback, const std::string& p_message);

	std::string disasm_iscripts(const Config& p_config, const std::vector<byte>& p_rom,
		bool p_shop_comments, std::size_t& p_entrypoint_count);
	void disasm_iscripts_to_file(const fe::Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_filename, bool p_shop_comments, bool p_overwrite,
		const MessageCallback& p_message);

}

#endif
