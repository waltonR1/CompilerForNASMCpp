#include "ir.hpp"
#include <queue>
#include <unordered_set>

// -------------------- type helpers (minimal) --------------------
static bool is_int_literal(const std::string& s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '-') { if (s.size() == 1) return false; i = 1; }
    for (; i < s.size(); ++i) if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

static bool isStringValue(const std::string& v,
                          const std::unordered_map<std::string, std::string>& identifiers,
                          const std::unordered_map<std::string, std::string>& constants) {
    // string literal symbol: S1/S2... in constants
    if (constants.find(v) != constants.end()) return true;

    // identifier declared as string (e.g., Vmsg : string)
    auto it = identifiers.find(v);
    if (it != identifiers.end() && it->second == "string") return true;

    return false;
}

static bool isIntValue(const std::string& v,
                       const std::unordered_map<std::string, std::string>& identifiers,
                       const std::unordered_map<std::string, std::string>& constants) {
    // numeric literal
    if (is_int_literal(v)) return true;

    // declared int temp/var
    auto it = identifiers.find(v);
    if (it != identifiers.end() && it->second == "int") return true;

    // if it is a constant symbol, it's a string constant in your design => not int
    if (constants.find(v) != constants.end()) return false;

    // unknown => treat as int by default (keeps runtime permissive)
    return true;
}

static std::shared_ptr<AssignmentCode> make_assign(const std::string &v, const std::string &l, const std::string &op, const std::string &r)
{
    auto a = std::make_shared<AssignmentCode>();
    a->var = v;
    a->left = l;
    a->op = op;
    a->right = r;
    return a;
}
static std::shared_ptr<JumpCode> make_jump(const std::string &d)
{
    auto j = std::make_shared<JumpCode>();
    j->dist = d;
    return j;
}
static std::shared_ptr<LabelCode> make_label(const std::string &l)
{
    auto x = std::make_shared<LabelCode>();
    x->label = l;
    return x;
}
static std::shared_ptr<CompareCodeIR> make_compare(const std::string &l, const std::string &op, const std::string &r, const std::string &j)
{
    auto c = std::make_shared<CompareCodeIR>();
    c->left = l;
    c->operation = op;
    c->right = r;
    c->jump = j;
    return c;
}
static std::shared_ptr<PrintCodeIR> make_print(PrintKind k, const std::string& v, bool nl)
{
    auto p = std::make_shared<PrintCodeIR>();
    p->printKind = k;
    p->value = v;
    p->newline = nl;
    return p;
}

IntermediateCodeGen::IntermediateCodeGen(const std::shared_ptr<Node> &root) : root(root)
{
    exec_statement(root);
}

static bool eval_cmp_int(int a, const std::string& op, int b) {
    if (op == "==") return a == b;
    if (op == "!=") return a != b;
    if (op == "<")  return a <  b;
    if (op == "<=") return a <= b;
    if (op == ">")  return a >  b;
    if (op == ">=") return a >= b;
    throw std::runtime_error("unknown cmp op");
}

InterCodeArray fold_const_conditions(const InterCodeArray& in)
{
    InterCodeArray out;
    for (size_t i = 0; i < in.code.size(); ++i) {
        auto& ins = in.code[i];

        if (auto c = dynamic_cast<CompareCodeIR*>(ins.get())) {
            // 只处理左右都是整数字面量
            if (is_int_literal(c->left) && is_int_literal(c->right)) {
                int a = std::stoi(c->left), b = std::stoi(c->right);
                bool cond = eval_cmp_int(a, c->operation, b);

                // 你的 IR 模式：CMP ... goto L_then;  下一条通常是 JMP L_else
                if (cond) {
                    out.append(make_jump(c->jump)); // 直接跳 then
                    // 顺手跳过紧跟的 JMP L_else（如果存在）
                    if (i + 1 < in.code.size() && in.code[i+1]->kind() == IRKind::Jump) i++;
                } else {
                    // 恒假：CMP 不要了，保留后面的 JMP L_else（如果存在就让它生效）
                    // 什么都不 append，让下一条 JMP L_else 自己进入 out
                }
                continue;
            }
        }

        out.append(ins);
    }
    return out;
}

