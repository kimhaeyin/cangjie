// TESTparse_full_parser_fixed.c
// 完整版语法分析器（修正版：修复strcmp判断、部分逻辑与健壮性）

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define maxsymbolIndex 100//定义符号表的容量

enum Category_symbol { variable, function }; //标志符的类型

int TESTparse();

int program();

int main_declaration();

int function_body();

int statement();

int expression_stat();

int expression();

int bool_expr();

int additive_expr();

int term();

int factor();

int if_stat();

int while_stat();

int for_stat();

int write_stat();

int read_stat();

int declaration_stat();

int declaration_list();

int statement_list();

int compound_stat();

int call_stat();

int insert_Symbol(enum Category_symbol category, char *name);

int lookup(char *name, int *pPosition);

char token[64], token1[256]; //单词类别值，自身值
char tokenfile[260]; //单词流文件名

FILE *fpTokenin; //单词流文件指针

struct {
    char name[64];
    enum Category_symbol kind;
    int address;
} symbol[maxsymbolIndex];

int symbolIndex = 0; //symbol数组中第一个空元素的下标，0序（下一个要填入的标识符在符号表中的位置）
int offset; //局部变量在所定义函数内部的相对地址

// 用于生成语法树文本
char *astText = NULL;
size_t astCap = 0;
int indentLevel = 0;

void ast_init() {
    astCap = 1 << 16;
    astText = (char *) malloc(astCap);
    if (!astText) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    astText[0] = '\0';
    indentLevel = 0; // 初始化缩进级别
}

void ast_add_indent() {
    for (int i = 0; i < indentLevel; i++) {
        strcat(astText, "  ");
    }
}

void ast_append(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    size_t cur = strlen(astText);
    char tmp[4096];
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    size_t need = strlen(tmp);
    if (cur + need + 16 > astCap) {
        astCap = (cur + need + 16) * 2;
        astText = (char *) realloc(astText, astCap);
        if (!astText) {
            fprintf(stderr, "内存重分配失败\n");
            exit(1);
        }
    }
    strcat(astText, tmp);
}

void ast_begin(const char *name) {
    ast_add_indent();
    ast_append("%s:\n", name);
    indentLevel++;
}

void ast_end() {
    if (indentLevel > 0) {
        indentLevel--;
    }
}

void ast_add_attr(const char *attr, const char *value) {
    ast_add_indent();
    ast_append("  %s: %s\n", attr, value);
}

// 修复token读取函数
int read_next_token() {
    char line[512];

    if (fgets(line, sizeof(line), fpTokenin) == NULL) {
        token[0] = '\0';
        token1[0] = '\0';
        printf("read_next_token: 文件结束\n"); // 添加调试
        return 0;
    }

    line[strcspn(line, "\n")] = '\0';

    // 解析格式：token类型 + 多个空格 + token值
    // 例如："main     main"

    // 找到第一个连续空格的位置
    char *token_end = line;
    while (*token_end != ' ' && *token_end != '\t' && *token_end != '\0') {
        token_end++;
    }

    // 提取token类型
    int type_len = token_end - line;
    if (type_len > 63) type_len = 63;
    strncpy(token, line, type_len);
    token[type_len] = '\0';

    // 跳过连续的空格
    char *value_start = token_end;
    while (*value_start == ' ' || *value_start == '\t') {
        value_start++;
    }

    // 提取token值
    if (*value_start != '\0') {
        strncpy(token1, value_start, 255);
        token1[255] = '\0';
    } else {
        token1[0] = '\0';
    }

    printf("读取token成功: type='%s' value='%s'\n", token, token1); // 修改这行
    return 1;
}

