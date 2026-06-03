#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>

using namespace std;

void process_fd(int fd, const string& searchTerm) {
    char buffer[4096];
    string currentLine;
    int bytesRead;

    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        for (int i = 0; i < bytesRead; i++) {
            currentLine += buffer[i];

            if (buffer[i] == '\n') {
                if (currentLine.find(searchTerm) != string::npos) {
                    write(STDOUT_FILENO, currentLine.c_str(), currentLine.size());
                }
                currentLine.clear();
            }
        }
    }

    // handle last line if file does not end with newline
    if (!currentLine.empty()) {
        if (currentLine.find(searchTerm) != string::npos) {
            write(STDOUT_FILENO, currentLine.c_str(), currentLine.size());
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "wgrep: searchterm [file ...]\n";
        exit(1);
    }

    string searchTerm = argv[1];

    if (argc == 2) {
        process_fd(STDIN_FILENO, searchTerm);
        return 0;
    }

    for (int i = 2; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);

        if (fd < 0) {
            cout << "wgrep: cannot open file\n";
            exit(1);
        }

        process_fd(fd, searchTerm);
        close(fd);
    }

    return 0;
}