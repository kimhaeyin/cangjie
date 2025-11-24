/* ASTparse.c
   基于你提供的 TESTparse 框架改造，生成抽象语法树（AST）并以 JSON 输出。
   假设输入文件为词法器输出，每行两个字段：token token1
   例如:
     ID main
     ( (
     ) )
     { {
     var var
     ID n
     : :
     int int
     ...
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TOKLEN 64
#define LEXLEN 256
#define MAX_CHILDREN 32

/* ---------- token stream (与老师原框架保持兼容) ---------- */
char token[TOKLEN], token1[LEXLEN];
FILE *fpTokenin;

/* 读取下一个 token；返回 0=正常，EOF=1 */
int next_token() {
    if (fscanf(fpTokenin, "%s %s", token, token1) == 2) {
        return 0;
    } else {
        strcpy(token, "EOF");
        token1[0] = '\0';
        return 1;
    }
}

/* ---------- AST 节点定义 ---------- */
typedef enum {
    AST_Program,
    AST_FunctionDecl,
    AST_MainDecl,
    AST_Block,            // function body => block
    AST_VarDecl,          // VariableDeclaration
    AST_VarDeclarator,
    AST_Identifier,
    AST_BasicLit,         // numeric literal
    AST_Assign,
    AST_BinaryExpr,
    AST_Read,
    AST_Write,
    AST_If,
    AST_While,
    AST_For,
    AST_Call,
    AST_ExpressionStmt,
    AST_Empty
} ASTKind;

typedef struct ASTNode {
    ASTKind kind;
    char type_name[64];    // optional textual type (e.g., "var"/"int", binary op)
    char lexeme[LEXLEN];   // for identifier names or literal raw
    struct ASTNode *children[MAX_CHILDREN];
    int child_count;
} ASTNode;

ASTNode* new_node(ASTKind k) {
    ASTNode *n = (ASTNode*)malloc(sizeof(ASTNode));
    memset(n, 0, sizeof(ASTNode));
    n->kind = k;
    n->child_count = 0;
    n->type_name[0] = '\0';
    n->lexeme[0] = '\0';
    return n;
}
void add_child(ASTNode *p, ASTNode *c) {
    if (!p || !c) return;
    if (p->child_count < MAX_CHILDREN) p->children[p->child_count++] = c;
    else fprintf(stderr, "warning: too many children\n");
}

/* helpers to create specific nodes */
ASTNode* new_identifier(const char *name) {
    ASTNode *n = new_node(AST_Identifier);
    strncpy(n->lexeme, name, sizeof(n->lexeme)-1);
    return n;
}
ASTNode* new_basiclit(const char *raw) {
    ASTNode *n = new_node(AST_BasicLit);
    strncpy(n->lexeme, raw, sizeof(n->lexeme)-1);
    return n;
}
ASTNode* new_vardeclarator(ASTNode *id, ASTNode *init) {
    ASTNode *n = new_node(AST_VarDeclarator);
    if (id) add_child(n, id);
    if (init) add_child(n, init);
    return n;
}
ASTNode* new_vardecl(const char *kind) {
    ASTNode *n = new_node(AST_VarDecl);
    if (kind) strncpy(n->type_name, kind, sizeof(n->type_name)-1);
    return n;
}
ASTNode* new_function_decl(const char *fname) {
    ASTNode *n = new_node(AST_FunctionDecl);
    if (fname) strncpy(n->type_name, fname, sizeof(n->type_name)-1);
    return n;
}
ASTNode* new_main_decl() {
    return new_node(AST_MainDecl);
}
ASTNode* new_block() { return new_node(AST_Block); }
ASTNode* new_assign(ASTNode *left, ASTNode *right) {
    ASTNode *n = new_node(AST_Assign);
    add_child(n, left);
    add_child(n, right);
    return n;
}
ASTNode* new_binary(const char *op, ASTNode *l, ASTNode *r) {
    ASTNode *n = new_node(AST_BinaryExpr);
    strncpy(n->type_name, op, sizeof(n->type_name)-1);
    if (l) add_child(n, l);
    if (r) add_child(n, r);
    return n;
}
ASTNode* new_read(ASTNode *id) {
    ASTNode *n = new_node(AST_Read);
    if (id) add_child(n, id);
    return n;
}
ASTNode* new_write(ASTNode *expr) {
    ASTNode *n = new_node(AST_Write);
    if (expr) add_child(n, expr);
    return n;
}
ASTNode* new_if(ASTNode *cond, ASTNode *thenb, ASTNode *elseb) {
    ASTNode *n = new_node(AST_If);
    if (cond) add_child(n, cond);
    if (thenb) add_child(n, thenb);
    if (elseb) add_child(n, elseb);
    return n;
}
ASTNode* new_while(ASTNode *cond, ASTNode *body) {
    ASTNode *n = new_node(AST_While);
    if (cond) add_child(n, cond);
    if (body) add_child(n, body);
    return n;
}
ASTNode* new_for(ASTNode *id, ASTNode *rangeExpr, ASTNode *body) {
    ASTNode *n = new_node(AST_For);
    if (id) add_child(n, id);
    if (rangeExpr) add_child(n, rangeExpr);
    if (body) add_child(n, body);
    return n;
}
ASTNode* new_call(const char *fname) {
    ASTNode *n = new_node(AST_Call);
    if (fname) strncpy(n->type_name, fname, sizeof(n->type_name)-1);
    return n;
}
ASTNode* new_exprstmt(ASTNode *expr) {
    ASTNode *n = new_node(AST_ExpressionStmt);
    if (expr) add_child(n, expr);
    return n;
}

