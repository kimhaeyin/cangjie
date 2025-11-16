#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXTOK 512
#define MAX_ID_LEN 255
#define MAX_NUM_LEN 255

int line = 1;
int col  = 0;

int getc_track(FILE *fp) {
    int c = fgetc(fp);
    if (c == '\n') {
        line++;
        col = 0;
    } else {
        col++;
    }
    return c;
}

void ungetc_track(int c, FILE *fp) {
    if (c == EOF) return;
    ungetc(c, fp);
    if (c == '\n') {
        line--;
        col = 0;
    } else {
        col--;
    }
}

const char *keywords[] = {
    "int","float","char","if","else","while","for","return",
    "void","main","break","continue","var","let"
};
const int n_keywords = sizeof(keywords)/sizeof(keywords[0]);

int is_keyword(const char *s) {
    for (int i = 0; i < n_keywords; ++i)
        if (strcmp(s, keywords[i]) == 0) return 1;
    return 0;
}

void out_keyword(const char *s, int ln, int cl) { printf("%-8s %-15s (%3d,%-3d)\n", s, s, ln, cl); }
void out_op(const char *s, int ln, int cl)      { printf("%-8s %-15s (%3d,%-3d)\n", s, s, ln, cl); }
void out_id(const char *s, int ln, int cl)      { printf("%-8s %-15s (%3d,%-3d)\n", "ID", s, ln, cl); }
void out_num(const char *s, int ln, int cl)     { printf("%-8s %-15s (%3d,%-3d)\n", "NUM", s, ln, cl); }
void out_error(const char *s, int ln, int cl)   { printf("%-8s %-15s (%3d,%-3d)\n", "ERROR", s, ln, cl); }
void out_error_msg(const char *msg, int ln, int cl) { printf("%-8s %-15s (%3d,%-3d)\n", "ERROR", msg, ln, cl); }

int is_op_start(char c) {
    const char *ops = ":+-*/%=!<>&|^~";
    return strchr(ops, c) != NULL;
}

int match_two_char_op(char a, char b, char *out) {
    const char *two_ops[] = {
        "==","!=","<=",">=","++","--","&&","||",
        "+=","-=","*=","/=","%=","<<",">>"
    };
    char s[3] = {a, b, 0};
    for (size_t i = 0; i < sizeof(two_ops)/sizeof(two_ops[0]); ++i)
        if (strcmp(s, two_ops[i]) == 0) { strcpy(out, s); return 1; }
    return 0;
}

int main(int argc, char *argv[]) {

    const char *filename = "test.c.txt";
    if (argc >= 2) filename = argv[1];

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "无法打开文件：%s\n", filename);
        return 1;
    }

    int c;
    while ((c = getc_track(fp)) != EOF) {

        if (isspace(c)) continue;

        /* ------------------------- 注释 ------------------------- */
        if (c == '/') {
            int nx = getc_track(fp);
            if (nx == '*') {
                /* 块注释 */
                int prev = 0, closed = 0;
                int start_line = line, start_col = col;

                while ((c = getc_track(fp)) != EOF) {
                    if (prev == '*' && c == '/') { closed = 1; break; }
                    prev = c;
                }
                if (!closed)
                    out_error_msg("Unclosed_comment", start_line, start_col);
                continue;
            }
            else if (nx == '/') {
                /* 行注释 */
                while ((c = getc_track(fp)) != EOF && c != '\n');
                continue;
            }
            else {
                ungetc_track(nx, fp);
                out_op("/", line, col);
                continue;
            }
        }

        /* ------------------------- 标识符或关键字 ------------------------- */
        if (isalpha(c) || c == '_') {
            char buf[MAXTOK];
            int idx = 0;
            int too_long = 0;
            int start_line = line, start_col = col;

            buf[idx++] = (char)c;

            while ((c = getc_track(fp)) != EOF && (isalnum(c) || c == '_')) {
                if (idx < MAXTOK-1) buf[idx++] = (char)c;
                if (idx > MAX_ID_LEN) too_long = 1;
            }
            buf[idx] = '\0';
            if (c != EOF) ungetc_track(c, fp);

            if (too_long) {
                out_error_msg("Identifier_too_long", start_line, start_col);
                continue;
            }

            if (is_keyword(buf)) out_keyword(buf, start_line, start_col);
            else out_id(buf, start_line, start_col);
            continue;
        }

        /* ------------------------- 数字 ------------------------- */
        if (isdigit(c)) {
            char buf[MAXTOK];
            int idx = 0;
            int has_dot = 0, illegal = 0, too_long = 0;
            int start_line = line, start_col = col;

            buf[idx++] = (char)c;

            while ((c = getc_track(fp)) != EOF) {
                if (isdigit(c)) {
                    if (idx < MAXTOK-1) buf[idx++] = (char)c;
                }
                else if (c == '.' && !has_dot) {
                    has_dot = 1;
                    buf[idx++] = (char)c;
                }
                else if (isalpha(c) || c == '_') {
                    illegal = 1;
                    buf[idx++] = (char)c;
                }
                else break;

                if (idx > MAX_NUM_LEN) too_long = 1;
            }

            buf[idx] = '\0';
            if (c != EOF) ungetc_track(c, fp);

            if (too_long) {
                out_error_msg("Number_too_long", start_line, start_col);
                continue;
            }
            if (illegal) {
                out_error(buf, start_line, start_col);
                continue;
            }

            out_num(buf, start_line, start_col);
            continue;
        }

        /* ------------------------- 字符常量 ------------------------- */
        if (c == '\'') {
            int prev = 0, closed = 0;
            int start_line = line, start_col = col;

            while ((c = getc_track(fp)) != EOF) {
                if (c == '\'' && prev != '\\') { closed = 1; break; }
                prev = (c == '\\') ? '\\' : 0;
            }

            if (!closed)
                out_error_msg("Unclosed_char", start_line, start_col);

            continue;
        }

        /* ------------------------- 运算符 ------------------------- */
        if (is_op_start(c)) {
            int nx = getc_track(fp);
            char two[4] = {0};
            if (nx != EOF) {
                if (match_two_char_op((char)c, (char)nx, two)) {
                    out_op(two, line, col - 1);
                    continue;
                } else {
                    ungetc_track(nx, fp);
                }
            }
            char s[2] = {(char)c, 0};
            out_op(s, line, col);
            continue;
        }

        /* ------------------------- 界符 ------------------------- */
        if (strchr(";,(){}[]", c)) {
            char s[2] = {(char)c, 0};
            out_op(s, line, col);
            continue;
        }

        /* ------------------------- 其他都是错误 ------------------------- */
        char tmp[2] = {(char)c, 0};
        out_error(tmp, line, col);
    }

    fclose(fp);
    return 0;
}
