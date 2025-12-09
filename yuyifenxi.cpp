#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#define maxsymbolIndex 100
#define MAX_CODES 200
#define MAX_ERRORS 100
#define MAX_SCOPE_LEVEL 10


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
int break_stat();
int continue_stat();
void report_error(int error_code, const char *fmt, ...);
int lookup_current_scope(char *name, int *pPosition);

// 数据类型枚举
enum DataType {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_VOID,
    TYPE_ARRAY,
    TYPE_UNKNOWN
};

enum Category_symbol { variable, function, parameter };

// 数组信息结构
typedef struct {
    int dimensions;     // 数组维数
    int size[5];        // 各维大小（最大支持5维）
} ArrayInfo;

// 作用域栈
typedef struct {
    int start_symbol;   // 该作用域开始的符号索引
    int level;          // 作用域层级
    char type[10];      // 作用域类型：global, function, block, loop
} ScopeEntry;

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

// 符号表结构（增强版）
typedef struct {
    char name[64];
    enum Category_symbol kind;
    enum DataType type;
    int address;
    int initialized;
    int line_declared;
    int scope_level;        // 新增：作用域层级
    ArrayInfo array_info;   // 新增：数组信息
    int is_param;           // 新增：是否为参数
    int param_index;        // 新增：参数索引
} SymbolEntry;

SymbolEntry symbol[maxsymbolIndex];
int symbolIndex = 0;
int offset;
int current_line = 1;

// 作用域管理
ScopeEntry scope_stack[MAX_SCOPE_LEVEL];
int scope_top = -1;
int current_scope_level = 0;
int in_loop = 0;  // 是否在循环内

// AST相关变量
char *astText = NULL;
size_t astCap = 0;
int indentLevel = 0;

// Token相关变量
char token[64], token1[256];
char tokenfile[260];
FILE *fpTokenin;

// ===================== 作用域管理函数 =====================

void enter_scope(const char *type) {
    if (scope_top >= MAX_SCOPE_LEVEL - 1) {
        report_error(60, "作用域嵌套过深");
        return;
    }

    scope_top++;
    scope_stack[scope_top].start_symbol = symbolIndex;
    scope_stack[scope_top].level = current_scope_level;
    strcpy(scope_stack[scope_top].type, type);

    current_scope_level++;

    printf("进入作用域: %s (层级: %d)\n", type, current_scope_level);

    if (strcmp(type, "loop") == 0) {
        in_loop++;
    }
}

void exit_scope() {
    if (scope_top < 0) return;

    printf("退出作用域: %s (层级: %d)\n",
           scope_stack[scope_top].type, current_scope_level);

    // 如果是循环作用域，减少循环计数
    if (strcmp(scope_stack[scope_top].type, "loop") == 0 && in_loop > 0) {
        in_loop--;
    }

    // 从符号表中移除该作用域的符号（简化：只标记作用域结束）
    current_scope_level--;
    scope_top--;
}

int get_current_scope_level() {
    return current_scope_level;
}

// ===================== 辅助函数 =====================

// 修复后的辅助函数
int is_float_string(const char *str) {
    if (!str || !*str) return 0;

    int has_dot = 0;
    int has_digit = 0;
    int i = 0;

    // 检查是否有符号
    if (str[i] == '+' || str[i] == '-') {
        i++;
    }

    // 检查整数部分
    while (str[i] != '\0' && str[i] >= '0' && str[i] <= '9') {
        has_digit = 1;
        i++;
    }

    // 检查小数点和小数部分
    if (str[i] == '.') {
        has_dot = 1;
        i++;

        // 检查小数部分
        while (str[i] != '\0' && str[i] >= '0' && str[i] <= '9') {
            has_digit = 1;
            i++;
        }
    }

    // 检查科学计数法（可选）
    if (has_digit && (str[i] == 'e' || str[i] == 'E')) {
        i++;
        if (str[i] == '+' || str[i] == '-') {
            i++;
        }
        while (str[i] != '\0' && str[i] >= '0' && str[i] <= '9') {
            i++;
        }
    }

    // 如果整个字符串都被处理了，并且有数字，且有小数点，才是浮点数
    return (str[i] == '\0' && has_digit && has_dot);
}

