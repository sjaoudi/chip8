CXX = gcc -framework OpenGL -framework GLUT
CXXFLAGS = -Wall -g -Wno-deprecated-declarations I/usr/local/include -L/usr/local/lib -lSDL2
LDFLAGS= -framework GLUT -framework OpenGL -framework Cocoa

chip8: chip8.c
	gcc -o chip8 chip8.c -g -Wno-deprecated-declarations -I/usr/local/include -L/usr/local/lib -lSDL2
