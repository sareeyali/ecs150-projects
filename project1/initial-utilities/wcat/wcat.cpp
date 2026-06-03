#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

using namespace std;

int main(int argc, char *argv[]) {

    // if no files provided
    if (argc == 1) {
        return 0;
    }

    char buffer[4096];

    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);

        if (fd < 0) {
            cout << "wcat: cannot open file\n";
            exit(1);
        }

        int bytes;
        while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {
            write(STDOUT_FILENO, buffer, bytes);
        }

        close(fd);
    }

    return 0;
}