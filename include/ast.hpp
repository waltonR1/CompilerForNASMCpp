#pragma once
#include <memory>
#include <string>
#include <vector>
#include "tokens.hpp"

struct Node {
    virtual ~Node() = default;
};

struct NumberNode final : Node {
    Token tok;

    explicit NumberNode(Token t) : tok(std::move(t)) {
    }

    [[nodiscard]] std::string getValue() const { return tok.value; }
};

struct IdentifierNode final : Node {
    Token tok;

    explicit IdentifierNode(Token t) : tok(std::move(t)) {
    }

    [[nodiscard]] std::string getValue() const { return tok.value; }
};

struct BinOpNode final : Node {
    std::shared_ptr<Node> left;
    Token op_tok;
    std::shared_ptr<Node> right;
};

struct Statement final : Node {
    std::shared_ptr<Node> left;
    std::shared_ptr<Node> right; // may be null
};

struct Condition final : Node {
    std::shared_ptr<Node> left_expression;
    Token comparison;
    std::shared_ptr<Node> right_expression;
};

struct IfStatement final : Node {
    std::shared_ptr<Condition> if_condition;
    std::shared_ptr<Node> if_body;
    std::shared_ptr<Node> else_body; // may be null
};

struct WhileStatement final : Node {
    std::shared_ptr<Condition> condition;
    std::shared_ptr<Node> body;
};

struct PrintStatement final : Node {
    std::string type; // "string" or "int"
    std::shared_ptr<Node> intExpr; // used if type=="int"
    std::string strValue; // used if type=="string"
};

struct Assignment final : Node {
    Token identifier;
    std::shared_ptr<Node> expression;
};

struct Declaration final : Node {
    Token declaration_type; // token of type Int or StringKw
    std::vector<Token> identifiers; // VAR tokens
};

struct StringNode final : Node {
    Token tok;

    explicit StringNode(Token t) : tok(std::move(t)) {
    }

    [[nodiscard]] std::string getValue() const { return tok.value; }
};