/* ---------- JSON 打印 ---------- */
void json_escape_print(const char *s) {
    putchar('"');
    for (const char *p = s; p && *p; ++p) {
        if (*p == '\\' || *p == '"') {
            putchar('\\'); putchar(*p);
        } else if (*p == '\n') {
            printf("\\n");
        } else {
            putchar(*p);
        }
    }
    putchar('"');
}

void print_indent(int indent) {
    for (int i = 0; i < indent; ++i) putchar(' ');
}

const char* kind_name(ASTKind k) {
    switch(k) {
        case AST_Program: return "Program";
        case AST_FunctionDecl: return "FunctionDeclaration";
        case AST_MainDecl: return "MainDeclaration";
        case AST_Block: return "Block";
        case AST_VarDecl: return "VariableDeclaration";
        case AST_VarDeclarator: return "VariableDeclarator";
        case AST_Identifier: return "Identifier";
        case AST_BasicLit: return "BasicLit";
        case AST_Assign: return "AssignExpression";
        case AST_BinaryExpr: return "BinaryExpression";
        case AST_Read: return "ReadStatement";
        case AST_Write: return "WriteStatement";
        case AST_If: return "IfStatement";
        case AST_While: return "WhileStatement";
        case AST_For: return "ForStatement";
        case AST_Call: return "CallExpression";
        case AST_ExpressionStmt: return "ExpressionStatement";
        case AST_Empty: return "Empty";
        default: return "Unknown";
    }
}