int is_int_string(const char *str) {
    if (!str || !*str) return 0;

    int i = 0;

    // 检查符号
    if (str[i] == '+' || str[i] == '-') {
        i++;
    }

    // 检查所有字符都是数字
    while (str[i] != '\0') {
        if (str[i] < '0' || str[i] > '9') {
            return 0;
        }
        i++;
    }

    // 确保至少有一个数字
    return (i > 0 && (str[0] != '+' && str[0] != '-')) || (i > 1);
}

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
    printf("\n=== 语义分析报告 ===\n");

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
        printf("语法语义分析通过！\n");
    } else if (error_num == 0) {
        printf("语义检查通过，但有警告需要注意。\n");
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

    // 修正操作码（根据你的指令集）
    if (strcmp(opt, "LIT") == 0) {
        strcpy(codes[codesIndex].opt, "LOADI");
    } else if (strcmp(opt, "LIT_BOOL") == 0) {
        strcpy(codes[codesIndex].opt, "LOADI");
    } else if (strcmp(opt, "INC") == 0) {
        // 自增操作需要多条指令
        // x++ 相当于: LOAD x; LOADI 1; ADD; STO x
        strcpy(codes[codesIndex].opt, "LOAD");  // 先加载变量
        codes[codesIndex].operand = operand;
        codesIndex++;

        if (codesIndex >= MAX_CODES) {
            report_error(99, "中间代码表已满");
            return;
        }
        strcpy(codes[codesIndex].opt, "LOADI");  // 加载常量1
        codes[codesIndex].operand = 1;
        codesIndex++;

        if (codesIndex >= MAX_CODES) {
            report_error(99, "中间代码表已满");
            return;
        }
        strcpy(codes[codesIndex].opt, "ADD");  // 相加
        codes[codesIndex].operand = 0;
        codesIndex++;

        if (codesIndex >= MAX_CODES) {
            report_error(99, "中间代码表已满");
            return;
        }
        strcpy(codes[codesIndex].opt, "STO");  // 存回
        codes[codesIndex].operand = operand;
        // 这次不用codesIndex++，因为外层函数会加
    } else if (strcmp(opt, "DEC") == 0) {
        // x-- 相当于: LOAD x; LOADI 1; SUB; STO x
        strcpy(codes[codesIndex].opt, "LOAD");
        codes[codesIndex].operand = operand;
        codesIndex++;

        if (codesIndex >= MAX_CODES) {
            report_error(99, "中间代码表已满");
            return;
        }
        strcpy(codes[codesIndex].opt, "LOADI");
        codes[codesIndex].operand = 1;
        codesIndex++;

        if (codesIndex >= MAX_CODES) {
            report_error(99, "中间代码表已满");
            return;
        }
        strcpy(codes[codesIndex].opt, "SUB");
        codes[codesIndex].operand = 0;
        codesIndex++;

        if (codesIndex >= MAX_CODES) {
            report_error(99, "中间代码表已满");
            return;
        }
        strcpy(codes[codesIndex].opt, "STO");
        codes[codesIndex].operand = operand;
    } else if (strcmp(opt, "PRE_INC") == 0 || strcmp(opt, "PRE_DEC") == 0) {
        // 前置自增/自减与后置类似，但使用顺序不同
        if (strcmp(opt, "PRE_INC") == 0) {
            strcpy(codes[codesIndex].opt, "LOAD");
            codes[codesIndex].operand = operand;
            codesIndex++;

            if (codesIndex >= MAX_CODES) {
                report_error(99, "中间代码表已满");
                return;
            }
            strcpy(codes[codesIndex].opt, "LOADI");
            codes[codesIndex].operand = 1;
            codesIndex++;

            if (codesIndex >= MAX_CODES) {
                report_error(99, "中间代码表已满");
                return;
            }
            strcpy(codes[codesIndex].opt, "ADD");
            codes[codesIndex].operand = 0;
            codesIndex++;

            if (codesIndex >= MAX_CODES) {
                report_error(99, "中间代码表已满");
                return;
            }
            strcpy(codes[codesIndex].opt, "LOAD");  // 再加载一次用于表达式
            codes[codesIndex].operand = operand;
            codesIndex++;

            if (codesIndex >= MAX_CODES) {
                report_error(99, "中间代码表已满");
                return;
            }
            strcpy(codes[codesIndex].opt, "STO");
        } else {
            strcpy(codes[codesIndex].opt, "LOAD");
            codes[codesIndex].operand = operand;
            codesIndex++;

            if (codesIndex >= MAX_CODES) {
                report_error(99, "中间代码表已满");
                return;
            }
            strcpy(codes[codesIndex].opt, "LOADI");
            codes[codesIndex].operand = 1;
            codesIndex++;

            if (codesIndex >= MAX_CODES) {
                report_error(99, "中间代码表已满");
                return;
            }
            strcpy(codes[codesIndex].opt, "SUB");
            codes[codesIndex].operand = 0;
            codesIndex++;

            if (codesIndex >= MAX_CODES) {
                report_error(99, "中间代码表已满");
                return;
            }
            strcpy(codes[codesIndex].opt, "LOAD");
            codes[codesIndex].operand = operand;
            codesIndex++;

            if (codesIndex >= MAX_CODES) {
                report_error(99, "中间代码表已满");
                return;
            }
            strcpy(codes[codesIndex].opt, "STO");
        }
    } else if (strcmp(opt, "MUL") == 0) {
        strcpy(codes[codesIndex].opt, "MULT");
    } else if (strcmp(opt, "DIV") == 0) {
        strcpy(codes[codesIndex].opt, "DIV");
    } else if (strcmp(opt, "GT") == 0) {
        strcpy(codes[codesIndex].opt, "GT");
    } else if (strcmp(opt, "GE") == 0) {
        strcpy(codes[codesIndex].opt, "GE");
    } else if (strcmp(opt, "LT") == 0) {
        strcpy(codes[codesIndex].opt, "LES");
    } else if (strcmp(opt, "LE") == 0) {
        strcpy(codes[codesIndex].opt, "LE");
    } else if (strcmp(opt, "EQ") == 0) {
        strcpy(codes[codesIndex].opt, "EQ");
    } else if (strcmp(opt, "NE") == 0) {
        strcpy(codes[codesIndex].opt, "NOTEQ");
    } else if (strcmp(opt, "AND") == 0) {
        strcpy(codes[codesIndex].opt, "AND");
    } else if (strcmp(opt, "OR") == 0) {
        strcpy(codes[codesIndex].opt, "OR");
    } else if (strcmp(opt, "NOT") == 0) {
        strcpy(codes[codesIndex].opt, "NOT");
    } else {
        // 其他操作码保持不变
        strcpy(codes[codesIndex].opt, opt);
    }

    codes[codesIndex].operand = operand;

    printf("生成代码[%d]: %s %d\n", codesIndex, codes[codesIndex].opt, codes[codesIndex].operand);
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
        case TYPE_BOOL: return "bool";
        case TYPE_VOID: return "void";
        case TYPE_ARRAY: return "array";
        default: return "unknown";
    }
}

