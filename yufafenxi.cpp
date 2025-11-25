//TESTparse 语法分析
#include <stdio.h>
#include <ctype.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>

// 语法树节点结构
typedef struct TreeNode {
    char nodeType[20];      // 节点类型
    char tokenValue[40];    // 单词值
    int line;               // 行号（可选）
    struct TreeNode *firstChild;  // 第一个子节点
    struct TreeNode *nextSibling; // 下一个兄弟节点
} TreeNode;

// 全局变量
char token[20], token1[40]; // 单词类别值、单词自身值
char tokenfile[30];         // 单词流文件的名字
FILE *fpTokenin;            // 单词流文件指针

TreeNode *root = NULL;      // 语法树根节点
int currentLine = 1;        // 当前行号

// 函数声明
int TESTparse();
TreeNode* createNode(char *nodeType, char *tokenValue);
void addChild(TreeNode *parent, TreeNode *child);
void printTree(TreeNode *node, int level);
int program(TreeNode **node);
int fun_declaration(TreeNode *parent);
int main_declaration(TreeNode *parent);
int function_body(TreeNode *parent);
int compound_stat(TreeNode *parent);
int statement(TreeNode *parent);
int expression_stat(TreeNode *parent);
int expression(TreeNode *parent);
int bool_expr(TreeNode *parent);
int additive_expr(TreeNode *parent);
int term(TreeNode *parent);
int factor(TreeNode *parent);
int if_stat(TreeNode *parent);
int while_stat(TreeNode *parent);
int for_stat(TreeNode *parent);
int write_stat(TreeNode *parent);
int read_stat(TreeNode *parent);
int declaration_stat(TreeNode *parent);
int declaration_list(TreeNode *parent);
int statement_list(TreeNode *parent);
int call_stat(TreeNode *parent);

// 创建语法树节点
TreeNode* createNode(char *nodeType, char *tokenValue) {
    TreeNode *node = (TreeNode*)malloc(sizeof(TreeNode));
    strcpy(node->nodeType, nodeType);
    if (tokenValue) {
        strcpy(node->tokenValue, tokenValue);
    } else {
        strcpy(node->tokenValue, "");
    }
    node->line = currentLine;
    node->firstChild = NULL;
    node->nextSibling = NULL;
    return node;
}

// 添加子节点
void addChild(TreeNode *parent, TreeNode *child) {
    if (parent == NULL || child == NULL) return;

    if (parent->firstChild == NULL) {
        parent->firstChild = child;
    } else {
        TreeNode *temp = parent->firstChild;
        while (temp->nextSibling != NULL) {
            temp = temp->nextSibling;
        }
        temp->nextSibling = child;
    }
}

// 打印语法树
void printTree(TreeNode *node, int level) {
    if (node == NULL) return;

    for (int i = 0; i < level; i++) {
        printf("  ");
    }
    printf("%s", node->nodeType);
    if (strlen(node->tokenValue) > 0) {
        printf(": %s", node->tokenValue);
    }
    printf("\n");

    printTree(node->firstChild, level + 1);
    printTree(node->nextSibling, level);
}

// 语法分析主程序
int TESTparse() {
    int es = 0;
    printf("请输入单词流文件名（包括路径）：");
    scanf("%s", tokenfile);

    if ((fpTokenin = fopen(tokenfile, "r")) == NULL) {
        printf("\n打开%s错误!\n", tokenfile);
        es = 10;
        return es;
    }

    // 创建程序节点作为根节点
    root = createNode("Program", NULL);

    es = program(&root);
    fclose(fpTokenin);

    printf("==语法分析结果==\n");
    switch(es) {
        case 0:
            printf("语法分析成功!\n");
            printf("\n语法分析树:\n");
            printTree(root, 0);
            break;
        case 10: printf("打开文件 %s失败!\n", tokenfile); break;
        case 1: printf("缺少{!\n"); break;
        case 2: printf("缺少}!\n"); break;
        case 3: printf("缺少标识符!\n"); break;
        case 4: printf("少分号!\n"); break;
        case 5: printf("缺少(!\n"); break;
        case 6: printf("缺少)!\n"); break;
        case 7: printf("缺少操作数!\n"); break;
        case 11: printf("函数开头缺少{!\n"); break;
        case 12: printf("函数结束缺少}!\n"); break;
        case 13: printf("最后一个函数的名字应该是main!\n"); break;
        case 24: printf("程序中main函数结束后，还有其它多余字符\n"); break;
    }

    return es;
}