/* 递归打印 AST 为 JSON 格式，indent 为当前缩进空格数 */
void print_json(ASTNode *n, int indent) {
    if (!n) { printf("null"); return; }

    print_indent(indent); printf("{\n");
    print_indent(indent+2); printf("\"type\": "); json_escape_print(kind_name(n->kind)); printf(",\n");

    /* 节点特有字段 */
    switch(n->kind) {
        case AST_VarDecl:
            print_indent(indent+2); printf("\"kind\": ");
            json_escape_print(n->type_name[0] ? n->type_name : "var"); printf(",\n");
            print_indent(indent+2); printf("\"declarations\": [\n");
            for (int i=0;i<n->child_count;i++){
                print_json(n->children[i], indent+4);
                if (i+1 < n->child_count) printf(",\n"); else printf("\n");
            }
            print_indent(indent+2); printf("]\n");
            break;

        case AST_VarDeclarator:
            /* child[0] = id, child[1] = init? */
            print_indent(indent+2); printf("\"type\": "); json_escape_print("VariableDeclarator"); printf(",\n");
            if (n->child_count > 0) {
                print_indent(indent+2); printf("\"id\": \n");
                print_json(n->children[0], indent+4); printf(",\n");
            }
            if (n->child_count > 1) {
                print_indent(indent+2); printf("\"init\": \n");
                print_json(n->children[1], indent+4); printf("\n");
            } else {
                /* no init */
                print_indent(indent+2); printf("\"init\": null\n");
            }
            break;

        case AST_Identifier:
            print_indent(indent+2); printf("\"type\": "); json_escape_print("Identifier"); printf(",\n");
            print_indent(indent+2); printf("\"name\": ");
            json_escape_print(n->lexeme); printf("\n");
            break;

        case AST_BasicLit:
            print_indent(indent+2); printf("\"type\": "); json_escape_print("BasicLit"); printf(",\n");
            /* 默认当作 INT */
            print_indent(indent+2); printf("\"kind\": ");
            json_escape_print("INT"); printf(",\n");
            print_indent(indent+2); printf("\"value\": ");
            json_escape_print(n->lexeme); printf(",\n");
            print_indent(indent+2); printf("\"raw\": ");
            json_escape_print(n->lexeme); printf("\n");
            break;

        case AST_FunctionDecl:
            print_indent(indent+2); printf("\"name\": ");
            json_escape_print(n->type_name); printf(",\n");
            print_indent(indent+2); printf("\"body\": \n");
            if (n->child_count>0) print_json(n->children[0], indent+4); else { print_indent(indent+4); printf("null\n"); }
            break;

        case AST_MainDecl:
            print_indent(indent+2); printf("\"body\": \n");
            if (n->child_count>0) print_json(n->children[0], indent+4); else { print_indent(indent+4); printf("null\n"); }
            break;

        case AST_Block:
            print_indent(indent+2); printf("\"statements\": [\n");
            for (int i=0;i<n->child_count;i++){
                print_json(n->children[i], indent+4);
                if (i+1 < n->child_count) printf(",\n"); else printf("\n");
            }
            print_indent(indent+2); printf("]\n");
            break;

        case AST_Assign:
            print_indent(indent+2); printf("\"left\": \n");
            print_json(n->children[0], indent+4); printf(",\n");
            print_indent(indent+2); printf("\"right\": \n");
            print_json(n->children[1], indent+4); printf("\n");
            break;

        case AST_BinaryExpr:
            print_indent(indent+2); printf("\"operator\": ");
            json_escape_print(n->type_name); printf(",\n");
            print_indent(indent+2); printf("\"left\": \n");
            print_json(n->children[0], indent+4); printf(",\n");
            print_indent(indent+2); printf("\"right\": \n");
            print_json(n->children[1], indent+4); printf("\n");
            break;

        case AST_Read:
            print_indent(indent+2); printf("\"id\": \n");
            print_json(n->children[0], indent+4); printf("\n");
            break;

        case AST_Write:
            print_indent(indent+2); printf("\"expr\": \n");
            print_json(n->children[0], indent+4); printf("\n");
            break;

        case AST_If:
            print_indent(indent+2); printf("\"cond\": \n");
            print_json(n->children[0], indent+4); printf(",\n");
            print_indent(indent+2); printf("\"then\": \n");
            print_json(n->children[1], indent+4);
            if (n->child_count > 2) { printf(",\n"); print_indent(indent+2); printf("\"else\": \n"); print_json(n->children[2], indent+4); printf("\n"); }
            else printf("\n");
            break;

        case AST_While:
            print_indent(indent+2); printf("\"cond\": \n");
            print_json(n->children[0], indent+4); printf(",\n");
            print_indent(indent+2); printf("\"body\": \n");
            print_json(n->children[1], indent+4); printf("\n");
            break;

        case AST_For:
            print_indent(indent+2); printf("\"id\": \n");
            print_json(n->children[0], indent+4); printf(",\n");
            print_indent(indent+2); printf("\"range\": \n");
            print_json(n->children[1], indent+4); printf(",\n");
            print_indent(indent+2); printf("\"body\": \n");
            print_json(n->children[2], indent+4); printf("\n");
            break;

        case AST_Call:
            print_indent(indent+2); printf("\"callee\": ");
            json_escape_print(n->type_name); printf("\n");
            break;

        case AST_ExpressionStmt:
            print_indent(indent+2); printf("\"expression\": \n");
            print_json(n->children[0], indent+4); printf("\n");
            break;

        case AST_Program:
            print_indent(indent+2); printf("\"body\": [\n");
            for (int i=0;i<n->child_count;i++){
                print_json(n->children[i], indent+4);
                if (i+1 < n->child_count) printf(",\n"); else printf("\n");
            }
            print_indent(indent+2); printf("]\n");
            break;

        default:
            /* generic fallback: print children array */
            print_indent(indent+2); printf("\"children\": [\n");
            for (int i=0;i<n->child_count;i++){
                print_json(n->children[i], indent+4);
                if (i+1 < n->child_count) printf(",\n"); else printf("\n");
            }
            print_indent(indent+2); printf("]\n");
            break;
    }

    print_indent(indent); printf("}");
}

/* ---------- 解析函数原型（返回 ASTNode*） ---------- */
ASTNode* parse_program();
ASTNode* parse_fun_declaration();
ASTNode* parse_main_declaration();
ASTNode* parse_function_body();
ASTNode* parse_declaration_list();
ASTNode* parse_declaration_stat();
ASTNode* parse_statement_list();
ASTNode* parse_statement();
ASTNode* parse_compound_stat();
ASTNode* parse_expression_stat();
ASTNode* parse_expression();
ASTNode* parse_bool_expr();
ASTNode* parse_additive_expr();
ASTNode* parse_term();
ASTNode* parse_factor();
ASTNode* parse_if_stat();
ASTNode* parse_while_stat();
ASTNode* parse_for_stat();
ASTNode* parse_write_stat();
ASTNode* parse_read_stat();
ASTNode* parse_call_stat();

/* ---------- 错误处理（简化：打印信息并 exit） ---------- */
void error_line(const char *msg) {
    /* 词法流中没有行号，所以直接打印 msg（保持老师风格） */
    printf("%s\n", msg);
    exit(1);
}
void error_fmt(const char *fmt, const char *arg) {
    if (arg) {
        char buf[512];
        snprintf(buf, sizeof(buf), fmt, arg);
        error_line(buf);
    } else error_line(fmt);
}

