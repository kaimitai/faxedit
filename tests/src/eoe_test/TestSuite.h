#ifndef EOE_TEST_TESTSUITE_H
#define EOE_TEST_TESTSUITE_H

#include <string>
#include <vector>

namespace eoe::test {

	enum class TestMode { Generate, Validate };

	class TestSuite {
		TestMode m_mode;
		std::string m_filename;
		std::vector<std::string> m_hashes;
		std::size_t m_index;

	public:
		TestSuite(TestMode p_mode, const std::string& p_testfile_path = std::string());
		void hash(const std::string& p_hash);
		void finalize(void) const;
	};
}

#endif
