int g(int i);

int f(int i) {
    if (i > 0) {
        return g(i - 1) + 1;
    } else {
        return 0;
    }
}

int g(int i) {
    if (i > 0) { return f(i - 1) + 1; }
    return 0;
}