int TESTparse() {
    int i;
    int es = 0;
    printf("请输入单词流文件名（包括路径）：");
    if (scanf("%s", tokenfile) != 1) return 10;
    if ((fpTokenin = fopen(tokenfile, "r")) == NULL) {
        printf("\n打开%s错误!\n", tokenfile);
        es = 10;
        return (es);
    }

    ast_init();


    if (!read_next_token()) {
        printf("错误: 文件为空\n");
        return 10;
    }

    es = program();
    fclose(fpTokenin);

    printf("==语法分析程序结果==\n");
    switch (es) {
        case 0: printf("语法分析成功!\n");
            break;
        case 10: printf("读取文件 %s结束!\n", tokenfile);
            break;
        case 1: printf("缺少{!\n");
            break;
        case 3: printf("缺少标识符!\n");
            break;
        case 4: printf("少分号!\n");
            break;
        case 5: printf("缺少(!\n");
            break;
        case 6: printf("缺少)!\n");
            break;
        case 7: printf("缺少操作数!\n");
            break;
        case 8: printf("缺少参数类型!\n");
            break;
        case 9: printf("赋值语句的左值不是变量名!\n");
            break;
        case 11: printf("函数开头缺少{!\n");
            break;
        case 12: printf("函数结束缺少}!\n");
            break;
        case 22: printf("变量%s重复定义!\n", token1);
            break;
        case 23: printf("变量未声明!\n");
            break;
        case 32: printf("函数名重复定义!\n");
            break;
        default: if (es > 0) printf("错误码: %d\n", es);
            break;
    }

    char astfile[512];
    snprintf(astfile, sizeof(astfile), "%s.ast.txt", tokenfile);
    FILE *fasta = fopen(astfile, "w");
    if (fasta) {
        fputs(astText, fasta);
        fclose(fasta);
        printf("语法树已输出到 %s\n", astfile);
    } else {
        printf("无法创建语法树文件 %s\n", astfile);
    }


    free(astText);
    return (es);
}

//<program> →{ fun_declaration }<main_declaration>
int program() {
    int es = 0;

    ast_begin("Program");

    // 检查第一个 token 是否为 main（token 类型或值都可能是 "main"）
    if (strcmp(token, "main") != 0 && strcmp(token1, "main") != 0) {
        printf("期望main，得到: %s %s\n", token, token1);
        es = 13;
        return es;
    }

    ast_begin("main_declaration");
    insert_Symbol(function, "main");

    // 读下一个 token（应该是 "("）
    if (!read_next_token()) return 10;

    es = main_declaration();
    if (es > 0) {
        ast_end(); // main_declaration
        ast_end(); // Program
        return (es);
    }

    // 检查main函数结束后是否还有多余字符
    if (token[0] != '\0' && !feof(fpTokenin)) {
        printf("程序中main函数结束后，还有其它多余字符: %s %s\n", token, token1);
        es = 24;
    }

    ast_end(); // main_declaration
    ast_end(); // Program
    return (es);
}

//<main_declaration>→ main’(‘ ‘ )’ < function_body>
int main_declaration() {
    int es = 0;

    ast_add_attr("ID", "main");

    // 现在检查 token 是否为 "("
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        printf("期望(，得到: %s %s\n", token, token1);
        es = 5;
        return es;
    }

    if (!read_next_token()) return 10;

    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        es = 6;
        return es;
    }

    if (!read_next_token()) return 10;

    es = function_body();
    if (es > 0) return es;

    return es;
}

//<function_body>→'{'<declaration_list><statement_list>'}'
int function_body() {
    int es = 0;

    ast_begin("Function_Body");

    // 期待 "{"
    if (strcmp(token, "{") != 0 && strcmp(token1, "{") != 0) {
        printf("期望{，得到: %s %s\n", token, token1);
        es = 11;
        return (es);
    }

    offset = 2; //进入一个新的函数，变量的相对地址从2开始

    if (!read_next_token()) return 10;

    es = declaration_list();
    if (es > 0) return (es);

    es = statement_list();
    if (es > 0) return (es);

    // 期待 "}"
    if (strcmp(token, "}") != 0 && strcmp(token1, "}") != 0) {
        printf("期望}，得到: %s %s\n", token, token1);
        es = 12;
        return (es);
    }

    ast_end();
    //if (!read_next_token()) return 10; // 读取下一 token，继续解析后续可能的内容

    return 0;
}

