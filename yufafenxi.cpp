/* TESTparse_AST_only.c
   语法分析器（只生成抽象语法树 JSON 输出）
   改进版：支持 var/类型/初始化、更新操作、比较符号
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_CHILDREN 16

typedef struct ASTNode {
    char *type;
    char *name;
    char *raw;
    struct ASTNode **children;
    int child_count;
} ASTNode;

char token[64], token1[256];
FILE *fpTokenin;

ASTNode* new_node(const char *type) {
    ASTNode *n = (ASTNode*)malloc(sizeof(ASTNode));
    n->type = strdup(type);
    n->name = NULL;
    n->raw = NULL;
    n->children = (ASTNode**)calloc(MAX_CHILDREN, sizeof(ASTNode*));
    n->child_count = 0;
    return n;
}

void set_node_name(ASTNode *n, const char *name) {
    if(!n) return;
    if(n->name) free(n->name);
    n->name = name ? strdup(name) : NULL;
}
void set_node_raw(ASTNode *n, const char *raw) {
    if(!n) return;
    if(n->raw) free(n->raw);
    n->raw = raw ? strdup(raw) : NULL;
}

void add_child(ASTNode *parent, ASTNode *child) {
    if(!parent || !child) return;
    if(parent->child_count < MAX_CHILDREN) {
        parent->children[parent->child_count++] = child;
    } else {
        int old = parent->child_count;
        int newcap = old + MAX_CHILDREN;
        parent->children = (ASTNode**)realloc(parent->children, sizeof(ASTNode*)*newcap);
        parent->children[parent->child_count++] = child;
    }
}

int nextToken() {
    if(fpTokenin == NULL) return EOF;
    if(fscanf(fpTokenin, "%63s %255s", token, token1) == 2) return 0;
    return EOF;
}

void print_json_string(const char *s) {
    if(!s) { printf("null"); return; }
    putchar('"');
    for(const char *p=s;*p;p++){
        switch(*p){
            case '\\': printf("\\\\"); break;
            case '"': printf("\\\""); break;
            case '\n': printf("\\n"); break;
            case '\r': printf("\\r"); break;
            case '\t': printf("\\t"); break;
            default: putchar(*p);
        }
    }
    putchar('"');
}

void print_indent(int indent){for(int i=0;i<indent;i++) putchar(' ');}
void print_ast_json(ASTNode *n, int indent) {
    if(!n) { print_indent(indent); printf("null\n"); return; }
    print_indent(indent); printf("{\n");

    // type
    print_indent(indent+2); printf("\"type\": "); print_json_string(n->type); printf(",\n");

    // 对二元表达式或更新表达式特殊处理
    if(strcmp(n->type, "BinaryExpression") == 0 || strcmp(n->type, "AssignmentExpression") == 0) {
        if(n->name) { print_indent(indent+2); printf("\"operator\": "); print_json_string(n->name); printf(",\n"); }
        if(n->child_count >= 1) { print_indent(indent+2); printf("\"left\": "); print_ast_json(n->children[0], indent+2); printf(",\n"); }
        if(n->child_count >= 2) { print_indent(indent+2); printf("\"right\": "); print_ast_json(n->children[1], indent+2); printf("\n"); }
    } else if(strcmp(n->type, "UpdateExpression") == 0) {
        if(n->name) { print_indent(indent+2); printf("\"operator\": "); print_json_string(n->name); printf(",\n"); }
        if(n->child_count >= 1) { print_indent(indent+2); printf("\"argument\": "); print_ast_json(n->children[0], indent+2); printf("\n"); }
    } else {
        if(n->name) { print_indent(indent+2); printf("\"name\": "); print_json_string(n->name); printf(",\n"); }
        if(n->raw) { print_indent(indent+2); printf("\"raw\": "); print_json_string(n->raw); printf(",\n"); }
        // children
        print_indent(indent+2); printf("\"children\": [\n");
        for(int i=0;i<n->child_count;i++) {
            print_ast_json(n->children[i], indent+4);
            if(i < n->child_count-1) printf(",\n"); else printf("\n");
        }
        print_indent(indent+2); printf("]\n");
    }

    print_indent(indent); printf("}");
}


// 前向声明
ASTNode* parse_program();
ASTNode* parse_fun_declaration();
ASTNode* parse_main_declaration();
ASTNode* parse_function_body();
ASTNode* parse_declaration_list();
ASTNode* parse_declaration_stat();
ASTNode* parse_statement_list();
ASTNode* parse_statement();
ASTNode* parse_if_stat();
ASTNode* parse_while_stat();
ASTNode* parse_for_stat();
ASTNode* parse_write_stat();
ASTNode* parse_read_stat();
ASTNode* parse_compound_stat();
ASTNode* parse_call_stat();
ASTNode* parse_expression_stat();
ASTNode* parse_expression();
ASTNode* parse_bool_expr();
ASTNode* parse_additive_expr();
ASTNode* parse_term();
ASTNode* parse_factor();

// program -> { fun_declaration } main_declaration
ASTNode* parse_program(){
    ASTNode *node = new_node("Program");
    while(strcmp(token,"function")==0){
        ASTNode *f = parse_fun_declaration();
        add_child(node,f);
    }
    if(strcmp(token,"ID")==0 && strcmp(token1,"main")==0){
        ASTNode *m = parse_main_declaration();
        add_child(node,m);
    } else {
        ASTNode *err = new_node("Error");
        set_node_name(err,token);
        add_child(node,err);
    }
    return node;
}

ASTNode* parse_fun_declaration(){
    ASTNode *node = new_node("FunctionDeclaration");
    nextToken(); // function -> ID
    if(strcmp(token,"ID")==0) set_node_name(node,token1);
    nextToken(); // (
    if(strcmp(token,"(")==0) nextToken();
    if(strcmp(token,")")==0) nextToken();
    ASTNode *body = parse_function_body();
    add_child(node,body);
    return node;
}

ASTNode* parse_main_declaration(){
    ASTNode *node = new_node("MainDeclaration");
    set_node_name(node,token1);
    nextToken(); // (
    if(strcmp(token,"(")==0) nextToken();
    if(strcmp(token,")")==0) nextToken();
    ASTNode *body = parse_function_body();
    add_child(node,body);
    return node;
}

ASTNode* parse_function_body(){
    ASTNode *node = new_node("FunctionBody");
    if(strcmp(token,"{")==0) nextToken();
    add_child(node,parse_declaration_list());
    add_child(node,parse_statement_list());
    if(strcmp(token,"}")==0) nextToken();
    return node;
}

ASTNode* parse_declaration_list(){
    ASTNode *node = new_node("DeclarationList");
    while(strcmp(token,"int")==0 || strcmp(token,"var")==0){
        add_child(node,parse_declaration_stat());
    }
    return node;
}

ASTNode* parse_declaration_stat(){
    ASTNode *node = new_node("VariableDeclaration");
    if(strcmp(token,"int")==0){
        set_node_name(node,"int");
        nextToken();
        if(strcmp(token,"ID")==0){
            ASTNode *decl = new_node("VariableDeclarator");
            ASTNode *idn = new_node("Identifier");
            set_node_name(idn,token1);
            add_child(decl,idn);
            add_child(node,decl);
            nextToken();
            if(strcmp(token,";")==0) nextToken();
        }
    } else if(strcmp(token,"var")==0){
        nextToken();
        if(strcmp(token,"ID")==0){
            ASTNode *decl = new_node("VariableDeclarator");
            ASTNode *idn = new_node("Identifier");
            set_node_name(idn,token1);
            add_child(decl,idn);
            nextToken();
            if(strcmp(token,":")==0){ nextToken(); if(strcmp(token,":")==0) nextToken(); }
            if(strcmp(token,"ID")==0){
                ASTNode *typeNode = new_node("TypeAnnotation");
                set_node_name(typeNode,token1);
                add_child(decl,typeNode);
                nextToken();
            }
            if(strcmp(token,"=")==0){ nextToken();
                if(strcmp(token,"NUM")==0 || strcmp(token,"ID")==0){
                    ASTNode *lit = new_node("Literal");
                    set_node_raw(lit,token1);
                    add_child(decl,lit);
                    nextToken();
                }
            }
            if(strcmp(token,";")==0) nextToken();
            add_child(node,decl);
        }
    }
    return node;
}

ASTNode* parse_statement_list(){
    ASTNode *node = new_node("StatementList");
    while(strcmp(token,"}")!=0 && feof(fpTokenin)==0){
        ASTNode *s = parse_statement();
        if(!s) break;
        add_child(node,s);
    }
    return node;
}

ASTNode* parse_statement(){
    if(strcmp(token,"if")==0) return parse_if_stat();
    if(strcmp(token,"while")==0) return parse_while_stat();
    if(strcmp(token,"for")==0) return parse_for_stat();
    if(strcmp(token,"read")==0) return parse_read_stat();
    if(strcmp(token,"write")==0) return parse_write_stat();
    if(strcmp(token,"{")==0) return parse_compound_stat();
    if(strcmp(token,"call")==0) return parse_call_stat();
    return parse_expression_stat();
}

ASTNode* parse_if_stat(){
    ASTNode *node = new_node("IfStatement");
    nextToken(); // if
    if(strcmp(token,"(")==0) nextToken();
    add_child(node,parse_bool_expr());
    if(strcmp(token,")")==0) nextToken();
    add_child(node,parse_statement());
    if(strcmp(token,"else")==0){ nextToken(); add_child(node,parse_statement()); }
    return node;
}

ASTNode* parse_while_stat(){
    ASTNode *node = new_node("WhileStatement");
    nextToken();
    if(strcmp(token,"(")==0) nextToken();
    add_child(node,parse_bool_expr());
    if(strcmp(token,")")==0) nextToken();
    add_child(node,parse_statement());
    return node;
}

ASTNode* parse_for_stat(){
    ASTNode *node = new_node("ForStatement");
    nextToken();
    if(strcmp(token,"(")==0) nextToken();
    add_child(node,parse_expression());
    if(strcmp(token,";")==0) nextToken();
    add_child(node,parse_expression());
    if(strcmp(token,";")==0) nextToken();
    add_child(node,parse_expression());
    if(strcmp(token,")")==0) nextToken();
    add_child(node,parse_statement());
    return node;
}

ASTNode* parse_write_stat(){
    ASTNode *node = new_node("WriteStatement");
    nextToken();
    add_child(node,parse_expression());
    if(strcmp(token,";")==0) nextToken();
    return node;
}

ASTNode* parse_read_stat(){
    ASTNode *node = new_node("ReadStatement");
    nextToken();
    if(strcmp(token,"ID")==0){
        ASTNode *idn = new_node("Identifier"); set_node_name(idn,token1); add_child(node,idn);
        nextToken();
    }
    if(strcmp(token,";")==0) nextToken();
    return node;
}

ASTNode* parse_compound_stat(){
    ASTNode *node = new_node("BlockStatement");
    if(strcmp(token,"{")==0) nextToken();
    add_child(node,parse_statement_list());
    if(strcmp(token,"}")==0) nextToken();
    return node;
}

ASTNode* parse_call_stat(){
    ASTNode *node = new_node("CallStatement");
    nextToken();
    if(strcmp(token,"ID")==0){
        ASTNode *idn = new_node("Identifier"); set_node_name(idn,token1); add_child(node,idn);
        nextToken();
    }
    if(strcmp(token,"(")==0) nextToken();
    if(strcmp(token,")")==0) nextToken();
    if(strcmp(token,";")==0) nextToken();
    return node;
}

ASTNode* parse_expression_stat(){
    if(strcmp(token,";")==0){ nextToken(); return new_node("EmptyStatement"); }
    ASTNode *expr = parse_expression();
    if(strcmp(token,";")==0) nextToken();
    ASTNode *node = new_node("ExpressionStatement"); add_child(node,expr);
    return node;
}

ASTNode* parse_expression(){
    if(strcmp(token,"ID")==0){
        char save_token[64], save_token1[256];
        strcpy(save_token,token); strcpy(save_token1,token1);
        nextToken();
        if(strcmp(token,"=")==0){
            ASTNode *node = new_node("AssignmentExpression");
            ASTNode *idn = new_node("Identifier"); set_node_name(idn,save_token1);
            add_child(node,idn);
            nextToken();
            add_child(node,parse_bool_expr());
            return node;
        } else {
            ASTNode *idn = new_node("Identifier"); set_node_name(idn,save_token1);
            return idn;
        }
    }
    return parse_bool_expr();
}

ASTNode* parse_bool_expr(){
    ASTNode *left = parse_additive_expr();
    if(left==NULL) return NULL;
    if(strcmp(token,">")==0 || strcmp(token,">=")==0 || strcmp(token,"<")==0 || strcmp(token,"<=")==0 || strcmp(token,"==")==0 || strcmp(token,"!=")==0){
        char op[4]; strcpy(op,token); nextToken();
        ASTNode *right = parse_additive_expr();
        ASTNode *node = new_node("BinaryExpression"); set_node_name(node,op);
        add_child(node,left); add_child(node,right);
        return node;
    }
    return left;
}

ASTNode* parse_additive_expr(){
    ASTNode *left = parse_term();
    while(strcmp(token,"+")==0 || strcmp(token,"-")==0){
        char op[4]; strcpy(op,token); nextToken();
        ASTNode *right = parse_term();
        ASTNode *node = new_node("BinaryExpression"); set_node_name(node,op);
        add_child(node,left); add_child(node,right);
        left=node;
    }
    return left;
}

ASTNode* parse_term(){
    ASTNode *left = parse_factor();
    while(strcmp(token,"*")==0 || strcmp(token,"/")==0){
        char op[4]; strcpy(op,token); nextToken();
        ASTNode *right = parse_factor();
        ASTNode *node = new_node("BinaryExpression"); set_node_name(node,op);
        add_child(node,left); add_child(node,right);
        left=node;
    }
    return left;
}

ASTNode* parse_factor(){
    if(strcmp(token,"(")==0){ nextToken(); ASTNode *e=parse_additive_expr(); if(strcmp(token,")")==0) nextToken(); return e; }
    if(strcmp(token,"ID")==0){
        ASTNode *idn = new_node("Identifier"); set_node_name(idn,token1); nextToken();
        if(strcmp(token,"++")==0 || strcmp(token,"--")==0){
            ASTNode *up = new_node("UpdateExpression"); set_node_name(up,token);
            add_child(up,idn); nextToken(); return up;
        }
        return idn;
    }
    if(strcmp(token,"NUM")==0){ ASTNode *lit = new_node("Literal"); set_node_raw(lit,token1); nextToken(); return lit; }
    ASTNode *err = new_node("Unknown"); set_node_name(err,token); nextToken(); return err;
}


int main() {
    fpTokenin = fopen("token", "r");
    if(!fpTokenin) {
        printf("无法打开文件\n");
        return 1;
    }

    if(nextToken() == EOF) {
        printf("文件为空！\n");
        return 1;
    }

    ASTNode *root = parse_program();

    print_ast_json(root, 0);
    printf("\n");

    fclose(fpTokenin);
    return 0;
}

