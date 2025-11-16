#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXTOK 512
#define MAX_ID_LEN 255
#define MAX_NUM_LEN 255

/* 关键字表 */
const char *keywords[] = {
    "int","float","char","if","else","while","for","return",
    "void","main","break","continue"
};
const int n_keywords = sizeof(keywords)/sizeof(keywords[0]);

int is_keyword(const char *s) {
    for (int i = 0; i < n_keywords; ++i)
        if (strcmp(s, keywords[i]) == 0) return 1;
    return 0;
}

void out_keyword(const char *s) { printf("%s %s\n", s, s); }
void out_op(const char *s)      { printf("%s %s\n", s, s); }
void out_id(const char *s)      { printf("ID %s\n", s); }
void out_num(const char *s)     { printf("NUM %s\n", s); }
void out_error(const char *s)   { printf("ERROR %s\n", s); }

/* 输出指定错误消息，如 Identifier_too_long */
void out_error_msg(const char *msg) {
    printf("ERROR %s\n", msg);
}

/* 判断是否为运算符起始字符 */
int is_op_start(char c) {
    const char *ops = "+-*/%=!<>&|^~";
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
    const char *filename = "C://Users//21557//Desktop//编译原理//project//test.c.txt";
    if (argc >= 2) filename = argv[1];

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "无法打开文件：%s\n", filename);
        return 1;
    }

    int c;
    while ((c = fgetc(fp)) != EOF) {

        if (isspace(c)) continue;

        /* 处理注释 */
        if (c == '/') {
            int nx = fgetc(fp);

            if (nx == '*') {
                /* 块注释 */
                int prev = 0, closed = 0;
                while ((c = fgetc(fp)) != EOF) {
                    if (prev == '*' && c == '/') { closed = 1; break; }
                    prev = c;
                }
                if (!closed) out_error_msg("Unclosed_comment");
                continue;
            }
            else if (nx == '/') {
                while ((c = fgetc(fp)) != EOF && c != '\n');
                continue;
            }
            else {
                if (nx != EOF) ungetc(nx, fp);
                out_op("/");
                continue;
            }
        }

        /* 标识符/关键字 */
        if (isalpha(c) || c == '_') {
            char buf[MAXTOK]; int idx = 0;
            int too_long = 0;

            buf[idx++] = (char)c;

            while ((c = fgetc(fp)) != EOF && (isalnum(c) || c == '_')) {
                if (idx < MAXTOK-1)
                    buf[idx++] = (char)c;
                if (idx > MAX_ID_LEN) too_long = 1;
            }
            buf[idx] = '\0';
            if (c != EOF) ungetc(c, fp);

            if (too_long) {
                out_error_msg("Identifier_too_long");
                continue;
            }

            if (is_keyword(buf)) out_keyword(buf);
            else out_id(buf);

            continue;
        }

        /* 数字 */
        if (isdigit(c)) {
            char buf[MAXTOK]; int idx = 0;
            int has_dot = 0, illegal = 0, too_long = 0;

            buf[idx++] = (char)c;

            while ((c = fgetc(fp)) != EOF) {
                if (isdigit(c)) {
                    if (idx < MAXTOK-1) buf[idx++] = (char)c;
                }
                else if (c == '.' && !has_dot) {
                    has_dot = 1;
                    if (idx < MAXTOK-1) buf[idx++] = (char)c;
                }
                else if (isalpha(c) || c == '_') {
                    illegal = 1;
                    if (idx < MAXTOK-1) buf[idx++] = (char)c;
                }
                else break;

                if (idx > MAX_NUM_LEN) too_long = 1;
            }

            if (illegal) {
                while (c != EOF && (isalnum(c) || c == '_')) {
                    if (idx < MAXTOK-1) buf[idx++] = (char)c;
                    c = fgetc(fp);
                }
            }

            buf[idx] = '\0';
            if (c != EOF) ungetc(c, fp);

            if (too_long) {
                out_error_msg("Number_too_long");
                continue;
            }
            if (illegal) {
                out_error(buf);
                continue;
            }

            out_num(buf);
            continue;
        }

        /* 字符串 */
        if (c == '"') {
            int prev = 0, closed = 0;
            while ((c = fgetc(fp)) != EOF) {
                if (c == '"' && prev != '\\') { closed = 1; break; }
                prev = (c == '\\' ? '\\' : 0);
            }
            if (!closed) out_error_msg("Unclosed_string");
            continue;
        }

        /* 字符常量 */
        if (c == '\'') {
            int prev = 0, closed = 0;
            while ((c = fgetc(fp)) != EOF) {
                if (c == '\'' && prev != '\\') { closed = 1; break; }
                prev = (c == '\\' ? '\\' : 0);
            }
            if (!closed) out_error_msg("Unclosed_char");
            continue;
        }

        /* 运算符 */
        if (is_op_start(c)) {
            int nx = fgetc(fp);
            char two[4] = {0};
            if (nx != EOF) {
                if (match_two_char_op((char)c, (char)nx, two)) {
                    out_op(two);
                    continue;
                } else {
                    ungetc(nx, fp);
                }
            }
            char s[2] = {(char)c, 0};
            out_op(s);
            continue;
        }

        /* 界符 */
        if (strchr(";,(){}[]", c)) {
            char s[2] = {(char)c, 0};
            out_op(s);
            continue;
        }

        /* 其他全部 ERROR */
        char tmp[2] = {(char)c, 0};
        out_error(tmp);
    }

    fclose(fp);
    return 0;
}