//<declaration_list>→{<declaration_stat>}
int declaration_list() {
    int es = 0;

    ast_begin("DeclarationList");

    while (strcmp(token, "var") == 0 || strcmp(token1, "var") == 0) {
        es = declaration_stat();
        if (es > 0) return (es);
    }

    ast_end();
    return (es);
}

//<declaration_stat>→int ID;
int declaration_stat() {
    int es = 0;

    ast_begin("VariableDeclaration");

    ast_begin("declarations");
    ast_begin("VariableDeclarator");

    // 当前 token 应该是 "var"（或控制已经到 var 之后），我们读下一个来拿 ID
    if (!read_next_token()) return 10;
    if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
        printf("期望ID，得到: %s %s\n", token, token1);
        return (es = 3); //不是标识符
    }

    ast_begin("id");
    ast_add_attr("type", "Identifier");
    ast_add_attr("name", token1);

    es = insert_Symbol(variable, token1);
    if (es > 0) return (es);

    if (!read_next_token()) return 10;
    if (strcmp(token, ":") != 0 && strcmp(token1, ":") != 0) {
        printf("期望:，得到: %s %s\n", token, token1);
        return (es = 4);
    }

    if (!read_next_token()) return 10;
    if (strcmp(token, "int") == 0 || strcmp(token1, "int") == 0) {
        ast_add_attr("kind", "int");
    } else if (strcmp(token, "double") == 0 || strcmp(token1, "double") == 0) {
        ast_add_attr("kind", "double");
    } else if (strcmp(token, "float") == 0 || strcmp(token1, "float") == 0) {
        ast_add_attr("kind", "float");
    } else if (strcmp(token, "char") == 0 || strcmp(token1, "char") == 0) {
        ast_add_attr("kind", "char");
    } else if (strcmp(token, "ID") == 0 && strcmp(token1, "ID") == 0) {
        ast_add_attr("kind", token1);
    } else {
        printf("期望类型，得到: %s %s\n", token, token1);
        return (es = 8);
    }
    ast_end(); // id

    // 仓颉语言：变量声明不需要分号。直接读取下一个 token 以继续解析。
    if (!read_next_token()) return 10;

    ast_end(); // VariableDeclarator
    ast_end(); // declarations
    ast_end(); // VariableDeclaration
    return (es);
}

//<statement_list>→{<statement>}
int statement_list() {
    int es = 0;

    ast_begin("StatementList");

    while ((strcmp(token, "}") != 0 && strcmp(token1, "}") != 0) &&(token[0] != '\0' && !feof(fpTokenin))) {
        printf("解析语句，当前token: %s %s\n", token, token1);
        es = statement();
        if (es > 0) return es;
        // 如果statement处理后文件结束，退出循环
        if (token[0] == '\0' && feof(fpTokenin)) {
            break;
        }
    }

    ast_end();
    return es;
}


