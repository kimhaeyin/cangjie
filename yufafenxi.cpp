// TESTparse_full_parser_fixed.c
// 完整版语法分析器（修正版：修复strcmp判断、部分逻辑与健壮性）

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define maxsymbolIndex 100//定义符号表的容量

enum Category_symbol { variable, function }; //标志符的类型（函数或变量）

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


// 抽象语法树（AST）文本生成的全局变量
char *astText = NULL; // 指向存储AST文本的字符串缓冲区
size_t astCap = 0; // 当前缓冲区的总容量（字节数）
int indentLevel = 0; // 当前缩进级别（用于格式化输出）

//初始化AST文本缓冲区，分配初始内存并设置初始状态
void ast_init() {
    astCap = 1 << 16; // 分配64KB初始容量 (1<<16 = 65536字节)
    astText = (char *) malloc(astCap); // 动态分配内存
    if (!astText) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    astText[0] = '\0'; // 初始化为空字符串
    indentLevel = 0; // 初始化缩进级别为0（无缩进）
}


//向AST文本中添加当前缩进级别的空格，根据indentLevel添加相应数量的缩进（每级2个空格）
void ast_add_indent() {
    for (int i = 0; i < indentLevel; i++) {
        strcat(astText, "  "); // 每级缩进添加2个空格
    }
}

//向AST文本缓冲区追加格式化内容
void ast_append(const char *fmt, ...) {
    va_list ap; // 可变参数列表指针
    va_start(ap, fmt); // 初始化可变参数列表

    size_t cur = strlen(astText); // 当前已使用的缓冲区长度
    char tmp[4096]; // 临时缓冲区，用于格式化输出

    // 将格式化内容写入临时缓冲区
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap); // 清理可变参数列表

    size_t need = strlen(tmp); // 计算需要追加的字符串长度

    // 检查缓冲区容量是否足够，不够则重新分配
    if (cur + need + 16 > astCap) {
        astCap = (cur + need + 16) * 2; // 新容量为当前需要的2倍，加16字节预留
        astText = (char *) realloc(astText, astCap); // 重新分配更大内存
        if (!astText) {
            fprintf(stderr, "内存重分配失败\n");
            exit(1);
        }
    }
    strcat(astText, tmp); // 将格式化后的内容追加到主缓冲区
}


//开始一个新的AST节点，添加带缩进的节点名称并增加缩进级别
void ast_begin(const char *name) {
    ast_add_indent(); // 添加当前缩进
    ast_append("%s:\n", name); // 写入节点名称和冒号，然后换行
    indentLevel++; // 增加缩进级别（子节点会更多缩进）
}

//结束当前AST节点，减少缩进级别，回到上一级缩进
void ast_end() {
    if (indentLevel > 0) {
        indentLevel--; // 减少缩进级别（回到父节点级别）
    }
}

/*向当前AST节点添加属性
  属性会以缩进形式显示在当前节点下方
  attr 属性名（如"name"、"type"、"value"等）
  value 属性值（如变量名、数值、类型等）*/
void ast_add_attr(const char *attr, const char *value) {
    ast_add_indent(); // 添加当前缩进
    ast_append("  %s: %s\n", attr, value); // 属性前多缩进2个空格，格式为"属性名: 属性值"
}

