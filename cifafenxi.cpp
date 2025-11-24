#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXTOK 512
#define MAX_ID_LEN 255
#define MAX_NUM_LEN 255

// ===========================================================
// 全局变量：当前扫描位置（行号、列号）
// ===========================================================
int line = 1, col = 0; // 初始位置：第1行，第0列
FILE *fin, *fout;

// ===========================================================
// 错误报告：输出到控制台，而不是输出文件
// 例如：第10行: 非法数字单词
// ===========================================================
void report_error(const char *msg) {
    printf("第%d行: %s\n", line, msg);
}

// ===========================================================
// 字符读取（带行列号跟踪）
// 每读取一个字符，就更新 line、col，便于报错定位
// ===========================================================
int getc_track(FILE *fp) {
    int c = fgetc(fp);

    if (c == '\n') {   // 遇到换行：行+1，列归零
        line++;
        col = 0;
    } else {
        col++;         // 普通字符：列+1
    }
    return c;
}

// ===========================================================
// 带回退功能的字符回推（ungetc）
// 回退时，要记得修正行号和列号
// ===========================================================
void ungetc_track(int c, FILE *fp) {
    if (c == EOF) return;

    ungetc(c, fp); // 真实回退到输入流

    if (c == '\n') {
        line--;   // 回到上一行
        col = 0;
    } else col--; // 回到前一列
}

// ===========================================================
// 跳过所有空白字符：空格、tab、换行
// 对词法分析来说，它们均无意义
// ===========================================================
void skip_whitespace() {
    int c;
    while ((c = getc_track(fin)) != EOF) {
        if (c == ' ' || c == '\t' || c == '\n')
            continue;          // 忽略空白
        else {
            ungetc_track(c, fin); // 遇到非空白 → 回退1个字符
            break;
        }
    }
}

// ===========================================================
// 关键字表（按字典序排序）
// 便于二分查找
// ===========================================================
const char *keywords[] = {
    "break","char","continue","else","float","for",
    "if","int","let","main","return","var","void","while"
};
const int n_keywords = sizeof(keywords)/sizeof(keywords[0]);

// ===========================================================
// 二分查找关键字
// 返回 1=关键字；0=普通标识符
// ===========================================================
int is_keyword(const char *s) {
    int l = 0, r = n_keywords - 1;

    while (l <= r) {
        int mid = (l + r) / 2;
        int cmp = strcmp(s, keywords[mid]);

        if (cmp == 0) return 1;
        else if (cmp < 0) r = mid - 1;
        else l = mid + 1;
    }
    return 0;
}

// ===========================================================
// 输出格式化（输出到结果文件）
// 所有 token 都按对齐格式输出
// ===========================================================
void out_keyword(const char *s) { fprintf(fout,"%-8s %-15s (%3d,%-3d)\n", s, s, line, col); }
void out_op(const char *s)      { fprintf(fout,"%-8s %-15s (%3d,%-3d)\n", s, s, line, col); }
void out_id(const char *s)      { fprintf(fout,"%-8s %-15s (%3d,%-3d)\n", "ID", s, line, col); }
void out_num(const char *s)     { fprintf(fout,"%-8s %-15s (%3d,%-3d)\n", "NUM", s, line, col); }
void out_char(const char *s)    { fprintf(fout,"%-8s %-15s (%3d,%-3d)\n", s, s, line, col); }
void out_error_file(const char *s) { fprintf(fout,"%-8s %-15s (%3d,%-3d)\n", "ERROR", s, line, col); }

// ===========================================================
// 判断是否为操作符起始字符
// ===========================================================
int is_op_start(char c) {
    const char *ops = ":+-*/%=!<>&|^~";
    return strchr(ops, c) != NULL;
}

// ===========================================================
// 匹配双字符操作符，如 == != <= >= ++ -- <<= >>=
// 匹配成功返回 1，并将操作符写入 out
// ===========================================================
int match_two_char_op(char a, char b, char *out) {
    const char *two_ops[] = {
        "==","!=","<=",">=","++","--","&&","||",
        "+=","-=","*=","/=","%=","<<",">>"
    };
    char s[3] = {a,b,0};
    for (size_t i=0;i<sizeof(two_ops)/sizeof(two_ops[0]);i++)
        if(strcmp(s,two_ops[i])==0){
            strcpy(out,s);
            return 1;
        }
    return 0;
}

