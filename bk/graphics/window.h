#ifndef MENTAL_WINDOW_H
#define MENTAL_WINDOW_H

#include <GLFW/glfw3.h>
#include <stdlib.h>

typedef struct MentalWindow {
    GLFWwindow* pWindow;
} MentalWindow;

static inline MentalWindow* mentalCreateWindow(void)
{
    if (!glfwInit()) return NULL;

    MentalWindow* win = (MentalWindow*)malloc(sizeof(MentalWindow));
    if (!win) {
        glfwTerminate();
        return NULL;
    }

    win->pWindow = glfwCreateWindow(640, 480, "Mental Framework (OPENGL)", NULL, NULL);
    if (!win->pWindow) {
        free(win);
        glfwTerminate();
        return NULL;
    }

    glfwMakeContextCurrent(win->pWindow);
    return win;
}

static inline void mentalDestroyWindow(MentalWindow* win)
{
    if (!win) return;
    if (win->pWindow) glfwDestroyWindow(win->pWindow);
    glfwTerminate();
    free(win);
}

#endif /* MENTAL_WINDOW_H */