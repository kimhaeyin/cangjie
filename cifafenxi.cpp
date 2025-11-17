#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXTOK 512
#define MAX_ID_LEN 255
#define MAX_NUM_LEN 255

// 全局变量：当前行号和列号
int line = 1;
int col  = 0;

// 带跟踪的字符读取函数
int getc_track(FILE *fp) {
    int c = fgetc(fp);
    if (c == '\n') {
        line++;
        col = 0;  // 新行开始，列号重置
    } else {
        col++;
    }
    return c;
}

// 带跟踪的字符回退函数
void ungetc_track(int c, FILE *fp) {
    if (c == EOF) return;
    ungetc(c, fp);
    if (c == '\n') {
        line--;
        col = 0;  // 注意：这里存在逻辑错误，见分析
    } else {
        col--;
    }
}

// 关键字列表
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

// 输出函数集合
void out_keyword(const char *s, int ln, int cl) { printf("%-8s %-15s (%3d,%-3d)\n", s, s, ln, cl); }
void out_op(const char *s, int ln, int cl)      { printf("%-8s %-15s (%3d,%-3d)\n", s, s, ln, cl); }
void out_id(const char *s, int ln, int cl)      { printf("%-8s %-15s (%3d,%-3d)\n", "ID", s, ln, cl); }
void out_num(const char *s, int ln, int cl)     { printf("%-8s %-15s (%3d,%-3d)\n", "NUM", s, ln, cl); }
void out_error(const char *s, int ln, int cl)   { printf("%-8s %-15s (%3d,%-3d)\n", "ERROR", s, ln, cl); }
void out_error_msg(const char *msg, int ln, int cl) { printf("%-8s %-15s (%3d,%-3d)\n", "ERROR", msg, ln, cl); }
void out_char(const char *s, int ln, int cl) { printf("%-8s %-15s (%3d,%-3d)\n", s, s, ln, cl); }
// 判断是否为运算符起始字符
int is_op_start(char c) {
    const char *ops = ":+-*/%=!<>&|^~";
    return strchr(ops, c) != NULL;
}