/* ---------- 解析实现（基于 PDF 与你的框架语法） ---------- */

/* <program> -> { fun_declaration } main_declaration */
ASTNode* parse_program() {
    ASTNode *prog = new_node(AST_Program);
    /* read first token before loop */
    if (next_token() != 0) {
        error_line("输入为空或文件格式错误。");
    }

    /* parse zero or more function declarations starting with "function" or "func" */
    while (strcmp(token, "function") == 0 || strcmp(token, "func") == 0) {
        /* read next token which should be ID */
        if (next_token() != 0) error_line("函数定义缺少标识符。");
        ASTNode *f = parse_fun_declaration();
        if (!f) return NULL;
        add_child(prog, f);
        /* token already advanced by fun_declaration to next */
    }

    /* now expect main function: original framework sometimes had token == "ID" and token1 == "main" */
    if (strcmp(token, "ID") == 0 && strcmp(token1, "main") == 0) {
        if (next_token() != 0) error_line("main 后缺少 ( 。");
        ASTNode *m = parse_main_declaration();
        if (!m) return NULL;
        add_child(prog, m);
    } else if (strcmp(token, "main") == 0) {
        /* some token streams might give keyword directly */
        if (next_token() != 0) error_line("main 后缺少 ( 。");
        ASTNode *m = parse_main_declaration();
        if (!m) return NULL;
        add_child(prog, m);
    } else {
        error_line("第1行：缺少 main 函数或 main 名称错误。");
    }

    return prog;
}

/* <fun_declaration> -> function ID '(' ')' function_body
   note: token currently points to token after function/func (we called next_token already)
*/
ASTNode* parse_fun_declaration() {
    /* current token should be ID with function name */
    if (strcmp(token, "ID") != 0) {
        error_line("函数定义缺少标识符。");
    }
    ASTNode *fname = new_identifier(token1);
    /* advance */
    if (next_token() != 0) error_line("函数定义缺少 '(' 。");

    if (strcmp(token, "(") != 0 && strcmp(token, "LP") != 0) {
        error_line("缺少'('。");
    }
    if (next_token() != 0) error_line("函数缺少 ')'");
    if (strcmp(token, ")") != 0 && strcmp(token, "RP") != 0) {
        error_line("缺少')'。");
    }
    /* advance to next token (start of function body) */
    if (next_token() != 0) error_line("函数缺少函数体。");

    ASTNode *fnode = new_function_decl(fname->lexeme);
    ASTNode *body = parse_function_body();
    add_child(fnode, body);
    /* after parse_function_body, token points to next token after '}' */
    return fnode;
}

/* <main_declaration> -> main '(' ')' function_body */
ASTNode* parse_main_declaration() {
    /* token currently at token after '(' ) reading managed by caller: we called next_token prior */
    if (strcmp(token, "(") != 0 && strcmp(token, "LP") != 0) {
        error_line("缺少'('。");
    }
    if (next_token() != 0) error_line("main 函数缺少 ')'");
    if (strcmp(token, ")") != 0 && strcmp(token, "RP") != 0) {
        error_line("缺少')'。");
    }
    if (next_token() != 0) error_line("main 函数缺少函数体。");
    ASTNode *m = new_main_decl();
    ASTNode *body = parse_function_body();
    add_child(m, body);
    return m;
}

/* <function_body> -> '{' declaration_list statement_list '}' */
ASTNode* parse_function_body() {
    if (!(strcmp(token, "{") == 0 || strcmp(token, "LC") == 0)) {
        error_line("函数体缺少 '{'。");
    }
    /* advance into body */
    if (next_token() != 0) error_line("函数体格式错误。");
    ASTNode *block = new_block();

    ASTNode *decls = parse_declaration_list();
    if (decls) {
        /* declaration_list returns a block of var decls as children */
        for (int i=0;i<decls->child_count;i++) add_child(block, decls->children[i]);
        free(decls); // temporary container
    }

    ASTNode *stmts = parse_statement_list();
    if (stmts) {
        for (int i=0;i<stmts->child_count;i++) add_child(block, stmts->children[i]);
        free(stmts);
    }

    /* current token should be '}' */
    if (!(strcmp(token, "}") == 0 || strcmp(token, "RC") == 0)) {
        error_line("函数体缺少 '}'。");
    }
    /* advance after '}' */
    next_token();
    return block;
}

/* <declaration_list> -> { declaration_stat } */
ASTNode* parse_declaration_list() {
    ASTNode *container = new_node(AST_Empty);
    while (strcmp(token, "var") == 0 || strcmp(token, "VAR") == 0) {
        ASTNode *d = parse_declaration_stat();
        if (d) add_child(container, d);
        /* token advanced by declaration_stat */
    }
    return container;
}

