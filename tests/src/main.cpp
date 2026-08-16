/*
 * Echoes of Eolis regression test suite
 *
 * The test suite operates in two modes using the same test execution path:
 *
 *   Generate - Executes all tests and writes the resulting SHA-256 hashes
 *              to the canonical test data file.
 *
 *   Validate - Executes the same tests and compares each generated hash
 *              sequentially against the canonical hashes.
 *
 * The test infrastructure is public, but the canonical test inputs are not
 * included in the repository because some contain copyrighted data. Maintainer
 * test data therefore lives under test_data/, which is excluded from Git.
 *
 * Contributors can provide their own local test data and test definitions using
 * the same infrastructure.
 */

#include <iostream>
#include <string>
#include "eoe_test/private_tests.h"

int main(int argc, char** argv) try {
	eoe::test::generate_tests();
	// eoe::test::run_tests();
}
catch (const std::exception& ex) {
	std::cout << ex.what() << "\n";
	return 1;
}
