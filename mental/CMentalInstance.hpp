#pragma once

#include "CMentalCommon.hpp"

class CMentalInstance : private DisableCopyAndMove
{
private:
public:
	CMentalInstance() {
		std::cout << "Constructor called\n";
	}

	~CMentalInstance() {
		std::cout << "Destructor called\n";
	}

}; // CMentalInstance