/* <declaration_stat> -> var ID : int  (or var ID : type) */
ASTNode* parse_declaration_stat() {
    /* token == "var" */
    if (!(strcmp(token, "var") == 0 || strcmp(token, "VAR") == 0)) {
        error_line("declaration_stat 预期 var。");
    }
    /* read next token -> should be ID */
    if (next_token() != 0) error_line("var 后缺少标识符。");
    if (strcmp(token, "ID") != 0) {
        error_line("第1行：缺少标识符。");
    }
    ASTNode *id = new_identifier(token1);
    if (next_token() != 0) error_line("var 声明格式错误，缺少 ':' 或类型。");
    if (!(strcmp(token, ":") == 0 || strcmp(token, "COLON") == 0)) {
        error_line("var 声明缺少 ':'。");
    }
    if (next_token() != 0) error_line("var 声明缺少类型。");
    /* assume type token is 'int' or 'TYPE' */
    char vtype[64] = "int";
    if (strcmp(token, "int") == 0 || strcmp(token, "TYPE") == 0 || strcmp(token, "INTTYPE") == 0) {
        strncpy(vtype, token1, sizeof(vtype)-1);
    } else {
        /* Accept token1 as type if provided */
        strncpy(vtype, token1, sizeof(vtype)-1);
    }
    /* optionally there can be '= NUM' initialization or a semicolon */
    /* peek next */
    if (next_token() != 0) error_line("var 声明缺少结尾。");
    ASTNode *init = NULL;
    if (strcmp(token, "=") == 0) {
        if (next_token() != 0) error_line("var 初始化缺少表达式值。");
        if (strcmp(token, "NUM") == 0 || strcmp(token, "INT") == 0) {
            init = new_basiclit(token1);
            if (next_token() != 0) error_line("var 初始化缺少分号。");
            if (strcmp(token, ";") != 0 && strcmp(token, "SEMI") != 0) error_line("缺少分号。");
        } else {
            error_line("var 初始化目前只支持整数字面量。");
        }
    } else {
        /* token should be ; */
        if (strcmp(token, ";") != 0 && strcmp(token, "SEMI") != 0) {
            error_line("缺少分号。");
        }
    }
    /* construct nodes */
    ASTNode *vdecl = new_vardecl("var");
    ASTNode *vdtor = new_vardeclarator(id, init);
    add_child(vdecl, vdtor);
    /* advance after semicolon: after we got ';' token, move next */
    next_token();
    return vdecl;
}

/* <statement_list> -> { statement } until '}' */
ASTNode* parse_statement_list() {
    ASTNode *container = new_node(AST_Empty);
    while (!(strcmp(token, "}") == 0 || strcmp(token, "RC") == 0 || strcmp(token, "EOF") == 0)) {
        ASTNode *s = parse_statement();
        if (s) add_child(container, s);
    }
    return container;
}

/* <statement> -> if | while | for | read | write | compound | call | expression_stat */
ASTNode* parse_statement() {
    if (strcmp(token, "if") == 0) return parse_if_stat();
    if (strcmp(token, "while") == 0) return parse_while_stat();
    if (strcmp(token, "for") == 0) return parse_for_stat();
    if (strcmp(token, "read") == 0) return parse_read_stat();
    if (strcmp(token, "write") == 0) return parse_write_stat();
    if (strcmp(token, "{") == 0 || strcmp(token, "LC") == 0) return parse_compound_stat();
    if (strcmp(token, "call") == 0) return parse_call_stat();
    /* expression or empty ; */
    return parse_expression_stat();
}

/* compound: '{' statement_list '}' */
ASTNode* parse_compound_stat() {
    if (!(strcmp(token, "{") == 0 || strcmp(token, "LC") == 0)) {
        error_line("复合语句应以 '{' 开始。");
    }
    /* advance into block */
    if (next_token() != 0) error_line("复合语句格式错误。");
    ASTNode *block = new_block();
    ASTNode *stmts = parse_statement_list();
    if (stmts) {
        for (int i=0;i<stmts->child_count;i++) add_child(block, stmts->children[i]);
        free(stmts);
    }
    if (!(strcmp(token, "}") == 0 || strcmp(token, "RC") == 0)) error_line("缺少 '}'。");
    next_token(); /* consume '}' */
    return block;
}

/* expression_stat -> expression ';' | ';' */
ASTNode* parse_expression_stat() {
    if (strcmp(token, ";") == 0 || strcmp(token, "SEMI") == 0) {
        next_token();
        return new_node(AST_Empty);
    }
    ASTNode *expr = parse_expression();
    if (!expr) error_line("表达式解析失败。");
    if (!(strcmp(token, ";") == 0 || strcmp(token, "SEMI") == 0)) error_line("缺少分号。");
    next_token();
    return new_exprstmt(expr);
}