InterCodeArray eliminate_unreachable_blocks(const InterCodeArray& in)
{
    const auto& code = in.code;
    const int n = (int)code.size();

    // ---- 1. label -> index ----
    std::unordered_map<std::string, int> labelIndex;
    for (int i = 0; i < n; ++i) {
        if (code[i]->kind() == IRKind::Label) {
            auto* l = dynamic_cast<LabelCode*>(code[i].get());
            labelIndex[l->label] = i;
        }
    }

    // ---- 2. BFS / DFS ----
    std::vector<bool> visited(n, false);
    std::queue<int> q;

    // entry point
    q.push(0);
    visited[0] = true;

    while (!q.empty()) {
        int i = q.front(); q.pop();
        auto& ins = code[i];

        auto push = [&](int j) {
            if (j >= 0 && j < n && !visited[j]) {
                visited[j] = true;
                q.push(j);
            }
        };

        switch (ins->kind()) {
            case IRKind::Jump: {
                auto* j = dynamic_cast<JumpCode*>(ins.get());
                push(labelIndex[j->dist]);
                break;
            }
            case IRKind::Compare: {
                auto* c = dynamic_cast<CompareCodeIR*>(ins.get());
                push(i + 1);                    // fallthrough
                push(labelIndex[c->jump]);     // taken branch
                break;
            }
            default:
                push(i + 1);
        }
    }

    // ---- 3. filter ----
    InterCodeArray out;
    for (int i = 0; i < n; ++i) {
        if (visited[i])
            out.append(code[i]);
    }

    return out;
}


InterCodeArray inline_temp_expr(const InterCodeArray& in)
{
    InterCodeArray out;
    const auto& code = in.code;

    for (size_t i = 0; i < code.size(); ++i)
    {
        // pattern:  (1) T = A op B
        auto* def = dynamic_cast<AssignmentCode*>(code[i].get());
        if (!def || def->op.empty() || def->var.empty() || def->var[0] != 'T') {
            out.append(code[i]);
            continue;
        }

        // need next instruction exists: (2) X = T   (copy)
        if (i + 1 >= code.size()) {
            out.append(code[i]);
            continue;
        }

        auto* use = dynamic_cast<AssignmentCode*>(code[i + 1].get());
        if (!use || !use->op.empty()) {              // must be pure copy
            out.append(code[i]);
            continue;
        }

        if (use->left != def->var) {                 // RHS must be that temp
            out.append(code[i]);
            continue;
        }

        // ✅ inline:  X = (A op B)
        out.append(make_assign(use->var, def->left, def->op, def->right));

        i++; // skip the next instruction (X = T)
    }

    return out;
}

InterCodeArray remove_dead_assignments(const InterCodeArray& in)
{
    std::unordered_set<std::string> read; // variables that are used (read)

    // -------- pass 1: collect reads --------
    for (auto& ins : in.code)
    {
        if (auto a = dynamic_cast<AssignmentCode*>(ins.get()))
        {
            // RHS reads
            if (!a->left.empty())  read.insert(a->left);
            if (!a->right.empty()) read.insert(a->right);
        }
        else if (auto c = dynamic_cast<CompareCodeIR*>(ins.get()))
        {
            read.insert(c->left);
            read.insert(c->right);
        }
        else if (auto p = dynamic_cast<PrintCodeIR*>(ins.get()))
        {
            read.insert(p->value);
        }
    }

    // -------- pass 2: filter assignments --------
    InterCodeArray out;
    for (auto& ins : in.code)
    {
        if (auto a = dynamic_cast<AssignmentCode*>(ins.get()))
        {
            // if LHS never read later, drop it
            // (safe for your language because assignment has no side effects)
            if (!a->var.empty() && read.find(a->var) == read.end())
                continue;
        }
        out.append(ins);
    }

    return out;
}

InterCodeArray remove_trivial_jumps(const InterCodeArray& in)
{
    InterCodeArray out;
    const auto& code = in.code;

    for (size_t i = 0; i < code.size(); ++i)
    {
        auto* j = dynamic_cast<JumpCode*>(code[i].get());
        if (j && i + 1 < code.size())
        {
            auto* l = dynamic_cast<LabelCode*>(code[i + 1].get());
            if (l && l->label == j->dist)
            {
                // skip this JMP
                continue;
            }
        }
        out.append(code[i]);
    }
    return out;
}

