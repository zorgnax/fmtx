#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

int width = 80;
int center = 0;
int convert_to_spaces = 0;

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

int pindent_len = 0;
int pindent_width = 0;
char pindent[INDENTSIZE];

#define COMMENTSIZE 256
int comment_len = 0;
char comment[COMMENTSIZE];

int pcomment_len = 0;
char pcomment[COMMENTSIZE];

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
        printf("%.*s", pindent_len, pindent);
        output_width += pindent_width;
    }
    if (comment_len) {
        printf("%.*s ", pcomment_len, pcomment);
        output_width += pcomment_len + 1;
    }
}

int get_char_len (char c) {
    int len = 0;
    if ((c & 0x80) == 0x00) { // byte is 0*** ****
        len = 1;
    }
    else if ((c & 0xe0) == 0xc0) { // byte is 110* ****
        len = 2;
    }
    else if ((c & 0xf0) == 0xe0) { // byte is 1110 ****
        len = 3;
    }
    else if ((c & 0xf8) == 0xf0) { // byte is 1111 0***
        len = 4;
    }
    else if ((c & 0xfc) == 0xf8) { // byte is 1111 10**
        len = 5;
    }
    else if ((c & 0xfe) == 0xfc) { // byte is 1111 110*
        len = 6;
    }
    else { // not a valid start byte for utf8
        len = 1;
    }
    return len;
}

int get_str_width (char *str, int len) {
    // Assuming each unicode character is 1 width which is not true
    // but for the most part it is. A character like ☃ is 3 bytes long
    // but 1 character wide.
    int width = 0;
    for (int i = 0; i < len; i++) {
        char c = str[i];
        int clen = get_char_len(c);
        width += 1;
        i += clen - 1;
    }
    return width;
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

    int word_width = get_str_width(word, word_len);

    if (word_count == 0) {
        print_prefix();
    }
    else if (output_width + 1 + word_width <= width) {
        printf(" ");
        output_width++;
    }
    else if (line_start_full_width + word_width > width && output_width + 1 < width) {
        // the word can't fit on a line by itself
        // and at least one character can fit on the current line
        if (output_width > line_start_full_width) {
            printf(" ");
            output_width++;
        }
    }
    else {
        printf("\n");
        print_prefix();
    }

    if (output_width + word_width <= width) {
        printf("%.*s", word_len, word);
        output_width += word_width;
    }
    else {
        for (int i = 0; i < word_len; i++) {
            int char_len = get_char_len(word[i]);
            printf("%.*s", char_len, word + i);
            i += char_len - 1;
            output_width += 1;
            if (output_width >= width) {
                printf("\n");
                print_prefix();
            }
        }
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

void process_indent () {
    if (convert_to_spaces) {
        pindent_len = 0;
        for (int i = 0; i < indent_len; i++) {
            char c = indent[i];
            if (c == '\t') {
                pindent_len += 4;
            }
            else {
                pindent_len += 1;
            }
        }
        if (pindent_len > INDENTSIZE) {
            fprintf(stderr, "Indent can't be larger than %d characters\n", INDENTSIZE);
            exit(1);
        }
        for (int i = 0; i < pindent_len; i++) {
            pindent[i] = ' ';
        }
    }
    else {
        memcpy(pindent, indent, indent_len);
        pindent_len = indent_len;
        pindent_width = indent_width;
    }
}

void process_comment () {
    // for comments like "> >>" we collapse the spaces to ">>>"
    pcomment_len = 0;
    for (int i = 0; i < comment_len; i++) {
        char c = comment[i];
        if (c != ' ') {
            pcomment[pcomment_len++] = c;
        }
    }
}

int parse_comment (char *str, int str_len) {
    // simple comments are ones that start with one of the strings
    // in the comments array like # or //
    int comment_len = 0;
    for (int i = 0; i < comments_count; i++) {
        char *pcomment = comments[i];
        int pcomment_len = strlen(pcomment);
        if (pcomment_len <= str_len && strncmp(str, pcomment, pcomment_len) == 0) {
            comment_len = pcomment_len;
            return comment_len;
        }
    }

    // this script also considers mail quotations like > >> as comments
    for (int i = 0; i < str_len; i++) {
        char c = str[i];
        if (c == '>' || c == ' ') {
            comment_len++;
        }
        else {
            break;
        }
    }
    for (int i = comment_len - 1; i >= 0; i--) {
        char c = str[i];
        if (c == ' ') {
            comment_len--;
        }
        else {
            break;
        }
    }
    return comment_len;
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
    int cur_comment_len = parse_comment(cur_comment, line_len - line_start_len);

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
        process_indent();

        if (cur_comment_len > COMMENTSIZE) {
            fprintf(stderr, "Comment start can't be larger than %d characters\n", COMMENTSIZE);
            exit(1);
        }
        memcpy(comment, cur_comment, cur_comment_len);
        comment_len = cur_comment_len;
        process_comment();
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
        {"s", no_argument, NULL, 's'},
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
        case 's':
            convert_to_spaces = 1;
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
// bullets
// numbered items
// lettered items
// roman numeraled items
// test program
// mail quotations that start with any number of "> >> >" collapse that into ">>>> "
//
