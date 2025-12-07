#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#define maxsymbolIndex 100
#define MAX_CODES 200
#define MAX_ERRORS 100
int lookup(char *name, int *pPosition);
// 数据类型枚举
enum DataType {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_UNKNOWN
};

enum Category_symbol { variable, function };

// 错误信息结构
typedef struct {
    int line;
    int error_code;
    char message[256];
    int is_warning;
} ErrorInfo;

ErrorInfo error_list[MAX_ERRORS];
int error_count = 0;
int has_fatal_error = 0;

// 中间代码结构
typedef struct Code {
    char opt[10];
    int operand;
} Code;

Code codes[MAX_CODES];
int codesIndex = 0;
int temp_var_count = 0;
int label_count = 0;

// 符号表结构
typedef struct {
    char name[64];
    enum Category_symbol kind;
    enum DataType type;
    int address;
    int initialized;
    int line_declared;
} SymbolEntry;

SymbolEntry symbol[maxsymbolIndex];
int symbolIndex = 0;
int offset;
int current_line = 1;

// AST相关变量
char *astText = NULL;
size_t astCap = 0;
int indentLevel = 0;

// Token相关变量
char token[64], token1[256];
char tokenfile[260];
FILE *fpTokenin;

// ===================== 辅助函数 =====================

// 检查字符串是否为浮点数
int is_float_string(const char *str) {
    if (!str || !*str) return 0;

    int has_dot = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '.') {
            if (has_dot) return 0; // 多个小数点
            has_dot = 1;
        } else if (str[i] < '0' || str[i] > '9') {
            return 0; // 非数字字符
        }
    }
    return has_dot;
}

// 检查字符串是否为整数
int is_int_string(const char *str) {
    if (!str || !*str) return 0;

    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') {
            return 0;
        }
    }
    return 1;
}

// 检查字符串是否为字符串字面量
int is_string_literal(const char *str) {
    if (!str || strlen(str) < 2) return 0;
    return (str[0] == '"' && str[strlen(str)-1] == '"');
}

// ===================== 错误处理函数 =====================

void report_error(int error_code, const char *fmt, ...) {
    if (error_count >= MAX_ERRORS) return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(error_list[error_count].message, sizeof(error_list[error_count].message), fmt, args);
    va_end(args);

    error_list[error_count].error_code = error_code;
    error_list[error_count].line = current_line;
    error_list[error_count].is_warning = 0;
    error_count++;

    printf("错误[%d]: %s\n", error_code, error_list[error_count-1].message);

    if (error_code >= 10 && error_code <= 35) {
        has_fatal_error = 1;
    }
}

void report_warning(const char *fmt, ...) {
    if (error_count >= MAX_ERRORS) return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(error_list[error_count].message, sizeof(error_list[error_count].message), fmt, args);
    va_end(args);

    error_list[error_count].error_code = 0;
    error_list[error_count].line = current_line;
    error_list[error_count].is_warning = 1;
    error_count++;

    printf("警告: %s\n", error_list[error_count-1].message);
}

void print_all_errors() {
    printf("\n=== 错误和警告报告 ===\n");

    int error_num = 0;
    int warning_num = 0;

    for (int i = 0; i < error_count; i++) {
        if (error_list[i].is_warning) {
            warning_num++;
            printf("警告 %d (行 %d): %s\n", warning_num, error_list[i].line, error_list[i].message);
        } else {
            error_num++;
            printf("错误 %d [代码%d] (行 %d): %s\n",
                   error_num, error_list[i].error_code, error_list[i].line, error_list[i].message);
        }
    }

    printf("\n总结: 共发现 %d 个错误，%d 个警告\n", error_num, warning_num);

    if (error_num == 0 && warning_num == 0) {
        printf("语法和语义检查通过！\n");
    } else if (error_num == 0) {
        printf("语法检查通过，但有警告需要注意。\n");
    } else if (has_fatal_error) {
        printf("存在致命错误，无法生成中间代码。\n");
    }
}

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

