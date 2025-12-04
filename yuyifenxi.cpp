#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define maxsymbolIndex 100
#define MAX_CODES 200

enum Category_symbol { variable, function };

// 中间代码结构
typedef struct Code {
    char opt[10];    // 操作码
    int operand;     // 操作数
} Code;

Code codes[MAX_CODES];
int codesIndex = 0;      // codes数组第一个空元素的下标
int temp_var_count = 0;  // 临时变量计数
int label_count = 0;     // 标签计数

// 符号表结构
typedef struct {
    char name[64];
    enum Category_symbol kind;
    int address;
} SymbolEntry;

SymbolEntry symbol[maxsymbolIndex];
int symbolIndex = 0;
int offset;
int scope_level = 0;

// AST相关变量
char *astText = NULL;
size_t astCap = 0;
int indentLevel = 0;

// ===================== AST函数 =====================
void ast_init() {
    astCap = 1 << 16;
    astText = (char *)malloc(astCap);
    if (!astText) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    astText[0] = '\0';
    indentLevel = 0;
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
        astText = (char *)realloc(astText, astCap);
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

// ===================== 中间代码生成函数 =====================

// 生成中间代码
void gen_code(const char *opt, int operand) {
    if (codesIndex >= MAX_CODES) {
        printf("错误：中间代码表已满\n");
        return;
    }
    strcpy(codes[codesIndex].opt, opt);
    codes[codesIndex].operand = operand;

    printf("生成代码[%d]: %s %d\n", codesIndex, opt, operand);
    codesIndex++;
}

// 生成临时变量
int new_temp() {
    return temp_var_count++;
}

// 生成标签
int new_label() {
    return label_count++;
}

// 输出所有中间代码
void print_intermediate_code() {
    printf("\n=== 中间代码生成结果 ===\n");
    printf("%-6s %-10s %-10s\n", "序号", "操作码", "操作数");
    printf("--------------------------\n");
    for (int i = 0; i < codesIndex; i++) {
        printf("%-6d %-10s %-10d\n", i, codes[i].opt, codes[i].operand);
    }
}

// ===================== 符号表函数 =====================
int insert_Symbol(enum Category_symbol category, char *name) {
    int i, es = 0;

    if (symbolIndex >= maxsymbolIndex) return (21);

    for (i = symbolIndex - 1; i >= 0; i--) {
        if (strcmp(symbol[i].name, name) == 0) {
            if (symbol[i].kind == category) {
                if (category == function) {
                    es = 32;
                } else {
                    es = 22;
                }
            } else {
                if (category == function) {
                    es = 32;
                } else {
                    es = 22;
                }
            }
            break;
        }
    }

    if (es > 0) return (es);

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

// ===================== Token读取函数 =====================
char token[64], token1[256];
char tokenfile[260];
FILE *fpTokenin;

int read_next_token() {
    char line[512];
    if (fgets(line, sizeof(line), fpTokenin) == NULL) {
        token[0] = '\0';
        token1[0] = '\0';
        return 0;
    }
    line[strcspn(line, "\n")] = '\0';

    char *token_end = line;
    while (*token_end != ' ' && *token_end != '\t' && *token_end != '\0') {
        token_end++;
    }

    int type_len = token_end - line;
    if (type_len > 63) type_len = 63;
    strncpy(token, line, type_len);
    token[type_len] = '\0';

    char *value_start = token_end;
    while (*value_start == ' ' || *value_start == '\t') {
        value_start++;
    }

    if (*value_start != '\0') {
        strncpy(token1, value_start, 255);
        token1[255] = '\0';
    } else {
        token1[0] = '\0';
    }

    printf("读取token: type='%s' value='%s'\n", token, token1);
    return 1;
}

// ===================== 函数声明 =====================
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

// ===================== 主测试函数 =====================
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
    codesIndex = 0;
    temp_var_count = 0;
    label_count = 0;

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
        case 10: printf("读取文件 %s失败!\n", tokenfile);
            break;
        case 1: printf("缺少{!\n");
            break;
        case 2: printf("缺少}!\n");
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
        case 13: printf("缺少main函数!\n");
            break;
        case 21: printf("符号表已满!\n");
            break;
        case 22: printf("变量%s重复定义!\n", token1);
            break;
        case 23: printf("变量未声明!\n");
            break;
        case 32: printf("函数名重复定义!\n");
            break;
        case 35: printf("不是变量名!\n");
            break;
        default: if (es > 0) printf("错误码: %d\n", es);
            break;
    }

    // 输出中间代码
    print_intermediate_code();

    // 保存中间代码到文件
    char codefile[512];
    snprintf(codefile, sizeof(codefile), "%s.codes.txt", tokenfile);
    FILE *fcode = fopen(codefile, "w");
    if (fcode) {
        fprintf(fcode, "中间代码列表:\n");
        fprintf(fcode, "%-6s %-10s %-10s\n", "序号", "操作码", "操作数");
        fprintf(fcode, "--------------------------\n");
        for (int i = 0; i < codesIndex; i++) {
            fprintf(fcode, "%-6d %-10s %-10d\n", i, codes[i].opt, codes[i].operand);
        }
        fclose(fcode);
        printf("中间代码已输出到 %s\n", codefile);
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

// <program> → main_declaration
int program() {
    int es = 0;
    ast_begin("Program");

    if (strcmp(token, "main") != 0 && strcmp(token1, "main") != 0) {
        printf("期望main，得到: %s %s\n", token, token1);
        es = 13;
        return es;
    }

    ast_begin("main_declaration");
    insert_Symbol(function, "main");

    // 生成函数入口代码
    gen_code("ENTRY", 0);

    if (!read_next_token()) return 10;
    es = main_declaration();
    if (es > 0) {
        ast_end();
        ast_end();
        return (es);
    }

    // 生成函数返回代码
    gen_code("RET", 0);

    ast_end();
    ast_end();
    return (es);
}

// <main_declaration>→ main '(' ')' <function_body>
int main_declaration() {
    int es = 0;
    ast_add_attr("ID", "main");

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

// <function_body> → '{' <declaration_list> <statement_list> '}'
int function_body() {
    int es = 0;
    ast_begin("Function_Body");

    if (strcmp(token, "{") != 0 && strcmp(token1, "{") != 0) {
        printf("期望{，得到: %s %s\n", token, token1);
        es = 11;
        return (es);
    }

    offset = 2;

    if (!read_next_token()) return 10;
    es = declaration_list();
    if (es > 0) return (es);

    es = statement_list();
    if (es > 0) return (es);

    if (strcmp(token, "}") != 0 && strcmp(token1, "}") != 0) {
        printf("期望}，得到: %s %s\n", token, token1);
        es = 12;
        return (es);
    }

    ast_end();
    return 0;
}

// <declaration_list> → { <declaration_stat> }
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

// <declaration_stat> → var ID : 类型
int declaration_stat() {
    int es = 0;
    ast_begin("VariableDeclaration");
    ast_begin("declarations");
    ast_begin("VariableDeclarator");

    if (!read_next_token()) return 10;
    if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
        printf("期望ID，得到: %s %s\n", token, token1);
        return (es = 3);
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

    // 生成变量声明代码
    gen_code("DECL", symbolIndex - 1);  // operand存储符号表索引

    ast_end();
    if (!read_next_token()) return 10;

    ast_end();
    ast_end();
    ast_end();
    return (es);
}

// <statement_list>→{<statement>}
int statement_list() {
    int es = 0;
    ast_begin("StatementList");

    while ((strcmp(token, "}") != 0 && strcmp(token1, "}") != 0) && (token[0] != '\0' && !feof(fpTokenin))) {
        printf("解析语句，当前token: %s %s\n", token, token1);
        es = statement();
        if (es > 0) return es;

        if (token[0] == '\0' && feof(fpTokenin)) {
            break;
        }
    }

    ast_end();
    return es;
}

// <statement>→ <if_stat>|<while_stat>|<for_stat>|<compound_stat>|<expression_stat>|<call_stat>
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
        ast_begin("CompoundStatement");
        es = compound_stat();
        ast_end();
    } else if (strcmp(token, "call") == 0 || strcmp(token1, "call") == 0) {
        ast_begin("CallStatement");
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
        ast_begin("AssignmentOrExpression");
        es = expression();
        ast_end();

        if (es == 0 && (strcmp(token, ";") == 0 || strcmp(token1, ";") == 0)) {
            if (!read_next_token()) return 10;
        }
    } else if (strcmp(token, "NUM") == 0 || strcmp(token, "(") == 0 ||
               strcmp(token1, "NUM") == 0 || strcmp(token1, "(") == 0) {
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

// <if_stat>→ if '(' <expr> ')' <statement> [else <statement>]
int if_stat() {
    int es = 0, cx1, cx2;

    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        printf("期望(，得到: %s %s\n", token, token1);
        return (es = 5);
    }

    if (!read_next_token()) return 10;
    es = bool_expr();
    if (es > 0) return (es);

    // 生成条件跳转代码
    strcpy(codes[codesIndex].opt, "BRF");
    cx1 = codesIndex++;

    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        return (es = 6);
    }

    if (!read_next_token()) return 10;
    es = statement();
    if (es > 0) return (es);

    strcpy(codes[codesIndex].opt, "BR");
    cx2 = codesIndex++;

    codes[cx1].operand = codesIndex;

    if (strcmp(token, "else") == 0 || strcmp(token1, "else") == 0) {
        ast_add_attr("has_else", "true");
        if (!read_next_token()) return 10;
        es = statement();
        if (es > 0) return (es);
    } else {
        ast_add_attr("has_else", "false");
    }

    codes[cx2].operand = codesIndex;
    return (es);
}

// <while_stat>→ while '(' <expr> ')' <statement>
int while_stat() {
    int es = 0, cx1, cx2;

    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        printf("期望(，得到: %s %s\n", token, token1);
        return 5;
    }

    int loop_start = codesIndex;

    if (!read_next_token()) return 10;
    es = bool_expr();
    if (es > 0) return es;

    // 生成条件跳转
    strcpy(codes[codesIndex].opt, "BRF");
    cx1 = codesIndex++;

    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        return 6;
    }

    if (!read_next_token()) return 10;
    es = statement();
    if (es > 0) return es;

    // 跳回循环开始
    strcpy(codes[codesIndex].opt, "BR");
    codes[codesIndex].operand = loop_start;
    codesIndex++;

    codes[cx1].operand = codesIndex;
    return es;
}

