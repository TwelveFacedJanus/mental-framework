#include <iostream>
#include "CMentalInstance.hpp"





int main(int argc, const char* argv[])
{
	CMentalInstance instance = CMentalInstance();
	auto k = instance;
	std::cout << "Hello, Mental Framework!\n";
	return EXIT_SUCCESS;
}