void gen_code(const char *opt, int operand) {
    if (codesIndex >= MAX_CODES) {
        report_error(99, "中间代码表已满");
        return;
    }
    strcpy(codes[codesIndex].opt, opt);
    codes[codesIndex].operand = operand;

    printf("生成代码[%d]: %s %d\n", codesIndex, opt, operand);
    codesIndex++;
}

int new_temp() {
    return temp_var_count++;
}

int new_label() {
    return label_count++;
}

void print_intermediate_code() {
    if (has_fatal_error) {
        printf("\n由于存在致命错误，跳过中间代码生成。\n");
        return;
    }

    printf("\n=== 中间代码生成结果 ===\n");
    printf("%-6s %-10s %-10s\n", "序号", "操作码", "操作数");
    printf("--------------------------\n");
    for (int i = 0; i < codesIndex; i++) {
        printf("%-6d %-10s %-10d\n", i, codes[i].opt, codes[i].operand);
    }
}

// ===================== 语义分析函数 =====================

const char* type_to_string(enum DataType type) {
    switch (type) {
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_DOUBLE: return "double";
        case TYPE_CHAR: return "char";
        case TYPE_STRING: return "string";
        default: return "unknown";
    }
}

int check_type_compatible(enum DataType t1, enum DataType t2, const char *context) {
    if (t1 == TYPE_UNKNOWN || t2 == TYPE_UNKNOWN) {
        return 1;
    }

    if (t1 == t2) return 1;

    // 字符串类型特殊处理
    if (t1 == TYPE_STRING || t2 == TYPE_STRING) {
        report_error(50, "%s: 字符串类型与其他类型不兼容", context);
        return 0;
    }

    // 允许的隐式类型转换
    if ((t1 == TYPE_INT && (t2 == TYPE_FLOAT || t2 == TYPE_DOUBLE)) ||
        (t1 == TYPE_FLOAT && t2 == TYPE_DOUBLE) ||
        (t1 == TYPE_CHAR && t2 == TYPE_INT)) {
        report_warning("%s: 从 %s 到 %s 的隐式类型转换",
                      context, type_to_string(t2), type_to_string(t1));
        return 1;
    }

    // 反向转换
    if ((t1 == TYPE_FLOAT && t2 == TYPE_INT) ||
        (t1 == TYPE_DOUBLE && (t2 == TYPE_INT || t2 == TYPE_FLOAT)) ||
        (t1 == TYPE_INT && t2 == TYPE_CHAR)) {
        report_warning("%s: 从 %s 到 %s 的隐式类型转换",
                      context, type_to_string(t2), type_to_string(t1));
        return 1;
    }

    report_error(50, "%s: 类型不匹配 (%s 和 %s)",
                context, type_to_string(t1), type_to_string(t2));
    return 0;
}

int check_variable_initialized(char *name) {
    int pos;
    if (lookup(name, &pos) == 0 && symbol[pos].kind == variable) {
        return symbol[pos].initialized;
    }
    return 1;
}

void mark_variable_initialized(char *name) {
    int pos;
    if (lookup(name, &pos) == 0 && symbol[pos].kind == variable) {
        symbol[pos].initialized = 1;
    }
}

enum DataType get_variable_type(char *name) {
    int pos;
    if (lookup(name, &pos) == 0) {
        return symbol[pos].type;
    }
    return TYPE_UNKNOWN;
}

// ===================== 符号表函数 =====================