// token读取函数
/*
 从单词流文件中读取下一个token
 单词流文件格式：token类型 + 多个空格 + token值
 例如："main     main" 或 "ID       fact"
 读取成功返回1，文件结束返回0
*/
int read_next_token() {
    char line[512]; // 临时缓冲区，用于存储从文件读取的一行内容

    // 从单词流文件中读取一行
    if (fgets(line, sizeof(line), fpTokenin) == NULL) {
        // 文件结束或读取失败时的处理
        token[0] = '\0'; // 清空token类型缓冲区
        token1[0] = '\0'; // 清空token值缓冲区
        return 0; // 返回0表示文件结束
    }

    // 移除行末的换行符
    // strcspn(line, "\n") 返回第一个换行符在line中的位置
    // 将该位置字符设为字符串结束符'\0'
    line[strcspn(line, "\n")] = '\0';

    // 解析格式：token类型 + 多个空格 + token值
    // 例如："main     main" 或 "ID       fact"

    // 第一步：找到第一个连续空格的位置（token类型和值的分界点）
    char *token_end = line; // 从行首开始扫描
    // 循环直到遇到空格、制表符或行尾
    while (*token_end != ' ' && *token_end != '\t' && *token_end != '\0') {
        token_end++; // 逐个字符前进
    }

    // 第二步：提取token类型（空格前的部分）
    int type_len = token_end - line; // 计算token类型的长度（指针相减）
    if (type_len > 63) type_len = 63; // 确保不超过token缓冲区的容量(64-1)
    strncpy(token, line, type_len); // 将token类型复制到全局变量token中
    token[type_len] = '\0'; // 添加字符串结束符

    // 第三步：跳过连续的空格和制表符，找到token值的起始位置
    char *value_start = token_end; // 从类型结束位置开始
    // 跳过所有连续的空格和制表符
    while (*value_start == ' ' || *value_start == '\t') {
        value_start++;
    }

    // 第四步：提取token值（空格后的部分）
    if (*value_start != '\0') {
        // 如果还有内容（不是空行），复制token值
        strncpy(token1, value_start, 255); // 复制到全局变量token1，最大255字符
        token1[255] = '\0'; // 确保字符串正确终止
    } else {
        // 如果空格后没有内容（如"main     "），清空token值
        token1[0] = '\0';
    }

    // 输出调试信息，显示成功读取的token信息
    printf("读取token成功: type='%s' value='%s'\n", token, token1);

    return 1; // 返回1表示成功读取一个token
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

    char astfile[512]; // 缓冲区，用于存储生成的语法树文件名
    snprintf(astfile, sizeof(astfile), "%s.ast.txt", tokenfile); //生成语法树输出文件名
    FILE *fasta = fopen(astfile, "w");
    if (fasta) {
        fputs(astText, fasta); //将内存中的语法树文本写入文件
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
    int es = 0; // 错误状态码，0表示无错误

    // 开始构建Program节点的AST
    ast_begin("Program");

    // 检查第一个token是否为main（token类型或值都可能是"main"）
    // 在单词流中，"main"可能作为关键字类型出现，也可能作为标识符值出现
    if (strcmp(token, "main") != 0 && strcmp(token1, "main") != 0) {
        printf("期望main，得到: %s %s\n", token, token1);
        es = 13; // 错误码13：缺少main函数
        return es;
    }

    // 开始main_declaration子节点的AST构建
    ast_begin("main_declaration");

    // 将"main"函数名插入符号表，类别为function
    // 符号表用于记录程序中所有的标识符（变量、函数等）
    insert_Symbol(function, "main");

    // 读取下一个token，期望是"("（函数参数列表的开始）
    if (!read_next_token()) return 10;

    // 解析main函数的声明部分（参数列表和函数体）
    // main_declaration函数处理：main '(' ')' <function_body>
    es = main_declaration();
    if (es > 0) {
        // 如果main_declaration解析失败，需要正确结束AST节点
        ast_end(); // 结束main_declaration节点
        ast_end(); // 结束Program节点
        return (es); // 返回错误代码
    }

    // 成功完成解析，正确结束AST节点
    ast_end(); // 结束main_declaration节点
    ast_end(); // 结束Program节点（整个语法树的根节点）

    return (es); // 返回最终的错误状态
}

//<main_declaration>→ main’(‘ ‘ )’ < function_body>
int main_declaration() {
    int es = 0; // 错误状态码

    // 在AST中添加main函数的ID属性
    ast_add_attr("ID", "main");

    // 检查当前token是否为"("（函数参数列表的开始）
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        printf("期望(，得到: %s %s\n", token, token1);
        es = 5; // 错误码5：缺少左括号
        return es;
    }

    // 读取下一个token，期望是")"（空参数列表）
    if (!read_next_token()) return 10; // 错误码10：文件读取失败

    // 检查当前token是否为")"（函数参数列表的结束）
    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        es = 6; // 错误码6：缺少右括号
        return es;
    }

    // 读取下一个token，期望是"{"（函数体的开始）
    if (!read_next_token()) return 10;

    // 解析函数体部分（变量声明和语句列表）
    es = function_body();
    if (es > 0) return es;

    return es;
}

