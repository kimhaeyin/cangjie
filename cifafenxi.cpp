#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKEN_LEN 64

// 关键字表
const char *keywords[] = {
    "int", "float", "char", "if", "else", "for", "while", "return", "void"
};
int keyword_count = sizeof(keywords) / sizeof(keywords[0]);

// 判断是否为关键字
int isKeyword(const char *s) {
    for (int i = 0; i < keyword_count; i++) {
        if (strcmp(s, keywords[i]) == 0)
            return 1;
    }
    return 0;
}

int main() {
    FILE *fp;
    char ch;
    char token[MAX_TOKEN_LEN];
    int i;

    fp = fopen("C:/Users/21557/Desktop/test.txt", "r");
    if (fp == NULL) {
        printf("can't open test.c.txt\n");
        return 1;
    }

    printf("词法分析结果如下：\n\n");
    while ((ch = fgetc(fp)) != EOF) {
        // 跳过空白字符
        if (isspace(ch))
            continue;

        // 识别标识符或关键字
        if (isalpha(ch) || ch == '_') {
            i = 0;
            token[i++] = ch;
            while ((ch = fgetc(fp)) != EOF && (isalnum(ch) || ch == '_')) {
                token[i++] = ch;
            }
            token[i] = '\0';
            ungetc(ch, fp); // 回退一个字符

            if (isKeyword(token))
                printf("<关键字, %s>\n", token);
            else
                printf("<标识符, %s>\n", token);
        }

        // 识别数字常量
        else if (isdigit(ch)) {
            i = 0;
            token[i++] = ch;
            int isFloat = 0;

            while ((ch = fgetc(fp)) != EOF && (isdigit(ch) || ch == '.')) {
                if (ch == '.')
                    isFloat = 1;
                token[i++] = ch;
            }
            token[i] = '\0';
            ungetc(ch, fp);
            if (isFloat)
                printf("<浮点数, %s>\n", token);
            else
                printf("<整数, %s>\n", token);
        }

        // 识别运算符和界符
        else {
            switch (ch) {
                case '+': case '-': case '*': case '/':
                case '=': case '<': case '>':
                    printf("<运算符, %c>\n", ch);
                    break;
                case ';': case ',': case '(': case ')':
                case '{': case '}':
                    printf("<界符, %c>\n", ch);
                    break;
                default:
                    printf("<未知符号, %c>\n", ch);
                    break;
            }
        }
    }

    fclose(fp);
    return 0;
}

