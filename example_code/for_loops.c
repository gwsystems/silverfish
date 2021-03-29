int f(int a) {
    int res = 0;
    for (int i = 0; i < a; i++) {
        res += i * i + i * i * i;
    }
    return res;
}