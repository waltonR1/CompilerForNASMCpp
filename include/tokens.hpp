#pragma once
#include <string>
#include <vector>
#include <stdexcept>

enum class TokenType {
    If,
    Else,
    While,
    Int,
    StringKw,
    Print,
    Prints,
    Assign,
    Comparison,
    Arth,
    L1,
    R1,
    L2,
    R2,
    Semicolon,
    Separator,
    Var,
    IntLit,
    String,
    End
};

struct Token {
    TokenType type{TokenType::End};
    std::string value;
    int line{0};
};

class TokenArray {
public:
    void push(const Token &t) { tokens.push_back(t); }

    [[nodiscard]] const Token &current() const {
        if (pos >= tokens.size())
            throw std::out_of_range("token pos");
        return tokens[pos];
    }

    void next() {
        if (pos + 1 < tokens.size())
            ++pos;
    }

    [[nodiscard]] bool empty() const { return tokens.empty(); }

    void appendEndIfMissing() {
        if (tokens.empty() || tokens.back().type != TokenType::End)
            tokens.push_back(Token{TokenType::End, "END", tokens.empty() ? 1 : tokens.back().line});
    }

    std::vector<Token> tokens;
    size_t pos{0};
};
