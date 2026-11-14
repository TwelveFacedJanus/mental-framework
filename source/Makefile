CC = clang
CFLAGS = -std=c11 -Wall -Wextra -rdynamic -fvisibility=hidden
LDFLAGS = -ldl

mental:
	$(CC) $(CFLAGS) $(LDFLAGS) main.c -o mental

clean:
	rm -f mental .mental_auth.dat 

windows8664:
	docker build -t win-mingw64 . 
	docker run --rm -v "%PWD":/work -w /work win-mingw64 \
		x86_64-w64-mingw32-gcc -O2 -o mental

unix:
	$(CC) $(CFLAGS) main.c -o mental

.PHONY: clean unix windows8664