// <for_stat>→ for '(' <expr> ; <expr> ; <expr> ')' <statement>
int for_stat() {
    int es = 0, cx1, cx2, cx3;

    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        printf("期望(，得到: %s %s\n", token, token1);
        return 5;
    }

    // 初始化表达式
    if (!read_next_token()) return 10;
    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        ast_begin("Initialization");
        es = expression();
        ast_end();
        if (es > 0) return es;
    }

    int loop_start = codesIndex;

    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        printf("期望;，得到: %s %s\n", token, token1);
        return 4;
    }

    // 条件表达式
    if (!read_next_token()) return 10;
    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        ast_begin("Condition");
        es = bool_expr();
        ast_end();
        if (es > 0) return es;
    }

    // 条件跳转
    strcpy(codes[codesIndex].opt, "BRF");
    cx1 = codesIndex++;

    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        printf("期望;，得到: %s %s\n", token, token1);
        return 4;
    }

    // 增量表达式跳转
    strcpy(codes[codesIndex].opt, "BR");
    cx2 = codesIndex++;

    int inc_start = codesIndex;

    // 增量表达式
    if (!read_next_token()) return 10;
    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        ast_begin("Increment");
        es = expression();
        ast_end();
        if (es > 0) return es;
    }

    // 跳回循环开始
    strcpy(codes[codesIndex].opt, "BR");
    codes[codesIndex].operand = loop_start;
    codesIndex++;

    codes[cx2].operand = codesIndex;

    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        return 6;
    }

    if (!read_next_token()) return 10;
    ast_begin("LoopBody");
    es = statement();
    ast_end();

    // 跳转到增量表达式
    strcpy(codes[codesIndex].opt, "BR");
    codes[codesIndex].operand = inc_start;
    codesIndex++;

    codes[cx1].operand = codesIndex;
    return es;
}

