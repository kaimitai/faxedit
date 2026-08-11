#include <iostream>
#include "eoe_test/ScriptRoundTripTests.h"

int main(int argc, char** argv) {

	const auto rt_result{ eoe_test::RoundTripTest() };

	std::cout << rt_result.success << " tests succeeded, " <<
		rt_result.failure << " tests failed\n";
}