int insert_Symbol(enum Category_symbol category, char *name, enum DataType type) {
    int i, es = 0;

    if (symbolIndex >= maxsymbolIndex) {
        report_error(21, "符号表已满");
        return 21;
    }

    for (i = symbolIndex - 1; i >= 0; i--) {
        if (strcmp(symbol[i].name, name) == 0) {
            if (symbol[i].kind == category) {
                if (category == function) {
                    report_error(32, "函数名 %s 重复定义", name);
                    es = 32;
                } else {
                    report_error(22, "变量名 %s 重复定义", name);
                    es = 22;
                }
            } else {
                if (category == function) {
                    report_error(32, "%s 已经定义为变量，不能作为函数名", name);
                    es = 32;
                } else {
                    report_error(22, "%s 已经定义为函数，不能作为变量名", name);
                    es = 22;
                }
            }
            break;
        }
    }

    if (es > 0) return es;

    switch (category) {
        case function:
            symbol[symbolIndex].kind = function;
            symbol[symbolIndex].type = type;
            symbol[symbolIndex].initialized = 1;
            break;
        case variable:
            symbol[symbolIndex].kind = variable;
            symbol[symbolIndex].type = type;
            symbol[symbolIndex].initialized = 0;
            symbol[symbolIndex].address = offset;
            symbol[symbolIndex].line_declared = current_line;
            offset++;
            break;
    }

    strncpy(symbol[symbolIndex].name, name, sizeof(symbol[symbolIndex].name) - 1);
    symbol[symbolIndex].name[sizeof(symbol[symbolIndex].name) - 1] = '\0';
    symbolIndex++;
    return 0;
}

int lookup(char *name, int *pPosition) {
    int i;
    for (i = symbolIndex - 1; i >= 0; i--) {
        if (strcmp(symbol[i].name, name) == 0) {
            *pPosition = i;
            return 0;
        }
    }
    report_error(23, "标识符 %s 未声明", name);
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

    if (line[0] != '\0') {
        current_line++;
    }

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

    printf("读取token[行%d]: type='%s' value='%s'\n", current_line, token, token1);
    return 1;
}

// ===================== 错误恢复函数 =====================

void skip_to_sync_point() {
    int skipped = 0;

    while (1) {
        if (feof(fpTokenin) || token[0] == '\0') {
            break;
        }

        if (strcmp(token, ";") == 0 || strcmp(token1, ";") == 0 ||
            strcmp(token, "}") == 0 || strcmp(token1, "}") == 0 ||
            strcmp(token, ")") == 0 || strcmp(token1, ")") == 0 ||
            strcmp(token, "else") == 0 || strcmp(token1, "else") == 0 ||
            strcmp(token, "if") == 0 || strcmp(token1, "if") == 0 ||
            strcmp(token, "while") == 0 || strcmp(token1, "while") == 0 ||
            strcmp(token, "for") == 0 || strcmp(token1, "for") == 0 ||
            strcmp(token, "var") == 0 || strcmp(token1, "var") == 0) {
            break;
        }

        if (!read_next_token()) break;
        skipped++;
    }

    if (skipped > 0) {
        printf("跳过 %d 个token到同步点\n", skipped);
    }
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
    error_count = 0;
    has_fatal_error = 0;
    current_line = 0;

    if (!read_next_token()) {
        printf("错误: 文件为空\n");
        fclose(fpTokenin);
        return 10;
    }

    es = program();
    fclose(fpTokenin);

    print_all_errors();

    if (!has_fatal_error) {
        print_intermediate_code();

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
    }

    char astfile[512];
    snprintf(astfile, sizeof(astfile), "%s.ast.txt", tokenfile);
    FILE *fasta = fopen(astfile, "w");
    if (fasta) {
        fputs(astText, fasta);
        fclose(fasta);
        printf("语法树已输出到 %s\n", astfile);
    }

    free(astText);
    return (es == 0 && error_count == 0) ? 0 : 1;
}

// ===================== 语法分析函数实现 =====================

