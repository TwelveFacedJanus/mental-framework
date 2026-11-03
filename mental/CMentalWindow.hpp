#pragma once


#include <GLFW/glfw3.h>
#include <stdexcept>
#include "CMentalRendererBackend.hpp"

class CMentalWindow
{
private:
	GLFWwindow* _pWindow = nullptr;

	void __initializeGLFW() noexcept(false) {
		if (!glfwInit()) throw std::runtime_error("Crash. Failed to initialize GLFW library.");
	}

	void __createGLFWwindow() noexcept(false) {
	
	}
public:
	CMentalWindow();
	~CMentalWindow();

}; // CMentalWindow
