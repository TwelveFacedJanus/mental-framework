#pragma once

#include "CMentalCommon.hpp"

#if defined(MENTAL_ENABLE_OPENGL_BACKEND)
	#include <GL/glew.h>
#endif // defined (MENTAL_ENABLE_OPENGL_BACKEND)

MentalResult initialize_glew(EMentalFlags eEnableExperimental)
{
	if (eEnableExperimental == 1) glewExperimental = GL_TRUE;

	if (glewInit() != GLEW_OK) return Err(MENTAL_STATUS_CODE_GLEW_INIT_FAIL, "Failed to initialize GLEW");
	return Ok(MENTAL_STATUS_CODE_OK, "GLEW library has been initialized.");
}