int program() {
    int es = 0;
    ast_begin("Program");

    if (strcmp(token, "main") != 0 && strcmp(token1, "main") != 0) {
        report_error(13, "缺少main函数，得到: %s %s", token, token1);
        es = 13;
        skip_to_sync_point();
        ast_end();
        return es;
    }

    ast_begin("main_declaration");

    es = insert_Symbol(function, "main", TYPE_INT);
    if (es > 0) {
        ast_end();
        ast_end();
        return es;
    }

    if (!has_fatal_error) {
        gen_code("ENTRY", 0);
    }

    if (!read_next_token()) return 10;
    es = main_declaration();
    if (es > 0) {
        ast_end();
        ast_end();
        return es;
    }

    if (!has_fatal_error) {
        gen_code("RET", 0);
    }

    ast_end();
    ast_end();
    return es;
}

int main_declaration() {
    int es = 0;
    ast_add_attr("ID", "main");

    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        report_error(5, "期望(，得到: %s %s", token, token1);
        es = 5;
        skip_to_sync_point();
        return es;
    }

    if (!read_next_token()) return 10;
    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        report_error(6, "期望)，得到: %s %s", token, token1);
        es = 6;
        skip_to_sync_point();
        return es;
    }

    if (!read_next_token()) return 10;
    es = function_body();
    return es;
}

int function_body() {
    int es = 0;
    ast_begin("Function_Body");

    if (strcmp(token, "{") != 0 && strcmp(token1, "{") != 0) {
        report_error(11, "期望{，得到: %s %s", token, token1);
        es = 11;
        skip_to_sync_point();
        ast_end();
        return es;
    }

    offset = 2;

    if (!read_next_token()) return 10;
    es = declaration_list();
    if (es > 0 && has_fatal_error) {
        ast_end();
        return es;
    }

    es = statement_list();
    if (es > 0 && has_fatal_error) {
        ast_end();
        return es;
    }

    if (strcmp(token, "}") != 0 && strcmp(token1, "}") != 0) {
        report_error(12, "期望}，得到: %s %s", token, token1);
        es = 12;
        skip_to_sync_point();
    }

    ast_end();
    return 0;
}

int declaration_list() {
    int es = 0;
    ast_begin("DeclarationList");

    while (strcmp(token, "var") == 0 || strcmp(token1, "var") == 0) {
        es = declaration_stat();
        if (es > 0 && has_fatal_error) {
            ast_end();
            return es;
        }
    }

    ast_end();
    return es;
}

int declaration_stat() {
    int es = 0;
    ast_begin("VariableDeclaration");
    ast_begin("declarations");
    ast_begin("VariableDeclarator");

    if (!read_next_token()) return 10;
    if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
        report_error(3, "期望标识符，得到: %s %s", token, token1);
        es = 3;
        skip_to_sync_point();
        ast_end();
        ast_end();
        ast_end();
        return es;
    }

    char var_name[256];
    strncpy(var_name, token1, sizeof(var_name) - 1);
    var_name[sizeof(var_name) - 1] = '\0';

    ast_begin("id");
    ast_add_attr("type", "Identifier");
    ast_add_attr("name", token1);

    if (!read_next_token()) return 10;
    if (strcmp(token, ":") != 0 && strcmp(token1, ":") != 0) {
        report_error(4, "期望:，得到: %s %s", token, token1);
        es = 4;
        skip_to_sync_point();
        ast_end();
        ast_end();
        ast_end();
        ast_end();
        return es;
    }

    if (!read_next_token()) return 10;

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
        report_error(8, "期望类型关键字，得到: %s %s", token, token1);
        es = 8;
        skip_to_sync_point();
        ast_end();
        ast_end();
        ast_end();
        ast_end();
        return es;
    }

    es = insert_Symbol(variable, var_name, var_type);
    if (es > 0) {
        ast_end();
        ast_end();
        ast_end();
        ast_end();
        return es;
    }

    if (!has_fatal_error) {
        gen_code("DECL", symbolIndex - 1);
    }

    ast_end();
    if (!read_next_token()) return 10;

    ast_end();
    ast_end();
    ast_end();
    return es;
}

