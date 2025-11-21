#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXTOK 512
#define MAX_ID_LEN 255
#define MAX_NUM_LEN 255

// ------------------------- 全局变量 -------------------------
int line = 1, col = 0;
FILE *fin, *fout;

// ------------------------- 字符读取函数 -------------------------
int getc_track(FILE *fp) {
    int c = fgetc(fp);
    if (c == '\n') { line++; col = 0; }
    else col++;
    return c;
}

void ungetc_track(int c, FILE *fp) {
    if (c == EOF) return;
    ungetc(c, fp);
    if (c == '\n') { line--; col = 0; }
    else col--;
}

// ------------------------- 跳过空白字符 -------------------------
void skip_whitespace() {
    int c;
    while ((c = getc_track(fin)) != EOF) {
        if (c == ' ' || c == '\t' || c == '\n') {
            continue;
        }
        else {
            ungetc_track(c, fin);
            break;
        }
    }
}

// ------------------------- 关键字表 -------------------------
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

// ------------------------- 输出函数 -------------------------
void out_keyword(const char *s) { fprintf(fout,"%-8s %-15s (%3d,%-3d)\n", s, s, line, col); }
void out_op(const char *s)      { fprintf(fout,"%-8s %-15s (%3d,%-3d)\n", s, s, line, col); }
void out_id(const char *s)      { fprintf(fout,"%-8s %-15s (%3d,%-3d)\n", "ID", s, line, col); }
void out_num(const char *s)     { fprintf(fout,"%-8s %-15s (%3d,%-3d)\n", "NUM", s, line, col); }
void out_char(const char *s)    { fprintf(fout,"%-8s %-15s (%3d,%-3d)\n", s, s, line, col); }
void out_error(const char *s)   { fprintf(fout,"%-8s %-15s (%3d,%-3d)\n", "ERROR", s, line, col); }

// ------------------------- 运算符 -------------------------
int is_op_start(char c) {
    const char *ops = ":+-*/%=!<>&|^~";
    return strchr(ops, c) != NULL;
}

int match_two_char_op(char a, char b, char *out) {
    const char *two_ops[] = {
        "==","!=","<=",">=","++","--","&&","||",
        "+=","-=","*=","/=","%=","<<",">>"
    };
    char s[3] = {a,b,0};
    for (size_t i=0;i<sizeof(two_ops)/sizeof(two_ops[0]);i++)
        if(strcmp(s,two_ops[i])==0){ strcpy(out,s); return 1; }
    return 0;
}

// ------------------------- 词法分析函数 -------------------------
void lexer() {
    int c;
    while (1) {
        skip_whitespace();          // 跳过空格、制表符、换行
        c = getc_track(fin);
        if (c == EOF) break;

        // 忽略空白字符，防止进入错误输出
        if (isspace(c)) continue;

        // 注释处理
        if (c == '/') {
            int nx = getc_track(fin);
            if (nx == '*') { // 块注释
                int prev = 0, closed = 0;
                while ((c = getc_track(fin)) != EOF) {
                    if (prev == '*' && c == '/') { closed = 1; break; }
                    prev = c;
                }
                if (!closed) out_error("Unclosed_comment");
                continue;
            } else if (nx == '/') { // 行注释
                while ((c = getc_track(fin)) != EOF && c != '\n');
                continue;
            } else { // '/'
                ungetc_track(nx, fin);
                out_op("/");
                continue;
            }
        }

        // 标识符/关键字
        if (isalpha(c) || c == '_') {
            char buf[MAXTOK]; int idx = 0;
            buf[idx++] = (char)c;
            while ((c = getc_track(fin)) != EOF && (isalnum(c) || c == '_')) {
                if (idx < MAXTOK - 1) buf[idx++] = (char)c;
            }
            buf[idx] = '\0';
            if (c != EOF) ungetc_track(c, fin);
            if (is_keyword(buf)) out_keyword(buf);
            else out_id(buf);
            continue;
        }

        // 数字
        if (isdigit(c)) {
            char buf[MAXTOK]; int idx = 0;
            int has_dot = 0, illegal = 0;
            buf[idx++] = (char)c;
            while ((c = getc_track(fin)) != EOF) {
                if (isdigit(c)) { if (idx < MAXTOK - 1) buf[idx++] = c; }
                else if (c == '.' && !has_dot) { has_dot = 1; buf[idx++] = c; }
                else if (isalpha(c) || c == '_') { illegal = 1; if (idx < MAXTOK - 1) buf[idx++] = c; }
                else break;
            }
            buf[idx] = '\0';
            if (c != EOF) ungetc_track(c, fin);
            if (illegal) out_error(buf);
            else out_num(buf);
            continue;
        }

        // 字符常量（支持转义）
        if (c == '\'') {
            char buf[MAXTOK]; int idx = 0;
            buf[idx++] = '\'';
            int closed = 0;
            while ((c = getc_track(fin)) != EOF && idx < MAXTOK - 1) {
                buf[idx++] = (char)c;
                if (c == '\\') {             // 转义字符
                    int next = getc_track(fin);
                    if (next != EOF) buf[idx++] = (char)next;
                    c = next;
                }
                else if (c == '\'') { closed = 1; break; }
            }
            buf[idx] = '\0';
            if (!closed) out_error("Unclosed_char");
            else out_char(buf);
            continue;
        }

        // 运算符
        if (is_op_start(c)) {
            int nx = getc_track(fin);
            char two[4] = {0};
            if (nx != EOF && match_two_char_op((char)c, (char)nx, two)) {
                out_op(two);
                continue;
            } else if (nx != EOF) ungetc_track(nx, fin);
            char s[2] = {(char)c, 0};
            out_op(s);
            continue;
        }

        // 界符
        if (strchr(";,(){}[]", c)) {
            char s[2] = {(char)c, 0};
            out_op(s);
            continue;
        }

        // 错误字符（过滤空白）
        if (!isspace(c)) {
            char tmp[2] = {(char)c, 0};
            out_error(tmp);
        }
    }
}

// ------------------------- 主函数 -------------------------
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

    lexer();  // 调用词法分析函数

    fclose(fin);
    fclose(fout);
    printf("词法分析完成，结果已输出到文件。\n");
    return 0;
}
