#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define maxsymbolIndex 100
#define MAX_CODES 200

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
int lookup(char *name, int *pPosition) ;

// 数据类型枚举
enum DataType {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_CHAR,
    TYPE_UNKNOWN
};

enum Category_symbol { variable, function };

// 中间代码结构
typedef struct Code {
    char opt[10];    // 操作码
    int operand;     // 操作数
} Code;

Code codes[MAX_CODES];
int codesIndex = 0;
int temp_var_count = 0;
int label_count = 0;

// 符号表结构（增强版）
typedef struct {
    char name[64];
    enum Category_symbol kind;
    enum DataType type;      // 新增：数据类型
    int address;
    int initialized;         // 新增：是否已初始化
} SymbolEntry;

SymbolEntry symbol[maxsymbolIndex];
int symbolIndex = 0;
int offset;
int scope_level = 0;

// AST相关变量
char *astText = NULL;
size_t astCap = 0;
int indentLevel = 0;

// Token相关变量
char token[64], token1[256];
char tokenfile[260];
FILE *fpTokenin;

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

// 生成临时变量（返回临时变量编号）
int new_temp() {
    return temp_var_count++;
}

// 生成标签（返回标签编号）
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

// ===================== 语义分析函数 =====================

// 类型兼容性检查
int check_type_compatible(enum DataType t1, enum DataType t2) {
    // 简单实现：只允许相同类型
    if (t1 == t2) return 1;

    // 允许int到float/double的隐式转换
    if (t1 == TYPE_INT && (t2 == TYPE_FLOAT || t2 == TYPE_DOUBLE)) {
        return 1;
    }
    if (t2 == TYPE_INT && (t1 == TYPE_FLOAT || t1 == TYPE_DOUBLE)) {
        return 1;
    }

    return 0;
}

// 检查变量是否已初始化
int check_variable_initialized(char *name) {
    int pos;
    if (lookup(name, &pos) == 0 && symbol[pos].kind == variable) {
        return symbol[pos].initialized;
    }
    return 1; // 如果不是变量或没找到，返回1（避免误报）
}

// 标记变量已初始化
void mark_variable_initialized(char *name) {
    int pos;
    if (lookup(name, &pos) == 0 && symbol[pos].kind == variable) {
        symbol[pos].initialized = 1;
    }
}

// 获取变量的类型
enum DataType get_variable_type(char *name) {
    int pos;
    if (lookup(name, &pos) == 0) {
        return symbol[pos].type;
    }
    return TYPE_UNKNOWN;
}

// ===================== 符号表函数 =====================