int statement_list() {
    int es = 0;
    ast_begin("StatementList");

    // 防止无限循环的计数器
    int loop_count = 0;
    int max_loops = 1000;

    while ((strcmp(token, "}") != 0 && strcmp(token1, "}") != 0) &&
           (token[0] != '\0' && !feof(fpTokenin))) {

        loop_count++;
        if (loop_count > max_loops) {
            report_error(99, "检测到可能无限循环，强制退出语句列表解析");
            break;
        }

        printf("解析语句，当前token: %s %s\n", token, token1);

        es = statement();
        if (es > 0 && has_fatal_error) {
            ast_end();
            return es;
        }

        if (token[0] == '\0' || feof(fpTokenin)) {
            break;
        }
    }

    ast_end();
    return es;
}

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
        report_error(9, "未知语句类型: %s %s", token, token1);
        es = 9;
        skip_to_sync_point();
    }

    return es;
}

int if_stat() {
    int es = 0, cx1, cx2;

    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        report_error(5, "期望(，得到: %s %s", token, token1);
        es = 5;
        skip_to_sync_point();
        return es;
    }

    if (!read_next_token()) return 10;
    es = bool_expr();
    if (es > 0) return es;

    if (!has_fatal_error) {
        strcpy(codes[codesIndex].opt, "BRF");
        cx1 = codesIndex++;
    }

    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        report_error(6, "期望)，得到: %s %s", token, token1);
        es = 6;
        skip_to_sync_point();
        return es;
    }

    if (!read_next_token()) return 10;
    es = statement();
    if (es > 0) return es;

    if (!has_fatal_error) {
        strcpy(codes[codesIndex].opt, "BR");
        cx2 = codesIndex++;
        codes[cx1].operand = codesIndex;
    }

    if (strcmp(token, "else") == 0 || strcmp(token1, "else") == 0) {
        ast_add_attr("has_else", "true");
        if (!read_next_token()) return 10;
        es = statement();
        if (es > 0) return es;
    } else {
        ast_add_attr("has_else", "false");
    }

    if (!has_fatal_error) {
        codes[cx2].operand = codesIndex;
    }
    return es;
}

int while_stat() {
    int es = 0, cx1;

    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        report_error(5, "期望(，得到: %s %s", token, token1);
        return 5;
    }

    int loop_start = codesIndex;

    if (!read_next_token()) return 10;
    es = bool_expr();
    if (es > 0) return es;

    if (!has_fatal_error) {
        strcpy(codes[codesIndex].opt, "BRF");
        cx1 = codesIndex++;
    }

    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        report_error(6, "期望)，得到: %s %s", token, token1);
        return 6;
    }

    if (!read_next_token()) return 10;
    es = statement();
    if (es > 0) return es;

    if (!has_fatal_error) {
        strcpy(codes[codesIndex].opt, "BR");
        codes[codesIndex].operand = loop_start;
        codesIndex++;
        codes[cx1].operand = codesIndex;
    }
    return es;
}

