#ifndef FI_FAX_STRING_H
#define FI_FAX_STRING_H

#include <map>
#include <string>
#include <vector>

using byte = unsigned char;

namespace fi {

	class FaxString {

		std::string m_string;

	public:
		FaxString(void) = default;
		FaxString(const std::string& p_string);
		const std::string& get_string(void) const;
		std::vector<byte> to_bytes(const std::map<std::string, byte>& p_smap) const;
	};
}

#endif