InterCodeArray cleanup_labels(const InterCodeArray& in)
{
    std::unordered_set<std::string> used_labels;

    // -------- pass 1: collect used labels --------
    for (auto& ins : in.code)
    {
        if (auto j = dynamic_cast<JumpCode*>(ins.get()))
        {
            used_labels.insert(j->dist);
        }
        else if (auto c = dynamic_cast<CompareCodeIR*>(ins.get()))
        {
            used_labels.insert(c->jump);
        }
    }

    // -------- pass 2: filter labels --------
    InterCodeArray out;
    for (auto& ins : in.code)
    {
        if (auto l = dynamic_cast<LabelCode*>(ins.get()))
        {
            // remove label if nobody jumps to it
            if (used_labels.find(l->label) == used_labels.end())
                continue;
        }
        out.append(ins);
    }

    return out;
}


GeneratedIR IntermediateCodeGen::get() {
    GeneratedIR g{arr, identifiers, constants};
    g.code = fold_const_conditions(g.code);
    g.code = eliminate_unreachable_blocks(g.code);
    g.code = inline_temp_expr(g.code);          // 你已经做到
    g.code = remove_dead_assignments(g.code);   // ⭐ 删 Vdead
    g.code = remove_trivial_jumps(g.code);      // ⭐ 删 JMP L12
    g.code = cleanup_labels(g.code);
    g.code = eliminate_unreachable_blocks(g.code); // 可选：再跑一次收尾


    return g;
}

std::string IntermediateCodeGen::nextTemp() { return "T" + std::to_string(tCounter++); }
std::string IntermediateCodeGen::nextLabel() { return "L" + std::to_string(lCounter++); }
std::string IntermediateCodeGen::currentLabel() const { return "L" + std::to_string(lCounter); }
std::string IntermediateCodeGen::nextStringSym() { return "S" + std::to_string(sCounter++); }

std::string IntermediateCodeGen::exec_expr(const std::shared_ptr<Node> &n)
{
    if (!n)
        throw std::runtime_error("Null expression in IR generation");

    // --- Identifier ---
    if (auto id = std::dynamic_pointer_cast<IdentifierNode>(n))
        return id->getValue();

    // --- Integer literal ---
    if (auto num = std::dynamic_pointer_cast<NumberNode>(n))
        return num->getValue();

    // --- String literal ---
    if (auto str = std::dynamic_pointer_cast<StringNode>(n))
    {
        // allocate a symbol name like S1, S2
        auto sym = nextStringSym();
        constants[sym] = str->getValue();   // place into constant table
        return sym;                         // expression returns symbol name
    }

    // --- Binary operation ---
    if (auto bin = std::dynamic_pointer_cast<BinOpNode>(n))
    {
        auto left  = exec_expr(bin->left);
        auto right = exec_expr(bin->right);
        const std::string &op = bin->op_tok.value;

        // ===== Constant Folding (INT only) =====
        if (is_int_literal(left) && is_int_literal(right))
        {
            int a = std::stoi(left);
            int b = std::stoi(right);
            int r = 0;

            if (op == "+")      r = a + b;
            else if (op == "-") r = a - b;
            else if (op == "*") r = a * b;
            else if (op == "/") r = a / b;   // assume b != 0
            else
                goto NO_FOLD;

            return std::to_string(r);   // ★ 不生成 IR
        }

        NO_FOLD:
            auto t = nextTemp();
        identifiers[t] = "int";
        arr.append(make_assign(t, left, op, right));
        return t;
    }

    throw std::runtime_error("Unsupported expression node in IR generation");
}


void IntermediateCodeGen::exec_assignment(const std::shared_ptr<Assignment> &a)
{
    auto right = exec_expr(a->expression);
    arr.append(make_assign(a->identifier.value, right, "", ""));
}

void IntermediateCodeGen::exec_condition(const std::shared_ptr<Condition> &c)
{
    auto left = exec_expr(c->left_expression);
    auto right = exec_expr(c->right_expression);
    // Compare should jump to the label of the next emitted label (body)
    auto body = currentLabel();
    arr.append(make_compare(left, c->comparison.value, right, body));
}

