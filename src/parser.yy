/* 1. PREAMBLE */
%{
#include <iostream>
#include <memory>
#include <string>

#include "ast.hpp"     // Your AST node definitions
#include "tokens.hpp"  // Your Token struct and TokenType enum

// Forward declare the scanner function (defined in Flex)
extern int yylex();
extern int yylineno;

// Forward declare the error function
void yyerror(const char *s);

// This will hold the final, complete AST
std::shared_ptr<Node> g_ast_root;

static int node_line(const std::shared_ptr<Node> &node)
{
    if (!node)
        return yylineno;
    if (auto num = std::dynamic_pointer_cast<NumberNode>(node))
        return num->tok.line;
    if (auto id = std::dynamic_pointer_cast<IdentifierNode>(node))
        return id->tok.line;
    if (auto bin = std::dynamic_pointer_cast<BinOpNode>(node))
        return bin->op_tok.line;
    if (auto assign = std::dynamic_pointer_cast<Assignment>(node))
        return assign->identifier.line;
    return yylineno;
}
%}

/* 2. BISON DECLARATIONS */

// The %union defines all possible data types that can be
// associated with a token (terminal) or a grammar rule (non-terminal).
%code requires {
    #include <memory>
    #include <vector>
    #include "ast.hpp"
    #include "tokens.hpp"

    struct SemanticValue {
        std::shared_ptr<Node> node;
        std::shared_ptr<Condition> condition;
        std::vector<Token> token_list;
        Token token;
    };
}

%define api.value.type {SemanticValue}

// Tokens that carry additional data use yylval.token from the scanner.
%token T_INTLIT T_VAR T_COMPARISON T_STRING

// Define simple tokens (keywords, punctuation) that don't carry data
%token T_IF T_ELSE T_WHILE T_INT T_STRINGKW T_PRINT T_PRINTS
%token T_ASSIGN
%token T_LPAREN T_RPAREN T_LBRACE T_RBRACE T_SEMICOLON T_END

// Define operator precedence (lowest to highest) and associativity.
%left T_COMPARISON          /* 比较运算符（在 condition 里用，放最外层） */
%left '+' '-'               /* 加减 */
%left '*' '/'               /* 乘除 */

// The top-level rule to start parsing
%start program

%%
/* 3. GRAMMAR RULES */

// The 'program' rule waits for all statements and an END token or EOF.
// $1 refers to the value of 'statements'. $$ is the value of 'program'.
program
    : statements T_END
    {
        g_ast_root = $1.node; // Save the completed AST
    }
    | statements
    {
        g_ast_root = $1.node; // Save the completed AST (EOF case)
    }
    ;

// This rule translates your `statements()` logic [cite: 2]
statements
    : /* empty */
    {
        $$.node = nullptr; // Base case: no statements
    }
    | statements statement
    {
        auto st = std::make_shared<Statement>();
        st->left = $1.node;  // The previous statements
        st->right = $2.node; // The new statement
        $$.node = st;        // Pass the combined list up
    }
    ;

statement
    : if_statement    { $$.node = $1.node; }
    | while_statement { $$.node = $1.node; }
    | declarations    { $$.node = $1.node; }
    | assignment      { $$.node = $1.node; }
    | printing        { $$.node = $1.node; }
    | T_LBRACE statements T_RBRACE // For nested blocks
    {
        $$.node = $2.node;
    }
    ;

// --- Expression Parsing (Replaces expr, term, factor) ---
// Note how the precedence rules make this simple.

expr
    : term
    {
        $$.node = $1.node;
    }
    | expr '+' term  // $1 is 'expr', $2 is '+', $3 is 'term'
    {
        auto bin = std::make_shared<BinOpNode>();
        bin->left = $1.node;
        bin->op_tok = Token{TokenType::Arth, "+", node_line($1.node)};
        bin->right = $3.node;
        $$.node = bin; // $$ is the new 'expr' node
    }
    | expr '-' term
    {
        auto bin  = std::make_shared<BinOpNode>();
        bin->left = $1.node;
        bin->op_tok = Token{TokenType::Arth, "-", node_line($1.node)};
        bin->right  = $3.node;
        $$.node     = bin;
    }
    ; 

term
    : factor
    {
        $$.node = $1.node;
    }
    | term '*' factor
    {
        auto bin  = std::make_shared<BinOpNode>();
        bin->left = $1.node;
        bin->op_tok = Token{TokenType::Arth, "*", node_line($1.node)};
        bin->right  = $3.node;
        $$.node     = bin;
    }
    | term '/' factor
    {
        auto bin  = std::make_shared<BinOpNode>();
        bin->left = $1.node;
        bin->op_tok = Token{TokenType::Arth, "/", node_line($1.node)};
        bin->right  = $3.node;
        $$.node     = bin;
    }
    ;