/* expression -> ID = bool_expr | bool_expr */
ASTNode* parse_expression() {
    /* if token == ID and next token is '=' -> assignment */
    if (strcmp(token, "ID") == 0) {
        /* peek next token by reading then potentially rolling back is complex with file-based stream;
           but original framework uses sequential reads; to emulate peek we can read next_token into temporaries
           and use a small buffer file pointer trick: here simpler approach: we rely on token stream tokens where '=' is token itself.
           So current token == "ID", and token1 contains lexeme. We'll read next token and check if it's '='.
        */
        char savedTok[TOKLEN], savedLex[LEXLEN];
        strncpy(savedTok, token, sizeof(savedTok));
        strncpy(savedLex, token1, sizeof(savedLex));
        if (next_token() != 0) {
            error_line("表达式后出现 EOF。");
        }
        if (strcmp(token, "=") == 0) {
            /* assignment: left = ID(savedLex) ; parse rhs */
            ASTNode *left = new_identifier(savedLex);
            if (next_token() != 0) error_line("赋值缺少右侧表达式。");
            ASTNode *right = parse_bool_expr();
            if (!right) error_line("赋值右侧表达式解析失败。");
            return new_assign(left, right);
        } else {
            /* not an assignment, treat saved ID as start of bool_expr: we need to rebuild stream state:
               we have already consumed an extra token (current token holds the token after the ID),
               but parse_bool_expr expects current token to be that token (so it's fine). We need to create a node for ID
               and pass control to parse_bool_expr with that weaved in. Simpler: create an Identifier node for savedLex,
               then treat grammar as starting from that token: We'll create an initial factor that returns that ID, and
               then let bool_expr continue parsing using current token (which is already the next token).
            */
            /* create an AST node representing the identifier literal for leftmost factor, and then call parse_additive_expr
               but our parse_bool_expr expects to start parsing an additive_expr possibly starting from current token.
               To integrate smoothly, we'll implement parse_bool_expr and lower-level functions to accept that current token may already
               be set to the subsequent token; since saved ID must be used as the first part of expression, we construct an AST representing that.
               Easiest workaround: create a Binary expression if next tokens indicate operator, else return the ID as BasicExpr.
            */
            /* Check if current token is relational operator, arithmetic operator, etc. We'll treat simple case: if current token is operator
               handled by additive_expr ( + - * / ( ) NUM ID ), then we'll synthesize an AST where left is Identifier(savedLex) and let additive/term/factor
               code use current token for the rest.
            */
            ASTNode *leftNode = new_identifier(savedLex);
            /* Now current token points to token after ID; call parse_bool_expr but provide leftNode as starting left.
               To avoid refactoring many functions, a practical approach: we manually handle simple cases: if current token is one of + - * / > < etc,
               call parse_additive_expr_by_left(leftNode), else if it's ';' or ')' or '}' or ',' or EOF, just return leftNode.
            */
            /* If token is operator that begins additive_expr */
            if (strcmp(token, "+")==0 || strcmp(token, "-")==0 || strcmp(token, "*")==0 || strcmp(token, "/")==0
                || strcmp(token, ">")==0 || strcmp(token, "<")==0 || strcmp(token, ">=")==0 || strcmp(token, "<=")==0
                || strcmp(token, "==")==0 || strcmp(token, "!=")==0) {
                /* we will call parse_additive_expr by creating a small wrapper: put leftNode into parsing pipeline */

                /* implement a simple loop: parse multiplicative and additive using leftNode as left */
                /* For simplicity implement single-level: while token is + or -, consume operator and parse term to build binary tree */
                ASTNode *curLeft = leftNode;
                while (strcmp(token, "+")==0 || strcmp(token, "-")==0 || strcmp(token, "*")==0 || strcmp(token, "/")==0) {
                    char opbuf[8];
                    strncpy(opbuf, token, sizeof(opbuf)-1);
                    if (next_token() != 0) error_line("运算符后缺少操作数。");
                    ASTNode *right = NULL;
                    /* right could be ID or NUM or '(' */
                    if (strcmp(token, "NUM")==0 || strcmp(token, "INT")==0) {
                        right = new_basiclit(token1);
                        next_token();
                    } else if (strcmp(token, "ID")==0) {
                        right = new_identifier(token1);
                        next_token();
                    } else if (strcmp(token, "(")==0) {
                        /* parse parenthesized expression */
                        next_token();
                        right = parse_bool_expr();
                        if (!(strcmp(token, ")")==0 || strcmp(token, "RP")==0)) error_line("缺少 ')'.");
                        next_token();
                    } else {
                        error_line("运算符后缺少操作数。");
                    }
                    curLeft = new_binary(opbuf, curLeft, right);
                }
                /* after building, check relational operators */
                if (strcmp(token, ">")==0 || strcmp(token, "<")==0 || strcmp(token, ">=")==0 || strcmp(token, "<=")==0
                    || strcmp(token, "==")==0 || strcmp(token, "!=")==0) {
                    char rel[8];
                    strncpy(rel, token, sizeof(rel)-1);
                    if (next_token() != 0) error_line("关系运算符后缺少操作数。");
                    ASTNode *r2 = NULL;
                    if (strcmp(token, "NUM")==0) { r2 = new_basiclit(token1); next_token(); }
                    else if (strcmp(token, "ID")==0) { r2 = new_identifier(token1); next_token(); }
                    else if (strcmp(token, "(")==0) { next_token(); r2 = parse_bool_expr(); if (!(strcmp(token, ")")==0)) error_line("缺少 )"); next_token(); }
                    else error_line("关系运算符后缺少操作数。");
                    curLeft = new_binary(rel, curLeft, r2);
                }
                return curLeft;
            } else {
                /* no operator, just return the ID node and keep current token as-is (it already points to next) */
                return leftNode;
            }
        }
    } else {
        /* not starting with ID => parse bool_expr directly */
        return parse_bool_expr();
    }
}