// 增强的类型兼容性检查
int check_type_compatible(enum DataType t1, enum DataType t2, const char *context) {
    if (t1 == TYPE_UNKNOWN || t2 == TYPE_UNKNOWN) {
        return 1;
    }

    if (t1 == t2) return 1;

    // 数组类型检查
    if (t1 == TYPE_ARRAY || t2 == TYPE_ARRAY) {
        report_error(50, "%s: 数组类型不能与其他类型混合运算", context);
        return 0;
    }

    // 字符串类型特殊处理
    if (t1 == TYPE_STRING || t2 == TYPE_STRING) {
        if (t1 != t2) {
            report_error(50, "%s: 字符串类型只能与字符串类型运算", context);
            return 0;
        }
        return 1;
    }

    // 布尔类型检查
    if (t1 == TYPE_BOOL || t2 == TYPE_BOOL) {
        if (t1 != t2) {
            report_error(50, "%s: 布尔类型只能与布尔类型运算", context);
            return 0;
        }
        return 1;
    }

    // 允许的隐式数值类型转换
    if ((t1 == TYPE_INT && (t2 == TYPE_FLOAT || t2 == TYPE_DOUBLE)) ||
        (t1 == TYPE_FLOAT && t2 == TYPE_DOUBLE) ||
        (t1 == TYPE_CHAR && (t2 == TYPE_INT || t2 == TYPE_FLOAT || t2 == TYPE_DOUBLE))) {
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
    if (lookup_current_scope(name, &pos) == 0 && symbol[pos].kind == variable) {
        return symbol[pos].initialized;
    }
    return 1;
}

void mark_variable_initialized(char *name) {
    int pos;
    if (lookup_current_scope(name, &pos) == 0 && symbol[pos].kind == variable) {
        symbol[pos].initialized = 1;
    }
}

enum DataType get_variable_type(char *name) {
    int pos;
    if (lookup_current_scope(name, &pos) == 0) {
        return symbol[pos].type;
    }
    return TYPE_UNKNOWN;
}

// 控制流检查：检查是否在循环内
void check_in_loop(const char *statement) {
    if (in_loop == 0) {
        report_error(61, "%s 语句不在循环内部", statement);
    }
}

// ===================== 符号表函数（增强） =====================

int lookup_current_scope(char *name, int *pPosition) {
    int i;
    for (i = symbolIndex - 1; i >= 0; i--) {
        if (symbol[i].scope_level <= current_scope_level &&
            strcmp(symbol[i].name, name) == 0) {
            *pPosition = i;
            return 0;
            }
    }

    // 只返回错误码，不自动报告错误
    // 让调用者决定是否报告错误
    return 23;  // 23表示未找到
}

// 全局查找符号
int lookup_global(char *name, int *pPosition) {
    int i;
    for (i = symbolIndex - 1; i >= 0; i--) {
        if (strcmp(symbol[i].name, name) == 0) {
            *pPosition = i;
            return 0;
        }
    }
    return 23;
}

// 插入符号到符号表
int insert_Symbol(enum Category_symbol category, char *name, enum DataType type,
                  int is_array, int array_dim, int *array_sizes, int is_param) {
    int i, es = 0;

    if (symbolIndex >= maxsymbolIndex) {
        report_error(21, "符号表已满");
        return 21;
    }

    // 检查当前作用域重复定义
    for (i = symbolIndex - 1; i >= 0; i--) {
        if (symbol[i].scope_level == current_scope_level &&
            strcmp(symbol[i].name, name) == 0) {
            if (symbol[i].kind == category) {
                if (category == function) {
                    report_error(32, "函数名 %s 重复定义", name);
                    es = 32;
                } else {
                    report_error(22, "变量名 %s 重复定义", name);
                    es = 22;
                }
            } else {
                report_error(22, "%s 名称冲突", name);
                es = 22;
            }
            break;
        }
    }

    if (es > 0) return es;

    // 插入新符号
    symbol[symbolIndex].kind = category;
    symbol[symbolIndex].type = type;
    symbol[symbolIndex].scope_level = current_scope_level;
    symbol[symbolIndex].line_declared = current_line;
    symbol[symbolIndex].is_param = is_param;

    if (category == function) {
        symbol[symbolIndex].initialized = 1;
    } else {
        symbol[symbolIndex].initialized = 0;
        if (!is_param) {
            symbol[symbolIndex].address = offset;
            offset++;
        }
    }

    // 处理数组信息
    if (is_array && array_dim > 0) {
        symbol[symbolIndex].type = TYPE_ARRAY;
        symbol[symbolIndex].array_info.dimensions = array_dim;
        for (i = 0; i < array_dim && i < 5; i++) {
            symbol[symbolIndex].array_info.size[i] = array_sizes[i];
        }
    }

    strncpy(symbol[symbolIndex].name, name, sizeof(symbol[symbolIndex].name) - 1);
    symbol[symbolIndex].name[sizeof(symbol[symbolIndex].name) - 1] = '\0';
    symbolIndex++;

    printf("插入符号: %s, 类型: %s, 作用域: %d\n",
           name, type_to_string(type), current_scope_level);
    return 0;
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
    scope_top = -1;
    current_scope_level = 0;
    in_loop = 0;

    // 进入全局作用域
    enter_scope("global");

    if (!read_next_token()) {
        printf("错误: 文件为空\n");
        fclose(fpTokenin);
        return 10;
    }

    es = program();
    fclose(fpTokenin);

    // 退出全局作用域
    exit_scope();

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

// 修改declaration_stat函数中的代码生成：

int declaration_stat() {
    int es = 0;
    ast_begin("VariableDeclaration");
    ast_begin("declarations");
    ast_begin("VariableDeclarator");

    if (!read_next_token()) return 10;
    if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
        report_error(3, "期望标识符，得到: %s", token1);
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
    } else if (strcmp(token, "bool") == 0 || strcmp(token1, "bool") == 0) {
        var_type = TYPE_BOOL;
        ast_add_attr("kind", "bool");
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

    es = insert_Symbol(variable, var_name, var_type, 0, 0, NULL, 0);
    if (es > 0) {
        ast_end();
        ast_end();
        ast_end();
        ast_end();
        return es;
    }

    // DECL指令改为分配存储空间
    if (!has_fatal_error) {
        // 在栈中分配空间
        strcpy(codes[codesIndex].opt, "ALLOC");
        codes[codesIndex].operand = 1;  // 分配1个单元
        codesIndex++;
    }

    ast_end();
    if (!read_next_token()) return 10;

    ast_end();
    ast_end();
    ast_end();
    return es;
}

// 修改main_declaration函数：

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

    // 进入函数作用域
    enter_scope("function");

    // 生成函数入口代码
    if (!has_fatal_error) {
        gen_code("ENTER", 0);  // 为函数调用开辟数据区
    }

    es = function_body();
    exit_scope();

    return es;
}

// 修改program函数中的代码生成：

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

    es = insert_Symbol(function, "main", TYPE_INT, 0, 0, NULL, 0);
    if (es > 0) {
        ast_end();
        ast_end();
        return es;
    }

    // 不需要生成ENTRY指令，因为你的指令集没有ENTRY

    if (!read_next_token()) return 10;
    es = main_declaration();
    if (es > 0) {
        ast_end();
        ast_end();
        return es;
    }

    // 生成停止指令
    if (!has_fatal_error) {
        gen_code("STOP", 0);
    }

    ast_end();
    ast_end();
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



int statement_list() {
    int es = 0;
    ast_begin("StatementList");

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

        // 检查break/continue语句
        if (strcmp(token, "break") == 0 || strcmp(token1, "break") == 0) {
            es = break_stat();
        } else if (strcmp(token, "continue") == 0 || strcmp(token1, "continue") == 0) {
            es = continue_stat();
        } else {
            es = statement();
        }

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

// break语句处理
int break_stat() {
    ast_begin("BreakStatement");

    // 控制流检查：确保在循环内
    check_in_loop("break");

    if (!has_fatal_error) {
        // 生成跳转代码（实际应该跳转到循环结束）
        // 简化处理：生成BREAK标记
        gen_code("BREAK", 0);
    }

    if (!read_next_token()) {
        ast_end();
        return 10;
    }

    if (strcmp(token, ";") == 0 || strcmp(token1, ";") == 0) {
        if (!read_next_token()) {
            ast_end();
            return 10;
        }
    }

    ast_end();
    return 0;
}

// continue语句处理
int continue_stat() {
    ast_begin("ContinueStatement");

    // 控制流检查：确保在循环内
    check_in_loop("continue");

    if (!has_fatal_error) {
        // 生成跳转代码（实际应该跳转到循环开始）
        // 简化处理：生成CONTINUE标记
        gen_code("CONTINUE", 0);
    }

    if (!read_next_token()) {
        ast_end();
        return 10;
    }

    if (strcmp(token, ";") == 0 || strcmp(token1, ";") == 0) {
        if (!read_next_token()) {
            ast_end();
            return 10;
        }
    }

    ast_end();
    return 0;
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
        // 进入块作用域
        enter_scope("block");
        es = compound_stat();
        exit_scope();
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
    int has_error = 0;  // 记录是否有错误

    printf("解析if语句，当前token: %s %s\n", token, token1);

    if (!read_next_token()) return 10;

    // 检查左括号
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        report_error(5, "期望(，得到: %s %s", token, token1);
        es = 5;
        has_error = 1;
        skip_to_sync_point();
        // 不立即返回
    } else {
        // 正常情况：有括号
        if (!read_next_token()) return 10;
    }

    // 尝试解析条件表达式
    int bool_es = bool_expr();
    if (bool_es > 0) {
        es = bool_es;
        has_error = 1;
        // 条件表达式解析出错，但仍然尝试继续
    }

    // 只有没有致命错误且条件表达式解析成功时才生成BRF指令
    if (!has_fatal_error && !has_error) {
        strcpy(codes[codesIndex].opt, "BRF");
        cx1 = codesIndex++;
    } else {
        cx1 = -1;  // 标记无效
    }

    // 检查右括号
    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        report_error(6, "期望)，得到: %s %s", token, token1);
        es = 6;
        has_error = 1;
        skip_to_sync_point();
        // 不立即返回
    } else {
        if (!read_next_token()) return 10;
    }

    // 解析if分支
    printf("解析if分支，当前token: %s %s\n", token, token1);
    int if_stmt_es = statement();
    if (if_stmt_es > 0) {
        es = if_stmt_es;
        has_error = 1;
    }

    // 只有没有错误时才生成BR指令
    if (!has_fatal_error && !has_error) {
        strcpy(codes[codesIndex].opt, "BR");
        cx2 = codesIndex++;
        if (cx1 != -1) {
            codes[cx1].operand = codesIndex;  // 指向else语句开始或if结束
        }
    } else {
        cx2 = -1;  // 标记无效
    }

    // 检查是否有else
    if (strcmp(token, "else") == 0 || strcmp(token1, "else") == 0) {
        ast_add_attr("has_else", "true");
        if (!read_next_token()) return 10;

        // 解析else分支
        printf("解析else分支，当前token: %s %s\n", token, token1);
        int else_stmt_es = statement();
        if (else_stmt_es > 0) {
            es = else_stmt_es;
            has_error = 1;
        }

        // 只有没有错误时才设置跳转地址
        if (!has_fatal_error && !has_error && cx2 != -1) {
            codes[cx2].operand = codesIndex;  // 指向if语句结束
        }
    } else {
        ast_add_attr("has_else", "false");

        // 只有没有错误时才设置条件为假时的跳转地址
        if (!has_fatal_error && !has_error && cx1 != -1) {
            codes[cx1].operand = codesIndex;  // 条件为假时直接跳到这里
        }
    }

    // 设置BR指令的跳转地址
    if (!has_fatal_error && !has_error && cx2 != -1) {
        codes[cx2].operand = codesIndex;  // BR指令跳转到这里
    }

    return es;
}
int while_stat() {
    int es = 0, cx1;

    printf("解析while语句，当前token: %s %s\n", token, token1);

    if (!read_next_token()) return 10;

    // 检查左括号
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        report_error(5, "期望(，得到: %s %s", token, token1);
        es = 5;
        // 不立即返回，尝试错误恢复
    } else {
        // 正常情况：有括号
        if (!read_next_token()) return 10;
    }

    int loop_start = codesIndex;
    enter_scope("loop");

    // 尝试解析条件表达式
    int bool_es = bool_expr();
    if (bool_es > 0) {
        // 条件表达式解析出错，但仍然尝试继续
        es = bool_es;
    }

    if (!has_fatal_error) {
        strcpy(codes[codesIndex].opt, "BRF");
        cx1 = codesIndex++;
    }

    // 检查是否有右括号
    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        // 缺少右括号，报告错误但继续
        report_error(6, "期望)，得到: %s %s", token, token1);
        es = 6;
        // 不立即返回，尝试继续解析循环体
    } else {
        // 有右括号，读取下一个token
        if (!read_next_token()) return 10;
    }

    // 解析循环体
    printf("准备解析while循环体，当前token: %s %s\n", token, token1);

    ast_begin("WhileBody");
    int stmt_es = statement();
    ast_end();

    exit_scope();

    if (stmt_es > 0) {
        es = stmt_es;
    }

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

    // 进入循环作用域
    enter_scope("loop");

    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        report_error(4, "期望;，得到: %s %s", token, token1);
        skip_to_sync_point();
    }

    if (!read_next_token()) return 10;
    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        ast_begin("Condition");
        es = bool_expr();
        ast_end();
        if (es > 0) {
            exit_scope();
            return es;
        }
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
        if (es > 0) {
            exit_scope();
            return es;
        }
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

    // 退出循环作用域
    exit_scope();

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

    if (lookup_global(token1, &symbolPos) != 0) {
        report_error(23, "函数 %s 未声明", token1);
        return 23;
    }
    if (symbol[symbolPos].kind != function) {
        report_error(35, "%s 不是函数名", token1);
        return 35;
    }

    // 函数调用一致性检查：检查参数个数和类型（简化）
    // 实际应该检查参数个数和类型是否匹配

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

    // 修改这里：先检查是否找到
    if (lookup_current_scope(token1, &pos) != 0) {
        report_error(23, "变量 %s 未声明", token1);
        // 继续执行，不立即返回
    } else {
        // 找到了，继续正常检查
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
    }

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

    // 同样的修改
    if (lookup_current_scope(token1, &pos) != 0) {
        report_error(23, "变量 %s 未声明", token1);
        // 继续执行
    } else {
        if (symbol[pos].kind != variable) {
            report_error(35, "%s 不是变量名", token1);
            return 35;
        }


        enum DataType var_type = get_variable_type(token1);
        if (var_type == TYPE_ARRAY) {
            report_error(63, "不能直接输出数组变量 %s", token1);
        }

        if (!has_fatal_error) {
            strcpy(codes[codesIndex].opt, "WRITE");
            codes[codesIndex].operand = pos;
            codesIndex++;
        }
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

// <expression>→ ID = <bool_expr> | <bool_expr>
int expression() {
    int es = 0;
    ast_begin("Expression");

    printf("进入expression，当前token: %s %s\n", token, token1);

    if (strcmp(token, "ID") == 0 || strcmp(token1, "ID") == 0) {
        char var_name[256];
        strncpy(var_name, token1, sizeof(var_name) - 1);
        var_name[sizeof(var_name) - 1] = '\0';

        int pos = -1;  // 初始化为-1，表示未找到
        int lookup_result = lookup_current_scope(var_name, &pos);

        if (lookup_result != 0) {
            // 变量未声明
            report_error(23, "变量 %s 未声明", var_name);
            // 不立即返回，继续解析以发现更多错误
        } else {
            // 找到了变量，检查是否是变量类型
            if (symbol[pos].kind != variable) {
                report_error(35, "%s 不是变量名", token1);
                es = 35;
            }
        }

        // 获取变量类型（如果找到了）
        enum DataType left_type = TYPE_UNKNOWN;
        if (lookup_result == 0) {
            left_type = symbol[pos].type;
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

            // 保存右值开始的token用于类型检查
            char saved_token[64], saved_token1[256];
            strcpy(saved_token, token);
            strcpy(saved_token1, token1);

            // 检查右值是否是立即数（NUM或STRING）
            int is_right_num = (strcmp(saved_token, "NUM") == 0 || strcmp(saved_token1, "NUM") == 0);
            int is_right_string = is_string_literal(saved_token1);
            int is_right_bool = (strcmp(saved_token, "true") == 0 || strcmp(saved_token1, "true") == 0 ||
                                strcmp(saved_token, "false") == 0 || strcmp(saved_token1, "false") == 0);

            ast_begin("RightValue");
            es = bool_expr();
            ast_end();

            // 详细的类型检查（只在变量已声明且是变量类型时）
            if (lookup_result == 0 && symbol[pos].kind == variable) {
                if (is_right_num) {
                    // 数值类型赋值检查
                    if (is_float_string(saved_token1)) {
                        if (left_type == TYPE_INT) {
                            report_error(51, "不能将浮点数 %s 赋值给整型变量 %s",
                                        saved_token1, var_name);
                        } else if (left_type == TYPE_BOOL) {
                            report_error(51, "不能将数值 %s 赋值给布尔变量 %s",
                                        saved_token1, var_name);
                        } else if (left_type == TYPE_STRING) {
                            report_error(51, "不能将数值 %s 赋值给字符串变量 %s",
                                        saved_token1, var_name);
                        }
                        // 浮点数赋值给浮点类型是允许的
                    } else {
                        // 整数常量赋值
                        if (left_type == TYPE_BOOL) {
                            report_error(51, "不能将整数 %s 赋值给布尔变量 %s",
                                        saved_token1, var_name);
                        } else if (left_type == TYPE_STRING) {
                            report_error(51, "不能将整数 %s 赋值给字符串变量 %s",
                                        saved_token1, var_name);
                        }
                    }
                } else if (is_right_string) {
                    // 字符串赋值检查
                    if (left_type != TYPE_STRING) {
                        report_error(51, "不能将字符串赋值给非字符串变量 %s", var_name);
                    }
                } else if (is_right_bool) {
                    // 布尔值赋值检查
                    if (left_type != TYPE_BOOL) {
                        report_error(51, "不能将布尔值赋值给非布尔变量 %s", var_name);
                    }
                }
                // 其他情况（如变量赋值、表达式结果赋值）会在bool_expr中进行类型检查

                // 检查数组类型
                if (left_type == TYPE_ARRAY) {
                    report_error(64, "不能直接给数组 %s 赋值", var_name);
                }


                // 标记变量已初始化
                mark_variable_initialized(var_name);
            }

            // 生成代码（只在没有致命错误且变量已声明时）
            if (!has_fatal_error && lookup_result == 0 && symbol[pos].kind == variable) {
                // STO指令：将栈顶值存储到变量
                strcpy(codes[codesIndex].opt, "STO");
                codes[codesIndex].operand = pos;
                codesIndex++;
            }

        } else if (strcmp(token, "++") == 0 || strcmp(token1, "++") == 0 ||
                   strcmp(token, "--") == 0 || strcmp(token1, "--") == 0) {
            // 后置自增/自减
            char op[4];
            strncpy(op, token, sizeof(op) - 1);
            op[sizeof(op) - 1] = '\0';

            ast_add_attr("operator", op);
            ast_add_attr("position", "postfix");

            // 变量已声明时的检查
            if (lookup_result == 0) {
                if (!check_variable_initialized(var_name)) {
                    report_warning("变量 %s 可能未初始化", var_name);
                }

                // 类型检查：自增自减只能用于数值类型
                if (left_type != TYPE_INT && left_type != TYPE_FLOAT && left_type != TYPE_DOUBLE) {
                    report_error(65, "自增/自减操作不能用于 %s 类型变量", type_to_string(left_type));
                }

                // 检查数组类型
                if (left_type == TYPE_ARRAY) {
                    report_error(64, "不能对数组 %s 进行自增/自减操作", var_name);
                }

                if (!has_fatal_error) {
                    // 生成自增/自减代码
                    if (strcmp(op, "++") == 0) {
                        // x++ 相当于: LOAD x; LOADI 1; ADD; STO x
                        strcpy(codes[codesIndex].opt, "LOAD");
                        codes[codesIndex].operand = pos;
                        codesIndex++;

                        if (codesIndex < MAX_CODES) {
                            strcpy(codes[codesIndex].opt, "LOADI");
                            codes[codesIndex].operand = 1;
                            codesIndex++;
                        }

                        if (codesIndex < MAX_CODES) {
                            strcpy(codes[codesIndex].opt, "ADD");
                            codes[codesIndex].operand = 0;
                            codesIndex++;
                        }

                        if (codesIndex < MAX_CODES) {
                            strcpy(codes[codesIndex].opt, "STO");
                            codes[codesIndex].operand = pos;
                            codesIndex++;
                        }
                    } else {
                        // x-- 相当于: LOAD x; LOADI 1; SUB; STO x
                        strcpy(codes[codesIndex].opt, "LOAD");
                        codes[codesIndex].operand = pos;
                        codesIndex++;

                        if (codesIndex < MAX_CODES) {
                            strcpy(codes[codesIndex].opt, "LOADI");
                            codes[codesIndex].operand = 1;
                            codesIndex++;
                        }

                        if (codesIndex < MAX_CODES) {
                            strcpy(codes[codesIndex].opt, "SUB");
                            codes[codesIndex].operand = 0;
                            codesIndex++;
                        }

                        if (codesIndex < MAX_CODES) {
                            strcpy(codes[codesIndex].opt, "STO");
                            codes[codesIndex].operand = pos;
                            codesIndex++;
                        }
                    }
                }

                mark_variable_initialized(var_name);
            }

            if (!read_next_token()) {
                ast_end();
                return 10;
            }
            ast_end();
            return 0;
        } else {
            // 读取变量值（不是赋值也不是自增自减）
            strcpy(token, current_token);
            strcpy(token1, current_token1);

            // 变量已声明时的检查
            if (lookup_result == 0) {
                if (!check_variable_initialized(var_name)) {
                    report_warning("变量 %s 可能未初始化", var_name);
                }

                // 检查数组类型
                if (left_type == TYPE_ARRAY) {
                    report_error(64, "数组 %s 需要下标访问", var_name);
                }

                if (!has_fatal_error) {
                    // LOAD指令：将变量值压入栈
                    strcpy(codes[codesIndex].opt, "LOAD");
                    codes[codesIndex].operand = pos;
                    codesIndex++;
                }
            } else {
                // 变量未声明，但仍然生成LOAD指令（地址为0）
                if (!has_fatal_error) {
                    strcpy(codes[codesIndex].opt, "LOAD");
                    codes[codesIndex].operand = 0;  // 无效地址
                    codesIndex++;
                }
            }

            // 继续解析可能的表达式（如 x + 1）
            es = bool_expr();
        }
    } else if (strcmp(token, "++") == 0 || strcmp(token1, "++") == 0 ||
               strcmp(token, "--") == 0 || strcmp(token1, "--") == 0) {
        // 前置自增/自减
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

        char var_name[256];
        strncpy(var_name, token1, sizeof(var_name) - 1);
        var_name[sizeof(var_name) - 1] = '\0';

        int pos = -1;
        int lookup_result = lookup_current_scope(token1, &pos);

        if (lookup_result != 0) {
            report_error(23, "变量 %s 未声明", token1);
            // 继续执行
        } else {
            // 类型检查
            enum DataType var_type = get_variable_type(token1);
            if (var_type != TYPE_INT && var_type != TYPE_FLOAT && var_type != TYPE_DOUBLE) {
                report_error(65, "自增/自减操作不能用于 %s 类型变量", type_to_string(var_type));
            }

            // 检查数组类型
            if (var_type == TYPE_ARRAY) {
                report_error(64, "不能对数组 %s 进行自增/自减操作", token1);
            }

            if (!check_variable_initialized(token1)) {
                report_warning("变量 %s 可能未初始化", token1);
            }

            if (!has_fatal_error) {
                // ++x 相当于: LOAD x; LOADI 1; ADD; STO x; LOAD x
                if (op[0] == '+') {
                    // 计算新值
                    strcpy(codes[codesIndex].opt, "LOAD");
                    codes[codesIndex].operand = pos;
                    codesIndex++;

                    if (codesIndex < MAX_CODES) {
                        strcpy(codes[codesIndex].opt, "LOADI");
                        codes[codesIndex].operand = 1;
                        codesIndex++;
                    }

                    if (codesIndex < MAX_CODES) {
                        strcpy(codes[codesIndex].opt, "ADD");
                        codes[codesIndex].operand = 0;
                        codesIndex++;
                    }

                    if (codesIndex < MAX_CODES) {
                        strcpy(codes[codesIndex].opt, "STO");
                        codes[codesIndex].operand = pos;
                        codesIndex++;
                    }

                    if (codesIndex < MAX_CODES) {
                        strcpy(codes[codesIndex].opt, "LOAD");
                        codes[codesIndex].operand = pos;
                        codesIndex++;
                    }
                } else {
                    // --x 相当于: LOAD x; LOADI 1; SUB; STO x; LOAD x
                    strcpy(codes[codesIndex].opt, "LOAD");
                    codes[codesIndex].operand = pos;
                    codesIndex++;

                    if (codesIndex < MAX_CODES) {
                        strcpy(codes[codesIndex].opt, "LOADI");
                        codes[codesIndex].operand = 1;
                        codesIndex++;
                    }

                    if (codesIndex < MAX_CODES) {
                        strcpy(codes[codesIndex].opt, "SUB");
                        codes[codesIndex].operand = 0;
                        codesIndex++;
                    }

                    if (codesIndex < MAX_CODES) {
                        strcpy(codes[codesIndex].opt, "STO");
                        codes[codesIndex].operand = pos;
                        codesIndex++;
                    }

                    if (codesIndex < MAX_CODES) {
                        strcpy(codes[codesIndex].opt, "LOAD");
                        codes[codesIndex].operand = pos;
                        codesIndex++;
                    }
                }
            }

            mark_variable_initialized(token1);
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
        // 普通表达式（不以标识符或自增自减开头）
        es = bool_expr();
    }

    ast_end();
    return es;
}

// 在bool_expr()中恢复类型检查
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

        // 保存右边的token用于类型检查
        char saved_token[64], saved_token1[256];
        strcpy(saved_token, token);
        strcpy(saved_token1, token1);

        es = additive_expr();
        if (es > 0) {
            ast_end();
            return es;
        }


        // 检查数值类型兼容性
        if (!check_type_compatible(TYPE_INT, TYPE_INT, "比较运算")) {
            // 错误已报告
        }

        if (!has_fatal_error) {
            if (strcmp(op, ">") == 0) gen_code("GT", 0);
            else if (strcmp(op, ">=") == 0) gen_code("GE", 0);
            else if (strcmp(op, "<") == 0) gen_code("LT", 0);
            else if (strcmp(op, "<=") == 0) gen_code("LE", 0);
            else if (strcmp(op, "==") == 0) gen_code("EQ", 0);
            else if (strcmp(op, "!=") == 0) gen_code("NE", 0);
        }

        ast_end();
    }

    return es;
}

