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
int line_count = 0;
int output_width = 0;

#define BUFSIZE 1024
int buf_len = 0;
char buf[BUFSIZE];

int line_size = 256;
int line_len = 0;
char *line = NULL;

#define INDENTSIZE 256
#define COMMENTSIZE 256
#define ITEMSIZE 128

typedef struct {
    int line_count;
    int word_count;
    int prefix_count;
    int only_whitespace;
    int line_start_len;
    int line_start_width;

    int indent_len;
    int indent_width;
    char indent[INDENTSIZE];

    int pindent_len;
    int pindent_width;
    char pindent[INDENTSIZE];

    int comment_len;
    char comment[COMMENTSIZE];

    int pcomment_len;
    char pcomment[COMMENTSIZE];

    int item_len;
    char item[ITEMSIZE];

} para_t;
para_t para = {0};

void usage () {
    char str[] =
        "Usage: fmtx [options] file...\n"
        "\n"
        "Options:\n"
        "    -c        center on line\n"
        "    -h        help text\n"
        "    -s        replace tabs with spaces\n"
        "    -w width  width of line\n";
    puts(str);
    exit(0);
}

void process_line_center () {
    int indent_len = 0;
    for (int i = 0; i < line_len; i++) {
        char c = line[i];
        if (c == ' ' || c == '\t') {
            indent_len++;
        }
        else {
            break;
        }
    }
    int trailing_len = 0;
    for (int i = line_len - 1; i >= 0; i--) {
        char c = line[i];
        if (c == ' ' || c == '\t') {
            trailing_len++;
        }
        else {
            break;
        }
    }
    int trimmed_len = line_len - indent_len - trailing_len;
    if (trimmed_len > width) {
        printf("%.*s\n", trimmed_len, line + indent_len);
    }
    else {
        int center_len = (width - trimmed_len) / 2;
        printf("%*s%.*s\n", center_len, "", trimmed_len, line + indent_len);
    }
    line_count++;
}

void print_prefix_nospace () {
    output_width = 0;
    if (para.pindent_len) {
        printf("%.*s", para.pindent_len, para.pindent);
        output_width += para.pindent_width;
    }
    if (para.pcomment_len) {
        printf("%.*s", para.pcomment_len, para.pcomment);
        output_width += para.pcomment_len;
    }
    if (para.item_len) {
        if (para.prefix_count) {
            printf("%*s", para.item_len, "");
        }
        else {
            printf("%.*s", para.item_len, para.item);
        }
        output_width += para.item_len;
    }
    para.prefix_count++;
}