// <compound_stat>→ '{' <statement_list> '}'
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

// <call_stat>→ call ID ( )
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
        return 23;
    }
    if (symbol[symbolPos].kind != function) {
        printf("%s不是函数名\n", token1);
        return 35;
    }

    // 生成函数调用代码
    strcpy(codes[codesIndex].opt, "CALL");
    codes[codesIndex].operand = symbolPos;
    codesIndex++;

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

// <read_stat>→ read ID ;
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

    // 生成读入代码
    strcpy(codes[codesIndex].opt, "READ");
    codes[codesIndex].operand = pos;
    codesIndex++;

    if (!read_next_token()) return 10;
    return es;
}

// <write_stat>→ write ID ;
int write_stat() {
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

    // 生成写出代码
    strcpy(codes[codesIndex].opt, "WRITE");
    codes[codesIndex].operand = pos;
    codesIndex++;

    if (!read_next_token()) return 10;
    return es;
}

// <expression_stat>→ <expression> ; | ;
int expression_stat() {
    int es = 0;

    if (strcmp(token, ";") == 0 || strcmp(token1, ";") == 0) {
        ast_add_attr("type", "empty_expression");
        if (!read_next_token()) return 10;
        return 0;
    }

    es = expression();
    if (es > 0) return es;

    if (strcmp(token, ";") == 0 || strcmp(token1, ";") == 0) {
        if (!read_next_token()) return 10;
    }

    return es;
}

