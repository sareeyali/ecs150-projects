#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <fstream>

using namespace std;

// required error message
static const char error_message[] = "An error has occurred\n";

// print error to stderr
void print_error() {
    write(STDERR_FILENO, error_message, strlen(error_message));
}

// remove leading/trailing spaces
string trim(const string &s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

// split string by spaces
vector<string> split_whitespace(const string &s) {
    vector<string> tokens;
    istringstream iss(s);
    string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// store a parsed command
struct Command {
    vector<string> args;
    string output_file;
    bool has_redirection = false;
};

// parse a single command (handle >)
bool parse_single_command(const string &segment, Command &cmd) {
    cmd.args.clear();
    cmd.output_file = "";
    cmd.has_redirection = false;

    int redir_count = 0;
    for (char c : segment) {
        if (c == '>') redir_count++;
    }

    if (redir_count > 1) return false;

    // no redirection case
    if (redir_count == 0) {
        string left = trim(segment);
        if (left.empty()) return false;
        cmd.args = split_whitespace(left);
        return !cmd.args.empty();
    }

    // split left and right of >
    size_t pos = segment.find('>');
    string left = trim(segment.substr(0, pos));
    string right = trim(segment.substr(pos + 1));

    if (left.empty() || right.empty()) return false;

    vector<string> right_tokens = split_whitespace(right);
    if (right_tokens.size() != 1) return false;

    cmd.args = split_whitespace(left);
    if (cmd.args.empty()) return false;

    cmd.has_redirection = true;
    cmd.output_file = right_tokens[0];
    return true;
}

// split commands by &
bool split_parallel_commands(const string &line, vector<string> &segments) {
    segments.clear();

    string current;
    bool saw_ampersand = false;

    for (char c : line) {
        if (c == '&') {
            string piece = trim(current);
            if (piece.empty()) return false;
            segments.push_back(piece);
            current.clear();
            saw_ampersand = true;
        } else {
            current += c;
        }
    }

    string last = trim(current);
    if (saw_ampersand && last.empty()) return false;

    if (!last.empty()) {
        segments.push_back(last);
    }

    return true;
}

// search for executable in path
string find_executable(const string &cmd, const vector<string> &paths) {
    for (const string &dir : paths) {
        string full = dir + "/" + cmd;
        if (access(full.c_str(), X_OK) == 0) {
            return full;
        }
    }
    return "";
}

// check if built-in
bool is_builtin(const string &cmd) {
    return (cmd == "exit" || cmd == "cd" || cmd == "path");
}

// handle built-in commands
bool handle_builtin(const Command &cmd, vector<string> &paths, int total_commands) {
    const string &name = cmd.args[0];

    if (cmd.has_redirection) {
        print_error();
        return true;
    }

    // exit command
    if (name == "exit") {
        if (cmd.args.size() != 1 || total_commands != 1) {
            print_error();
            return true;
        }
        exit(0);
    }

    // cd command
    if (name == "cd") {
        if (cmd.args.size() != 2) {
            print_error();
            return true;
        }
        if (chdir(cmd.args[1].c_str()) != 0) {
            print_error();
        }
        return true;
    }

    // path command
    if (name == "path") {
        paths.clear();
        for (size_t i = 1; i < cmd.args.size(); i++) {
            paths.push_back(cmd.args[i]);
        }
        return true;
    }

    return false;
}

// run external command using fork + execv
void execute_external(const Command &cmd, const vector<string> &paths, vector<pid_t> &children) {
    if (paths.empty()) {
        print_error();
        return;
    }

    string executable = find_executable(cmd.args[0], paths);
    if (executable.empty()) {
        print_error();
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        print_error();
        return;
    }

    // child process
    if (pid == 0) {
        if (cmd.has_redirection) {
            int fd = open(cmd.output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd < 0) {
                print_error();
                exit(1);
            }

            // redirect stdout + stderr
            if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) {
                print_error();
                close(fd);
                exit(1);
            }
            close(fd);
        }

        // build argv for execv
        vector<char*> argv;
        for (const string &arg : cmd.args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execv(executable.c_str(), argv.data());

        // exec only returns on failure
        print_error();
        exit(1);
    } else {
        children.push_back(pid);
    }
}

// process a full input line
void process_line(const string &line, vector<string> &paths) {
    string cleaned = trim(line);
    if (cleaned.empty()) return;

    vector<string> segments;
    if (!split_parallel_commands(cleaned, segments)) {
        print_error();
        return;
    }

    vector<Command> commands;
    for (const string &seg : segments) {
        Command cmd;
        if (!parse_single_command(seg, cmd)) {
            print_error();
            return;
        }
        commands.push_back(cmd);
    }

    vector<pid_t> children;

    // run each command
    for (const Command &cmd : commands) {
        if (is_builtin(cmd.args[0])) {
            handle_builtin(cmd, paths, (int)commands.size());
        } else {
            execute_external(cmd, paths, children);
        }
    }

    // wait for all children
    for (pid_t pid : children) {
        waitpid(pid, nullptr, 0);
    }
}

// main shell loop
int main(int argc, char *argv[]) {
    vector<string> paths;
    paths.push_back("/bin");

    istream *input = &cin;
    ifstream batch_file;

    // error if too many args
    if (argc > 2) {
        print_error();
        exit(1);
    }

    // batch mode
    if (argc == 2) {
        batch_file.open(argv[1]);
        if (!batch_file.is_open()) {
            print_error();
            exit(1);
        }
        input = &batch_file;
    }

    string line;

    // shell loop
    while (true) {
        if (argc == 1) {
            cout << "wish> ";
            cout.flush();
        }

        if (!getline(*input, line)) {
            exit(0);
        }

        process_line(line, paths);
    }

    return 0;
}