//<statement>→ <if_stat>|<while_stat>|<for_stat>
//               |<compound_stat> |<expression_stat>| <call _stat>
int statement() {
    int es = 0;

    printf("解析statement，当前token: %s %s\n", token, token1);

    if (strcmp(token, "if") == 0 || strcmp(token1, "if") == 0) {
        ast_begin("IfStatement");
        es = if_stat();
        ast_end();
    } else if (strcmp(token, "while") == 0 || strcmp(token1, "while") == 0) {
        ast_begin("WhileStatement");
        es = while_stat();
        ast_end();
    } else if (strcmp(token, "for") == 0 || strcmp(token1, "for") == 0) {
        ast_begin("ForStatement");
        es = for_stat();
        ast_end();
    } else if (strcmp(token, "{") == 0 || strcmp(token1, "{") == 0) {
        ast_begin("CompoundStatement"); //复合语句
        es = compound_stat();
        ast_end();
    } else if (strcmp(token, "call") == 0 || strcmp(token1, "call") == 0) {
        ast_begin("CallStatement"); //函数调用
        es = call_stat();
        ast_end();
    } else if (strcmp(token, "read") == 0 || strcmp(token1, "read") == 0) {
        ast_begin("ReadStatement");
        es = read_stat();
        ast_end();
    } else if (strcmp(token, "write") == 0 || strcmp(token1, "write") == 0) {
        ast_begin("WriteStatement");
        es = write_stat();
        ast_end();
    } else if (strcmp(token, "ID") == 0 || strcmp(token1, "ID") == 0) {
        // 赋值语句或表达式语句
        ast_begin("AssignmentOrExpression");
        es = expression();
        ast_end();

        // 仓颉语言：语句可以没有分号
        if (es == 0 && (strcmp(token, ";") == 0 || strcmp(token1, ";") == 0)) {
            // 如果有分号，跳过
            if (!read_next_token()) return 10;
        }
    } else if (strcmp(token, "NUM") == 0 || strcmp(token, "(") == 0 || strcmp(token1, "NUM") == 0 || strcmp(token1, "(")
               == 0) {
        es = expression_stat();
    } else if (strcmp(token, ";") == 0 || strcmp(token1, ";") == 0) {
        ast_begin("EmptyStatement");
        ast_add_attr("type", "empty");
        if (!read_next_token()) return 10;
        ast_end();
    } else if (strcmp(token, "var") == 0 || strcmp(token1, "var") == 0) {
        es = declaration_stat();
    } else {
        printf("错误: 未知语句类型: %s %s\n", token, token1);
        es = 9;
    }

    return es;
}

//<if_stat>→ if '('<expr>')' <statement > [else < statement >]
int if_stat() {
    int es = 0;

    // 已读到 if，读取 "("
    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        printf("期望(，得到: %s %s\n", token, token1);
        return 5;
    }

    if (!read_next_token()) return 10;
    ast_begin("Condition");
    es = bool_expr();
    ast_end();
    if (es > 0) return es;

    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        return 6;
    }

    if (!read_next_token()) return 10;

    if (strcmp(token, "{") != 0 && strcmp(token1, "{") != 0) {
        printf("期望{，得到: %s %s\n", token, token1);
        return 1;
    }

    ast_begin("ThenBranch");
    es = statement();
    ast_end();
    if (es > 0) return es;

    /*if(strcmp(token, "}") != 0 && strcmp(token1, "}") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        return 2;
    }*/

    if (strcmp(token, "else") == 0 || strcmp(token1, "else") == 0) {
        ast_add_attr("has_else", "true");
        if (!read_next_token()) return 10;
        ast_begin("ElseBranch");
        es = statement();
        ast_end();
    } else {
        ast_add_attr("has_else", "false");
    }

    return es;
}

//<while_stat>→ while '('<expr >')' < statement >
int while_stat() {
    int es = 0;

    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        printf("期望(，得到: %s %s\n", token, token1);
        return 5;
    }

    if (!read_next_token()) return 10;
    ast_begin("Condition");
    es = bool_expr();
    ast_end();
    if (es > 0) return es;

    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        return 6;
    }

    if (!read_next_token()) return 10;
    ast_begin("LoopBody");
    es = statement();
    ast_end();

    return es;
}

//<for_stat>→ for'('<expr>;<expr>;<expr>')'<statement>
int for_stat() {
    int es = 0;

    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        printf("期望(，得到: %s %s\n", token, token1);
        return 5;
    }

    if (!read_next_token()) return 10;
    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        ast_begin("Initialization");
        es = expression();
        ast_end();
        if (es > 0) return es;
    }

    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        printf("期望;，得到: %s %s\n", token, token1);
        return 4;
    }

    if (!read_next_token()) return 10;
    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        ast_begin("Condition");
        es = bool_expr();
        ast_end();
        if (es > 0) return es;
    }

    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        printf("期望;，得到: %s %s\n", token, token1);
        return 4;
    }

    if (!read_next_token()) return 10;
    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        ast_begin("Increment");
        es = expression();
        ast_end();
        if (es > 0) return es;
    }

    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        return 6;
    }

    if (!read_next_token()) return 10;
    ast_begin("LoopBody");
    es = statement();
    ast_end();

    return es;
}

