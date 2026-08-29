#include "mantra/MantraCli.h"
#include <iostream>

int main(void) {
	char program[]{ "eoe-cli" };
	char command[]{ "m" };
	char terse[]{ "-t" };

	char* default_arguments[]{ program, command };
	fman::MantraCli default_cli(2, default_arguments);
	if (default_cli.is_terse()) {
		std::cerr << "Mantra output defaulted to terse mode\n";
		return 1;
	}

	char* terse_arguments[]{ program, command, terse };
	fman::MantraCli terse_cli(3, terse_arguments);
	if (!terse_cli.is_terse()) {
		std::cerr << "Explicit terse mode was not enabled\n";
		return 1;
	}

	return 0;
}
