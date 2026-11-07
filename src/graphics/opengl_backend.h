#ifndef MENTAL_OPENGL_BACKEND_H
#define MENTAL_OPENGL_BACKEND_H

/*
 * MENTAL_OPENGL_BACKEND.h
 * ------------------------------------------------------------------
 * GLEW backend for the Mental framework.
 *
 * Include order matters:
 *   GL/glew.h  ->  GLFW/glfw3.h
 * This header enforces that order for you, but do NOT include
 * <GL/gl.h>, <GL/glew.h> or <GLFW/glfw3.h> before this file.
 *
 * Usage:
 *   MentalWindow* win = mentalCreateWindow();
 *   if (!win) return 1;
 *
 *   if (!mentalOpenGLInit(win)) {
 *       mentalDestroyWindow(win);
 *       return 1;
 *   }
 *
 *   while (mentalWindowShouldClose(win)) {
 *       glClear(GL_COLOR_BUFFER_BIT);
 *       // draw...
 *       glfwSwapBuffers(win->pWindow);
 *       glfwPollEvents();
 *   }
 *
 *   mentalDestroyWindow(win);   // also terminates GLEW state
 * ------------------------------------------------------------------
 */

#include <stdio.h>

/* GLEW BEFORE GLFW — required. */
#include <GL/glew.h>
#include <GLFW/glfw3.h>

/* Pull in the window abstraction so we can validate it. */
#include "window.h"   /* adjust if your path differs (e.g. "graphics/window.h") */

/* ------------------------------------------------------------------ */
/*  Initialization                                                     */
/* ------------------------------------------------------------------ */

/*
 * Initialize GLEW on the given window's current context.
 * The window must already exist and its context must be current
 * (mentalCreateWindow already calls glfwMakeContextCurrent).
 *
 * Returns 1 on success, 0 on failure (and prints a diagnostic).
 */
static inline int mentalOpenGLInit(MentalWindow* win)
{
    if (!win || !win->pWindow) {
        fprintf(stderr, "[mental/opengl] mentalOpenGLInit: invalid window\n");
        return 0;
    }

    /* Sanity check: the window must own the current context. */
    if (glfwGetCurrentContext() != win->pWindow) {
        fprintf(stderr,
                "[mental/opengl] mentalOpenGLInit: window context is not current\n");
        return 0;
    }

    glewExperimental = GL_TRUE;  /* needed for modern GL on core profiles */
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr,
                "[mental/opengl] glewInit failed: %s\n",
                glewGetErrorString(err));
        return 0;
    }

    /*
     * GLEW sometimes leaves a spurious GL_INVALID_ENUM error behind on
     * core profiles because it probes for deprecated extensions.
     * Drain it so the user's first glGetError() isn't confused.
     */
    while (glGetError() != GL_NO_ERROR) { /* drain */ }

    printf("[mental/opengl] GLEW %s, GL %s, GLSL %s\n",
           glewGetString(GLEW_VERSION),
           (const char*)glGetString(GL_VERSION),
           (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

    return 1;
}

/* ------------------------------------------------------------------ */
/*  Capability / info queries                                          */
/* ------------------------------------------------------------------ */

/* Returns non-zero if the extension is supported (e.g. "GL_ARB_compute_shader"). */
static inline int mentalHasGLExtension(const char* name)
{
    return name && glewIsSupported(name);
}

/* Print a short summary of the OpenGL context to stdout. */
static inline void mentalPrintGLInfo(void)
{
    printf("---- OpenGL context ----\n");
    printf("  Vendor   : %s\n", (const char*)glGetString(GL_VENDOR));
    printf("  Renderer : %s\n", (const char*)glGetString(GL_RENDERER));
    printf("  Version  : %s\n", (const char*)glGetString(GL_VERSION));
    printf("  GLSL     : %s\n", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("------------------------\n");
}

/* ------------------------------------------------------------------ */
/*  Convenience: close check                                           */
/* ------------------------------------------------------------------ */

/*
 * Thin wrapper so callers don't need to reach into ->pWindow for the
 * loop condition. (Your current run-loop helper calls glfwTerminate,
 * which you may want to remove — see note at bottom.)
 */
static inline int mentalWindowShouldClose(MentalWindow* win)
{
    return !win || !win->pWindow || glfwWindowShouldClose(win->pWindow);
}

/* ------------------------------------------------------------------ */
/*  Optional helpers (very small, safe to drop)                        */
/* ------------------------------------------------------------------ */

/* Set a clear color by 0–255 RGBA components. */
static inline void mentalSetClearColorRGB(unsigned char r,
                                          unsigned char g,
                                          unsigned char b,
                                          unsigned char a)
{
    glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

/* Enable a modern core-profile baseline (depth test + seamless cubemap). */
static inline void mentalEnableCoreDefaults(void)
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}


#endif /* MENTAL_OPENGL_BACKEND_H */