int for_stat() {
    int es = 0, cx1, cx2;

    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        report_error(5, "期望(，得到: %s %s", token, token1);
        return 5;
    }

    if (!read_next_token()) return 10;
    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        ast_begin("Initialization");
        es = expression();
        ast_end();
        if (es > 0) return es;
    }

    int loop_start = codesIndex;

    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        report_error(4, "期望;，得到: %s %s", token, token1);
        skip_to_sync_point();
    }

    if (!read_next_token()) return 10;
    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        ast_begin("Condition");
        es = bool_expr();
        ast_end();
        if (es > 0) return es;
    }

    if (!has_fatal_error) {
        strcpy(codes[codesIndex].opt, "BRF");
        cx1 = codesIndex++;
    }

    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        report_error(4, "期望;，得到: %s %s", token, token1);
        skip_to_sync_point();
    }

    if (!has_fatal_error) {
        strcpy(codes[codesIndex].opt, "BR");
        cx2 = codesIndex++;
    }

    int inc_start = codesIndex;

    if (!read_next_token()) return 10;
    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        ast_begin("Increment");
        es = expression();
        ast_end();
        if (es > 0) return es;
    }

    if (!has_fatal_error) {
        strcpy(codes[codesIndex].opt, "BR");
        codes[codesIndex].operand = loop_start;
        codesIndex++;
        codes[cx2].operand = codesIndex;
    }

    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        report_error(6, "期望)，得到: %s %s", token, token1);
        skip_to_sync_point();
    }

    if (!read_next_token()) return 10;
    ast_begin("LoopBody");
    es = statement();
    ast_end();

    if (!has_fatal_error) {
        strcpy(codes[codesIndex].opt, "BR");
        codes[codesIndex].operand = inc_start;
        codesIndex++;
        codes[cx1].operand = codesIndex;
    }
    return es;
}

int compound_stat() {
    int es = 0;
    ast_add_attr("start", "{");

    if (!read_next_token()) return 10;
    es = statement_list();

    if (es > 0) return es;

    if (strcmp(token, "}") != 0 && strcmp(token1, "}") != 0) {
        report_error(12, "期望}，得到: %s %s", token, token1);
        es = 12;
        skip_to_sync_point();
    }

    ast_add_attr("end", "}");

    if (!read_next_token()) return 10;
    return es;
}

int call_stat() {
    int es = 0;
    int symbolPos;

    if (!read_next_token()) return 10;
    if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
        report_error(3, "期望标识符，得到: %s %s", token, token1);
        return 3;
    }

    ast_add_attr("function_name", token1);

    if (lookup(token1, &symbolPos) != 0) {
        return 23;
    }
    if (symbol[symbolPos].kind != function) {
        report_error(35, "%s 不是函数名", token1);
        return 35;
    }

    if (!has_fatal_error) {
        strcpy(codes[codesIndex].opt, "CALL");
        codes[codesIndex].operand = symbolPos;
        codesIndex++;
    }

    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        report_error(5, "期望(，得到: %s %s", token, token1);
        return 5;
    }

    if (!read_next_token()) return 10;
    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        report_error(6, "期望)，得到: %s %s", token, token1);
        return 6;
    }

    if (!read_next_token()) return 10;
    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        report_error(4, "期望;，得到: %s %s", token, token1);
        return 4;
    }

    if (!read_next_token()) return 10;
    return es;
}

int read_stat() {
    int es = 0;
    int pos;

    if (!read_next_token()) return 10;
    if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
        report_error(3, "期望标识符，得到: %s %s", token, token1);
        return 3;
    }

    ast_add_attr("variable_name", token1);

    if (lookup(token1, &pos) != 0) {
        return 23;
    }
    if (symbol[pos].kind != variable) {
        report_error(35, "%s 不是变量名", token1);
        return 35;
    }

    if (!has_fatal_error) {
        strcpy(codes[codesIndex].opt, "READ");
        codes[codesIndex].operand = pos;
        codesIndex++;
    }

    mark_variable_initialized(token1);

    if (!read_next_token()) return 10;
    return es;
}