// <expression>→ ID = <bool_expr> | <bool_expr>
int expression() {
    int es = 0;
    ast_begin("Expression");

    printf("进入expression，当前token: %s %s\n", token, token1);

    if (strcmp(token, "ID") == 0 || strcmp(token1, "ID") == 0) {
        char var_name[256];
        strncpy(var_name, token1, sizeof(var_name) - 1);
        var_name[sizeof(var_name) - 1] = '\0';

        int pos;
        if (lookup(var_name, &pos) != 0) {
            printf("变量%s未声明\n", var_name);
            es = 23;
            ast_end();
            return es;
        }

        char current_token[64], current_token1[256];
        strcpy(current_token, token);
        strcpy(current_token1, token1);

        if (!read_next_token()) {
            ast_end();
            return 0;
        }

        if (strcmp(token, "=") == 0 || strcmp(token1, "=") == 0) {
            ast_begin("LeftValue");
            ast_add_attr("variable", var_name);
            ast_end();

            ast_add_attr("operator", "=");

            if (!read_next_token()) {
                ast_end();
                return 10;
            }

            ast_begin("RightValue");
            es = bool_expr();
            ast_end();

            // 生成赋值代码
            strcpy(codes[codesIndex].opt, "STO");
            codes[codesIndex].operand = pos;
            codesIndex++;
        } else if (strcmp(token, "++") == 0 || strcmp(token1, "++") == 0 ||
                   strcmp(token, "--") == 0 || strcmp(token1, "--") == 0) {
            ast_add_attr("operator", token);
            ast_add_attr("position", "postfix");

            // 生成自增自减代码
            if (strcmp(token, "++") == 0) {
                strcpy(codes[codesIndex].opt, "INC");
            } else {
                strcpy(codes[codesIndex].opt, "DEC");
            }
            codes[codesIndex].operand = pos;
            codesIndex++;

            if (!read_next_token()) {
                ast_end();
                return 10;
            }
            ast_end();
            return 0;
        } else {
            strcpy(token, current_token);
            strcpy(token1, current_token1);
            es = bool_expr();
        }
    } else if (strcmp(token, "++") == 0 || strcmp(token1, "++") == 0 ||
               strcmp(token, "--") == 0 || strcmp(token1, "--") == 0) {
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

        int pos;
        if (lookup(token1, &pos) != 0) {
            printf("变量%s未声明\n", token1);
            ast_end();
            return 23;
        }

        ast_begin("Operand");
        ast_add_attr("variable", token1);
        ast_end();

        // 生成前置自增自减代码
        if (op[0] == '+') {
            strcpy(codes[codesIndex].opt, "PRE_INC");
        } else {
            strcpy(codes[codesIndex].opt, "PRE_DEC");
        }
        codes[codesIndex].operand = pos;
        codesIndex++;

        if (!read_next_token()) {
            ast_end();
            return 10;
        }
        ast_end();
        return 0;
    } else {
        es = bool_expr();
    }

    ast_end();
    return es;
}