/* bool_expr -> additive_expr ( (relop) additive_expr )? */
ASTNode* parse_bool_expr() {
    ASTNode *left = parse_additive_expr();
    if (!left) return NULL;
    /* check relational operator */
    if (strcmp(token, ">")==0 || strcmp(token, "<")==0 || strcmp(token, ">=")==0 || strcmp(token, "<=")==0
        || strcmp(token, "==")==0 || strcmp(token, "!=")==0) {
        char rel[8]; strncpy(rel, token, sizeof(rel)-1);
        if (next_token() != 0) error_line("关系运算符后缺少操作数。");
        ASTNode *right = parse_additive_expr();
        if (!right) error_line("关系表达式右侧解析失败。");
        return new_binary(rel, left, right);
    }
    return left;
}

/* additive_expr -> term { (+|-) term } */
ASTNode* parse_additive_expr() {
    ASTNode *left = parse_term();
    if (!left) return NULL;
    while (strcmp(token, "+")==0 || strcmp(token, "-")==0) {
        char op[4]; strncpy(op, token, sizeof(op)-1);
        if (next_token() != 0) error_line("运算符后缺少操作数。");
        ASTNode *right = parse_term();
        if (!right) error_line("加减法右侧解析错误。");
        left = new_binary(op, left, right);
    }
    return left;
}

/* term -> factor { (*|/) factor } */
ASTNode* parse_term() {
    ASTNode *left = parse_factor();
    if (!left) return NULL;
    while (strcmp(token, "*")==0 || strcmp(token, "/")==0) {
        char op[4]; strncpy(op, token, sizeof(op)-1);
        if (next_token() != 0) error_line("运算符后缺少操作数。");
        ASTNode *right = parse_factor();
        if (!right) error_line("乘除法右侧解析错误。");
        left = new_binary(op, left, right);
    }
    return left;
}

/* factor -> '(' additive_expr ')' | ID | NUM */
ASTNode* parse_factor() {
    if (strcmp(token, "(")==0) {
        if (next_token() != 0) error_line("括号内表达式错误。");
        ASTNode *expr = parse_additive_expr();
        if (!(strcmp(token, ")")==0 || strcmp(token, "RP")==0)) error_line("缺少 ')'。");
        next_token();
        return expr;
    } else if (strcmp(token, "NUM")==0 || strcmp(token, "INT")==0) {
        ASTNode *lit = new_basiclit(token1);
        next_token();
        return lit;
    } else if (strcmp(token, "ID")==0) {
        ASTNode *id = new_identifier(token1);
        next_token();
        return id;
    } else {
        error_line("因子不是 ID、NUM 或括号表达式。");
    }
    return NULL;
}

/* if '(' expr ')' statement [ else statement ] */
ASTNode* parse_if_stat() {
    if (strcmp(token, "if") != 0) error_line("if 语句语法错误。");
    if (next_token() != 0) error_line("if 后缺少 '('。");
    if (!(strcmp(token, "(")==0 || strcmp(token, "LP")==0)) error_line("if 缺少 '('。");
    if (next_token() != 0) error_line("if 条件缺失。");
    ASTNode *cond = parse_expression();
    if (!(strcmp(token, ")")==0 || strcmp(token, "RP")==0)) error_line("if 缺少 ')'。");
    if (next_token() != 0) error_line("if 后缺少语句体。");
    ASTNode *thenb = parse_statement();
    ASTNode *elseb = NULL;
    if (strcmp(token, "else")==0) {
        if (next_token() != 0) error_line("else 后缺少语句体。");
        elseb = parse_statement();
    }
    return new_if(cond, thenb, elseb);
}

