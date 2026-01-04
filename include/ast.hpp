#pragma once
#include <memory>
#include <string>
#include <vector>
#include "tokens.hpp"

struct Node
{
    virtual ~Node() = default;
};

struct NumberNode : Node
{
    Token tok;
    explicit NumberNode(Token t) : tok(std::move(t)) {}
    std::string getValue() const { return tok.value; }
};

struct IdentifierNode : Node
{
    Token tok;
    explicit IdentifierNode(Token t) : tok(std::move(t)) {}
    std::string getValue() const { return tok.value; }
};

struct BinOpNode : Node
{
    std::shared_ptr<Node> left;
    Token op_tok;
    std::shared_ptr<Node> right;
};

struct Statement : Node
{
    std::shared_ptr<Node> left;
    std::shared_ptr<Node> right; // may be null
};

struct Condition : Node
{
    std::shared_ptr<Node> left_expression;
    Token comparison;
    std::shared_ptr<Node> right_expression;
};

struct IfStatement : Node
{
    std::shared_ptr<Condition> if_condition;
    std::shared_ptr<Node> if_body;
    std::shared_ptr<Node> else_body; // may be null
};

struct WhileStatement : Node
{
    std::shared_ptr<Condition> condition;
    std::shared_ptr<Node> body;
};

struct PrintStatement : Node
{
    std::string type;              // "string" or "int"
    std::shared_ptr<Node> intExpr; // used if type=="int"
    std::string strValue;          // used if type=="string"
};

struct Assignment : Node
{
    Token identifier;
    std::shared_ptr<Node> expression;
};

struct Declaration : Node
{
    Token declaration_type;         // token of type Int or StringKw
    std::vector<Token> identifiers; // VAR tokens
};

struct StringNode : Node
{
    Token tok;
    explicit StringNode(Token t) : tok(std::move(t)) {}
    std::string getValue() const { return tok.value; }
};