//<compound_stat>→'{'<statement_list>'}'
int compound_stat() {
    int es = 0;

    ast_add_attr("start", "{");

    if (!read_next_token()) return 10;

    es = statement_list();

    if (es > 0) return es;

    if (strcmp(token, "}") != 0 && strcmp(token1, "}") != 0) {
        printf("期望}，得到: %s %s\n", token, token1);
        return 12;
    }

    ast_add_attr("end", "}");

    if (!read_next_token()) return 10;

    return es;
}

//< call _stat>→call ID( )
int call_stat() {
    int es = 0;
    int symbolPos;

    if (!read_next_token()) return 10;
    if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
        printf("期望ID，得到: %s %s\n", token, token1);
        return 3;
    }

    ast_add_attr("function_name", token1);

    if (lookup(token1, &symbolPos) != 0) {
        printf("函数%s未声明\n", token1);
        return 34;
    }
    if (symbol[symbolPos].kind != function) {
        printf("%s不是函数名\n", token1);
        return 34;
    }

    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        printf("期望(，得到: %s %s\n", token, token1);
        return 5;
    }

    if (!read_next_token()) return 10;
    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        return 6;
    }

    if (!read_next_token()) return 10;
    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        printf("期望;，得到: %s %s\n", token, token1);
        return 4;
    }

    if (!read_next_token()) return 10;

    return es;
}

//<read_stat>→read ID;
int read_stat() {
    int es = 0;
    int pos;

    if (!read_next_token()) return 10;
    if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
        printf("期望ID，得到: %s %s\n", token, token1);
        return 3;
    }

    ast_add_attr("variable_name", token1);

    if (lookup(token1, &pos) != 0) {
        printf("变量%s未声明\n", token1);
        return 23;
    }
    if (symbol[pos].kind != variable) {
        printf("%s不是变量名\n", token1);
        return 35;
    }

    if (!read_next_token()) return 10;

    return es;
}

//<write_stat>→write <expression>;
int write_stat() {
    int es = 0;

    if (!read_next_token()) return 10;

    es = expression();

    if (es > 0) return es;


    if (!read_next_token()) return 10;

    return es;
}
//<expression_stat>→<expression>;|;
int expression_stat() {
    int es = 0;

    // 空表达式语句
    if (strcmp(token, ";") == 0 || strcmp(token1, ";") == 0) {
        ast_add_attr("type", "empty_expression");
        if (!read_next_token()) return 10;
        return 0;
    }

    es = expression();
    if (es > 0) return es;

    // 仓颉语言：表达式语句可以没有分号
    // 如果有分号就跳过，没有就直接继续
    if (strcmp(token, ";") == 0 || strcmp(token1, ";") == 0) {
        if (!read_next_token()) return 10;
    }

    return es;
}