// 在arithmetic_expr中恢复类型检查
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

        // 检查运算数类型
        char saved_token[64], saved_token1[256];
        strcpy(saved_token, token);
        strcpy(saved_token1, token1);

        es = term();
        if (es > 0) {
            ast_end();
            return es;
        }

        // 算术运算的类型检查
        if (is_string_literal(saved_token1)) {
            if (op[0] == '+') {
                report_warning("字符串连接操作: %s", saved_token1);
                // 字符串连接特殊处理
            } else {
                report_error(50, "算术运算: 字符串不能参与减、乘、除运算");
            }
        } else if (strcmp(saved_token, "true") == 0 || strcmp(saved_token1, "true") == 0 ||
                   strcmp(saved_token, "false") == 0 || strcmp(saved_token1, "false") == 0) {
            report_error(50, "算术运算: 布尔值不能参与数值运算");
        }

        if (!check_type_compatible(TYPE_INT, TYPE_INT, "算术运算")) {
            // 错误已报告
        }

        if (!has_fatal_error) {
            if (op[0] == '+') {
                gen_code("ADD", 0);
            } else {
                gen_code("SUB", 0);
            }
        }

        ast_end();
    }

    return es;
}

// 同样恢复term()中的类型检查
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

        char saved_token[64], saved_token1[256];
        strcpy(saved_token, token);
        strcpy(saved_token1, token1);

        es = factor();
        if (es > 0) {
            ast_end();
            return es;
        }

        // 乘除运算的类型检查
        if (is_string_literal(saved_token1)) {
            report_error(50, "算术运算: 字符串不能参与乘、除运算");
        } else if (strcmp(saved_token, "true") == 0 || strcmp(saved_token1, "true") == 0 ||
                   strcmp(saved_token, "false") == 0 || strcmp(saved_token1, "false") == 0) {
            report_error(50, "算术运算: 布尔值不能参与数值运算");
        }

        if (!check_type_compatible(TYPE_INT, TYPE_INT, "算术运算")) {
            // 错误已报告
        }

        if (!has_fatal_error) {
            if (op[0] == '*') {
                gen_code("MUL", 0);
            } else {
                gen_code("DIV", 0);
            }
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
        if (lookup_current_scope(token1, &pos) == 0) {
            if (!check_variable_initialized(token1)) {
                report_warning("变量 %s 可能未初始化", token1);
            }

            // 检查数组类型
            if (symbol[pos].type == TYPE_ARRAY) {
                report_error(64, "数组 %s 需要下标访问", token1);
            }

            if (!has_fatal_error) {
                strcpy(codes[codesIndex].opt, "LOAD");
                codes[codesIndex].operand = pos;
                codesIndex++;
            }
        } else {
            // 变量未声明
            report_error(23, "变量 %s 未声明", token1);
            return 23;
        }

        if (!read_next_token()) return 10;
    } else if (strcmp(token, "NUM") == 0 || strcmp(token1, "NUM") == 0) {
        ast_begin("BasicLit");
        ast_add_attr("value", token1);
        ast_end();

        if (is_float_string(token1)) {
            report_warning("浮点数常量 %s 可能损失精度", token1);
        }

        if (!has_fatal_error) {
            // 生成LOADI指令加载常量
            strcpy(codes[codesIndex].opt, "LOADI");

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
    } else if (strcmp(token, "STRING") == 0 || strcmp(token1, "STRING") == 0 ||
               is_string_literal(token1)) {
        ast_begin("StringLiteral");
        ast_add_attr("value", token1);
        ast_end();

        // 字符串常量不支持数值运算
        report_error(50, "字符串常量 %s 不能参与数值运算", token1);

        if (!read_next_token()) return 10;
    } else if (strcmp(token, "true") == 0 || strcmp(token1, "true") == 0 ||
               strcmp(token, "false") == 0 || strcmp(token1, "false") == 0) {
        ast_begin("BoolLiteral");
        ast_add_attr("value", token1);
        ast_end();

        if (!has_fatal_error) {
            strcpy(codes[codesIndex].opt, "LOADI");
            codes[codesIndex].operand = (strcmp(token1, "true") == 0) ? 1 : 0;
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