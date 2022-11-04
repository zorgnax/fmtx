#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

int width = 80;
int center = 0;
int input_word_count = 0;
int output_word_count = 0;
int output_width = 0;
int paragraph_count = 0;
int parsing_glyphs = 0;
int continue_word = 0;

#define BUFSIZE 1024
int buf_len = 0;
char buf[BUFSIZE];

#define WORDSIZE 256
int word_len = 0;
char word[WORDSIZE];

#define INDENTSIZE 256
int indent_len = 0;
int indent_width = 0;
char indent[INDENTSIZE];

#define LINESIZE 80
int line_len = 0;
char line[LINESIZE];

void usage () {
    char str[] =
        "Usage: fmtx [options] file...\n"
        "\n"
        "Options:\n"
        "    -w width  width of line\n"
        "    -c        center on line\n"
        "    -h        help text\n";
    puts(str);
    exit(0);
}

void display_word () {
    if (indent_width >= width) {
        if (output_word_count) {
            printf("\n");
        }
        printf("%.*s%.*s", indent_len, indent, word_len, word);
        output_width = indent_width + word_len;
        output_word_count++;
        return;
    }

    if (output_word_count == 0) {
        printf("%.*s", indent_len, indent);
        output_width = indent_width;
    }
    else if (output_width + 1 + word_len <= width) {
        printf(" ");
        output_width++;
    }
    else if (indent_width + word_len > width &&
            output_width + 1 < width) {
        // the word can't fit on a line by itself
        // and at least one character can fit on the current line
        printf(" ");
        output_width++;
    }
    else {
        printf("\n%.*s", indent_len, indent);
        output_width = indent_width;
    }

    if (output_width + word_len <= width) {
        printf("%.*s", word_len, word);
        output_width += word_len;
    }
    else if (indent_width + word_len <= width) {
        printf("%.*s", word_len, word);
        output_width = indent_width + word_len;
    }
    else {
        int remaining_len = word_len;
        int word_offset = 0;
        while (output_width + remaining_len > width) {
            int chunk_len = width - output_width;
            printf("%.*s\n%.*s", chunk_len, word + word_offset, indent_len, indent);
            output_width = indent_width;
            word_offset += chunk_len;
            remaining_len -= chunk_len;
        }
        printf("%.*s", remaining_len, word + word_offset);
        output_width += remaining_len;
    }
    output_word_count++;
}

void set_indent () {
    memcpy(indent, word, word_len);
    indent_len = word_len;
    paragraph_count++;
    output_word_count = 0;
    indent_width = 0;
    for (int i = 0; i < indent_len; i++) {
        char c = indent[i];
        if (c == '\t') {
            indent_width += 4;
        }
        else {
            indent_width++;
        }
    }
}

void process_buf_center () {
    for (int i = 0; i < buf_len; i++) {
        char c = buf[i];
        if (c == '\n') {
            if (line_len) {
                if (line_len <= LINESIZE) {
                    int indent_len = (width - line_len) / 2;
                    printf("%*s%.*s\n", indent_len, "", line_len, line);
                }
                else {
                    printf("\n");
                }
            }
            else {
                printf("\n");
            }
            input_word_count = 0;
            parsing_glyphs = 0;
            line_len = 0;
        }
        else if (c == ' ' || c == '\t') {
            if (parsing_glyphs) {
                input_word_count++;
                parsing_glyphs = 0;
            }
        }
        else {
            if (!parsing_glyphs) {
                input_word_count++;
                parsing_glyphs = 1;
                if (input_word_count > 1) {
                    if (line_len < LINESIZE) {
                        line[line_len] = ' ';
                        line_len++;
                    }
                    else {
                        if (line_len == LINESIZE) {
                            printf("%.*s", line_len, line);
                            line_len++;
                        }
                        printf(" ");
                    }
                }
            }
            if (line_len < LINESIZE) {
                line[line_len] = c;
                line_len++;
            }
            else {
                if (line_len == LINESIZE) {
                    printf("%.*s", line_len, line);
                    line_len++;
                }
                printf("%c", c);
            }
        }
    }
}

void process_buf_wrap () {
    for (int i = 0; i < buf_len; i++) {
        char c = buf[i];
        if (c == '\n') {
            if (parsing_glyphs) {
                display_word();
                word_len = 0;
                input_word_count++;
            }
            else if (input_word_count == 0) {
                // line is just whitespace
                printf("\n");
                paragraph_count++;
                output_word_count = 0;
            }
            input_word_count = 0;
            parsing_glyphs = 0;
        }
        else if (c == ' ' || c == '\t') {
            // "A" -> " "
            if (parsing_glyphs) {
                display_word();
                word_len = 0;
                input_word_count++;
                parsing_glyphs = 0;
            }
            // * -> " "
            // append the whitespace character to the current word
            if (word_len == WORDSIZE) {
                fprintf(stderr, "Can't be over %d consecutive whitespaces.\n", WORDSIZE);
                exit(1);
            }
            word[word_len++] = c;
        }
        else {
            // " " -> "A"
            if (!parsing_glyphs) {
                if (input_word_count == 0) {
                    if (paragraph_count) {
                        if (word_len == indent_len && strncmp(word, indent, word_len) == 0) {
                            // this line is a part of the current paragraph
                        }
                        else {
                            printf("\n");
                            set_indent();
                        }
                    }
                    else {
                        set_indent();
                    }
                }
                // you don't know if you want to print the whitespace because the
                // following word might be moved to the next line, so the whitespace
                // prior to a word is displayed when the visible word is displayed.
                word_len = 0;
                input_word_count++;
                parsing_glyphs = 1;
            }
            // * -> "A"
            if (word_len == WORDSIZE) {
                // the word is too large for the buffer but we can show what
                // we have and not show whitespace before next word
                display_word();
                continue_word = 1;
                word_len = 0;
                input_word_count++;
            }
            word[word_len++] = c;
        }
    }
}

void process_file (int fd) {
    input_word_count = 0;
    output_word_count = 0;
    paragraph_count = 0;
    parsing_glyphs = 0;

    while (1) {
        int retval = read(fd, buf, BUFSIZE);
        if (retval < 0) {
            fprintf(stderr, "read: %s\n", strerror(errno));
            break;
        }
        else if (retval == 0) {
            if (output_word_count) {
                printf("\n");
            }
            break;
        }
        else {
            buf_len = retval;
            if (center) {
                process_buf_center();
            }
            else {
                process_buf_wrap();
            }
        }
    }

    close(fd);
}

int main (int argc, char **argv) {
    struct option longopts[] = {
        {"w", required_argument, NULL, 'w'},
        {"c", no_argument, NULL, 'c'},
        {"h", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    int c;
    while ((c = getopt_long_only(argc, argv, "", longopts, NULL)) != -1) {
        switch (c) {
        case 'w':
            width = atoi(optarg);
            break;
        case 'c':
            center = 1;
            break;
        case 'h':
            usage();
            break;
        default:
            exit(1);
        }
    }

    if (!isatty(STDIN_FILENO)) {
        process_file(STDIN_FILENO);
    }
    else if (optind == argc) {
        process_file(STDIN_FILENO);
    }
    for (int i = optind; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "Can't open %s: %s\n", argv[i], strerror(errno));
            continue;
        }
        process_file(fd);
    }

    return 0;
}

// TODO
// -w width
// -c center
// make sure if I have to break a word, multibyte characters arent broken up
// -s convert indents to spaces if they contains tabs
// parsing_glyphs to parsing_whitespace
//