// 插入符号到符号表（增强版）
int insert_Symbol(enum Category_symbol category, char *name, enum DataType type) {
    int i, es = 0;

    if (symbolIndex >= maxsymbolIndex) return (21);

    // 检查重复定义（在当前作用域）
    for (i = symbolIndex - 1; i >= 0; i--) {
        if (strcmp(symbol[i].name, name) == 0) {
            if (symbol[i].kind == category) {
                if (category == function) {
                    es = 32; // 函数名重复定义
                } else {
                    es = 22; // 变量名重复定义
                }
            } else {
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
            symbol[symbolIndex].type = type;
            symbol[symbolIndex].initialized = 1; // 函数视为已初始化
            break;
        case variable:
            symbol[symbolIndex].kind = variable;
            symbol[symbolIndex].type = type;
            symbol[symbolIndex].initialized = 0; // 变量初始时未初始化
            symbol[symbolIndex].address = offset;
            offset++;
            break;
    }

    strncpy(symbol[symbolIndex].name, name, sizeof(symbol[symbolIndex].name) - 1);
    symbol[symbolIndex].name[sizeof(symbol[symbolIndex].name) - 1] = '\0';
    symbolIndex++;
    return es;
}

// 查找符号
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


// ===================== 主测试函数 =====================
int TESTparse() {
    int es = 0;
    printf("请输入单词流文件名（包括路径）：");
    if (scanf("%s", tokenfile) != 1) return 10;
    if ((fpTokenin = fopen(tokenfile, "r")) == NULL) {
        printf("\n打开%s错误!\n", tokenfile);
        return 10;
    }

    ast_init();
    codesIndex = 0;
    temp_var_count = 0;
    label_count = 0;
    symbolIndex = 0;

    if (!read_next_token()) {
        printf("错误: 文件为空\n");
        fclose(fpTokenin);
        return 10;
    }

    es = program();
    fclose(fpTokenin);

    // 输出结果
    printf("\n==语法分析程序结果==\n");
    if (es == 0) {
        printf("语法分析成功!\n");
    } else {
        // 错误码映射
        const char* error_messages[] = {
            "成功",
            "缺少{!", "缺少}!", "缺少标识符!", "少分号!", "缺少(!", "缺少)!",
            "缺少操作数!", "缺少参数类型!", "赋值语句的左值不是变量名!",
            "读取文件失败!", "函数开头缺少{!", "函数结束缺少}!", "缺少main函数!",
            "", "", "", "", "", "",
            "符号表已满!", "变量重复定义!", "变量未声明!",
            "", "", "", "", "", "", "",
            "函数名重复定义!", "", "", "", "不是变量名!"
        };

        if (es >= 0 && es < sizeof(error_messages)/sizeof(error_messages[0])) {
            if (error_messages[es][0] != '\0') {
                printf("错误[%d]: %s\n", es, error_messages[es]);
            } else {
                printf("错误码: %d\n", es);
            }
        } else {
            printf("未知错误码: %d\n", es);
        }
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

    // 输出AST
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

// ===================== 语法分析函数实现 =====================

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

    // 插入main函数，返回类型为int
    es = insert_Symbol(function, "main", TYPE_INT);
    if (es > 0) {
        ast_end();
        ast_end();
        return es;
    }

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

    offset = 2;  // 重置局部变量地址

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

    char var_name[256];
    strncpy(var_name, token1, sizeof(var_name) - 1);
    var_name[sizeof(var_name) - 1] = '\0';

    ast_begin("id");
    ast_add_attr("type", "Identifier");
    ast_add_attr("name", token1);

    if (!read_next_token()) return 10;
    if (strcmp(token, ":") != 0 && strcmp(token1, ":") != 0) {
        printf("期望:，得到: %s %s\n", token, token1);
        return (es = 4);
    }

    if (!read_next_token()) return 10;

    // 确定变量类型
    enum DataType var_type = TYPE_UNKNOWN;
    if (strcmp(token, "int") == 0 || strcmp(token1, "int") == 0) {
        var_type = TYPE_INT;
        ast_add_attr("kind", "int");
    } else if (strcmp(token, "double") == 0 || strcmp(token1, "double") == 0) {
        var_type = TYPE_DOUBLE;
        ast_add_attr("kind", "double");
    } else if (strcmp(token, "float") == 0 || strcmp(token1, "float") == 0) {
        var_type = TYPE_FLOAT;
        ast_add_attr("kind", "float");
    } else if (strcmp(token, "char") == 0 || strcmp(token1, "char") == 0) {
        var_type = TYPE_CHAR;
        ast_add_attr("kind", "char");
    } else {
        printf("期望类型，得到: %s %s\n", token, token1);
        return (es = 8);
    }

    // 插入符号表，记录类型
    es = insert_Symbol(variable, var_name, var_type);
    if (es > 0) return (es);

    // 生成变量声明代码
    gen_code("DECL", symbolIndex - 1);  // operand存储符号表索引

    ast_end(); // id
    if (!read_next_token()) return 10;

    ast_end(); // VariableDeclarator
    ast_end(); // declarations
    ast_end(); // VariableDeclaration
    return (es);
}

// <statement_list>→{<statement>}
int statement_list() {
    int es = 0;
    ast_begin("StatementList");

    while ((strcmp(token, "}") != 0 && strcmp(token1, "}") != 0) &&
           (token[0] != '\0' && !feof(fpTokenin))) {
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
    int es = 0, cx1, cx2;

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

    // 语义：read后变量已初始化
    mark_variable_initialized(token1);

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

    // 语义检查：写入前检查变量是否初始化
    if (!check_variable_initialized(token1)) {
        printf("警告: 变量 %s 可能未初始化\n", token1);
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
            // 赋值语句
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

            // 语义：赋值后标记变量已初始化
            mark_variable_initialized(var_name);

            // 生成赋值代码
            strcpy(codes[codesIndex].opt, "STO");
            codes[codesIndex].operand = pos;
            codesIndex++;

        } else if (strcmp(token, "++") == 0 || strcmp(token1, "++") == 0 ||
                   strcmp(token, "--") == 0 || strcmp(token1, "--") == 0) {
            // 自增自减
            ast_add_attr("operator", token);
            ast_add_attr("position", "postfix");

            // 语义检查：使用前检查是否初始化
            if (!check_variable_initialized(var_name)) {
                printf("警告: 变量 %s 可能未初始化\n", var_name);
            }

            // 生成自增自减代码
            if (strcmp(token, "++") == 0) {
                strcpy(codes[codesIndex].opt, "INC");
            } else {
                strcpy(codes[codesIndex].opt, "DEC");
            }
            codes[codesIndex].operand = pos;
            codesIndex++;

            // 自增自减后变量已初始化
            mark_variable_initialized(var_name);

            if (!read_next_token()) {
                ast_end();
                return 10;
            }
            ast_end();
            return 0;
        } else {
            // 读取变量值
            strcpy(token, current_token);
            strcpy(token1, current_token1);

            // 语义检查：使用前检查是否初始化
            if (!check_variable_initialized(var_name)) {
                printf("警告: 变量 %s 可能未初始化\n", var_name);
            }

            // 生成加载变量代码
            strcpy(codes[codesIndex].opt, "LOAD");
            codes[codesIndex].operand = pos;
            codesIndex++;

            es = bool_expr();
        }
    } else if (strcmp(token, "++") == 0 || strcmp(token1, "++") == 0 ||
               strcmp(token, "--") == 0 || strcmp(token1, "--") == 0) {
        // 前置自增自减
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

        // 语义检查：使用前检查是否初始化
        if (!check_variable_initialized(token1)) {
            printf("警告: 变量 %s 可能未初始化\n", token1);
        }

        // 生成前置自增自减代码
        if (op[0] == '+') {
            strcpy(codes[codesIndex].opt, "PRE_INC");
        } else {
            strcpy(codes[codesIndex].opt, "PRE_DEC");
        }
        codes[codesIndex].operand = pos;
        codesIndex++;

        // 自增自减后变量已初始化
        mark_variable_initialized(token1);

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
            // 语义检查：使用前检查是否初始化
            if (!check_variable_initialized(token1)) {
                printf("警告: 变量 %s 可能未初始化\n", token1);
            }

            // 生成加载变量代码
            strcpy(codes[codesIndex].opt, "LOAD");
            codes[codesIndex].operand = pos;
            codesIndex++;
        } else {
            printf("变量%s未声明\n", token1);
            return 23;
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