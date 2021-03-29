#include <stdlib.h>

#define COUNT 10

int v_index     = 0;
int vals[COUNT] = { 1, 7, 400, 5, 63, 6, 900, 47, 0, 4 };

void put_int(char* str, int v);

int get_c() {
    int res = vals[v_index];
    v_index = (v_index + 1) % COUNT;
    return res;
}

int main() {
    int c = get_c();
    if (c >= 40 && c <= 77) {
        put_int("v", c);
        return 1;
    }
    return 0;
}