//<function_body> → '{' <declaration_list> <statement_list> '}'
int function_body() {
    int es = 0;

    // 开始构建Function_Body节点的AST
    ast_begin("Function_Body");

    // 检查当前token是否为"{"（函数体的开始）
    if (strcmp(token, "{") != 0 && strcmp(token1, "{") != 0) {
        printf("期望{，得到: %s %s\n", token, token1);
        es = 11; // 错误码11：缺少左花括号
        return (es);
    }

    // 进入新函数，初始化局部变量的相对地址计数器
    // 从2开始：地址0可能用于返回地址，地址1可能用于旧的栈帧指针
    offset = 2;

    // 读取下一个token，进入函数体内部
    if (!read_next_token()) return 10;

    // 解析变量声明列表（所有以"var"开头的变量声明）
    es = declaration_list();
    if (es > 0) return (es);

    // 解析语句列表（函数体内的所有可执行语句）
    es = statement_list();
    if (es > 0) return (es);

    // 检查当前token是否为"}"（函数体的结束）
    if (strcmp(token, "}") != 0 && strcmp(token1, "}") != 0) {
        printf("期望}，得到: %s %s\n", token, token1);
        es = 12; // 错误码12：缺少右花括号
        return (es);
    }

    // 结束Function_Body节点的AST构建
    ast_end();

    return 0;
}

//<declaration_list> → { <declaration_stat> }
int declaration_list() {
    int es = 0;

    // 开始构建DeclarationList节点的AST
    ast_begin("DeclarationList");

    /*
     循环处理所有以"var"开头的变量声明
     条件：当前token的类型或值为"var"
     这允许"var"既可以是关键字类型，也可以是标识符值
    */
    while (strcmp(token, "var") == 0 || strcmp(token1, "var") == 0) {
        // 解析单个变量声明语句
        es = declaration_stat();
        if (es > 0) return (es);

        // 循环继续条件：declaration_stat()处理后，如果下一个token还是"var"则继续
    }

    // 结束DeclarationList节点的AST构建
    ast_end();
    return (es);
}


//<declaration_stat> → var ID : 类型
int declaration_stat() {
    int es = 0;

    // 开始构建VariableDeclaration节点的AST
    ast_begin("VariableDeclaration");

    // 开始declarations子节点（可能用于支持多个变量声明）
    ast_begin("declarations");
    // 开始VariableDeclarator子节点（单个变量声明器）
    ast_begin("VariableDeclarator");

    /*
     读取下一个token，期望是变量名（ID）
     当前token是"var"，所以需要读取下一个来获取变量名
    */
    if (!read_next_token()) return 10;

    // 检查当前token是否为标识符（变量名）
    if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
        printf("期望ID，得到: %s %s\n", token, token1);
        return (es = 3); // 错误码3：缺少标识符
    }

    // 开始构建变量标识符的AST节点
    ast_begin("id");
    ast_add_attr("type", "Identifier"); // 标识符类型
    ast_add_attr("name", token1); // 变量名称

    // 将变量名插入符号表，类别为variable
    es = insert_Symbol(variable, token1);
    if (es > 0) return (es);

    // 读取下一个token，期望是":"（类型声明的分隔符）
    if (!read_next_token()) return 10;
    if (strcmp(token, ":") != 0 && strcmp(token1, ":") != 0) {
        printf("期望:，得到: %s %s\n", token, token1);
        return (es = 4); // 错误码4：缺少冒号
    }

    // 读取下一个token，期望是类型关键字
    if (!read_next_token()) return 10;

    // 检查并记录变量类型
    if (strcmp(token, "int") == 0 || strcmp(token1, "int") == 0) {
        ast_add_attr("kind", "int"); // 整型
    } else if (strcmp(token, "double") == 0 || strcmp(token1, "double") == 0) {
        ast_add_attr("kind", "double"); // 双精度浮点型
    } else if (strcmp(token, "float") == 0 || strcmp(token1, "float") == 0) {
        ast_add_attr("kind", "float"); // 单精度浮点型
    } else if (strcmp(token, "char") == 0 || strcmp(token1, "char") == 0) {
        ast_add_attr("kind", "char"); // 字符型
    } else if (strcmp(token, "ID") == 0 && strcmp(token1, "ID") == 0) {
        ast_add_attr("kind", token1); // 自定义类型（标识符）
    } else {
        printf("期望类型，得到: %s %s\n", token, token1);
        return (es = 8); // 错误码8：缺少参数类型
    }

    // 结束id节点的AST构建
    ast_end(); // id

    /*
     仓颉语言特性：变量声明不需要分号
     直接读取下一个token以继续解析后续内容
    */
    if (!read_next_token()) return 10;

    // 结束各个AST节点的构建
    ast_end(); // VariableDeclarator
    ast_end(); // declarations
    ast_end(); // VariableDeclaration

    return (es);
}