int write_stat() {
    int es = 0;
    int pos;

    if (!read_next_token()) return 10;
    if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
        report_error(3, "期望标识符，得到: %s %s", token, token1);
        return 3;
    }

    ast_add_attr("variable_name", token1);

    if (lookup(token1, &pos) != 0) {
        return 23;
    }
    if (symbol[pos].kind != variable) {
        report_error(35, "%s 不是变量名", token1);
        return 35;
    }

    if (!check_variable_initialized(token1)) {
        report_warning("变量 %s 可能未初始化", token1);
    }

    if (!has_fatal_error) {
        strcpy(codes[codesIndex].opt, "WRITE");
        codes[codesIndex].operand = pos;
        codesIndex++;
    }

    if (!read_next_token()) return 10;
    return es;
}

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
            es = 23;
            ast_end();
            return es;
        }

        enum DataType left_type = get_variable_type(var_name);

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

            // 记录当前token，用于检查赋值右侧的类型
            char saved_token[64], saved_token1[256];
            strcpy(saved_token, token);
            strcpy(saved_token1, token1);

            ast_begin("RightValue");
            es = bool_expr();
            ast_end();

            // 类型检查：检查赋值语句类型匹配
            // 获取右值的类型信息
            if (strcmp(saved_token, "NUM") == 0 || strcmp(saved_token1, "NUM") == 0) {
                // 检查数值类型
                if (is_float_string(saved_token1) && left_type == TYPE_INT) {
                    report_error(51, "不能将浮点数 %s 赋值给整型变量 %s", saved_token1, var_name);
                }
            } else if (is_string_literal(saved_token1) && left_type != TYPE_STRING) {
                report_error(51, "不能将字符串赋值给非字符串变量 %s", var_name);
            }

            mark_variable_initialized(var_name);

            if (!has_fatal_error) {
                strcpy(codes[codesIndex].opt, "STO");
                codes[codesIndex].operand = pos;
                codesIndex++;
            }

        } else if (strcmp(token, "++") == 0 || strcmp(token1, "++") == 0 ||
                   strcmp(token, "--") == 0 || strcmp(token1, "--") == 0) {
            ast_add_attr("operator", token);
            ast_add_attr("position", "postfix");

            if (!check_variable_initialized(var_name)) {
                report_warning("变量 %s 可能未初始化", var_name);
            }

            if (!has_fatal_error) {
                if (strcmp(token, "++") == 0) {
                    strcpy(codes[codesIndex].opt, "INC");
                } else {
                    strcpy(codes[codesIndex].opt, "DEC");
                }
                codes[codesIndex].operand = pos;
                codesIndex++;
            }

            mark_variable_initialized(var_name);

            if (!read_next_token()) {
                ast_end();
                return 10;
            }
            ast_end();
            return 0;
        } else {
            strcpy(token, current_token);
            strcpy(token1, current_token1);

            if (!check_variable_initialized(var_name)) {
                report_warning("变量 %s 可能未初始化", var_name);
            }

            if (!has_fatal_error) {
                strcpy(codes[codesIndex].opt, "LOAD");
                codes[codesIndex].operand = pos;
                codesIndex++;
            }

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
            report_error(7, "期望标识符，得到: %s %s", token, token1);
            ast_end();
            return 7;
        }

        int pos;
        if (lookup(token1, &pos) != 0) {
            ast_end();
            return 23;
        }

        if (!check_variable_initialized(token1)) {
            report_warning("变量 %s 可能未初始化", token1);
        }

        if (!has_fatal_error) {
            if (op[0] == '+') {
                strcpy(codes[codesIndex].opt, "PRE_INC");
            } else {
                strcpy(codes[codesIndex].opt, "PRE_DEC");
            }
            codes[codesIndex].operand = pos;
            codesIndex++;
        }

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
        es = bool_expr();
    }

    ast_end();
    return es;
}

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

        // 检查右操作数是否为字符串
        char saved_token[64], saved_token1[256];
        strcpy(saved_token, token);
        strcpy(saved_token1, token1);

        es = additive_expr();
        if (es > 0) {
            ast_end();
            return es;
        }

        // 类型检查：比较运算的类型匹配
        if (is_string_literal(saved_token1)) {
            report_error(50, "比较运算: 字符串不能参与数值比较");
        }

        if (!has_fatal_error) {
            if (strcmp(op, ">") == 0) strcpy(codes[codesIndex].opt, "GT");
            else if (strcmp(op, ">=") == 0) strcpy(codes[codesIndex].opt, "GE");
            else if (strcmp(op, "<") == 0) strcpy(codes[codesIndex].opt, "LT");
            else if (strcmp(op, "<=") == 0) strcpy(codes[codesIndex].opt, "LE");
            else if (strcmp(op, "==") == 0) strcpy(codes[codesIndex].opt, "EQ");
            else if (strcmp(op, "!=") == 0) strcpy(codes[codesIndex].opt, "NE");
            codesIndex++;
        }

        ast_end();
    }

    return es;
}

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

        if (!check_type_compatible(TYPE_INT, TYPE_INT, "算术运算")) {
            // 错误已报告
        }

        if (!has_fatal_error) {
            if (op[0] == '+') {
                strcpy(codes[codesIndex].opt, "ADD");
            } else {
                strcpy(codes[codesIndex].opt, "SUB");
            }
            codesIndex++;
        }

        ast_end();
    }

    return es;
}

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

        if (!check_type_compatible(TYPE_INT, TYPE_INT, "算术运算")) {
            // 错误已报告
        }

        if (!has_fatal_error) {
            if (op[0] == '*') {
                strcpy(codes[codesIndex].opt, "MUL");
            } else {
                strcpy(codes[codesIndex].opt, "DIV");
            }
            codesIndex++;
        }

        ast_end();
    }

    return es;
}

