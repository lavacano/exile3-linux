all: libexile3audio.so

libexile3audio.so: libexile3audio.c
	gcc -m32 -std=gnu99 -O2 -shared -fPIC -Wall libexile3audio.c -o libexile3audio.so -ldl -lX11

clean:
	rm -f libexile3audio.so

.PHONY: all clean
