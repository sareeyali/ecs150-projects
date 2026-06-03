#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

using namespace std;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cout << "wunzip: file1 [file2 ...]\n";
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);

        if (fd < 0) {
            cout << "wunzip: cannot open file\n";
            exit(1);
        }

        int count;
        char c;

        while (read(fd, &count, sizeof(int)) == sizeof(int)) {
            if (read(fd, &c, 1) != 1) {
                break;
            }

            for (int j = 0; j < count; j++) {
                write(STDOUT_FILENO, &c, 1);
            }
        }

        close(fd);
    }

    return 0;
}