//<statement_list>→{<statement>}
int statement_list() {
    int es = 0;

    ast_begin("StatementList");

    /*
     循环处理所有语句，直到遇到函数结束符'}'或文件结束
     循环条件说明：
     当前token不是'}'（函数体结束标记）
     token不为空（避免空token无限循环）
     文件未结束（避免在文件结束时继续读取）
     */
    while ((strcmp(token, "}") != 0 && strcmp(token1, "}") != 0) && (token[0] != '\0' && !feof(fpTokenin))) {
        printf("解析语句，当前token: %s %s\n", token, token1);
        es = statement();
        if (es > 0) return es;

        // 安全检查：如果statement处理后文件结束，退出循环
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

    /*
     根据当前token类型分发到不同的语句处理函数
     支持多种语句类型：条件、循环、复合、表达式、函数调用等
     */
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
        // 赋值语句或表达式语句（以标识符开头）
        ast_begin("AssignmentOrExpression");
        es = expression();
        ast_end();

        // 仓颉语言特性：语句可以没有分号
        // 但如果存在分号，就跳过它
        if (es == 0 && (strcmp(token, ";") == 0 || strcmp(token1, ";") == 0)) {
            if (!read_next_token()) return 10;
        }
    } else if (strcmp(token, "NUM") == 0 || strcmp(token, "(") == 0 ||
               strcmp(token1, "NUM") == 0 || strcmp(token1, "(") == 0) {
        // 表达式语句（以数字或左括号开头）
        es = expression_stat();
    } else if (strcmp(token, ";") == 0 || strcmp(token1, ";") == 0) {
        // 空语句（只有一个分号）
        ast_begin("EmptyStatement");
        ast_add_attr("type", "empty");
        if (!read_next_token()) return 10;
        ast_end();
    } else if (strcmp(token, "var") == 0 || strcmp(token1, "var") == 0) {
        // 变量声明语句（在语句列表中允许变量声明）
        es = declaration_stat();
    } else {
        // 未知语句类型错误
        printf("错误: 未知语句类型: %s %s\n", token, token1);
        es = 9; // 错误码9：未知语句类型
    }

    return es;
}

//<if_stat>→ if '('<expr>')' <statement > [else < statement >]
int if_stat() {
    int es = 0;

    // 当前token是"if"，读取下一个token期望是"("
    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        printf("期望(，得到: %s %s\n", token, token1);
        return 5; // 错误码5：缺少左括号
    }

    // 读取条件表达式
    if (!read_next_token()) return 10;
    ast_begin("Condition");
    es = bool_expr(); // 解析布尔表达式作为if条件
    ast_end();
    if (es > 0) return es;

    // 检查条件表达式后的右括号
    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        return 6; // 错误码6：缺少右括号
    }

    // 读取then分支语句
    if (!read_next_token()) return 10;

    // 检查then分支的开始（期望左花括号）
    if (strcmp(token, "{") != 0 && strcmp(token1, "{") != 0) {
        printf("期望{，得到: %s %s\n", token, token1);
        return 1; // 错误码1：缺少左花括号
    }

    // 解析then分支语句
    ast_begin("ThenBranch");
    es = statement();
    ast_end();
    if (es > 0) return es;

    // 检查是否有else分支（可选）
    if (strcmp(token, "else") == 0 || strcmp(token1, "else") == 0) {
        ast_add_attr("has_else", "true"); // 在AST中标记存在else分支
        if (!read_next_token()) return 10;
        ast_begin("ElseBranch");
        es = statement(); // 解析else分支语句
        ast_end();
    } else {
        ast_add_attr("has_else", "false"); // 在AST中标记没有else分支
    }

    return es;
}

