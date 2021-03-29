#define EXPORT __attribute__((visibility("default")))
#define IMPORT __attribute__((visibility("default")))

typedef int (*fun_ptr)(int);

EXPORT int f(fun_ptr p) {
    return p(5);
}

EXPORT int h(int a) {
    return a * 2;
}

EXPORT int g(int a) {
    if (a == 1) {
        return f(h);
    } else {
        return f(g);
    }
}

int main() {
    return g(1);
}
