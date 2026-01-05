#pragma once
#include "ir.hpp"
#include <string>
#include <unordered_map>

class CodeGenerator
{
public:
    CodeGenerator(const InterCodeArray &arr,
                  const std::unordered_map<std::string, std::string> &identifiers,
                  const std::unordered_map<std::string, std::string> &constants,
                  const std::unordered_map<std::string, std::string> &tempmap);
    void writeAsm(const std::string &path);
    int assembleAndRun(const std::string &asmPath, const std::string &objPath, const std::string &exePath);

private:
    void pr(const std::string &s);
    void gen_variables();
    void gen_start();
    void gen_end();
    void gen_code();
    void gen_assignment(const AssignmentCode &a);
    void gen_jump(const JumpCode &j);
    void gen_label(const LabelCode &l);
    void gen_compare(const CompareCodeIR &c);
    void gen_print(const PrintCodeIR &p);
    void gen_print_newline();
    void gen_print_num_function();
    void gen_print_string_function();
    void gen_support_functions();
    static std::string handleVar(const std::string &a, const std::unordered_map<std::string, std::string> &tempmap);

private:
    const InterCodeArray &arr;
    std::unordered_map<std::string, std::string> ids;
    std::unordered_map<std::string, std::string> consts;
    std::unordered_map<std::string, std::string> tempmap;
    std::string out;
    bool need_print_num = false;
    bool need_print_string = false;
};