// <program> → { fun_declaration } <main_declaration>
int program(TreeNode **node) {
    int es = 0;

    fscanf(fpTokenin, "%s %s\n", token, token1);
    while (!strcmp(token, "function")) {
        TreeNode *funNode = createNode("FunctionDeclaration", NULL);
        addChild(*node, funNode);

        fscanf(fpTokenin, "%s %s\n", token, token1);
        es = fun_declaration(funNode);

        if (es != 0) return es;
        fscanf(fpTokenin, "%s %s\n", token, token1);
    }

    if (strcmp(token, "ID")) {
        es = 1;
        return es;
    }
    if (strcmp(token1, "main")) {
        es = 13;
        return es;
    }

    TreeNode *mainNode = createNode("MainDeclaration", NULL);
    addChild(*node, mainNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    es = main_declaration(mainNode);

    if (es > 0) return es;

    if (!feof(fpTokenin)) {
        return (es = 24);
    }

    return es;
}

// <fun_declaration> → function ID '(' ')' <function_body>
int fun_declaration(TreeNode *parent) {
    int es = 0;

    if (strcmp(token, "ID")) {
        es = 2;
        return es;
    }

    TreeNode *idNode = createNode("ID", token1);
    addChild(parent, idNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    if (strcmp(token, "(")) {
        es = 5;
        return es;
    }

    TreeNode *leftParen = createNode("(", "(");
    addChild(parent, leftParen);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    if (strcmp(token, ")")) {
        es = 6;
        return es;
    }

    TreeNode *rightParen = createNode(")", ")");
    addChild(parent, rightParen);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    es = function_body(parent);

    return es;
}

// <main_declaration> → main '(' ')' <function_body>
int main_declaration(TreeNode *parent) {
    int es = 0;

    if (strcmp(token, "(")) {
        es = 5;
        return es;
    }

    TreeNode *leftParen = createNode("(", "(");
    addChild(parent, leftParen);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    if (strcmp(token, ")")) {
        es = 6;
        return es;
    }

    TreeNode *rightParen = createNode(")", ")");
    addChild(parent, rightParen);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    es = function_body(parent);

    return es;
}

// <function_body> → '{' <declaration_list> <statement_list> '}'
int function_body(TreeNode *parent) {
    int es = 0;

    if (strcmp(token, "{")) {
        es = 11;
        return es;
    }

    TreeNode *leftBrace = createNode("{", "{");
    addChild(parent, leftBrace);

    fscanf(fpTokenin, "%s %s\n", token, token1);

    // 处理声明列表
    TreeNode *declList = createNode("DeclarationList", NULL);
    addChild(parent, declList);
    es = declaration_list(declList);
    if (es > 0) return es;

    // 处理语句列表
    TreeNode *stmtList = createNode("StatementList", NULL);
    addChild(parent, stmtList);
    es = statement_list(stmtList);
    if (es > 0) return es;

    if (strcmp(token, "}")) {
        es = 12;
        return es;
    }

    TreeNode *rightBrace = createNode("}", "}");
    addChild(parent, rightBrace);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    return es;
}

// <declaration_list> → { <declaration_stat> }
int declaration_list(TreeNode *parent) {
    int es = 0;

    while (strcmp(token, "int") == 0) {
        TreeNode *declNode = createNode("Declaration", NULL);
        addChild(parent, declNode);

        es = declaration_stat(declNode);
        if (es > 0) return es;
    }

    return es;
}

// <declaration_stat> → int ID ;
int declaration_stat(TreeNode *parent) {
    int es = 0;

    TreeNode *typeNode = createNode("Type", "int");
    addChild(parent, typeNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    if (strcmp(token, "ID")) return (es = 3);

    TreeNode *idNode = createNode("ID", token1);
    addChild(parent, idNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    if (strcmp(token, ";")) return (es = 4);

    TreeNode *semiNode = createNode(";", ";");
    addChild(parent, semiNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    return es;
}

// <statement_list> → { <statement> }
int statement_list(TreeNode *parent) {
    int es = 0;

    while (strcmp(token, "}") != 0) {
        TreeNode *stmtNode = createNode("Statement", NULL);
        addChild(parent, stmtNode);

        es = statement(stmtNode);
        if (es > 0) return es;
    }

    return es;
}

// <statement> → <if_stat> | <while_stat> | <for_stat> | <compound_stat>
//              | <expression_stat> | <call_stat> | <read_stat> | <write_stat>
int statement(TreeNode *parent) {
    int es = 0;

    if (strcmp(token, "if") == 0) {
        es = if_stat(parent);
    } else if (strcmp(token, "while") == 0) {
        es = while_stat(parent);
    } else if (strcmp(token, "for") == 0) {
        es = for_stat(parent);
    } else if (strcmp(token, "read") == 0) {
        es = read_stat(parent);
    } else if (strcmp(token, "write") == 0) {
        es = write_stat(parent);
    } else if (strcmp(token, "{") == 0) {
        es = compound_stat(parent);
    } else if (strcmp(token, "call") == 0) {
        es = call_stat(parent);
    } else if (strcmp(token, "ID") == 0 || strcmp(token, "NUM") == 0 || strcmp(token, "(") == 0) {
        es = expression_stat(parent);
    }

    return es;
}

// <if_stat> → if '(' <expr> ')' <statement> [else <statement>]
int if_stat(TreeNode *parent) {
    int es = 0;

    TreeNode *ifNode = createNode("IfStatement", NULL);
    addChild(parent, ifNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    if (strcmp(token, "(")) return (es = 5);

    TreeNode *leftParen = createNode("(", "(");
    addChild(ifNode, leftParen);

    fscanf(fpTokenin, "%s %s\n", token, token1);

    TreeNode *condition = createNode("Condition", NULL);
    addChild(ifNode, condition);
    es = bool_expr(condition);
    if (es > 0) return es;

    if (strcmp(token, ")")) return (es = 6);

    TreeNode *rightParen = createNode(")", ")");
    addChild(ifNode, rightParen);

    fscanf(fpTokenin, "%s %s\n", token, token1);

    TreeNode *thenStmt = createNode("ThenStatement", NULL);
    addChild(ifNode, thenStmt);
    es = statement(thenStmt);
    if (es > 0) return es;

    // 处理可选的else部分
    if (strcmp(token, "else") == 0) {
        TreeNode *elseStmt = createNode("ElseStatement", NULL);
        addChild(ifNode, elseStmt);

        fscanf(fpTokenin, "%s %s\n", token, token1);
        es = statement(elseStmt);
        if (es > 0) return es;
    }

    return es;
}

// <while_stat> → while '(' <expr> ')' <statement>
int while_stat(TreeNode *parent) {
    int es = 0;

    TreeNode *whileNode = createNode("WhileStatement", NULL);
    addChild(parent, whileNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    if (strcmp(token, "(")) return (es = 5);

    TreeNode *leftParen = createNode("(", "(");
    addChild(whileNode, leftParen);

    fscanf(fpTokenin, "%s %s\n", token, token1);

    TreeNode *condition = createNode("Condition", NULL);
    addChild(whileNode, condition);
    es = bool_expr(condition);
    if (es > 0) return es;

    if (strcmp(token, ")")) return (es = 6);

    TreeNode *rightParen = createNode(")", ")");
    addChild(whileNode, rightParen);

    fscanf(fpTokenin, "%s %s\n", token, token1);

    TreeNode *body = createNode("Body", NULL);
    addChild(whileNode, body);
    es = statement(body);

    return es;
}

// <for_stat> → for '(' <expr> ; <expr> ; <expr> ')' <statement>
int for_stat(TreeNode *parent) {
    int es = 0;

    TreeNode *forNode = createNode("ForStatement", NULL);
    addChild(parent, forNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    if (strcmp(token, "(")) return (es = 5);

    TreeNode *leftParen = createNode("(", "(");
    addChild(forNode, leftParen);

    fscanf(fpTokenin, "%s %s\n", token, token1);

    // 初始化表达式
    if (strcmp(token, ";") != 0) {
        TreeNode *init = createNode("Init", NULL);
        addChild(forNode, init);
        es = expression(init);
        if (es > 0) return es;
    }

    if (strcmp(token, ";")) return (es = 4);

    TreeNode *semi1 = createNode(";", ";");
    addChild(forNode, semi1);

    fscanf(fpTokenin, "%s %s\n", token, token1);

    // 条件表达式
    if (strcmp(token, ";") != 0) {
        TreeNode *condition = createNode("Condition", NULL);
        addChild(forNode, condition);
        es = bool_expr(condition);
        if (es > 0) return es;
    }

    if (strcmp(token, ";")) return (es = 4);

    TreeNode *semi2 = createNode(";", ";");
    addChild(forNode, semi2);

    fscanf(fpTokenin, "%s %s\n", token, token1);

    // 增量表达式
    if (strcmp(token, ")") != 0) {
        TreeNode *increment = createNode("Increment", NULL);
        addChild(forNode, increment);
        es = expression(increment);
        if (es > 0) return es;
    }

    if (strcmp(token, ")")) return (es = 6);

    TreeNode *rightParen = createNode(")", ")");
    addChild(forNode, rightParen);

    fscanf(fpTokenin, "%s %s\n", token, token1);

    TreeNode *body = createNode("Body", NULL);
    addChild(forNode, body);
    es = statement(body);

    return es;
}

// <write_stat> → write <expression> ;
int write_stat(TreeNode *parent) {
    int es = 0;

    TreeNode *writeNode = createNode("WriteStatement", NULL);
    addChild(parent, writeNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);

    TreeNode *exprNode = createNode("Expression", NULL);
    addChild(writeNode, exprNode);
    es = expression(exprNode);
    if (es > 0) return es;

    if (strcmp(token, ";")) return (es = 4);

    TreeNode *semiNode = createNode(";", ";");
    addChild(writeNode, semiNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    return es;
}

// <read_stat> → read ID ;
int read_stat(TreeNode *parent) {
    int es = 0;

    TreeNode *readNode = createNode("ReadStatement", NULL);
    addChild(parent, readNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    if (strcmp(token, "ID")) return (es = 3);

    TreeNode *idNode = createNode("ID", token1);
    addChild(readNode, idNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    if (strcmp(token, ";")) return (es = 4);

    TreeNode *semiNode = createNode(";", ";");
    addChild(readNode, semiNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    return es;
}

// <compound_stat> → '{' <statement_list> '}'
int compound_stat(TreeNode *parent) {
    int es = 0;

    TreeNode *compoundNode = createNode("CompoundStatement", NULL);
    addChild(parent, compoundNode);

    TreeNode *leftBrace = createNode("{", "{");
    addChild(compoundNode, leftBrace);

    fscanf(fpTokenin, "%s %s\n", token, token1);

    TreeNode *stmtList = createNode("StatementList", NULL);
    addChild(compoundNode, stmtList);
    es = statement_list(stmtList);

    if (strcmp(token, "}")) return (es = 2);

    TreeNode *rightBrace = createNode("}", "}");
    addChild(compoundNode, rightBrace);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    return es;
}

// <call_stat> → call ID ( )
int call_stat(TreeNode *parent) {
    int es = 0;

    TreeNode *callNode = createNode("CallStatement", NULL);
    addChild(parent, callNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    if (strcmp(token, "ID")) return (es = 3);

    TreeNode *idNode = createNode("ID", token1);
    addChild(callNode, idNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    if (strcmp(token, "(")) return (es = 5);

    TreeNode *leftParen = createNode("(", "(");
    addChild(callNode, leftParen);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    if (strcmp(token, ")")) return (es = 6);

    TreeNode *rightParen = createNode(")", ")");
    addChild(callNode, rightParen);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    if (strcmp(token, ";")) return (es = 4);

    TreeNode *semiNode = createNode(";", ";");
    addChild(callNode, semiNode);

    fscanf(fpTokenin, "%s %s\n", token, token1);
    return es;
}

// <expression_stat> → <expression> ; | ;
int expression_stat(TreeNode *parent) {
    int es = 0;

    TreeNode *exprStmtNode = createNode("ExpressionStatement", NULL);
    addChild(parent, exprStmtNode);

    if (strcmp(token, ";") == 0) {
        TreeNode *semiNode = createNode(";", ";");
        addChild(exprStmtNode, semiNode);
        fscanf(fpTokenin, "%s %s\n", token, token1);
        return es;
    }

    TreeNode *exprNode = createNode("Expression", NULL);
    addChild(exprStmtNode, exprNode);
    es = expression(exprNode);
    if (es > 0) return es;

    if (strcmp(token, ";") == 0) {
        TreeNode *semiNode = createNode(";", ";");
        addChild(exprStmtNode, semiNode);
        fscanf(fpTokenin, "%s %s\n", token, token1);
        return es;
    } else {
        return (es = 4); // 少分号
    }
}

// <expression> → ID = <bool_expr> | <bool_expr>
int expression(TreeNode *parent) {
    int es = 0;

    TreeNode *exprNode = createNode("Expression", NULL);
    addChild(parent, exprNode);

    // 检查是否是赋值表达式
    if (strcmp(token, "ID") == 0) {
        TreeNode *idNode = createNode("ID", token1);
        addChild(exprNode, idNode);

        // 预读下一个token
        char nextToken[20], nextToken1[40];
        long pos = ftell(fpTokenin);
        fscanf(fpTokenin, "%s %s\n", nextToken, nextToken1);
        fseek(fpTokenin, pos, SEEK_SET);

        if (strcmp(nextToken, "=") == 0) {
            // 是赋值表达式
            TreeNode *assignNode = createNode("Assignment", NULL);
            addChild(exprNode, assignNode);
            addChild(assignNode, idNode);

            fscanf(fpTokenin, "%s %s\n", token, token1); // 读取 '='
            TreeNode *opNode = createNode("=", "=");
            addChild(assignNode, opNode);

            fscanf(fpTokenin, "%s %s\n", token, token1);
            es = bool_expr(assignNode);
            return es;
        }
    }

    // 不是赋值表达式，直接处理bool_expr
    es = bool_expr(exprNode);
    return es;
}

// <bool_expr> → <additive_expr> | <additive_expr> (>|<|>=|<=|==|!=) <additive_expr>
int bool_expr(TreeNode *parent) {
    int es = 0;

    TreeNode *boolNode = createNode("BoolExpression", NULL);
    addChild(parent, boolNode);

    es = additive_expr(boolNode);
    if (es > 0) return es;

    if (strcmp(token, ">") == 0 || strcmp(token, ">=") == 0
        || strcmp(token, "<") == 0 || strcmp(token, "<=") == 0
        || strcmp(token, "==") == 0 || strcmp(token, "!=") == 0) {

        TreeNode *opNode = createNode("RelOp", token);
        addChild(boolNode, opNode);

        fscanf(fpTokenin, "%s %s\n", token, token1);
        es = additive_expr(boolNode);
    }

    return es;
}

// <additive_expr> → <term> {(+|-) <term>}
int additive_expr(TreeNode *parent) {
    int es = 0;

    TreeNode *addNode = createNode("AdditiveExpression", NULL);
    addChild(parent, addNode);

    es = term(addNode);
    if (es > 0) return es;

    while (strcmp(token, "+") == 0 || strcmp(token, "-") == 0) {
        TreeNode *opNode = createNode("AddOp", token);
        addChild(addNode, opNode);

        fscanf(fpTokenin, "%s %s\n", token, token1);
        es = term(addNode);
        if (es > 0) return es;
    }

    return es;
}

// <term> → <factor> {(*|/) <factor>}
int term(TreeNode *parent) {
    int es = 0;

    TreeNode *termNode = createNode("Term", NULL);
    addChild(parent, termNode);

    es = factor(termNode);
    if (es > 0) return es;

    while (strcmp(token, "*") == 0 || strcmp(token, "/") == 0) {
        TreeNode *opNode = createNode("MulOp", token);
        addChild(termNode, opNode);

        fscanf(fpTokenin, "%s %s\n", token, token1);
        es = factor(termNode);
        if (es > 0) return es;
    }

    return es;
}

// <factor> → '(' <additive_expr> ')' | ID | NUM
int factor(TreeNode *parent) {
    int es = 0;

    TreeNode *factorNode = createNode("Factor", NULL);
    addChild(parent, factorNode);

    if (strcmp(token, "(") == 0) {
        TreeNode *leftParen = createNode("(", "(");
        addChild(factorNode, leftParen);

        fscanf(fpTokenin, "%s %s\n", token, token1);
        es = additive_expr(factorNode);
        if (es > 0) return es;

        if (strcmp(token, ")")) return (es = 6);

        TreeNode *rightParen = createNode(")", ")");
        addChild(factorNode, rightParen);

        fscanf(fpTokenin, "%s %s\n", token, token1);
    } else if (strcmp(token, "ID") == 0) {
        TreeNode *idNode = createNode("ID", token1);
        addChild(factorNode, idNode);
        fscanf(fpTokenin, "%s %s\n", token, token1);
    } else if (strcmp(token, "NUM") == 0) {
        TreeNode *numNode = createNode("NUM", token1);
        addChild(factorNode, numNode);
        fscanf(fpTokenin, "%s %s\n", token, token1);
    } else {
        return (es = 7); // 缺少操作数
    }

    return es;
}

int main() {
    return TESTparse();
}
/*// TESTparse_full_parser.c
// 完整版语法分析器

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define maxsymbolIndex 100

enum Category_symbol {variable,function};

int TESTparse();
int program();
int fun_declaration();
int main_declaration();
int function_body();
int compound_stat();
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

char token[64], token1[256];
char tokenfile[260];

FILE *fpTokenin;

struct
{
    char name[64];
    enum Category_symbol kind;
    int address;
} symbol[maxsymbolIndex];

int symbolIndex = 0;
int offset;

// 用于生成语法树文本
char *astText = NULL;
size_t astCap = 0;
int indentLevel = 0;

void ast_init(){
    astCap = 1<<16;
    astText = (char*)malloc(astCap);
    astText[0] = '\0';
}

void ast_add_indent() {
    for (int i = 0; i < indentLevel; i++) {
        strcat(astText, "  ");
    }
}

void ast_append(const char *fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    size_t cur = strlen(astText);
    char tmp[4096];
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    size_t need = strlen(tmp);
    if(cur + need + 16 > astCap){
        astCap = (cur + need + 16) * 2;
        astText = (char*)realloc(astText, astCap);
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

    printf("读取token: type='%s' value='%s'\n", token, token1);
    return 1;
}

int TESTparse()
{
    int i;
    int es = 0;
    printf("请输入单词流文件名（包括路径）：");
    if(scanf("%s", tokenfile)!=1) return 10;
    if((fpTokenin = fopen(tokenfile, "r")) == NULL){
        printf("\n打开%s错误!\n",tokenfile );
        es = 10;
        return(es);
       }

    ast_init();
    ast_append("语法分析树：\n");
    ast_append("==========\n\n");

    if (!read_next_token()) {
        printf("错误: 文件为空\n");
        return 10;
    }

    es = program();
    fclose(fpTokenin);

    printf("==语法、语义分析程序结果==\n");
    switch(es){
        case 0: printf("语法、语义分析成功!\n"); break;
        case 10: printf("打开文件 %s失败!\n", tokenfile); break;
        case 1: printf("缺少{!\n"); break;
        case 2: printf("缺少}!\n"); break;
        case 3: printf("缺少标识符!\n"); break;
        case 4: printf("少分号!\n"); break;
        case 5: printf("缺少(!\n"); break;
        case 6: printf("缺少)!\n"); break;
        case 7: printf("缺少操作数!\n"); break;
        case 11: printf("函数开头缺少{!\n"); break;
        case 12: printf("函数结束缺少}!\n"); break;
        case 13:printf("最后一个函数的名字应该是main!\n"); break;
        case 21: printf("符号表溢出!\n"); break;
        case 22: printf("变量%s重复定义!\n",token1); break;
        case 23: printf("变量未声明!\n"); break;
        case 24:printf("程序中main函数结束后，还有其它多余字符\n");break;
        case 32: printf("函数名重复定义!\n"); break;
        case 34: printf("call语句后面的标识符%s不是函数名!\n",token1 ); break;
        case 35: printf("read语句后面的标识符不是变量名!\n"); break;
        case 36: printf("赋值语句的左值%s不是变量名!\n",token1); break;
        case 37: printf("因子对应的标识符不是变量名!\n"); break;
        }

    char astfile[512];
    snprintf(astfile,sizeof(astfile),"%s.ast.txt", tokenfile);
    FILE *fasta = fopen(astfile, "w");
    if(fasta){
        fputs(astText, fasta);
        fclose(fasta);
        printf("语法树已输出到 %s\n", astfile);
    } else {
        printf("无法创建语法树文件 %s\n", astfile);
    }

    printf("\n生成的语法树：\n");
    printf("============\n");
    printf("%s", astText);

    printf("\n        符号表\n");
    printf(" 名字\t \t类型 \t地址\n");
    for(i = 0; i < symbolIndex; i++)
        printf("  %-8s \t%d \t%d\n", symbol[i].name, symbol[i].kind,symbol[i].address);

    free(astText);
    return(es);
}

int program()
{
    int es = 0;

    ast_begin("Program");

    // 修改：直接检查token值是否为"main"，而不是检查token类型
    if(strcmp(token, "main") != 0 || strcmp(token1, "main") != 0) {
        printf("期望main，得到: %s %s\n", token, token1);
        es=13;
        return es;
    }

    ast_add_attr("type", "main_declaration");
    insert_Symbol(function,"main");

    if (!read_next_token()) return 10;

    es=main_declaration();
    if(es > 0) return(es);

    ast_end();
    return(es);
}

int main_declaration()
{
    int es=0;

    ast_begin("MainFunction");
    ast_add_attr("ID", "main");

    // 现在检查token值而不是类型
    if(strcmp(token, "(") != 0 || strcmp(token1, "(") != 0){
        printf("期望(，得到: %s %s\n", token, token1);
        es=5;
        return es;
    }

    if (!read_next_token()) return 10;

    if(strcmp(token, ")") != 0 || strcmp(token1, ")") != 0) {
        printf("期望)，得到: %s %s\n", token, token1);
        es=6;
        return es;
    }

    if (!read_next_token()) return 10;

    es= function_body();
    if(es > 0) return es;

    ast_end();
    return es;
}

int function_body()
{
    int es=0;

    ast_begin("Function_Body");

    if(strcmp(token,"{")){
        printf("期望{，得到: %s\n", token);
        es=11;
        return(es);
    }

    offset=2;
    if (!read_next_token()) return 10;

    es=declaration_list();
    if (es>0) return(es);

    es=statement_list();
    if (es>0) return(es);

    if(strcmp(token,"}")){
        printf("期望}，得到: %s\n", token);
        es=12;
        return(es);
    }

    ast_end();
    read_next_token();
    return es;
}

int declaration_list()
{
    int es = 0;

    ast_begin("DeclarationList");

    while(strcmp(token, "var") == 0) {
        es = declaration_stat();
        if(es > 0) return(es);
    }

    ast_end();
    return(es);
}

int declaration_stat()
{
    int es = 0;

    ast_begin("VariableDeclaration");

    if (!read_next_token()) return 10;
    if(strcmp(token, "ID")) {
        printf("期望ID，得到: %s\n", token);
        return(es = 3);
    }

    ast_add_attr("name", token1);

    es = insert_Symbol(variable,token1);
    if(es > 0) return(es);

    if (!read_next_token()) return 10;
    if(strcmp(token, ":")) {
        printf("期望:，得到: %s\n", token);
        return(es = 4);
    }

    if (!read_next_token()) return 10;
    if(strcmp(token, "int") == 0) {
        ast_add_attr("type", "int");
    }
    else if(strcmp(token, "double") == 0) {
        ast_add_attr("type", "double");
    }
    else if(strcmp(token, "ID") == 0) {
        ast_add_attr("type", token1);
    }
    else {
        printf("期望类型，得到: %s\n", token);
        return(es = 4);
    }

    // 仓颉语言：变量声明不需要分号
    // 直接读取下一个token
    if (!read_next_token()) return 10;

    ast_end();
    return(es);
}

int statement_list()
{
    int es = 0;

    ast_begin("StatementList");

    while(strcmp(token, "}") != 0 && !feof(fpTokenin)) {
        printf("解析语句，当前token: %s %s\n", token, token1);
        es = statement();
        if(es > 0) return es;
    }

    ast_end();
    return es;
}

int statement()
{
    int es = 0;

    printf("解析statement，当前token: %s %s\n", token, token1);

    if(strcmp(token, "if") == 0) {
        ast_begin("IfStatement");
        es = if_stat();
        ast_end();
    }
    else if(strcmp(token, "while") == 0) {
        ast_begin("WhileStatement");
        es = while_stat();
        ast_end();
    }
    else if(strcmp(token, "for") == 0) {
        ast_begin("ForStatement");
        es = for_stat();
        ast_end();
    }
    else if(strcmp(token, "{") == 0) {
        ast_begin("CompoundStatement");
        es = compound_stat();
        ast_end();
    }
    else if(strcmp(token, "call") == 0) {
        ast_begin("CallStatement");
        es = call_stat();
        ast_end();
    }
    else if(strcmp(token, "read") == 0) {
        ast_begin("ReadStatement");
        es = read_stat();
        ast_end();
    }
    else if(strcmp(token, "write") == 0) {
        ast_begin("WriteStatement");
        es = write_stat();
        ast_end();
    }
    else if(strcmp(token, "ID") == 0) {
        // 赋值语句或表达式语句
        ast_begin("AssignmentOrExpression");
        es = expression();
        ast_end();

        // 仓颉语言：语句可以没有分号
        if(es == 0 && strcmp(token, ";") == 0) {
            // 如果有分号，跳过
            if (!read_next_token()) return 10;
        }
    }
    else if(strcmp(token, "NUM") == 0 || strcmp(token, "(") == 0) {
        ast_begin("ExpressionStatement");
        es = expression_stat();
        ast_end();
    }
    else if(strcmp(token, ";") == 0) {
        ast_begin("EmptyStatement");
        ast_add_attr("type", "empty");
        if (!read_next_token()) return 10;
        ast_end();
    }
    else if(strcmp(token, "var") == 0) {
        es = declaration_stat();
    }
    else {
        printf("错误: 未知语句类型: %s %s\n", token, token1);
        es = 4;
    }

    return es;
}

int if_stat()
{
    int es = 0;

    ast_add_attr("keyword", "if");

    if (!read_next_token()) return 10;
    if(strcmp(token, "(")) {
        printf("期望(，得到: %s\n", token);
        return 5;
    }

    if (!read_next_token()) return 10;
    ast_begin("Condition");
    es = bool_expr();
    ast_end();
    if(es > 0) return es;

    if(strcmp(token, ")")) {
        printf("期望)，得到: %s\n", token);
        return 6;
    }

    if (!read_next_token()) return 10;
    ast_begin("ThenBranch");
    es = statement();
    ast_end();
    if(es > 0) return es;

    if(strcmp(token, "else") == 0) {
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

int while_stat()
{
    int es = 0;

    ast_add_attr("keyword", "while");

    if (!read_next_token()) return 10;
    if(strcmp(token, "(")) {
        printf("期望(，得到: %s\n", token);
        return 5;
    }

    if (!read_next_token()) return 10;
    ast_begin("Condition");
    es = bool_expr();
    ast_end();
    if(es > 0) return es;

    if(strcmp(token, ")")) {
        printf("期望)，得到: %s\n", token);
        return 6;
    }

    if (!read_next_token()) return 10;
    ast_begin("LoopBody");
    es = statement();
    ast_end();

    return es;
}

int for_stat()
{
    int es = 0;

    ast_add_attr("keyword", "for");

    if (!read_next_token()) return 10;
    if(strcmp(token, "(")) {
        printf("期望(，得到: %s\n", token);
        return 5;
    }

    if (!read_next_token()) return 10;
    if(strcmp(token, ";") != 0) {
        ast_begin("Initialization");
        es = expression();
        ast_end();
        if(es > 0) return es;
    }

    if(strcmp(token, ";")) {
        printf("期望;，得到: %s\n", token);
        return 4;
    }

    if (!read_next_token()) return 10;
    if(strcmp(token, ";") != 0) {
        ast_begin("Condition");
        es = bool_expr();
        ast_end();
        if(es > 0) return es;
    }

    if(strcmp(token, ";")) {
        printf("期望;，得到: %s\n", token);
        return 4;
    }

    if (!read_next_token()) return 10;
    if(strcmp(token, ")") != 0) {
        ast_begin("Increment");
        es = expression();
        ast_end();
        if(es > 0) return es;
    }

    if(strcmp(token, ")")) {
        printf("期望)，得到: %s\n", token);
        return 6;
    }

    if (!read_next_token()) return 10;
    ast_begin("LoopBody");
    es = statement();
    ast_end();

    return es;
}

int compound_stat()
{
    int es = 0;

    ast_add_attr("start", "{");

    if (!read_next_token()) return 10;

    ast_begin("StatementList");
    es = statement_list();
    ast_end();
    if(es > 0) return es;

    if(strcmp(token, "}")) {
        printf("期望}，得到: %s\n", token);
        return 12;
    }

    ast_add_attr("end", "}");

    if (!read_next_token()) return 10;

    return es;
}

int call_stat()
{
    int es = 0;
    int symbolPos;

    if (!read_next_token()) return 10;
    if(strcmp(token, "ID")) {
        printf("期望ID，得到: %s\n", token);
        return 3;
    }

    ast_add_attr("function_name", token1);

    if(lookup(token1, &symbolPos) != 0) {
        printf("函数%s未声明\n", token1);
        return 34;
    }
    if(symbol[symbolPos].kind != function) {
        printf("%s不是函数名\n", token1);
        return 34;
    }

    if (!read_next_token()) return 10;
    if(strcmp(token, "(")) {
        printf("期望(，得到: %s\n", token);
        return 5;
    }

    if (!read_next_token()) return 10;
    if(strcmp(token, ")")) {
        printf("期望)，得到: %s\n", token);
        return 6;
    }

    if (!read_next_token()) return 10;
    if(strcmp(token, ";")) {
        printf("期望;，得到: %s\n", token);
        return 4;
    }

    if (!read_next_token()) return 10;

    return es;
}

int read_stat()
{
    int es = 0;
    int pos;

    ast_add_attr("keyword", "read");

    if (!read_next_token()) return 10;
    if(strcmp(token, "ID")) {
        printf("期望ID，得到: %s\n", token);
        return 3;
    }

    ast_add_attr("variable_name", token1);

    if(lookup(token1, &pos) != 0) {
        printf("变量%s未声明\n", token1);
        return 35;
    }
    if(symbol[pos].kind != variable) {
        printf("%s不是变量名\n", token1);
        return 35;
    }

    if (!read_next_token()) return 10;
    if(strcmp(token, ";")) {
        printf("期望;，得到: %s\n", token);
        return 4;
    }

    if (!read_next_token()) return 10;

    return es;
}

int write_stat()
{
    int es = 0;

    ast_add_attr("keyword", "write");

    if (!read_next_token()) return 10;
    ast_begin("Expression");
    es = expression();
    ast_end();
    if(es > 0) return es;

    if(strcmp(token, ";")) {
        printf("期望;，得到: %s\n", token);
        return 4;
    }

    if (!read_next_token()) return 10;

    return es;
}

int expression_stat()
{
    int es = 0;

    // 空表达式语句
    if(strcmp(token, ";") == 0) {
        ast_add_attr("type", "empty_expression");
        if (!read_next_token()) return 10;
        return 0;
    }

    ast_begin("Expression");
    es = expression();
    ast_end();
    if(es > 0) return es;

    // 仓颉语言：表达式语句可以没有分号
    // 如果有分号就跳过，没有就直接继续
    if(strcmp(token, ";") == 0) {
        if (!read_next_token()) return 10;
    }

    return es;
}

int expression()
{
    int es = 0;
    ast_begin("Expression");

    printf("进入expression，当前token: %s %s\n", token, token1);

    // 先检查是否是自增/自减表达式 (++x 或 x++)
    if(strcmp(token, "ID") == 0) {
        char var_name[256];
        strcpy(var_name, token1);

        // 保存当前变量信息
        ast_begin("LeftValue");
        ast_add_attr("variable", var_name);
        ast_end();

        // 查看下一个token
        if (!read_next_token()) {
            ast_end();
            return 0;
        }

        if(strcmp(token, "=") == 0) {
            // 赋值表达式: x = ...
            ast_add_attr("operator", "assignment");

            if (!read_next_token()) {
                ast_end();
                return 10;
            }

            ast_begin("RightValue");
            es = bool_expr();
            ast_end();
        }
        else if(strcmp(token, "++") == 0 || strcmp(token, "--") == 0) {
            // 后置自增/自减: x++ 或 x--
            ast_add_attr("operator", token);
            ast_add_attr("position", "postfix");

            // 读取下一个token并直接返回，这是一个完整的表达式
            if (!read_next_token()) {
                ast_end();
                return 10;
            }
            // 直接返回，不进入bool_expr
            ast_end();
            return 0;
        }
        else {
            // 其他情况，可能是普通表达式如 x + 1
            // 回退到bool_expr解析
            es = bool_expr();
        }
    }
    else if(strcmp(token, "++") == 0 || strcmp(token, "--") == 0) {
        // 前置自增/自减: ++x 或 --x
        char op[4];
        strcpy(op, token);
        ast_add_attr("operator", op);
        ast_add_attr("position", "prefix");

        if (!read_next_token()) {
            ast_end();
            return 10;
        }

        if(strcmp(token, "ID") != 0) {
            printf("期望标识符，得到: %s\n", token);
            ast_end();
            return 7;
        }

        ast_begin("Operand");
        ast_add_attr("variable", token1);
        ast_end();

        // 读取下一个token并返回
        if (!read_next_token()) {
            ast_end();
            return 10;
        }
        ast_end();
        return 0;
    }
    else {
        // 普通表达式
        es = bool_expr();
    }

    ast_end();
    return es;
}

int bool_expr()
{
    int es = 0;
    ast_begin("BoolExpression");

    es = additive_expr();
    if(es > 0) {
        ast_end();
        return es;
    }

    if(strcmp(token, ">") == 0 || strcmp(token, ">=") == 0 ||
       strcmp(token, "<") == 0 || strcmp(token, "<=") == 0 ||
       strcmp(token, "==") == 0 || strcmp(token, "!=") == 0) {

        char op[8];
        strcpy(op, token);
        ast_add_attr("relational_operator", op);

        if (!read_next_token()) {
            ast_end();
            return 10;
        }

        ast_begin("RightOperand");
        es = additive_expr();
        ast_end();
    }

    ast_end();
    return es;
}

int term()
{
    int es = 0;
    ast_begin("Term");

    es = factor();
    if(es > 0) {
        ast_end();
        return es;
    }

    // 只有在当前token是 * 或 / 时才继续解析
    // 如果当前token是 ; } 等其他符号，说明表达式已经结束
    while(strcmp(token, "*") == 0 || strcmp(token, "/") == 0) {
        char op[4];
        strcpy(op, token);
        ast_add_attr("multiplicative_operator", op);

        if (!read_next_token()) {
            ast_end();
            return 10;
        }

        ast_begin("RightFactor");
        es = factor();
        ast_end();
        if(es > 0) break;
    }

    ast_end();
    return es;
}

int additive_expr()
{
    int es = 0;
    ast_begin("AdditiveExpression");

    es = term();
    if(es > 0) {
        ast_end();
        return es;
    }

    // 只有在当前token是 + 或 - 时才继续解析
    while(strcmp(token, "+") == 0 || strcmp(token, "-") == 0) {
        char op[4];
        strcpy(op, token);
        ast_add_attr("additive_operator", op);

        if (!read_next_token()) {
            ast_end();
            return 10;
        }

        ast_begin("RightTerm");
        es = term();
        ast_end();
        if(es > 0) break;
    }

    ast_end();
    return es;
}

int factor()
{
    int es = 0;
    ast_begin("Factor");

    if(strcmp(token, "(") == 0) {
        ast_add_attr("type", "parenthesized_expression");

        if (!read_next_token()) {
            ast_end();
            return 10;
        }

        es = expression();
        if(es > 0) {
            ast_end();
            return es;
        }

        if(strcmp(token, ")") != 0) {
            printf("期望)，得到: %s\n", token);
            ast_end();
            return 6;
        }

        if (!read_next_token()) {
            ast_end();
            return 10;
        }
    }
    else if(strcmp(token, "ID") == 0) {
        ast_add_attr("type", "variable");
        ast_add_attr("name", token1);
        if (!read_next_token()) {
            ast_end();
            return 0;
        }
    }
    else if(strcmp(token, "NUM") == 0) {
        ast_add_attr("type", "number");
        ast_add_attr("value", token1);
        if (!read_next_token()) {
            ast_end();
            return 10;
        }
    }
    else {
        printf("期望因子，得到: %s\n", token);
        ast_end();
        return 7;
    }

    ast_end();
    return es;
}

int insert_Symbol(enum Category_symbol category, char *name)
{
    int i, es = 0;

    if(symbolIndex >= maxsymbolIndex) return(21);
    switch (category) {
        case function:
            for(i = symbolIndex - 1; i >= 0; i--) {
                if((strcmp(symbol[i].name, name) == 0)&&(symbol[i].kind==function)){
                    es = 32;
                    break;
                }
            }
            symbol[symbolIndex].kind=function;
            break;
        case variable:
            for(i = symbolIndex - 1; i >= 0; i--) {
                if((strcmp(symbol[i].name, name) == 0)&&(symbol[i].kind==variable)){
                    es = 22;
                    break;
                }
            }
            symbol[symbolIndex].kind=variable;
            symbol[symbolIndex].address = offset;
            offset++;
            break;
    }
    if(es > 0) return(es);
    strcpy(symbol[symbolIndex].name, name);
    symbolIndex++;
    return es;
}

int lookup(char *name, int *pPosition)
{
    int i;
    for(i = symbolIndex - 1; i >= 0; i--){
        if(strcmp(symbol[i].name, name) == 0){
            *pPosition = i;
            return 0;
        }
    }
    return 23;
}

int main(){
    return TESTparse();
}*/