#include "printf.h"

void         PUT32(unsigned int, unsigned int);
unsigned int GET32(unsigned int);
void         dummy(unsigned int);
void         write(unsigned int, char*, unsigned int);

void _putchar(char character) {
    char buf[1];
    buf[0] = character;
    write(1, buf, 1);
}