// 匹配双字符运算符
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
    // 文件处理：默认使用 "test.c.txt"，支持命令行参数指定文件
    const char *filename = "test.c.txt";
    if (argc >= 2) filename = argv[1];

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "无法打开文件：%s\n", filename);
        return 1;
    }

    int c;
    // 主循环：逐个字符读取直到文件结束
    while ((c = getc_track(fp)) != EOF) {
        // 跳过空白字符（空格、制表符、换行等）
        if (isspace(c)) continue;

        /* ========================== 注释处理 ========================== */
        if (c == '/') {
            int nx = getc_track(fp);  // 预读下一个字符
            if (nx == '*') {
                /* 块注释处理 */
                int prev = 0, closed = 0;
                int start_line = line, start_col = col;  // 记录注释开始位置

                // 循环读取直到遇到注释结束标记 "*/"
                while ((c = getc_track(fp)) != EOF) {
                    if (prev == '*' && c == '/') { closed = 1; break; }
                    prev = c;
                }
                // 如果文件结束仍未找到结束标记，报错
                if (!closed)
                    out_error_msg("Unclosed_comment", start_line, start_col);
                continue;  // 注释处理完毕，继续下一个token
            }
            else if (nx == '/') {
                /* 行注释处理：一直读取直到行尾 */
                while ((c = getc_track(fp)) != EOF && c != '\n');
                continue;
            }
            else {
                // 不是注释，只是单个 '/' 运算符
                ungetc_track(nx, fp);  // 回退预读的字符
                out_op("/", line, col);
                continue;
            }
        }

        /* ========================== 标识符或关键字处理 ========================== */
        if (isalpha(c) || c == '_') {
            char buf[MAXTOK];  // 缓冲区存储标识符
            int idx = 0;
            int too_long = 0;
            int start_line = line, start_col = col;  // 记录标识符开始位置

            buf[idx++] = (char)c;  // 存入第一个字符

            // 继续读取标识符的后续字符（字母、数字、下划线）
            while ((c = getc_track(fp)) != EOF && (isalnum(c) || c == '_')) {
                if (idx < MAXTOK-1) buf[idx++] = (char)c;
                if (idx > MAX_ID_LEN) too_long = 1;  // 检查长度限制
            }
            buf[idx] = '\0';  // 字符串结束符
            if (c != EOF) ungetc_track(c, fp);  // 回退多读的字符

            // 处理过长的标识符
            if (too_long) {
                out_error_msg("Identifier_too_long", start_line, start_col);
                continue;
            }

            // 判断是关键字还是普通标识符并输出
            if (is_keyword(buf)) out_keyword(buf, start_line, start_col);
            else out_id(buf, start_line, start_col);
            continue;
        }

        /* ========================== 数字常量处理 ========================== */
        if (isdigit(c)) {
            char buf[MAXTOK];
            int idx = 0;
            int has_dot = 0, illegal = 0, too_long = 0;  // 标记：是否有小数点、是否非法、是否过长
            int start_line = line, start_col = col;  // 记录数字开始位置

            buf[idx++] = (char)c;  // 存入第一个数字字符

            while ((c = getc_track(fp)) != EOF) {
                if (isdigit(c)) {
                    // 数字字符
                    if (idx < MAXTOK-1) buf[idx++] = (char)c;
                }
                else if (c == '.' && !has_dot) {
                    // 第一个小数点（用于浮点数）
                    has_dot = 1;
                    buf[idx++] = (char)c;
                }
                else if (isalpha(c) || c == '_') {
                    // 非法字符（如：123abc）
                    illegal = 1;
                    buf[idx++] = (char)c;
                }
                else break;  // 遇到其他字符，数字结束

                if (idx > MAX_NUM_LEN) too_long = 1;  // 检查长度限制
            }

            buf[idx] = '\0';
            if (c != EOF) ungetc_track(c, fp);  // 回退多读的字符

            // 错误处理
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

        /*/* ========================== 字符常量处理 ========================== #1#
        if (c == '\'') {
            int prev = 0, closed = 0;
            int start_line = line, start_col = col;  // 记录字符常量开始位置

            // 读取字符内容，考虑转义字符
            while ((c = getc_track(fp)) != EOF) {
                if (c == '\'' && prev != '\\') { closed = 1; break; }  // 找到结束引号
                prev = (c == '\\') ? '\\' : 0;  // 处理转义字符
            }

            // 检查字符常量是否正确关闭
            if (!closed)
                out_error_msg("Unclosed_char", start_line, start_col);

            continue;
        }*/
        if (c == '\'') {
            char char_content[MAXTOK] = "'";
            int idx = 1;
            int prev = 0, closed = 0;
            int start_line = line, start_col = col;

            while ((c = getc_track(fp)) != EOF && idx < MAXTOK - 2) {
                if (c == '\'' && prev != '\\') {
                    closed = 1;
                    char_content[idx++] = '\'';
                    char_content[idx] = '\0';
                    break;
                }

                char_content[idx++] = (char)c;
                prev = (c == '\\') ? '\\' : 0;
            }

            if (!closed) {
                out_error_msg("Unclosed_char", start_line, start_col);
            } else {
                out_char(char_content, start_line, start_col);  // 使用专门的输出函数
            }
            continue;
        }

        /* ========================== 运算符处理 ========================== */
        if (is_op_start(c)) {
            int nx = getc_track(fp);  // 预读下一个字符
            char two[4] = {0};
            if (nx != EOF) {
                // 尝试匹配双字符运算符（如：==, !=, <= 等）
                if (match_two_char_op((char)c, (char)nx, two)) {
                    out_op(two, line, col - 1);  // 注意列号调整
                    continue;
                } else {
                    ungetc_track(nx, fp);  // 不是双字符运算符，回退
                }
            }
            // 单字符运算符
            char s[2] = {(char)c, 0};
            out_op(s, line, col);
            continue;
        }

        /* ========================== 界符处理 ========================== */
        if (strchr(";,(){}[]", c)) {
            char s[2] = {(char)c, 0};
            out_op(s, line, col);
            continue;
        }

        /* ========================== 错误字符处理 ========================== */
        char tmp[2] = {(char)c, 0};
        out_error(tmp, line, col);
    }

    fclose(fp);
    return 0;
}
