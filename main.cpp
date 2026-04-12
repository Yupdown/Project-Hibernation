#include "stdafx.h"
#include "hb_application.h"

int main(int argc, char** argv) {
	HBApplication app;

	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}