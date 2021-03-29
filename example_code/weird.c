extern int a;

int other(int b);

int main() {
    return other(2) + a;
}