//<expression>→ ID=<bool_expr>|<bool_expr>
int expression() {
    int es = 0;
    ast_begin("Expression");

    printf("进入expression，当前token: %s %s\n", token, token1);

    // 先检查是否是自增/自减表达式 (++x 或 x++)
    if (strcmp(token, "ID") == 0 || strcmp(token1, "ID") == 0) {
        char var_name[256];
        strncpy(var_name, token1, sizeof(var_name) - 1);
        var_name[sizeof(var_name) - 1] = '\0';

        int pos;
        // 检查变量是否已声明
        if (lookup(var_name, &pos) != 0) {
            printf("变量%s未声明\n", var_name);
            es = 23; // 变量未声明
            ast_end();
            return es;
        }

        // 保存当前token，用于回退
        char current_token[64], current_token1[256];
        strcpy(current_token, token);
        strcpy(current_token1, token1);

        // 查看下一个token
        if (!read_next_token()) {
            ast_end();
            return 0;
        }

        if (strcmp(token, "=") == 0 || strcmp(token1, "=") == 0) {
            // 赋值表达式: x = ...
            // 只有在这里才创建 LeftValue
            ast_begin("LeftValue");
            ast_add_attr("variable", var_name);
            ast_end();

            ast_add_attr("operator", "=");

            if (!read_next_token()) {
                ast_end();
                return 10;
            }

            // 直接使用 bool_expr 解析右值，不再进行复杂预读判断
            ast_begin("RightValue");
            es = bool_expr();
            ast_end();
        } else if (strcmp(token, "++") == 0 || strcmp(token1, "++") == 0 ||
                   strcmp(token, "--") == 0 || strcmp(token1, "--") == 0) {
            // 后置自增/自减: x++ 或 x--
            ast_add_attr("operator", token);
            ast_add_attr("position", "postfix");

            if (!read_next_token()) {
                ast_end();
                return 10;
            }
            ast_end();
            return 0;
        } else {
            // 其他情况，可能是普通表达式如 x + 1 或单独的 x
            // 回退到ID位置，让bool_expr从头解析
            // 注意：这里不创建 LeftValue！
            strcpy(token, current_token);
            strcpy(token1, current_token1);

            es = bool_expr();
        }
    } else if (strcmp(token, "++") == 0 || strcmp(token1, "++") == 0 ||
               strcmp(token, "--") == 0 || strcmp(token1, "--") == 0) {
        // 前置自增/自减: ++x 或 --x
        char op[4];
        strncpy(op, token, sizeof(op) - 1);
        op[sizeof(op) - 1] = '\0';
        ast_add_attr("operator", op);
        ast_add_attr("position", "prefix");

        if (!read_next_token()) {
            ast_end();
            return 10;
        }

        if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
            printf("期望标识符，得到: %s %s\n", token, token1);
            ast_end();
            return 7;
        }

        ast_begin("Operand");
        ast_add_attr("variable", token1);
        ast_end();

        if (!read_next_token()) {
            ast_end();
            return 10;
        }
        ast_end();
        return 0;
    } else {
        // 普通表达式
        es = bool_expr();
    }

    ast_end();
    return es;
}

//<bool_expr>-><additive_expr> |< additive_expr >(>|<|>=|<=|==|!=)< additive_expr >
int bool_expr() {
    int es = 0;

    // 先解析左操作数
    es = additive_expr();
    if (es > 0) return es;

    // 检查是否有比较运算符
    if (strcmp(token, ">") == 0 || strcmp(token, ">=") == 0 ||
        strcmp(token, "<") == 0 || strcmp(token, "<=") == 0 ||
        strcmp(token, "==") == 0 || strcmp(token, "!=") == 0 ||
        strcmp(token1, ">") == 0 || strcmp(token1, ">=") == 0 ||
        strcmp(token1, "<") == 0 || strcmp(token1, "<=") == 0 ||
        strcmp(token1, "==") == 0 || strcmp(token1, "!=") == 0) {
        char op[8];
        strncpy(op, token, sizeof(op) - 1);
        op[sizeof(op) - 1] = '\0';

        if (!read_next_token()) return 10;

        // 创建 BinaryExpression 作为比较表达式
        ast_begin("BinaryExpression");
        ast_add_attr("operator", op);

        // 解析右操作数
        es = additive_expr();
        if (es > 0) {
            ast_end();
            return es;
        }

        ast_end();
    }

    return es;
}

//<additive_expr>→<term>{(+|-)< term >}
int additive_expr() {
    int es = 0;

    es = term();
    if (es > 0) return es;

    // 处理加法运算
    while (strcmp(token, "+") == 0 || strcmp(token, "-") == 0 ||
           strcmp(token1, "+") == 0 || strcmp(token1, "-") == 0) {
        char op[4];
        if (strcmp(token, "+") == 0 || strcmp(token, "-") == 0) strncpy(op, token, sizeof(op) - 1);
        else strncpy(op, token1, sizeof(op) - 1);
        op[sizeof(op) - 1] = '\0';

        if (!read_next_token()) return 10;

        ast_begin("BinaryExpression");
        ast_add_attr("operator", op);

        es = term();
        if (es > 0) {
            ast_end();
            return es;
        }

        ast_end();
    }

    return es;
}

