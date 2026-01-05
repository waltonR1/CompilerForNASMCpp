#include "codegen.hpp"
#include <fstream>
#include <cstdlib>

static std::string op_to_asm(const std::string &op)
{
  if (op == "+") return "add";
  if (op == "-") return "sub";
  if (op == "*") return "imul";
  if (op == "&") return "and";
  if (op == "|") return "or";
  if (op == "^") return "xor";
  return "";
}

static std::string cmp_to_jmp(const std::string &c)
{
  if (c == "<")  return "jl";
  if (c == "<=") return "jle";
  if (c == ">")  return "jg";
  if (c == ">=") return "jge";
  if (c == "==") return "je";
  if (c == "!=") return "jne";
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

static bool is_int_literal(const std::string &s) {
  if (s.empty()) return false;
  size_t i = 0;
  if (s[0] == '-') { if (s.size() == 1) return false; i = 1; }
  for (; i < s.size(); ++i) if (s[i] < '0' || s[i] > '9') return false;
  return true;
}

std::string CodeGenerator::handleVar(const std::string &a, const std::unordered_map<std::string, std::string> &tempmap)
{
  std::string x = a;
  auto it = tempmap.find(x);
  if (it != tempmap.end()) x = it->second; // temp remap

  if (is_int_literal(x)) return x;         // immediate number

  // 常量一般像 S1 / S2...（你也可以按 consts map 判断，但这里只有 tempmap 参数，所以用命名约定）
  if (!x.empty() && x[0] == 'S') return x; // address label, e.g., S1

  // 其他一律当作 bss 里的 8-byte 槽位（变量/临时）
  return "[" + x + "]";
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
  pr("\tnl db 10");

  for (auto &kv : consts)
  {
    pr("\t" + kv.first + " db \"" + kv.second + "\", 0");
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
  // ⚠️ 按 ir.hpp 替换字段名：
  // a.dst, a.src1, a.op, a.src2
  // 约定：a.op 为空 => dst = src1

  const std::string dst = a.var; // 目标槽位名（不要加[]）
  const std::string s1  = handleVar(a.left, tempmap);

  if (a.op.empty())
  {
    // dst = src1
    pr("\tmov rax, " + s1);

    // 如果 src1 是字符串常量 label（S1），这里 mov rax, S1 会把地址放进 rax
    // 再 mov [dst], rax 即可（msg = S1）
    pr("\tmov [" + dst + "], rax");
    return;
  }

  const std::string s2 = handleVar(a.right, tempmap);
  const std::string op = op_to_asm(a.op);

  // rax = src1
  pr("\tmov rax, " + s1);

  // rax = rax (op) src2
  // 对于 imul，两操作数形式：imul rax, <src>
  pr("\t" + op + " rax, " + s2);

  // store back
  pr("\tmov [" + dst + "], rax");
}

void CodeGenerator::gen_jump(const JumpCode &j)
{
  // 假设字段名叫 j.dist
  pr("\tjmp " + j.dist);
}
void CodeGenerator::gen_label(const LabelCode &l)
{
  // 假设字段名叫 l.label
  pr(l.label + ":");
}
void CodeGenerator::gen_compare(const CompareCodeIR &c)
{
  // ⚠️ 下面字段名按你的 ir.hpp 替换：
  // c.left, c.op, c.right, c.label

  const std::string lhs = handleVar(c.left, tempmap);
  const std::string rhs = handleVar(c.right, tempmap);
  const std::string jcc = cmp_to_jmp(c.operation);

  // lhs 可能是 [Va] 或立即数。cmp 的第一个操作数不能是立即数，所以用 rax 做中转：
  pr("\tmov rax, " + lhs);
  pr("\tcmp rax, " + rhs);
  pr("\t" + jcc + " " + c.jump);
}

void CodeGenerator::gen_print(const PrintCodeIR &p)
{
  if (p.printKind == PrintKind::String)
  {
    // rax = address of string (S1 or [Vmsg])
    pr("\tmov rax, " + handleVar(p.value, tempmap));
    pr("\tcall _print_string");

    if (p.newline)
      gen_print_newline();

    return;
  }

  // PrintKind::Int
  pr("\tmov rax, " + handleVar(p.value, tempmap));
  pr("\tcall _print_num"); // _print_num already prints '\n'
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
      if (print_ins->printKind == PrintKind::String)
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


void CodeGenerator::gen_print_num_function()
{
  pr("");
  pr("_print_num:");
  pr("\t; rax = signed integer to print");
  pr("\tpush rbx");
  pr("\tpush rcx");
  pr("\tpush rdx");
  pr("\tpush rsi");

  pr("\tmov rbx, 10");

  // rcx points to end-1 (we keep newline at last byte)
  pr("\tlea rcx, [digitSpace+99]");
  pr("\tmov byte [rcx], 10");      // '\n'
  pr("\tdec rcx");

  // sign handling
  pr("\txor r8, r8");              // r8 = 0 means non-negative
  pr("\tcmp rax, 0");
  pr("\tjge .pn_convert");
  pr("\tneg rax");
  pr("\tmov r8, 1");               // negative
  pr(".pn_convert:");

  // handle 0 explicitly
  pr("\tcmp rax, 0");
  pr("\tjne .pn_loop");
  pr("\tmov byte [rcx], '0'");
  pr("\tdec rcx");
  pr("\tjmp .pn_after_digits");

  pr(".pn_loop:");
  pr("\txor rdx, rdx");
  pr("\tdiv rbx");                 // rax=quotient, rdx=remainder
  pr("\tadd dl, '0'");
  pr("\tmov [rcx], dl");
  pr("\tdec rcx");
  pr("\tcmp rax, 0");
  pr("\tjne .pn_loop");

  pr(".pn_after_digits:");
  pr("\tcmp r8, 1");
  pr("\tjne .pn_write");
  pr("\tmov byte [rcx], '-'");
  pr("\tdec rcx");

  pr(".pn_write:");
  // rsi = start pointer
  pr("\tlea rsi, [rcx+1]");
  // rdx = length = (digitSpace+100) - rsi
  pr("\tlea rdx, [digitSpace+100]");
  pr("\tsub rdx, rsi");

  pr("\tmov rax, 1");              // sys_write
  pr("\tmov rdi, 1");              // stdout
  pr("\tsyscall");

  pr("\tpop rsi");
  pr("\tpop rdx");
  pr("\tpop rcx");
  pr("\tpop rbx");
  pr("\tret");
}

void CodeGenerator::gen_print_string_function()
{
  pr("");
  pr("_print_string:");
  pr("\t; rax = address of 0-terminated string");
  pr("\tpush rbx");
  pr("\tmov rbx, rax");
  pr("\txor rdx, rdx");                 // len = 0
  pr(".ps_len_loop:");
  pr("\tcmp byte [rbx+rdx], 0");
  pr("\tje .ps_len_done");
  pr("\tinc rdx");
  pr("\tjmp .ps_len_loop");
  pr(".ps_len_done:");
  pr("\tmov rax, 1");                   // sys_write
  pr("\tmov rdi, 1");                   // fd=stdout
  pr("\tmov rsi, rbx");                 // buf
  pr("\tsyscall");
  pr("\tpop rbx");
  pr("\tret");
}

void CodeGenerator::gen_print_newline()
{
  pr("\tmov rax, 1");   // sys_write
  pr("\tmov rdi, 1");   // stdout
  pr("\tmov rsi, nl");  // buf
  pr("\tmov rdx, 1");   // len
  pr("\tsyscall");
}
