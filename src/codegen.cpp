#include "codegen.hpp"
#include <fstream>
#include <cstdlib>

static std::string op_to_asm(const std::string &op)
{
  if (op == "+")
    return "add";
  // TBD
  return "";
}
static std::string cmp_to_jmp(const std::string &c)
{
  if (c == "<")
    return "jl";
  // TBD
  return "";
}

CodeGenerator::CodeGenerator(const InterCodeArray &arr,
                             const std::unordered_map<std::string, std::string> &identifiers,
                             const std::unordered_map<std::string, std::string> &constants,
                             const std::unordered_map<std::string, std::string> &tempmap)
    : arr(arr), ids(identifiers), consts(constants), tempmap(tempmap), need_print_num(false), need_print_string(false) {}

void CodeGenerator::pr(const std::string &s)
{
  out.append(s);
  out.push_back('\n');
}

std::string CodeGenerator::handleVar(const std::string &a, const std::unordered_map<std::string, std::string> &tempmap)
{
  // TBD
  return "";
}

void CodeGenerator::gen_variables()
{
  pr("section .bss");
  // Only emit print helper buffers if we actually print numbers
  if (need_print_num)
  {
    pr("\tdigitSpace resb 100");
    pr("\tdigitSpacePos resb 8\n");
  }
  for (auto &kv : ids)
  {
    pr("\t" + kv.first + " resb 8");
  }
}

void CodeGenerator::gen_start()
{
  pr("section .data");
  if (need_print_string)
  {
    // TBD
  }
  for (auto &kv : consts)
  {
    pr("\t" + kv.first + " db \"" + kv.second + "\",10,0");
  }
  pr("section .text");
  pr("\tglobal _start\n");
  pr("_start:");
}

void CodeGenerator::gen_end()
{
  pr("\tmov rax, 60");
  pr("\tmov rdi, 0");
  pr("\tsyscall\n");
}

void CodeGenerator::gen_assignment(const AssignmentCode &a)
{
  // TBD
}

void CodeGenerator::gen_jump(const JumpCode &j)
{
  // TBD
}
void CodeGenerator::gen_label(const LabelCode &l)
{
  // TBD
}
void CodeGenerator::gen_compare(const CompareCodeIR &c)
{
  // TBD
}

void CodeGenerator::gen_print(const PrintCodeIR &p)
{
  // TBD
}

void CodeGenerator::gen_code()
{
  for (auto &ins : arr.code)
  {
    switch (ins->kind())
    {
    case IRKind::Assignment:
      gen_assignment(*std::static_pointer_cast<AssignmentCode>(ins));
      break;
    case IRKind::Jump:
      gen_jump(*std::static_pointer_cast<JumpCode>(ins));
      break;
    case IRKind::Label:
      gen_label(*std::static_pointer_cast<LabelCode>(ins));
      break;
    case IRKind::Compare:
      gen_compare(*std::static_pointer_cast<CompareCodeIR>(ins));
      break;
    case IRKind::Print:
      gen_print(*std::static_pointer_cast<PrintCodeIR>(ins));
      break;
    }
  }
}

void CodeGenerator::writeAsm(const std::string &path)
{
  out.clear();

  // Pre-scan IR to determine which helpers are needed (before gen_variables)
  for (auto &ins : arr.code)
  {
    if (ins->kind() == IRKind::Print)
    {
      auto print_ins = std::static_pointer_cast<PrintCodeIR>(ins);
      if (print_ins->type == "string")
        need_print_string = true;
      else
        need_print_num = true;
    }
  }

  gen_variables();
  gen_start();
  gen_code();
  gen_end();
  if (need_print_num)
    gen_print_num_function();
  if (need_print_string)
    gen_print_string_function();
  std::ofstream f(path, std::ios::binary);
  f << out;
  f.close();
}