void print_prefix () {
    print_prefix_nospace();
    if (para.pcomment_len || para.item_len) {
        printf(" ");
        output_width += 1;
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
    if (para.line_start_width >= width) {
        if (para.word_count) {
            printf("\n");
        }
        print_prefix();
        printf("%.*s", word_len, word);
        output_width += word_len;
        return;
    }

    int word_width = get_str_width(word, word_len);
    if (para.word_count == 0) {
        print_prefix();
    }

    // if the word can fit on the current line after a space
    else if (output_width + 1 + word_width <= width) {
        // if were not at the start of the line
        if (output_width > para.line_start_width) {
            printf(" ");
            output_width++;
        }
    }

    // the word can't fit on a line by itself
    // and at least one character can fit on the current line
    else if (para.line_start_width + word_width > width && output_width + 1 < width) {
        // if were not at the start of the line
        if (output_width > para.line_start_width) {
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
            if (output_width >= width) {
                printf("\n");
                print_prefix();
            }
            int char_len = get_char_len(word[i]);
            if (i + char_len > word_len) {
                // the character says it should be longer than it is
                char_len = 1;
            }
            printf("%.*s", char_len, word + i);
            i += char_len - 1;
            output_width += 1;
        }
    }
}

void process_line_words () {
    int word_len = 0;
    char *word = line + para.line_start_len;
    for (int i = para.line_start_len; i < line_len; i++) {
        char c = line[i];
        if (c == ' ' || c == '\t') {
            if (word_len) {
                process_word(word, word_len);
                para.word_count++;
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
        para.word_count++;
        word_len = 0;
    }
}

void end_paragraph () {
    if (para.word_count) {
        printf("\n");
    }
    else if (para.only_whitespace) {
        for (int i = 0; i < para.line_count; i++) {
            printf("\n");
        }
    }
    else {
        print_prefix_nospace();
        printf("\n");
    }
    para.word_count = 0;
    para.line_count = 0;
    para.prefix_count = 0;
    paragraph_count++;
}

void process_indent () {
    if (convert_to_spaces) {
        para.pindent_len = 0;
        for (int i = 0; i < para.indent_len; i++) {
            char c = para.indent[i];
            if (c == '\t') {
                para.pindent_len += 4;
            }
            else {
                para.pindent_len += 1;
            }
        }
        if (para.pindent_len > INDENTSIZE) {
            fprintf(stderr, "Indent can't be larger than %d characters\n", INDENTSIZE);
            exit(1);
        }
        for (int i = 0; i < para.pindent_len; i++) {
            para.pindent[i] = ' ';
        }
    }
    else {
        memcpy(para.pindent, para.indent, para.indent_len);
        para.pindent_len = para.indent_len;
        para.pindent_width = para.indent_width;
    }
}

void process_comment () {
    // for comments like "> >>" we collapse the spaces to ">>>"
    para.pcomment_len = 0;
    for (int i = 0; i < para.comment_len; i++) {
        char c = para.comment[i];
        if (c != ' ') {
            para.pcomment[para.pcomment_len++] = c;
        }
    }
}

int parse_comment (char *str, int str_len) {
    // simple comments are ones that start with // or #
    int comment_len = 0;
    if (str_len >= 2 && strncmp(str, "//", 2) == 0) {
        comment_len = 2;
    }
    else if (str_len >= 1 && strncmp(str, "#", 1) == 0) {
        comment_len = 1;
    }
    if (comment_len) {
        return comment_len;
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

int parse_item (char *str, int str_len) {
    if (str_len >= 2 && strncmp(str, "- ", 2) == 0) {
        return 1;
    }
    else if (str_len >= 2 && strncmp(str, "* ", 2) == 0) {
        return 1;
    }
    else if (str_len < 3) {
        return 0;
    }

    int item_len = 0;
    int initial = 0;
    int period = 0;
    int space = 0;
    for (int i = 0; i < str_len; i++) {
        char c = str[i];
        if (c >= '0' && c <= '9') {
            initial++;
        }
        else {
            break;
        }
    }
    if (!initial) {
        for (int i = 0; i < str_len; i++) {
            char c = str[i];
            if (c >= 'a' && c <= 'z') {
                initial++;
            }
            else {
                break;
            }
        }
    }
    if (!initial) {
        for (int i = 0; i < str_len; i++) {
            char c = str[i];
            if (c >= 'A' && c <= 'Z') {
                initial++;
            }
            else {
                break;
            }
        }
    }
    if (!initial) {
        return 0;
    }
    item_len = initial;
    if (item_len < str_len && str[item_len] == '.') {
        period = 1;
        item_len++;
    }
    if (!period) {
        return 0;
    }
    if (item_len < str_len && str[item_len] == ' ') {
        space = 1;
    }
    if (!space) {
        return 0;
    }
    return item_len;
}

void process_line_wrap () {
    char *indent = line;
    int indent_len = 0;
    int indent_width = 0;
    for (int i = 0; i < line_len; i++) {
        char c = line[i];
        if (c == ' ') {
            indent_len++;
            indent_width++;
        }
        else if (c == '\t') {
            indent_len++;
            indent_width += 4;
        }
        else {
            break;
        }
    }

    char *comment = line + indent_len;
    int comment_len = parse_comment(comment, line_len - indent_len);
    int line_start_len = indent_len + comment_len;

    char *item = line + line_start_len;
    int item_len = 0;
    if (comment_len == 0) {
        item_len = parse_item(item, line_len - line_start_len);
        line_start_len += item_len;
    }

    int only_whitespace = 0;
    if (indent_len == line_len) {
        only_whitespace = 1;
    }

    int expected_indent_width = para.indent_width;
    if (para.item_len) {
        expected_indent_width += para.item_len + 1;
    }

    int end = 0;
    if (line_count) {
        // if whitespace only para to a regular para and vice versa
        if (only_whitespace ^ para.only_whitespace) {
            end = 1;
        }

        // if the indents don't match
        else if (indent_len < para.indent_len || strncmp(indent, para.indent, para.indent_len) != 0) {
            end = 1;
        }

        // if the indent is not equal to the full width of the line start, for
        // example a line that starts with:
        //     - a list item
        //       more text
        // the "more text" part is considered still a part of the list item
        else if (indent_width != expected_indent_width) {
            end = 1;
        }

        // if the comment starts don't match
        else if (comment_len != para.comment_len || strncmp(comment, para.comment, para.comment_len) != 0) {
            end = 1;
        }

        // if the line starts a new item
        else if (item_len) {
            end = 1;
        }

        if (end) {
            end_paragraph();
        }
    }

    if (para.line_count == 0) {
        if (indent_len > INDENTSIZE) {
            fprintf(stderr, "Indent can't be larger than %d characters\n", INDENTSIZE);
            exit(1);
        }
        memcpy(para.indent, indent, indent_len);
        para.indent_len = indent_len;
        para.indent_width = indent_width;
        process_indent();

        if (comment_len > COMMENTSIZE) {
            fprintf(stderr, "Comment start can't be larger than %d characters\n", COMMENTSIZE);
            exit(1);
        }
        memcpy(para.comment, comment, comment_len);
        para.comment_len = comment_len;
        process_comment();

        if (item_len > ITEMSIZE) {
            fprintf(stderr, "Item start can't be larger than %d characters\n", ITEMSIZE);
            exit(1);
        }
        memcpy(para.item, item, item_len);
        para.item_len = item_len;

        // line_start_len is the input len that does not include a space after
        // // (or # or 1.) but line_start_width is the output line start width
        // that does include the space.
        int line_start_width = para.pindent_width + para.pcomment_len + para.item_len;
        if (comment_len || item_len) {
            line_start_width++;
        }

        para.line_start_len = line_start_len;
        para.line_start_width = line_start_width;
        para.only_whitespace = only_whitespace;
    }

    process_line_words();

    line_count++;
    para.line_count++;
}

void process_buf () {
    for (int i = 0; i < buf_len; i++) {
        char c = buf[i];
        if (c == '\n') {
            if (center) {
                process_line_center();
            }
            else {
                process_line_wrap();
            }
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
}

void process_file (int fd) {
    line_count = 0;
    paragraph_count = 0;
    memset(&para, 0, sizeof(para));

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
    if (line_len) {
        if (center) {
            process_line_center();
        }
        else {
            process_line_wrap();
        }
    }
    if (!center) {
        end_paragraph();
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

