#include "FaxString.h"
#include "fi_constants.h"
#include <format>
#include <stdexcept>

fi::FaxString::FaxString(const std::string& p_string) :
	m_string{ p_string }
{
}

const std::string& fi::FaxString::get_string(void) const {
	return m_string;
}