//< term >→<factor>{(*| /)< factor >}
int term() {
    int es = 0;

    es = factor();
    if (es > 0) return es;

    // 处理乘法运算
    while (strcmp(token, "*") == 0 || strcmp(token, "/") == 0 ||
           strcmp(token1, "*") == 0 || strcmp(token1, "/") == 0) {
        char op[4];
        if (strcmp(token, "*") == 0 || strcmp(token, "/") == 0) strncpy(op, token, sizeof(op) - 1);
        else strncpy(op, token1, sizeof(op) - 1);
        op[sizeof(op) - 1] = '\0';

        if (!read_next_token()) return 10;

        ast_begin("BinaryExpression");
        ast_add_attr("operator", op);

        es = factor();
        if (es > 0) {
            ast_end();
            return es;
        }

        ast_end();
    }

    return es;
}

//< factor >→'('<additive_expr>')'| ID|NUM
int factor() {
    int es = 0;

    if (strcmp(token, "(") == 0 || strcmp(token1, "(") == 0) {
        if (!read_next_token()) return 10;

        es = additive_expr();
        if (es > 0) return es;

        if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
            printf("期望)，得到: %s %s\n", token, token1);
            return 6;
        }

        if (!read_next_token()) return 10;
    } else if (strcmp(token, "ID") == 0 || strcmp(token1, "ID") == 0) {
        ast_begin("Identifier");
        ast_add_attr("name", token1);
        ast_end();
        if (!read_next_token()) return 10;

    } else if (strcmp(token, "NUM") == 0 || strcmp(token1, "NUM") == 0) {
        ast_begin("BasicLit");
        ast_add_attr("value", token1);
        ast_end();
        if (!read_next_token()) return 10;
    } else {
        printf("期望因子，得到: %s %s\n", token, token1);
        return 7;
    }

    return es;
}


int insert_Symbol(enum Category_symbol category, char *name) {
    int i, es = 0;

    if (symbolIndex >= maxsymbolIndex) return (21);

    // 检查符号是否已存在
    for (i = symbolIndex - 1; i >= 0; i--) {
        if (strcmp(symbol[i].name, name) == 0) {
            // 符号已存在，检查类型
            if (symbol[i].kind == category) {
                // 同类型的重复定义
                if (category == function) {
                    es = 32; // 函数名重复定义
                } else {
                    es = 22; // 变量名重复定义
                }
            } else {
                // 同名但不同类型，可能也是错误，根据你的语言规则决定
                // 这里假设不允许同名（即使是不同类型）
                if (category == function) {
                    es = 32; // 函数名与变量名冲突
                } else {
                    es = 22; // 变量名与函数名冲突
                }
            }
            break;
        }
    }

    if (es > 0) return (es);

    // 插入新符号
    switch (category) {
        case function:
            symbol[symbolIndex].kind = function;
            break;
        case variable:
            symbol[symbolIndex].kind = variable;
            symbol[symbolIndex].address = offset;
            offset++;
            break;
    }

    strncpy(symbol[symbolIndex].name, name, sizeof(symbol[symbolIndex].name) - 1);
    symbol[symbolIndex].name[sizeof(symbol[symbolIndex].name) - 1] = '\0';
    symbolIndex++;
    return es;
}

int lookup(char *name, int *pPosition) {
    int i;
    for (i = symbolIndex - 1; i >= 0; i--) {
        if (strcmp(symbol[i].name, name) == 0) {
            *pPosition = i;
            return 0;
        }
    }
    return 23;
}

int main() {
    return TESTparse();
}