/* while '(' expr ')' statement */
ASTNode* parse_while_stat() {
    if (strcmp(token, "while") != 0) error_line("while 语句语法错误。");
    if (next_token() != 0) error_line("while 后缺少 '('。");
    if (!(strcmp(token, "(")==0 || strcmp(token, "LP")==0)) error_line("while 缺少 '('。");
    if (next_token() != 0) error_line("while 条件缺失。");
    ASTNode *cond = parse_expression();
    if (!(strcmp(token, ")")==0 || strcmp(token, "RP")==0)) error_line("while 缺少 ')'。");
    if (next_token() != 0) error_line("while 后缺少语句体。");
    ASTNode *body = parse_statement();
    return new_while(cond, body);
}

/* for '(' ID in expr ')' statement
   Note: PDF shows form "for ( i in 1..n+1 ) stmt"  ? 本实现将 "in" 和 range 表达式作为二元组合解析，
   简化处理：parse id, expect 'in', then parse expression as range expression (treat as expression node)
*/
ASTNode* parse_for_stat() {
    if (strcmp(token, "for") != 0) error_line("for 语句语法错误。");
    if (next_token() != 0) error_line("for 后缺少 '('。");
    if (!(strcmp(token, "(")==0 || strcmp(token, "LP")==0)) error_line("for 缺少 '('。");
    if (next_token() != 0) error_line("for 括号内缺少内容。");
    if (strcmp(token, "ID") != 0) error_line("for 语句期望 ID。");
    ASTNode *id = new_identifier(token1);
    if (next_token() != 0) error_line("for 语句缺少 in。");
    if (strcmp(token, "in") != 0) error_line("for 语句缺少 in。");
    if (next_token() != 0) error_line("for in 后缺少范围表达式。");
    ASTNode *range = parse_expression();
    if (!(strcmp(token, ")")==0 || strcmp(token, "RP")==0)) error_line("for 缺少 ')'.");
    if (next_token() != 0) error_line("for 后缺少语句体。");
    ASTNode *body = parse_statement();
    return new_for(id, range, body);
}

/* write expr ; */
ASTNode* parse_write_stat() {
    if (strcmp(token, "write") != 0) error_line("write 语句语法错误。");
    if (next_token() != 0) error_line("write 后缺少表达式。");
    ASTNode *expr = parse_expression();
    if (!(strcmp(token, ";")==0 || strcmp(token, "SEMI")==0)) error_line("write 缺少分号。");
    next_token();
    return new_write(expr);
}

/* read ID ; */
ASTNode* parse_read_stat() {
    if (strcmp(token, "read") != 0) error_line("read 语句语法错误。");
    if (next_token() != 0) error_line("read 后缺少标识符。");
    if (strcmp(token, "ID") != 0) error_line("read 后应跟标识符。");
    ASTNode *id = new_identifier(token1);
    if (next_token() != 0) error_line("read 语句缺少分号。");
    if (strcmp(token, ";") != 0 && strcmp(token, "SEMI") != 0) error_line("read 缺少分号。");
    next_token();
    return new_read(id);
}

/* call ID ( ) ;  or call ID ; (simple) */
ASTNode* parse_call_stat() {
    if (strcmp(token, "call") != 0) error_line("call 语句语法错误。");
    if (next_token() != 0) error_line("call 后缺少标识符。");
    if (strcmp(token, "ID") != 0) error_line("call 后应跟标识符。");
    char fname[128]; strncpy(fname, token1, sizeof(fname)-1);
    if (next_token() != 0) error_line("call 后缺少 '(' 或分号。");
    if (strcmp(token, "(")==0 || strcmp(token, "LP")==0) {
        if (next_token() != 0) error_line("call 缺少 ')'。");
        if (strcmp(token, ")") != 0 && strcmp(token, "RP") != 0) error_line("call 缺少 ')'。");
        if (next_token() != 0) error_line("call 缺少分号。");
        if (strcmp(token, ";") != 0 && strcmp(token, "SEMI") != 0) error_line("call 缺少分号。");
        next_token();
    } else if (strcmp(token, ";")==0 || strcmp(token, "SEMI")==0) {
        next_token();
    } else {
        error_line("call 语句格式错误。");
    }
    ASTNode *c = new_call(fname);
    return c;
}

/* ---------- main ---------- */
int main(int argc, char **argv) {
    char filename[260];
    printf("请输入单词流文件名（包括路径）：");
    if (scanf("%s", filename) != 1) {
        printf("未输入文件名。\n");
        return 1;
    }
    fpTokenin = fopen(filename, "r");
    if (!fpTokenin) {
        printf("打开 %s 失败！\n", filename);
        return 1;
    }

    ASTNode *root = parse_program();
    if (!root) {
        printf("解析失败或返回空 AST。\n");
        fclose(fpTokenin);
        return 1;
    }

    /* 输出 AST JSON */
    print_json(root, 0);
    printf("\n");

    fclose(fpTokenin);
    return 0;
}
