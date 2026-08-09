#include <cstring>
#include <iostream>
#include "fi/cli/Cli.h"
#include "mantra/MantraCli.h"

int main(int argc, char** argv) try {

	if (argc > 2 && std::strcmp(argv[1], "m") == 0) {
		fman::MantraCli l_cli(argc, argv);
		fman::Mantra l_man{ l_cli.get_mantra() };
		std::cout << (l_cli.is_terse() ? l_man.get_mantra() : l_man.get_printable_summary());
	}
	else
		fi::Cli cli(argc, argv);
}
catch (const std::runtime_error& ex) {
	std::cerr << "Runtime error: " << ex.what() << "\n";
}
catch (const std::exception& ex) {
	std::cerr << "Runtime exception: " << ex.what() << "\n";
}
catch (...) {
	std::cerr << "Unknown runtime error:\n";
}
