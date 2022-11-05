#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

int width = 80;
int center = 0;
int paragraph_count = 0;
int paragraph_line_count = 0;
int line_count = 0;
int word_count = 0;
int output_width = 0;
int line_start_len = 0;
int line_start_width = 0;
int line_start_full_width = 0;
int max_indent_width = 0;

#define BUFSIZE 1024
int buf_len = 0;
char buf[BUFSIZE];

#define INDENTSIZE 256
int indent_len = 0;
int indent_width = 0;
char indent[INDENTSIZE];

#define COMMENTSIZE 2
int comment_len = 0;
char comment[COMMENTSIZE];

int line_size = 256;
int line_len = 0;
char *line = NULL;

char *comments[] = {"//", "#"};
int comments_count = sizeof(comments) / sizeof(comments[0]);

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

void process_line_center () {
    int trailing_ws_len = 0;
    for (int i = 0; i < line_len; i++) {
        char c = line[line_len - i - 1];
        if (c == ' ' || c == '\t') {
            trailing_ws_len++;
        }
        else {
            break;
        }
    }
    int trimmed_line_len = line_len - indent_len - trailing_ws_len;
    if (trimmed_line_len > width) {
        printf("%.*s\n", trimmed_line_len, line + indent_len);
    }
    else {
        int center_indent_len = (width - trimmed_line_len) / 2;
        printf("%*s%.*s\n", center_indent_len, "", trimmed_line_len, line + indent_len);
    }
}

void print_prefix () {
    output_width = 0;
    if (indent_len) {
        printf("%.*s", indent_len, indent);
        output_width += indent_width;
    }
    if (comment_len) {
        printf("%.*s ", comment_len, comment);
        output_width += comment_len + 1;
    }
}

void process_word (char *word, int word_len) {
    if (line_start_full_width >= width) {
        if (word_count) {
            printf("\n");
        }
        print_prefix();
        printf("%.*s", word_len, word);
        output_width += word_len;
        return;
    }

    if (word_count == 0) {
        print_prefix();
    }
    else if (output_width + 1 + word_len <= width) {
        printf(" ");
        output_width++;
    }
    else if (line_start_full_width + word_len > width && output_width + 1 < width) {
        // the word can't fit on a line by itself
        // and at least one character can fit on the current line
        printf(" ");
        output_width++;
    }
    else {
        printf("\n");
        print_prefix();
    }

    if (output_width + word_len <= width) {
        printf("%.*s", word_len, word);
        output_width += word_len;
    }
    else {
        int remaining_len = word_len;
        int word_offset = 0;
        while (output_width + remaining_len > width) {
            int chunk_len = width - output_width;
            printf("%.*s\n", chunk_len, word + word_offset);
            print_prefix();
            word_offset += chunk_len;
            remaining_len -= chunk_len;
        }
        printf("%.*s", remaining_len, word + word_offset);
        output_width += remaining_len;
    }
}

void process_line_wrap () {
    int word_len = 0;
    char *word = line + line_start_len;
    for (int i = line_start_len; i < line_len; i++) {
        char c = line[i];
        if (c == ' ' || c == '\t') {
            if (word_len) {
                process_word(word, word_len);
                word_count++;
                word_len = 0;
            }
        }
        else {
            if (!word_len) {
                word = line + i;
            }
            word_len++;
        }
    }
    if (word_len) {
        process_word(word, word_len);
        word_count++;
        word_len = 0;
    }
}

void end_paragraph () {
    if (word_count) {
        printf("\n");
        word_count = 0;
        paragraph_count++;
    }
    paragraph_line_count = 0;
}

void process_line () {
    int cur_indent_len = 0;
    int cur_indent_width = 0;
    for (int i = 0; i < line_len; i++) {
        char c = line[i];
        if (c == ' ') {
            cur_indent_len++;
            cur_indent_width++;
        }
        else if (c == '\t') {
            cur_indent_len++;
            cur_indent_width += 4;
        }
        else {
            break;
        }
    }
    line_start_len = cur_indent_len;
    line_start_width = cur_indent_width;

    char *cur_comment = line + cur_indent_len;
    int cur_comment_len = 0;

    for (int i = 0; i < comments_count; i++) {
        char *pcomment = comments[i];
        int len = strlen(pcomment);
        if (line_len - line_start_len >= len && strncmp(cur_comment, pcomment, len) == 0) {
            cur_comment_len = len;
            break;
        }
    }
    line_start_len += cur_comment_len;
    line_start_width += cur_comment_len;

    // Lines that start with a comment get an extra space before
    // the content of the line.
    line_start_full_width = line_start_width;
    if (cur_comment_len) {
        line_start_full_width += 1;
    }

    if (!center) {
        if (line_count) {
            // if it's an empty line
            if (cur_indent_len == line_len) {
                end_paragraph();
            }

            // if the indents don't match
            else if (cur_indent_len < indent_len || strncmp(indent, line, indent_len) != 0) {
                end_paragraph();
            }

            // if the indent is larger than the allowed maximum the indent can be, for
            // example a line that starts with:
            //     - a list item
            //       more text
            // the "more text" part could be aligned with the "-" or the "a"
            else if (cur_indent_width > max_indent_width) {
                end_paragraph();
            }

            // if the comment starts don't match
            else if (cur_comment_len != comment_len || strncmp(comment, cur_comment, comment_len) != 0) {
                end_paragraph();
            }
        }
        if (cur_indent_len == line_len) {
            printf("\n");
        }
    }

    if (paragraph_line_count == 0) {
        max_indent_width = cur_indent_width;
        if (cur_indent_len > INDENTSIZE) {
            fprintf(stderr, "Indent can't be larger than %d characters\n", INDENTSIZE);
            exit(1);
        }
        memcpy(indent, line, cur_indent_len);
        indent_len = cur_indent_len;
        indent_width = cur_indent_width;

        if (cur_comment_len > COMMENTSIZE) {
            fprintf(stderr, "Comment start can't be larger than %d characters\n", COMMENTSIZE);
            exit(1);
        }
        memcpy(comment, cur_comment, cur_comment_len);
        comment_len = cur_comment_len;
    }

    if (center) {
        process_line_center();
    }
    else {
        process_line_wrap();
    }
    line_count++;
    paragraph_line_count++;
}

void process_buf () {
    for (int i = 0; i < buf_len; i++) {
        char c = buf[i];
        if (c == '\n') {
            process_line();
            line_len = 0;
        }
        else {
            if (line_len >= line_size) {
                line_size *= 2;
                line = realloc(line, line_size);
            }
            line[line_len++] = c;
        }
    }
    if (line_len) {
        process_line();
    }
    end_paragraph();
}

void process_file (int fd) {
    word_count = 0;
    paragraph_count = 0;
    line_len = 0;
    indent_len = 0;

    while (1) {
        int retval = read(fd, buf, BUFSIZE);
        if (retval < 0) {
            fprintf(stderr, "read: %s\n", strerror(errno));
            break;
        }
        else if (retval == 0) {
            break;
        }
        else {
            buf_len = retval;
            process_buf();
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

    line = malloc(line_size);
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
// make sure if I have to break a word, multibyte characters arent broken up
// -s convert indents to spaces if they contains tabs
// bullets
// numbered items
// lettered items
// roman numeraled items

