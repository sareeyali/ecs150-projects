#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

using namespace std;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cout << "wzip: file1 [file2 ...]\n";
        exit(1);
    }

    char currentChar;
    int count = 0;
    bool hasRun = false;

    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);

        if (fd < 0) {
            cout << "wzip: cannot open file\n";
            exit(1);
        }

        char c;
        int bytesRead;

        while ((bytesRead = read(fd, &c, 1)) > 0) {
            if (!hasRun) {
                currentChar = c;
                count = 1;
                hasRun = true;
            } else if (c == currentChar) {
                count++;
            } else {
                write(STDOUT_FILENO, &count, sizeof(int));
                write(STDOUT_FILENO, &currentChar, 1);

                currentChar = c;
                count = 1;
            }
        }

        close(fd);
    }

    if (hasRun) {
        write(STDOUT_FILENO, &count, sizeof(int));
        write(STDOUT_FILENO, &currentChar, 1);
    }

    return 0;
}