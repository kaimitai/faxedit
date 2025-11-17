#ifndef FI_FAX_STRING_H
#define FI_FAX_STRING_H

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
		std::vector<byte> to_bytes(void) const;
	};
}

#endif