int factor() {
    int es = 0;

    if (strcmp(token, "(") == 0 || strcmp(token1, "(") == 0) {
        if (!read_next_token()) return 10;

        es = additive_expr();
        if (es > 0) return es;

        if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
            report_error(6, "期望)，得到: %s %s", token, token1);
            return 6;
        }

        if (!read_next_token()) return 10;
    } else if (strcmp(token, "ID") == 0 || strcmp(token1, "ID") == 0) {
        ast_begin("Identifier");
        ast_add_attr("name", token1);
        ast_end();

        int pos;
        if (lookup(token1, &pos) == 0) {
            if (!check_variable_initialized(token1)) {
                report_warning("变量 %s 可能未初始化", token1);
            }

            if (!has_fatal_error) {
                strcpy(codes[codesIndex].opt, "LOAD");
                codes[codesIndex].operand = pos;
                codesIndex++;
            }
        } else {
            return 23;
        }

        if (!read_next_token()) return 10;
    } else if (strcmp(token, "NUM") == 0 || strcmp(token1, "NUM") == 0) {
        ast_begin("BasicLit");
        ast_add_attr("value", token1);
        ast_end();

        // 检查数值类型
        if (is_float_string(token1)) {
            report_warning("浮点数常量 %s 可能损失精度", token1);
        }

        if (!has_fatal_error) {
            strcpy(codes[codesIndex].opt, "LIT");

            // 处理浮点数（简化：截断为整数）
            if (is_float_string(token1)) {
                double float_val = atof(token1);
                codes[codesIndex].operand = (int)float_val;
                report_warning("浮点数 %s 被截断为 %d", token1, (int)float_val);
            } else {
                codes[codesIndex].operand = atoi(token1);
            }
            codesIndex++;
        }

        if (!read_next_token()) return 10;
    }else if (strcmp(token, "STRING") == 0 || strcmp(token1, "STRING") == 0 ||
               (token1[0] == '"' && token1[strlen(token1)-1] == '"')) {
        // 处理字符串字面量
        ast_begin("StringLiteral");
        ast_add_attr("value", token1);
        ast_end();

        // 可以生成字符串常量代码
        if (!has_fatal_error) {
            strcpy(codes[codesIndex].opt, "LIT_STR");
            // 这里应该将字符串存入常量表，返回索引
            codes[codesIndex].operand = 0; // 暂时用0表示
            codesIndex++;
        }

        if (!read_next_token()) return 10;
               } else {
                   report_error(7, "期望因子，得到: %s %s", token, token1);
                   return 7;
               }

    return es;

}

// ===================== 主函数 =====================
int main() {
    return TESTparse();
}