factor
    : T_INTLIT
    {
        // $1 is the Token from the scanner (via %union.token)
        $$.node = std::make_shared<NumberNode>($1.token);
    }
    | T_VAR
    {
        $$.node = std::make_shared<IdentifierNode>($1.token);
    }
    | T_STRING
    {
        $$.node = std::make_shared<StringNode>($1.token);
    }
    | T_LPAREN expr T_RPAREN
    {
        $$.node = $2.node; // Pass the inner expression's node up
    }
    ;

// --- Other Statements ---

assignment
    : T_VAR T_ASSIGN expr T_SEMICOLON
    {
        auto asg = std::make_shared<Assignment>();
        asg->identifier = $1.token;
        asg->expression = $3.node;
        $$.node = asg;
    }
    ;

// --------- if / while / condition ---------

if_statement
    : T_IF T_LPAREN condition T_RPAREN T_LBRACE statements T_RBRACE
    {
        auto ifs = std::make_shared<IfStatement>();
        ifs->if_condition = $3.condition;
        ifs->if_body = $6.node;
        ifs->else_body = nullptr;
        $$.node = ifs;
    }
    | T_IF T_LPAREN condition T_RPAREN T_LBRACE statements T_RBRACE T_ELSE T_LBRACE statements T_RBRACE
    {
        auto ifs = std::make_shared<IfStatement>();
        ifs->if_condition = $3.condition;
        ifs->if_body      = $6.node;
        ifs->else_body    = $10.node;
        $$.node           = ifs;
    }
    ;

condition
    : expr T_COMPARISON expr
    {
        auto cond = std::make_shared<Condition>();
        cond->left_expression  = $1.node;
        cond->comparison       = $2.token;   // 包含 "==", "<", ">" 等
        cond->right_expression = $3.node;
        $$.condition = cond;
    }
    ;

while_statement
    : T_WHILE T_LPAREN condition T_RPAREN T_LBRACE statements T_RBRACE
    {
        auto w = std::make_shared<WhileStatement>();
        w->condition = $3.condition;
        w->body = $6.node;
        $$.node = w;
    }
    ;

// --------- print / prints ---------

printing
    : T_PRINT T_LPAREN expr T_RPAREN T_SEMICOLON
    {
        auto p = std::make_shared<PrintStatement>();
        p->type = "int";
        p->intExpr = $3.node;
        $$.node = p;
    }
    | T_PRINTS T_LPAREN T_STRING T_RPAREN T_SEMICOLON
    {
        auto p  = std::make_shared<PrintStatement>();
        p->type     = "string";
        p->strValue = $3.token.value;  // 字符串字面量内容
        $$.node     = p;
    }
    ;

// --------- 声明 ---------

declarations
    : T_INT identifier_list T_SEMICOLON
    {
        auto decl = std::make_shared<Declaration>();
        decl->declaration_type = Token{TokenType::Int, "int", yylineno};
        decl->identifiers      = $2.token_list;
        $$.node = decl;
    }
    | T_INT T_VAR T_ASSIGN expr T_SEMICOLON
    {
        // int x = expr; 视为「声明 + 赋值」组合成一个 Statement
        auto decl = std::make_shared<Declaration>();
        decl->declaration_type = Token{TokenType::Int, "int", yylineno};
        decl->identifiers      = std::vector<Token>{ $2.token };

        auto asg = std::make_shared<Assignment>();
        asg->identifier = $2.token;
        asg->expression = $4.node;

        auto st  = std::make_shared<Statement>();
        st->left  = decl;
        st->right = asg;

        $$.node = st;
    }
    | T_STRINGKW identifier_list T_SEMICOLON
    {
        auto decl = std::make_shared<Declaration>();
        decl->declaration_type = Token{TokenType::StringKw, "string", yylineno};
        decl->identifiers      = $2.token_list;
        $$.node = decl;
    }
   | T_STRINGKW T_VAR T_ASSIGN T_STRING T_SEMICOLON
   {
       auto decl = std::make_shared<Declaration>();
       decl->declaration_type = Token{TokenType::StringKw, "string", yylineno};
       decl->identifiers = { $2.token };

       auto asg = std::make_shared<Assignment>();
       asg->identifier = $2.token;
       asg->expression = std::make_shared<StringNode>($4.token);

       auto st = std::make_shared<Statement>();
       st->left = decl;
       st->right = asg;

       $$.node = st;
   }
    ;

identifier_list
    : T_VAR
    {
        $$.token_list = std::vector<Token>{ $1.token };
    }
    | identifier_list ',' T_VAR
    {
        auto list = $1.token_list;
        list.push_back($3.token);
        $$.token_list = list;
    }
    ;

%%
/* 4. EPILOGUE */

// Bison calls this function on a syntax error.
void yyerror(const char *s) {
    std::string error_msg = std::string(s) + " at line " + std::to_string(yylineno);
    throw std::runtime_error(error_msg);
}