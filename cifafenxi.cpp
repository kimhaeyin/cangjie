#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXTOK 512      // 词法单元缓冲区最大长度
#define MAX_ID_LEN 255  // 标识符最大长度
#define MAX_NUM_LEN 255 // 数字最大长度

/* ------------------------- 关键字表 ------------------------- */
const char *keywords[] = {
    "int","float","char","if","else","while","for","return",
    "void","main","break","continue","var","let"
};
const int n_keywords = sizeof(keywords)/sizeof(keywords[0]);

// 判断是否为关键字
int is_keyword(const char *s) {
    for (int i = 0; i < n_keywords; ++i)
        if (strcmp(s, keywords[i]) == 0) return 1;
    return 0;
}

/* ------------------------- 输出函数 ------------------------- */
// 输出关键字
void out_keyword(const char *s) { printf("%s %s\n", s, s); }
// 输出运算符
void out_op(const char *s)      { printf("%s %s\n", s, s); }
// 输出标识符
void out_id(const char *s)      { printf("ID %s\n", s); }
// 输出数字
void out_num(const char *s)     { printf("NUM %s\n", s); }
// 输出错误
void out_error(const char *s)   { printf("ERROR %s\n", s); }
// 输出指定错误信息（如 Identifier_too_long）
void out_error_msg(const char *msg) { printf("ERROR %s\n", msg); }

/* ------------------------- 运算符相关 ------------------------- */
// 判断是否为运算符起始字符
int is_op_start(char c) {
    const char *ops = ":+-*/%=!<>&|^~";
    return strchr(ops, c) != NULL;
}

// 判断是否为双字符运算符
int match_two_char_op(char a, char b, char *out) {
    const char *two_ops[] = {
        "==","!=","<=",">=","++","--","&&","||",
        "+=","-=","*=","/=","%=","<<",">>"
    };
    char s[3] = {a, b, 0}; // 构造字符串
    for (size_t i = 0; i < sizeof(two_ops)/sizeof(two_ops[0]); ++i)
        if (strcmp(s, two_ops[i]) == 0) { strcpy(out, s); return 1; }
    return 0;
}

/* ------------------------- 主函数 ------------------------- */
int main(int argc, char *argv[]) {
    // 默认文件路径，可通过命令行参数传入文件
    const char *filename = "test.c.txt";
    if (argc >= 2) filename = argv[1];

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "无法打开文件：%s\n", filename);
        return 1;
    }

    int c;
    while ((c = fgetc(fp)) != EOF) { // 逐字符读取文件

        if (isspace(c)) continue; // 跳过空白字符

        /* ------------------------- 注释处理 ------------------------- */
        if (c == '/') {
            int nx = fgetc(fp);

            if (nx == '*') {
                /* 块注释处理 */
                int prev = 0, closed = 0;
                //一直扫描到文件尾
                while ((c = fgetc(fp)) != EOF) {
                    if (prev == '*' && c == '/') { closed = 1; break; }
                    prev = c;
                }
                //未闭合输出错误信息
                if (!closed) out_error_msg("Unclosed_comment");
                continue;
            }
            else if (nx == '/') {
                /* 行注释处理 */
                while ((c = fgetc(fp)) != EOF && c != '\n');
                continue;
            }
            else {
                /* 不是注释，恢复读取的字符 */
                if (nx != EOF) ungetc(nx, fp);//将字符放回输入流
                out_op("/"); // 输出除号
                continue;
            }
        }

        /* ------------------------- 标识符或关键字 ------------------------- */
        //是一个字符数字或下划线
        if (isalpha(c) || c == '_') {
            char buf[MAXTOK];//缓冲区，存储标识符
            int idx = 0;
            int too_long = 0;//长度超标标志

            buf[idx++] = (char)c;

            while ((c = fgetc(fp)) != EOF && (isalnum(c) || c == '_')) {
                if (idx < MAXTOK-1)
                    buf[idx++] = (char)c;
                if (idx > MAX_ID_LEN) too_long = 1; // 检查长度
            }
            buf[idx] = '\0';
            if (c != EOF) ungetc(c, fp);//遇空格写回输入流，后续才能识别其他字符

            if (too_long) {
                out_error_msg("Identifier_too_long");
                continue;
            }

            if (is_keyword(buf)) out_keyword(buf); // 关键字输出
            else out_id(buf); // 标识符输出

            continue;
        }

        /* ------------------------- 数字处理 ------------------------- */
        if (isdigit(c)) {
            char buf[MAXTOK]; int idx = 0;
            int has_dot = 0, illegal = 0, too_long = 0;//has_dog标记是否已经包含小数点（用于浮点数）

            buf[idx++] = (char)c;//将第一个数字字符存入缓冲区

            while ((c = fgetc(fp)) != EOF) {
                if (isdigit(c)) {
                    if (idx < MAXTOK-1) buf[idx++] = (char)c;
                }
                else if (c == '.' && !has_dot) { // 处理浮点数
                    has_dot = 1;
                    if (idx < MAXTOK-1) buf[idx++] = (char)c;
                }
                else if (isalpha(c) || c == '_') { // 数字后面出现非法字符
                    illegal = 1;
                    if (idx < MAXTOK-1) buf[idx++] = (char)c;
                }
                else break;

                if (idx > MAX_NUM_LEN) too_long = 1;
            }

            if (illegal) { // 继续读取非法字符
                while (c != EOF && (isalnum(c) || c == '_')) {
                    if (idx < MAXTOK-1) buf[idx++] = (char)c;
                    c = fgetc(fp);
                }
            }

            buf[idx] = '\0';
            if (c != EOF) ungetc(c, fp);

            if (too_long) { out_error_msg("Number_too_long"); continue; }
            if (illegal) { out_error(buf); continue; }

            out_num(buf); // 输出数字
            continue;
        }


        /* ------------------------- 字符常量处理 ------------------------- */
        if (c == '\'') {
            int prev = 0, closed = 0;
            while ((c = fgetc(fp)) != EOF) {
                if (c == '\'' && prev != '\\') { closed = 1; break; }
                prev = (c == '\\' ? '\\' : 0);
            }
            if (!closed) out_error_msg("Unclosed_char");
            continue;
        }

        /* ------------------------- 运算符处理 ------------------------- */
        if (is_op_start(c)) {
            int nx = fgetc(fp);
            char two[4] = {0};
            if (nx != EOF) {
                if (match_two_char_op((char)c, (char)nx, two)) {
                    out_op(two); // 输出双字符运算符
                    continue;
                } else {
                    ungetc(nx, fp); // 不是双字符运算符，恢复字符
                }
            }
            char s[2] = {(char)c, 0};
            out_op(s); // 输出单字符运算符
            continue;
        }

        /* ------------------------- 界符处理 ------------------------- */
        if (strchr(";,(){}[]", c)) {
            char s[2] = {(char)c, 0};
            out_op(s);
            continue;
        }

        /* ------------------------- 其他全部视为错误 ------------------------- */
        char tmp[2] = {(char)c, 0};
        out_error(tmp);
    }

    fclose(fp);
    return 0;
}