//<while_stat>→ while '('<expr >')' < statement >
int while_stat() {
    int es = 0;

    // 当前token是"while"，读取下一个token期望是"("
    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        printf("期望(，得到: %s %s\n", token, token1);
        return 5; // 错误码5：缺少左括号
    }

    // 读取循环条件表达式
    if (!read_next_token()) return 10;
    ast_begin("Condition");
    es = bool_expr(); // 解析布尔表达式作为循环条件
    ast_end();
    if (es > 0) return es;

    // 检查条件表达式后的右括号
    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        return 6; // 错误码6：缺少右括号
    }

    // 读取循环体语句
    if (!read_next_token()) return 10;

    // 检查循环体的开始（期望左花括号）
    if (strcmp(token, "{") != 0 && strcmp(token1, "{") != 0) {
        printf("期望{，得到: %s %s\n", token, token1);
        return 1; // 错误码1：缺少左花括号
    }

    // 解析循环体语句
    ast_begin("LoopBody");
    es = statement();
    ast_end();

    return es;
}

//<for_stat>→ for'('<expr>;<expr>;<expr>')'<statement>
int for_stat() {
    int es = 0;

    // 当前token是"for"，读取下一个token期望是"("
    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        printf("期望(，得到: %s %s\n", token, token1);
        return 5; // 错误码5：缺少左括号
    }

    // 解析初始化表达式（可选）
    if (!read_next_token()) return 10;
    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        ast_begin("Initialization");
        es = expression(); // 解析for循环的初始化表达式
        ast_end();
        if (es > 0) return es;
    }

    // 检查初始化表达式后的分号
    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        printf("期望;，得到: %s %s\n", token, token1);
        return 4; // 错误码4：缺少分号
    }

    // 解析循环条件表达式（可选）
    if (!read_next_token()) return 10;
    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        ast_begin("Condition");
        es = bool_expr(); // 解析for循环的继续条件
        ast_end();
        if (es > 0) return es;
    }

    // 检查条件表达式后的分号
    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        printf("期望;，得到: %s %s\n", token, token1);
        return 4; // 错误码4：缺少分号
    }

    // 解析增量表达式（可选）
    if (!read_next_token()) return 10;
    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        ast_begin("Increment");
        es = expression(); // 解析for循环的增量表达式
        ast_end();
        if (es > 0) return es;
    }

    // 检查for循环头的结束括号
    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        return 6; // 错误码6：缺少右括号
    }

    // 读取循环体语句
    if (!read_next_token()) return 10;
    ast_begin("LoopBody");
    es = statement(); // 解析for循环体
    ast_end();

    return es;
}

//<compound_stat>→'{'<statement_list>'}'
int compound_stat() {
    int es = 0;

    // 在AST中标记复合语句的开始
    ast_add_attr("start", "{");

    // 当前token是"{"，读取下一个token进入语句列表
    if (!read_next_token()) return 10;

    // 解析复合语句内部的语句列表
    es = statement_list();

    if (es > 0) return es;

    // 检查复合语句的结束符"}"
    if (strcmp(token, "}") != 0 && strcmp(token1, "}") != 0) {
        printf("期望}，得到: %s %s\n", token, token1);
        return 12; // 错误码12：缺少右花括号
    }

    // 在AST中标记复合语句的结束
    ast_add_attr("end", "}");

    // 读取下一个token，继续解析后续内容
    if (!read_next_token()) return 10;

    return es;
}

//< call _stat>→call ID( )
int call_stat() {
    int es = 0;
    int symbolPos;

    // 当前token是"call"，读取下一个token期望是函数名
    if (!read_next_token()) return 10;
    if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
        printf("期望ID，得到: %s %s\n", token, token1);
        return 3; // 错误码3：缺少标识符
    }

    // 在AST中记录被调用的函数名
    ast_add_attr("function_name", token1);

    // 在符号表中查找函数名，检查是否已声明
    if (lookup(token1, &symbolPos) != 0) {
        printf("函数%s未声明\n", token1);
        return 34; // 错误码34：函数未声明
    }
    if (symbol[symbolPos].kind != function) {
        printf("%s不是函数名\n", token1);
        return 34; // 错误码34：标识符不是函数
    }

    // 读取函数调用的左括号
    if (!read_next_token()) return 10;
    if (strcmp(token, "(") != 0 && strcmp(token1, "(") != 0) {
        printf("期望(，得到: %s %s\n", token, token1);
        return 5; // 错误码5：缺少左括号
    }

    // 读取函数调用的右括号（当前实现不支持参数）
    if (!read_next_token()) return 10;
    if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        return 6; // 错误码6：缺少右括号
    }

    // 检查函数调用语句结束的分号
    if (!read_next_token()) return 10;
    if (strcmp(token, ";") != 0 && strcmp(token1, ";") != 0) {
        printf("期望;，得到: %s %s\n", token, token1);
        return 4; // 错误码4：缺少分号
    }

    // 读取下一个token，继续解析后续语句
    if (!read_next_token()) return 10;

    return es;
}