void IntermediateCodeGen::exec_if(const std::shared_ptr<IfStatement> &i)
{
    if (i->else_body)
    {
        auto L_then = nextLabel();
        auto L_else = nextLabel();
        auto L_end  = nextLabel();

        auto left  = exec_expr(i->if_condition->left_expression);
        auto right = exec_expr(i->if_condition->right_expression);

        // 条件成立 -> 进入 then
        arr.append(make_compare(left,
                                i->if_condition->comparison.value,
                                right,
                                L_then));

        // 条件不成立 -> 直接跳 else
        arr.append(make_jump(L_else));

        // then 部分
        arr.append(make_label(L_then));
        exec_statement(i->if_body);
        arr.append(make_jump(L_end));

        // else 部分
        arr.append(make_label(L_else));
        exec_statement(i->else_body);

        // if 结束
        arr.append(make_label(L_end));
    }
    else
    {
        auto L_then = nextLabel();
        auto L_end  = nextLabel();

        auto left  = exec_expr(i->if_condition->left_expression);
        auto right = exec_expr(i->if_condition->right_expression);

        arr.append(make_compare(left,
                                i->if_condition->comparison.value,
                                right,
                                L_then));

        arr.append(make_jump(L_end));

        arr.append(make_label(L_then));
        exec_statement(i->if_body);

        arr.append(make_label(L_end));
    }
}


void IntermediateCodeGen::exec_while(const std::shared_ptr<WhileStatement> &w)
{
    auto L_start = nextLabel();
    auto L_body  = nextLabel();
    auto L_end   = nextLabel();

    arr.append(make_label(L_start));

    auto left  = exec_expr(w->condition->left_expression);
    auto right = exec_expr(w->condition->right_expression);

    // 条件成立 -> 进入循环体
    arr.append(make_compare(left,
                            w->condition->comparison.value,
                            right,
                            L_body));

    // 条件不成立 -> 跳出循环
    arr.append(make_jump(L_end));

    arr.append(make_label(L_body));
    exec_statement(w->body);
    arr.append(make_jump(L_start));

    arr.append(make_label(L_end));
}


void IntermediateCodeGen::exec_print(const std::shared_ptr<PrintStatement> &p)
{
    // prints("...") —— 直接输出字符串字面量并换行
    if (p->type == "string") {
        auto sym = nextStringSym();
        constants[sym] = p->strValue;
        arr.append(make_print(PrintKind::String, sym, true));
        return;
    }

    // print(expr)
    auto expr = p->intExpr;

    if (auto bin = std::dynamic_pointer_cast<BinOpNode>(expr)) {
        auto left  = exec_expr(bin->left);
        auto right = exec_expr(bin->right);

        // string + int
        if (isStringValue(left, identifiers, constants) &&
            isIntValue(right, identifiers, constants)) {
            arr.append(make_print(PrintKind::String, left, false));
            arr.append(make_print(PrintKind::Int, right, true));
            return;
            }

        // int + string
        if (isIntValue(left, identifiers, constants) &&
            isStringValue(right, identifiers, constants)) {
            arr.append(make_print(PrintKind::Int, left, true));
            arr.append(make_print(PrintKind::String, right, true));
            return;
            }

    }

    // fallback：普通 int
    auto v = exec_expr(expr);

    auto it = identifiers.find(v);
    if (it != identifiers.end() && it->second == "string") {
        arr.append(make_print(PrintKind::String, v, true));
        return;
    }

    arr.append(make_print(PrintKind::Int, v, true));
}


void IntermediateCodeGen::exec_declaration(const std::shared_ptr<Declaration> &d)
{
    for (const auto &i : d->identifiers)
        identifiers[i.value] = d->declaration_type.value;
}

void IntermediateCodeGen::exec_statement(const std::shared_ptr<Node> &n)
{
    if (!n)
        return;
    if (auto st = std::dynamic_pointer_cast<Statement>(n))
    {
        exec_statement(st->left);
        exec_statement(st->right);
        return;
    }
    if (auto is = std::dynamic_pointer_cast<IfStatement>(n))
    {
        exec_if(is);
        return;
    }
    if (auto wh = std::dynamic_pointer_cast<WhileStatement>(n))
    {
        exec_while(wh);
        return;
    }
    if (auto pr = std::dynamic_pointer_cast<PrintStatement>(n))
    {
        exec_print(pr);
        return;
    }
    if (auto de = std::dynamic_pointer_cast<Declaration>(n))
    {
        exec_declaration(de);
        return;
    }
    if (auto asg = std::dynamic_pointer_cast<Assignment>(n))
    {
        exec_assignment(asg);
        return;
    }
}
