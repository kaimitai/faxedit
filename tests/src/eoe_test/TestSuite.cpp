#include "TestSuite.h"
#include "common/klib/Kfile.h"
#include <format>
#include <stdexcept>

eoe::test::TestSuite::TestSuite(TestMode p_mode, const std::string& p_testfile_path) :
	m_index{ 0 },
	m_mode{ p_mode },
	m_filename{ p_testfile_path }
{
	if (p_mode == TestMode::Validate)
		m_hashes = klib::file::read_file_as_strings(p_testfile_path);
}

void eoe::test::TestSuite::hash(const std::string& p_hash) {
	if (m_mode == TestMode::Generate)
		m_hashes.push_back(p_hash);
	else {
		if (m_index >= m_hashes.size())
			throw std::runtime_error("Test produced more hashes than expected");

		if (p_hash != m_hashes[m_index])
			throw std::runtime_error(std::format("Hash mismatch at test #{}", m_index));

		++m_index;
	}
}

void eoe::test::TestSuite::finalize(void) const {
	if (m_mode == TestMode::Generate) {
		// TODO: Make a new such helper in klib::Kfile
		std::string out;
		for (const auto& s : m_hashes)
			out += s + "\n";

		klib::file::write_string_to_file(out, m_filename);
	}
	else if (m_index != m_hashes.size()) {
		throw std::runtime_error(std::format("Test produced fewer hashes than expected ({} of {})", m_index, m_hashes.size()));
	}
}