//<read_stat>→read ID;
int read_stat() {
    int es = 0;
    int pos;

    // 当前token是"read"，读取下一个token期望是变量名
    if (!read_next_token()) return 10;
    if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
        printf("期望ID，得到: %s %s\n", token, token1);
        return 3; // 错误码3：缺少标识符
    }

    // 在AST中记录要读取的变量名
    ast_add_attr("variable_name", token1);

    // 在符号表中查找变量名，检查是否已声明且是变量类型
    if (lookup(token1, &pos) != 0) {
        printf("变量%s未声明\n", token1);
        return 23; // 错误码23：变量未声明
    }
    if (symbol[pos].kind != variable) {
        printf("%s不是变量名\n", token1);
        return 35; // 错误码35：标识符不是变量
    }

    // 读取下一个token，继续解析后续内容
    if (!read_next_token()) return 10;

    return es;
}

int write_stat() {
    int es = 0;
    int pos;

    // 当前token是"read"，读取下一个token期望是变量名
    if (!read_next_token()) return 10;
    if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
        printf("期望ID，得到: %s %s\n", token, token1);
        return 3; // 错误码3：缺少标识符
    }

    // 在AST中记录要读取的变量名
    ast_add_attr("variable_name", token1);

    // 在符号表中查找变量名，检查是否已声明且是变量类型
    if (lookup(token1, &pos) != 0) {
        printf("变量%s未声明\n", token1);
        return 23; // 错误码23：变量未声明
    }
    if (symbol[pos].kind != variable) {
        printf("%s不是变量名\n", token1);
        return 35; // 错误码35：标识符不是变量
    }

    // 读取下一个token，继续解析后续内容
    if (!read_next_token()) return 10;

    return es;
}

//<write_stat>→write <expression>;
/*int write_stat() {
    int es = 0;

    // 当前token是"write"，读取下一个token期望是表达式
    if (!read_next_token()) return 10;

    // 解析要输出的表达式
    es = expression();

    if (es > 0) return es;

    // 读取下一个token，继续解析后续内容
    if (!read_next_token()) return 10;

    return es;
}*/