// ===========================================================
// 词法分析主过程 lexer()
// 循环读取字符，根据不同规则识别 Token
// ===========================================================
void lexer() {
    int c;

    while (1) {
        // ---------- 跳过空白 ----------
        skip_whitespace();

        // 获取下一个有效字符
        c = getc_track(fin);
        if (c == EOF) break;

        if (isspace(c)) continue; // 冗余保护

        // ---------- 注释 ----------
        if (c == '/') {
            int nx = getc_track(fin);

            // 块注释 /* ... */
            if (nx == '*') {
                int prev = 0, closed = 0;
                while ((c = getc_track(fin)) != EOF) {
                    if (prev == '*' && c == '/') { closed = 1; break; }
                    prev = c;
                }
                if (!closed) {
                    report_error("注释未闭合");
                    out_error_file("Unclosed_comment");
                }
                continue;
            }
            // 行注释 //
            else if (nx == '/') {
                while ((c = getc_track(fin)) != EOF && c != '\n');
                continue;
            }
            // 单个 '/'
            else {
                ungetc_track(nx, fin);
                out_op("/");
                continue;
            }
        }

        // ---------- 标识符 / 关键字 ----------
        if (isalpha(c) || c == '_') {
            char buf[MAXTOK]; int idx = 0;
            buf[idx++] = (char)c;

            // 继续读取标识符字符（字母/数字/_）
            while ((c = getc_track(fin)) != EOF &&
                   (isalnum(c) || c == '_')) {
                if (idx < MAXTOK - 1)
                    buf[idx++] = (char)c;
            }
            buf[idx] = '\0';
            if (c != EOF) ungetc_track(c, fin);

            // 判断是否关键字
            if (is_keyword(buf)) out_keyword(buf);
            else out_id(buf);

            continue;
        }

        // ---------- 数字（整型/浮点数 + 非法处理） ----------
        if (isdigit(c)) {
            char buf[MAXTOK]; int idx = 0;
            int has_dot = 0, illegal = 0;
            buf[idx++] = (char)c;

            while ((c = getc_track(fin)) != EOF) {
                if (isdigit(c)) buf[idx++] = c;
                else if (c == '.' && !has_dot) {
                    has_dot = 1;        // 允许一个小数点
                    buf[idx++] = c;
                }
                else if (isalpha(c)) { // 数字后不能接字母
                    illegal = 1;
                    buf[idx++] = c;
                }
                else break; // 数字结束
            }
            buf[idx] = '\0';
            if (c != EOF) ungetc_track(c, fin);

            // 判断是否合法
            if (illegal) {
                report_error("非法数字单词");
                out_error_file(buf);
            }
            else out_num(buf);

            continue;
        }

        // ---------- 字符常量 ----------
        if (c == '\'') {
            char buf[MAXTOK]; int idx = 0;
            buf[idx++] = '\'';
            int closed = 0;

            // 处理内部字符（包括转义）
            while ((c = getc_track(fin)) != EOF && idx < MAXTOK - 1) {
                buf[idx++] = (char)c;

                if (c == '\\') {  // 转义，如 '\n'
                    int next = getc_track(fin);
                    if (next != EOF) buf[idx++] = (char)next;
                    c = next;
                }
                else if (c == '\'') { // 正常闭合
                    closed = 1;
                    break;
                }
            }
            buf[idx] = '\0';

            if (!closed) {
                report_error("字符常量未闭合");
                out_error_file("Unclosed_char");
            }
            else out_char(buf);

            continue;
        }

        // ---------- 运算符 ----------
        if (is_op_start(c)) {
            int nx = getc_track(fin);
            char two[4];

            // 先尝试匹配双字符运算符
            if (nx != EOF && match_two_char_op((char)c, (char)nx, two)) {
                out_op(two);
                continue;
            }

            // 否则是单字符运算符
            if (nx != EOF) ungetc_track(nx, fin);
            char s[2] = {(char)c,0};
            out_op(s);
            continue;
        }

        // ---------- 界符 ----------
        if (strchr(";,(){}[]", c)) {
            char s[2] = {(char)c,0};
            out_op(s);
            continue;
        }

        // ---------- 非法字符 ----------
        char tmp[20];
        sprintf(tmp,"非法单词 \"%c\"", c);
        report_error(tmp);

        char buf2[2] = {(char)c,0};
        out_error_file(buf2);
    }
}

// ===========================================================
// 主函数：输入文件 → 输出文件
// ===========================================================
int main() {
    char input_file[300], output_file[300];

    printf("请输入源程序文件名（含路径）：");
    scanf("%s", input_file);

    printf("请输入输出文件名（含路径）：");
    scanf("%s", output_file);

    fin = fopen(input_file,"r");
    if (!fin) { printf("打开输入文件失败！\n"); return 1; }

    fout = fopen(output_file,"w");
    if (!fout) { printf("创建输出文件失败！\n"); return 2; }

    lexer(); // 调用词法分析器

    fclose(fin);
    fclose(fout);

    printf("词法分析完成，结果已输出到文件。\n");
    return 0;
}
