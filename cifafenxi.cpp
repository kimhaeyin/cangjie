
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXTOK 512

/* 关键字表（可按需扩展）*/
const char *keywords[] = {
    "int","float","char","if","else","while","for","return","void","main","break","continue"
};
const int n_keywords = sizeof(keywords)/sizeof(keywords[0]);

/* 判断是否为关键字（完全匹配） */
int is_keyword(const char *s) {
    for (int i = 0; i < n_keywords; ++i)
        if (strcmp(s, keywords[i]) == 0) return 1;
    return 0;
}

/* 输出 helper：符合老师示例输出风格 */
/* 关键字：main main
   运算符/界符：+ +
   标识符：ID name
   数字：NUM 123   （整数或浮点数）
   非法：ERROR token
*/
void out_keyword(const char *s) { printf("%s %s\n", s, s); }
void out_op(const char *s)      { printf("%s %s\n", s, s); }
void out_id(const char *s)      { printf("ID %s\n", s); }
void out_num(const char *s)     { printf("NUM %s\n", s); }
void out_error(const char *s)   { printf("ERROR %s\n", s); }

/* 判断是否为运算符起始字符（简化） */
int is_op_start(char c) {
    const char *ops = "+-*/%=!<>&|^~";
    return strchr(ops, c) != NULL;
}

/* 尝试匹配双字符运算符 */
int match_two_char_op(char a, char b, char *out) {
    // 支持的双字符运算符集合
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
        /* 跳过空白 */
        if (isspace(c)) continue;

        /* 处理注释或 / 运算符 */
        if (c == '/') {
            int nx = fgetc(fp);
            if (nx == '*') {
                /* 块注释：/* ... * / */
                int prev = 0;
                while ((c = fgetc(fp)) != EOF) {
                    if (prev == '*' && c == '/') break;
                    prev = c;
                }
                continue; /* 注释处理完，继续主循环 */
            } else if (nx == '/') {
                /* 行注释：// ... 到 行尾 */
                while ((c = fgetc(fp)) != EOF && c != '\n');
                continue;
            } else {
                if (nx != EOF) ungetc(nx, fp);
                out_op("/"); continue;
            }
        }

        /* 标识符或关键字（字母或下划线开头） */
        if (isalpha(c) || c == '_') {
            char buf[MAXTOK]; int idx = 0;
            buf[idx++] = (char)c;
            while ((c = fgetc(fp)) != EOF && (isalnum(c) || c == '_')) {
                if (idx < MAXTOK-1) buf[idx++] = (char)c;
            }
            buf[idx] = '\0';
            if (c != EOF) ungetc(c, fp);
            if (is_keyword(buf)) out_keyword(buf);
            else out_id(buf);
            continue;
        }

        /* 数字开头：可能是合法数字，也可能是非法（以数字开头但含字母） */
        if (isdigit(c)) {
            char buf[MAXTOK]; int idx = 0;
            buf[idx++] = (char)c;
            int has_dot = 0;
            int illegal = 0;
            while ((c = fgetc(fp)) != EOF) {
                if (isdigit(c)) {
                    if (idx < MAXTOK-1) buf[idx++] = (char)c;
                } else if (c == '.' && !has_dot) {
                    has_dot = 1;
                    if (idx < MAXTOK-1) buf[idx++] = (char)c;
                } else if (isalpha(c) || c == '_') {
                    /* 数字开头后出现字母 => 非法标识符 */
                    illegal = 1;
                    if (idx < MAXTOK-1) buf[idx++] = (char)c;
                } else {
                    break;
                }
            }
            /* 若继续有字母或数字，继续读完整个 token（比如 1abc123） */
            if (illegal) {
                while (c != EOF && (isalnum(c) || c == '_')) {
                    if (idx < MAXTOK-1) buf[idx++] = (char)c;
                    c = fgetc(fp);
                }
            }
            buf[idx] = '\0';
            if (c != EOF) ungetc(c, fp);

            if (illegal) {
                out_error(buf);
            } else {
                out_num(buf);
            }
            continue;
        }

        /* 字符串字面量（简单跳过，不输出为 token）*/
        if (c == '"') {
            int prev = 0;
            while ((c = fgetc(fp)) != EOF) {
                if (c == '"' && prev != '\\') break;
                if (c == '\\' && prev == 0) prev = 1;
                else prev = 0;
            }
            continue;
        }

        /* 字符常量，如 'a' （跳过）*/
        if (c == '\'') {
            int prev = 0;
            while ((c = fgetc(fp)) != EOF) {
                if (c == '\'' && prev != '\\') break;
                if (c == '\\' && prev == 0) prev = 1;
                else prev = 0;
            }
            continue;
        }

        /* 运算符与界符 */
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
            /* 单字符运算符或界符 */
            char s[2] = {(char)c, 0};
            /* 把分号、逗号、小括号、大括号等也当成“符号”，输出格式相同 */
            out_op(s);
            continue;
        }

        /* 其它可识别界符：; , ( ) { } [ ] */
        if (strchr(";,(){}[]", c)) {
            char s[2] = {(char)c, 0};
            out_op(s);
            continue;
        }

        /* 其它不可识别字符，作为 ERROR */
        char tmp[4] = {(char)c, 0,0,0};
        out_error(tmp);
    }

    fclose(fp);
    return 0;
}