//<expression_stat>→<expression>;|;
int expression_stat() {
    int es = 0;

    // 处理空表达式语句（只有一个分号的情况）
    if (strcmp(token, ";") == 0 || strcmp(token1, ";") == 0) {
        ast_add_attr("type", "empty_expression");
        if (!read_next_token()) return 10;
        return 0;
    }

    // 解析表达式
    es = expression();
    if (es > 0) return es;

    // 仓颉语言特性：表达式语句可以没有分号
    // 如果有分号就跳过，没有就直接继续解析后续内容
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

    // 处理以标识符开头的表达式（可能是赋值、自增/自减或普通表达式）
    if (strcmp(token, "ID") == 0 || strcmp(token1, "ID") == 0) {
        char var_name[256];
        strncpy(var_name, token1, sizeof(var_name) - 1);
        var_name[sizeof(var_name) - 1] = '\0';

        int pos;
        // 在符号表中查找变量，检查是否已声明
        if (lookup(var_name, &pos) != 0) {
            printf("变量%s未声明\n", var_name);
            es = 23; // 错误码23：变量未声明
            ast_end();
            return es;
        }

        // 保存当前token状态，用于可能的回退
        char current_token[64], current_token1[256];
        strcpy(current_token, token);
        strcpy(current_token1, token1);

        // 预读下一个token来判断表达式类型
        if (!read_next_token()) {
            ast_end();
            return 0;
        }

        if (strcmp(token, "=") == 0 || strcmp(token1, "=") == 0) {
            // 赋值表达式：x = ...
            ast_begin("LeftValue");
            ast_add_attr("variable", var_name);
            ast_end();

            ast_add_attr("operator", "=");

            if (!read_next_token()) {
                ast_end();
                return 10;
            }

            // 解析赋值语句的右值表达式
            ast_begin("RightValue");
            es = bool_expr();
            ast_end();
        } else if (strcmp(token, "++") == 0 || strcmp(token1, "++") == 0 ||
                   strcmp(token, "--") == 0 || strcmp(token1, "--") == 0) {
            // 后置自增/自减表达式：x++ 或 x--
            ast_add_attr("operator", token);
            ast_add_attr("position", "postfix");

            if (!read_next_token()) {
                ast_end();
                return 10;
            }
            ast_end();
            return 0;
        } else {
            // 其他情况：普通标识符表达式（如 x + 1 或单独的 x）
            // 回退到标识符位置，让bool_expr从头解析
            strcpy(token, current_token);
            strcpy(token1, current_token1);

            es = bool_expr();
        }
    } else if (strcmp(token, "++") == 0 || strcmp(token1, "++") == 0 ||
               strcmp(token, "--") == 0 || strcmp(token1, "--") == 0) {
        // 前置自增/自减表达式：++x 或 --x
        char op[4];
        strncpy(op, token, sizeof(op) - 1);
        op[sizeof(op) - 1] = '\0';
        ast_add_attr("operator", op);
        ast_add_attr("position", "prefix");

        if (!read_next_token()) {
            ast_end();
            return 10;
        }

        // 检查自增/自减操作的操作数是否为标识符
        if (strcmp(token, "ID") != 0 && strcmp(token1, "ID") != 0) {
            printf("期望标识符，得到: %s %s\n", token, token1);
            ast_end();
            return 7; // 错误码7：缺少操作数
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
        // 普通表达式（不以标识符或自增/自减操作符开头）
        es = bool_expr();
    }

    ast_end();
    return es;
}

//<bool_expr>-><additive_expr> |< additive_expr >(>|<|>=|<=|==|!=)< additive_expr >
int bool_expr() {
    int es = 0;

    // 解析左操作数（加法表达式）
    es = additive_expr();
    if (es > 0) return es;

    // 检查是否存在比较运算符
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

        // 创建二元比较表达式节点
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

    // 解析第一个项
    es = term();
    if (es > 0) return es;

    // 处理连续的加法运算（+ 或 -）
    while (strcmp(token, "+") == 0 || strcmp(token, "-") == 0 ||
           strcmp(token1, "+") == 0 || strcmp(token1, "-") == 0) {
        char op[4];
        // 提取操作符（可能来自token类型或token值）
        if (strcmp(token, "+") == 0 || strcmp(token, "-") == 0)
            strncpy(op, token, sizeof(op) - 1);
        else
            strncpy(op, token1, sizeof(op) - 1);
        op[sizeof(op) - 1] = '\0';

        if (!read_next_token()) return 10;

        // 创建二元运算表达式节点
        ast_begin("BinaryExpression");
        ast_add_attr("operator", op);

        // 解析下一个项作为右操作数
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

    // 解析第一个因子
    es = factor();
    if (es > 0) return es;

    // 处理连续的乘法运算（* 或 /）
    while (strcmp(token, "*") == 0 || strcmp(token, "/") == 0 ||
           strcmp(token1, "*") == 0 || strcmp(token1, "/") == 0) {
        char op[4];
        // 提取操作符（可能来自token类型或token值）
        if (strcmp(token, "*") == 0 || strcmp(token, "/") == 0)
            strncpy(op, token, sizeof(op) - 1);
        else
            strncpy(op, token1, sizeof(op) - 1);
        op[sizeof(op) - 1] = '\0';

        if (!read_next_token()) return 10;

        // 创建二元运算表达式节点
        ast_begin("BinaryExpression");
        ast_add_attr("operator", op);

        // 解析下一个因子作为右操作数
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

    // 处理括号表达式
    if (strcmp(token, "(") == 0 || strcmp(token1, "(") == 0) {
        if (!read_next_token()) return 10;

        // 解析括号内的加法表达式
        es = additive_expr();
        if (es > 0) return es;

        // 检查右括号
        if (strcmp(token, ")") != 0 && strcmp(token1, ")") != 0) {
            printf("期望)，得到: %s %s\n", token, token1);
            return 6; // 错误码6：缺少右括号
        }

        if (!read_next_token()) return 10;
    } else if (strcmp(token, "ID") == 0 || strcmp(token1, "ID") == 0) {
        // 处理标识符因子（变量名）
        ast_begin("Identifier");
        ast_add_attr("name", token1);
        ast_end();
        if (!read_next_token()) return 10;
    } else if (strcmp(token, "NUM") == 0 || strcmp(token1, "NUM") == 0) {
        // 处理数字字面量因子
        ast_begin("BasicLit");
        ast_add_attr("value", token1);
        ast_end();
        if (!read_next_token()) return 10;
    } else {
        // 无法识别的因子类型
        printf("期望因子，得到: %s %s\n", token, token1);
        return 7; // 错误码7：缺少操作数
    }

    return es;
}


/*向符号表中插入新的符号
 符号表用于记录程序中所有的标识符（变量、函数）及其属性
 category 符号类别：variable（变量）或 function（函数）
 name 符号名称（标识符名字）
 return int 错误代码：0表示成功，非0表示各种错误
*/
int insert_Symbol(enum Category_symbol category, char *name) {
    int i, es = 0;

    // 检查符号表是否已满
    if (symbolIndex >= maxsymbolIndex) return (21); // 错误码21：符号表已满

    // 检查符号是否已存在（避免重复定义）
    // 从后向前查找，支持相同名字在不同作用域的情况（虽然当前实现是单一作用域）
    for (i = symbolIndex - 1; i >= 0; i--) {
        if (strcmp(symbol[i].name, name) == 0) {
            // 符号已存在，检查类型是否相同
            if (symbol[i].kind == category) {
                // 同类型的重复定义错误
                if (category == function) {
                    es = 32; // 错误码32：函数名重复定义
                } else {
                    es = 22; // 错误码22：变量名重复定义
                }
            } else {
                // 同名但不同类型：这里假设不允许任何同名（即使是不同类型）
                // 在实际编译器中，这可能根据作用域规则有所不同
                if (category == function) {
                    es = 32; // 错误码32：函数名与变量名冲突
                } else {
                    es = 22; // 错误码22：变量名与函数名冲突
                }
            }
            break;
        }
    }

    // 如果发现错误，直接返回
    if (es > 0) return (es);

    // 插入新符号到符号表
    switch (category) {
        case function:
            symbol[symbolIndex].kind = function;
        // 函数符号的address字段可能未使用或用于其他用途
            break;
        case variable:
            symbol[symbolIndex].kind = variable;
        // 为变量分配相对地址（在函数栈帧中的位置）
            symbol[symbolIndex].address = offset;
            offset++; // 地址计数器递增，为下一个变量做准备
            break;
    }

    // 安全地复制符号名称到符号表
    strncpy(symbol[symbolIndex].name, name, sizeof(symbol[symbolIndex].name) - 1);
    symbol[symbolIndex].name[sizeof(symbol[symbolIndex].name) - 1] = '\0'; // 确保字符串终止

    symbolIndex++; // 移动符号表索引到下一个空位置
    return es;
}

/*
 在符号表中查找符号
 用于检查标识符是否已声明，并获取其在符号表中的位置
 name 要查找的符号名称
 pPosition 输出参数，返回符号在符号表中的位置索引
 return int 查找结果：0表示找到，23表示未找到
 */
int lookup(char *name, int *pPosition) {
    int i;

    // 从后向前查找符号表（支持简单的局部优先，虽然当前是单一作用域）
    // 从symbolIndex-1开始到0，这样后定义的符号会先被找到
    for (i = symbolIndex - 1; i >= 0; i--) {
        if (strcmp(symbol[i].name, name) == 0) {
            *pPosition = i; // 返回符号在符号表中的位置
            return 0; // 返回0表示查找成功
        }
    }

    // 符号未找到
    return 23; // 错误码23：符号未声明
}

int main() {
    return TESTparse();
}
