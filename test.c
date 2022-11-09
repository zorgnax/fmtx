#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

int test_count = 0;

int trim (char *str, int len, char *start_chars, char *end_chars) {
    int start = 0;
    for (int i = 0; i < len; i++) {
        char c = str[i];
        if (strchr(start_chars, c)) {
            start++;
        }
        else {
            break;
        }
    }
    int end = 0;
    for (int i = len - 1; i > start; i--) {
        char c = str[i];
        if (strchr(end_chars, c)) {
            end++;
        }
        else {
            break;
        }
    }
    int new_len = len - start - end;
    memmove(str, str + start, new_len);
    str[new_len] = '\0';
    return new_len;
}

void make_argv (char **argv, int size, char *str, int str_len) {
    char *word = str;
    int word_len = 0;
    int argc = 0;
    for (int i = 0; i < str_len + 1; i++) {
        char c = str[i];
        if (c == ' ' || c == '\t' || c == '\0') {
            if (word_len) {
                str[i] = '\0';
                if (argc < size) {
                    argv[argc++] = word;
                }
                word_len = 0;
            }
        }
        else {
            if (!word_len) {
                word = str + i;
            }
            word_len++;
        }
    }
    argv[argc] = NULL;
}

void test_input (char *cmd, int cmd_len, char *input, int input_len, char *expected, int expected_len) {
    input_len = trim(input, input_len, "", " \n");
    strcpy(input + input_len, "\n");
    input_len++;
    expected_len = trim(expected, expected_len, "", " \n");
    strcpy(expected + expected_len, "\n");
    expected_len++;
    char output[1024] = "";
    int output_len = 0;
    int output_size = sizeof(output);

    int pipe_in_fd[2]; // data flows into the fmtx program
    int pipe_out_fd[2]; // data flows out of the fmtx program

    int retval = pipe(pipe_in_fd);
    if (retval < 0) {
        fprintf(stderr, "Can't open pipe: %s\n", strerror(errno));
        exit(1);
    }

    retval = pipe(pipe_out_fd);
    if (retval < 0) {
        fprintf(stderr, "Can't open pipe: %s\n", strerror(errno));
        exit(1);
    }

    int pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Can't fork: %s\n", strerror(errno));
        exit(1);
    }
    else if (pid == 0) {
        // child process
        close(pipe_in_fd[1]);
        close(pipe_out_fd[0]);
        dup2(pipe_in_fd[0], 0);
        dup2(pipe_out_fd[1], 1);

        char *argv[8];
        make_argv(argv, 8, cmd, cmd_len);
        argv[0] = "./fmtx";
        execv(argv[0], argv);
        exit(1);
    }
    else {
        // parent process
        close(pipe_in_fd[0]);
        close(pipe_out_fd[1]);

        while (1) {
            int written_len = 0;
            int retval = write(pipe_in_fd[1], input + written_len, input_len - written_len);
            if (retval < 0) {
                fprintf(stderr, "write: %s\n", strerror(errno));
                break;
            }
            else {
                written_len += retval;
                if (written_len == input_len) {
                    break;
                }
            }
        }
        close(pipe_in_fd[1]);

        while (1) {
            int retval = read(pipe_out_fd[0], output + output_len, output_size - output_len);
            if (retval < 0) {
                fprintf(stderr, "read: %s\n", strerror(errno));
                break;
            }
            else if (retval == 0) {
                break;
            }
            else {
                output_len += retval;
                if (output_len == output_size) {
                    break;
                }
            }
        }
        close(pipe_out_fd[0]);

        if (output_len != expected_len || strncmp(output, expected, expected_len) != 0) {
            printf("not ok %d\n", test_count);
            printf("input:\n%.*s\n", input_len, input);
            printf("expected:\n%.*s\n", expected_len, expected);
            printf("but got:\n%.*s\n", output_len, output);
        }
        else {
            printf("ok %d\n", test_count);
        }
    }
}

int main (int argc, char **argv) {
    char file[] = "test.txt";
    FILE *fh = fopen(file, "r");
    if (!fh) {
        fprintf(stderr, "Can't open \"%s\": %s\n", file, strerror(errno));
        exit(1);
    }
    char line[256];
    int line_len = 0;
    char cmd[256] = "";
    int cmd_len = 0;
    char input[1024] = "";
    int input_len = 0;
    char expected[1024] = "";
    int expected_len = 0;
    char *active;

    while (fgets(line, sizeof(line), fh)) {
        line_len = strlen(line);
        if (line[line_len - 1] == '\n') {
            line[line_len - 1] = '\0';
            line_len--;
        }
        if (strncmp(line, "[fmt", 4) == 0) {
            if (test_count) {
                test_input(cmd, cmd_len, input, input_len, expected, expected_len);
            }
            test_count++;
            strcpy(cmd, line);
            cmd_len = trim(cmd, line_len, "[ ", " ]");
            active = input;
            strcpy(input, "");
            input_len = 0;
            strcpy(expected, "");
            expected_len = 0;
        }
        else if (strncmp(line, "---", 3) == 0) {
            active = expected;
        }
        else if (active == expected) {
            if (expected_len + line_len + 2 < sizeof(expected)) {
                strncpy(expected + expected_len, line, line_len);
                expected_len += line_len;
                strcpy(expected + expected_len, "\n");
                expected_len++;
            }
        }
        else {
            if (input_len + line_len + 2 < sizeof(input)) {
                strncpy(input + input_len, line, line_len);
                input_len += line_len;
                strcpy(input + input_len, "\n");
                input_len++;
            }
        }
    }
    if (test_count) {
        test_input(cmd, cmd_len, input, input_len, expected, expected_len);
    }
    fclose(fh);
}

