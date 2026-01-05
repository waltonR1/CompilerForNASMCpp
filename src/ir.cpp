#include "ir.hpp"

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

GeneratedIR IntermediateCodeGen::get() { return GeneratedIR{arr, identifiers, constants}; }

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
        auto left = exec_expr(bin->left);
        auto right = exec_expr(bin->right);

        auto t = nextTemp();
        identifiers[t] = "int";

        arr.append(make_assign(t, left, bin->op_tok.value, right));
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

    if (identifiers[v] == "string") {
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
