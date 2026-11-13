CC      = clang
CFLAGS  = -std=c11 -Wall -Wextra -I. -I/usr/local/include
LDFLAGS = -L/usr/local/lib
LDLIBS  = -lGLEW -lglfw -lGL -lcurl

mental: mental.c graphics/window.h graphics/opengl_backend.h
	$(CC) $(CFLAGS) mental.c $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -f mental

.PHONY: clean
