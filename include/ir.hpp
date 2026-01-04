#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "ast.hpp"

enum class IRKind
{
    Assignment,
    Jump,
    Label,
    Compare,
    Print
};

struct IRInstr
{
    virtual ~IRInstr() = default;
    virtual IRKind kind() const = 0;
};

struct AssignmentCode : IRInstr
{
    std::string var;
    std::string left;
    std::string op;    // empty if none
    std::string right; // may be empty
    IRKind kind() const override { return IRKind::Assignment; }
};

struct JumpCode : IRInstr
{
    std::string dist;
    IRKind kind() const override { return IRKind::Jump; }
};

struct LabelCode : IRInstr
{
    std::string label;
    IRKind kind() const override { return IRKind::Label; }
};

struct CompareCodeIR : IRInstr
{
    std::string left;
    std::string operation;
    std::string right;
    std::string jump;
    IRKind kind() const override { return IRKind::Compare; }
};

struct PrintCodeIR : IRInstr
{
    std::string type; // "string" or "int"
    std::string value;
    IRKind kind() const override { return IRKind::Print; }
};

struct InterCodeArray
{
    std::vector<std::shared_ptr<IRInstr>> code;
    void append(const std::shared_ptr<IRInstr> &n) { code.push_back(n); }
};

struct GeneratedIR
{
    InterCodeArray code;
    std::unordered_map<std::string, std::string> identifiers;
    std::unordered_map<std::string, std::string> constants;
};

class IntermediateCodeGen
{
public:
    explicit IntermediateCodeGen(const std::shared_ptr<Node> &root);
    GeneratedIR get();

private:
    std::string exec_expr(const std::shared_ptr<Node> &n);
    void exec_assignment(const std::shared_ptr<Assignment> &a);
    void exec_if(const std::shared_ptr<IfStatement> &i);
    void exec_while(const std::shared_ptr<WhileStatement> &w);
    void exec_condition(const std::shared_ptr<Condition> &c);
    void exec_print(const std::shared_ptr<PrintStatement> &p);
    void exec_declaration(const std::shared_ptr<Declaration> &d);
    void exec_statement(const std::shared_ptr<Node> &n);
    std::string nextTemp();
    std::string nextLabel();
    std::string currentLabel() const;
    std::string nextStringSym();

private:
    std::shared_ptr<Node> root;
    InterCodeArray arr;
    std::unordered_map<std::string, std::string> identifiers;
    std::unordered_map<std::string, std::string> constants;
    int tCounter{1};
    int lCounter{1};
    int sCounter{1};
};