// <bool_expr>-> <additive_expr> | <additive_expr> (>|<|>=|<=|==|!=) <additive_expr>
int bool_expr() {
    int es = 0;

    es = additive_expr();
    if (es > 0) return es;

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

        ast_begin("BinaryExpression");
        ast_add_attr("operator", op);

        es = additive_expr();
        if (es > 0) {
            ast_end();
            return es;
        }

        // 生成比较运算代码
        if (strcmp(op, ">") == 0) strcpy(codes[codesIndex].opt, "GT");
        else if (strcmp(op, ">=") == 0) strcpy(codes[codesIndex].opt, "GE");
        else if (strcmp(op, "<") == 0) strcpy(codes[codesIndex].opt, "LT");
        else if (strcmp(op, "<=") == 0) strcpy(codes[codesIndex].opt, "LE");
        else if (strcmp(op, "==") == 0) strcpy(codes[codesIndex].opt, "EQ");
        else if (strcmp(op, "!=") == 0) strcpy(codes[codesIndex].opt, "NE");
        codesIndex++;

        ast_end();
    }

    return es;
}

// <additive_expr>→ <term> {(+|-) <term>}
int additive_expr() {
    int es = 0;

    es = term();
    if (es > 0) return es;

    while (strcmp(token, "+") == 0 || strcmp(token, "-") == 0 ||
           strcmp(token1, "+") == 0 || strcmp(token1, "-") == 0) {

        char op[4];
        if (strcmp(token, "+") == 0 || strcmp(token, "-") == 0)
            strncpy(op, token, sizeof(op) - 1);
        else
            strncpy(op, token1, sizeof(op) - 1);
        op[sizeof(op) - 1] = '\0';

        if (!read_next_token()) return 10;

        ast_begin("BinaryExpression");
        ast_add_attr("operator", op);

        es = term();
        if (es > 0) {
            ast_end();
            return es;
        }

        // 生成加法/减法运算代码
        if (op[0] == '+') {
            strcpy(codes[codesIndex].opt, "ADD");
        } else {
            strcpy(codes[codesIndex].opt, "SUB");
        }
        codesIndex++;

        ast_end();
    }

    return es;
}

// <term>→ <factor> {(*|/) <factor>}
int term() {
    int es = 0;

    es = factor();
    if (es > 0) return es;

    while (strcmp(token, "*") == 0 || strcmp(token, "/") == 0 ||
           strcmp(token1, "*") == 0 || strcmp(token1, "/") == 0) {

        char op[4];
        if (strcmp(token, "*") == 0 || strcmp(token, "/") == 0)
            strncpy(op, token, sizeof(op) - 1);
        else
            strncpy(op, token1, sizeof(op) - 1);
        op[sizeof(op) - 1] = '\0';

        if (!read_next_token()) return 10;

        ast_begin("BinaryExpression");
        ast_add_attr("operator", op);

        es = factor();
        if (es > 0) {
            ast_end();
            return es;
        }

        // 生成乘法/除法运算代码
        if (op[0] == '*') {
            strcpy(codes[codesIndex].opt, "MUL");
        } else {
            strcpy(codes[codesIndex].opt, "DIV");
        }
        codesIndex++;

        ast_end();
    }

    return es;
}

// <factor>→ '(' <additive_expr> ')' | ID | NUM
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

        int pos;
        if (lookup(token1, &pos) == 0) {
            // 生成加载变量代码
            strcpy(codes[codesIndex].opt, "LOAD");
            codes[codesIndex].operand = pos;
            codesIndex++;
        }

        if (!read_next_token()) return 10;
    } else if (strcmp(token, "NUM") == 0 || strcmp(token1, "NUM") == 0) {
        ast_begin("BasicLit");
        ast_add_attr("value", token1);
        ast_end();

        // 生成加载常数代码
        strcpy(codes[codesIndex].opt, "LIT");
        codes[codesIndex].operand = atoi(token1);
        codesIndex++;

        if (!read_next_token()) return 10;
    } else {
        printf("期望因子，得到: %s %s\n", token, token1);
        return 7;
    }

    return es;
}

// ===================== 主函数 =====================
int main() {
    return TESTparse();
}