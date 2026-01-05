#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "tokens.hpp"
#include "ast.hpp"
#include "ir.hpp"
#include "codegen.hpp"

void flatten_statement(
    const std::shared_ptr<Node>& node,
    std::vector<std::shared_ptr<Node>>& out)
{
    if (!node) return;

    if (auto st = std::dynamic_pointer_cast<Statement>(node))
    {
        flatten_statement(st->left, out);
        flatten_statement(st->right, out);
    }
    else
    {
        out.push_back(node);
    }
}

void print_ast(const std::shared_ptr<Node>& node, std::string prefix = "", bool isLast = true)
{
    if (!node)
        return;

    std::cout << prefix;
    std::cout << (isLast ? "└── " : "├── ");

    // ---- Number ----
    if (auto n = std::dynamic_pointer_cast<NumberNode>(node))
    {
        std::cout << "Number(" << n->tok.value << ")\n";
        return;
    }

    // ---- String ----
    if (auto n = std::dynamic_pointer_cast<StringNode>(node))
    {
        std::cout << "String(\"" << n->tok.value << "\")\n";
        return;
    }


    // ---- Identifier ----
    if (auto n = std::dynamic_pointer_cast<IdentifierNode>(node))
    {
        std::cout << "Identifier(" << n->tok.value << ")\n";
        return;
    }

    // ---- Binary Operation ----
    if (auto n = std::dynamic_pointer_cast<BinOpNode>(node))
    {
        std::cout << "BinOp(" << n->op_tok.value << ")\n";

        print_ast(n->left,  prefix + (isLast ? "    " : "│   "), false);
        print_ast(n->right, prefix + (isLast ? "    " : "│   "), true);
        return;
    }

    // ---- Assignment ----
    if (auto n = std::dynamic_pointer_cast<Assignment>(node))
    {
        std::cout << "Assignment(" << n->identifier.value << ")\n";
        print_ast(n->expression, prefix + (isLast ? "    " : "│   "), true);
        return;
    }

    // ---- Declaration ----
    if (auto n = std::dynamic_pointer_cast<Declaration>(node))
    {
        std::cout << "Declaration(type=" << n->declaration_type.value << ")\n";

        for (size_t i = 0; i < n->identifiers.size(); i++)
        {
            const auto &tok = n->identifiers[i];
            bool last = (i + 1 == n->identifiers.size());

            std::cout << prefix + (isLast ? "    " : "│   ");
            std::cout << (last ? "└── " : "├── ");
            std::cout << "Var(" << tok.value << ")\n";
        }
        return;
    }

    // ---- Print ----
    if (auto n = std::dynamic_pointer_cast<PrintStatement>(node))
    {
        std::cout << "Print(" << n->type << ")\n";

        if (n->type == "string")
        {
            std::cout << prefix + (isLast ? "    " : "│   ") << "└── "
                      << "\"" << n->strValue << "\"\n";
        }
        else
        {
            print_ast(n->intExpr, prefix + (isLast ? "    " : "│   "), true);
        }
        return;
    }

    // ---- Condition ----
    if (auto n = std::dynamic_pointer_cast<Condition>(node))
    {
        std::cout << "Condition(" << n->comparison.value << ")\n";

        print_ast(n->left_expression,
                  prefix + (isLast ? "    " : "│   "), false);

        print_ast(n->right_expression,
                  prefix + (isLast ? "    " : "│   "), true);
        return;
    }

    // ---- If ----
    if (auto n = std::dynamic_pointer_cast<IfStatement>(node))
    {
        std::cout << "IfStatement\n";

        // Condition
        print_ast(n->if_condition,
                  prefix + (isLast ? "    " : "│   "),
                  false);

        // 判断 Then 是否是最后一个
        bool thenIsLast = (n->else_body == nullptr);

        // Then
        std::cout << prefix + (isLast ? "    " : "│   ")
                  << (thenIsLast ? "└── Then\n" : "├── Then\n");

        print_ast(n->if_body,
                  prefix + (isLast ? "    " : "│   ")
                         + (thenIsLast ? "    " : "│   "),
                  true);

        // Else (only if exists)
        if (n->else_body)
        {
            std::cout << prefix + (isLast ? "    " : "│   ")
                      << "└── Else\n";

            print_ast(n->else_body,
                      prefix + (isLast ? "    " : "│   ") + "    ",
                      true);
        }

        return;
    }


    // ---- While ----
    if (auto n = std::dynamic_pointer_cast<WhileStatement>(node))
    {
        std::cout << "WhileStatement\n";

        print_ast(n->condition, prefix + (isLast ? "    " : "│   "), false);
        print_ast(n->body,      prefix + (isLast ? "    " : "│   "), true);
        return;
    }

    // ---- Statement list ----
    if (auto n = std::dynamic_pointer_cast<Statement>(node))
    {
        std::cout << "Block\n";

        std::vector<std::shared_ptr<Node>> stmts;
        flatten_statement(node, stmts);

        for (size_t i = 0; i < stmts.size(); ++i)
        {
            bool last = (i + 1 == stmts.size());
            print_ast(stmts[i], prefix + "    ", last);
        }
        return;
    }


    std::cout << "UnknownNode\n";
}


void print_ir(const GeneratedIR& ir)
{
    std::cout << "\n===== IR CODE =====\n";

    for (auto &instr : ir.code.code)
    {
        switch (instr->kind())
        {
            case IRKind::Assignment:
            {
                auto *a = dynamic_cast<AssignmentCode*>(instr.get());
                if (a->op.empty())
                    std::cout << a->var << " = " << a->left << "\n";
                else
                    std::cout << a->var << " = " << a->left << " "
                              << a->op << " " << a->right << "\n";
                break;
            }

            case IRKind::Compare:
            {
                auto *c = dynamic_cast<CompareCodeIR*>(instr.get());
                std::cout << "CMP " << c->left << " "
                          << c->operation << " "
                          << c->right << "  -> goto "
                          << c->jump << "\n";
                break;
            }

            case IRKind::Jump:
            {
                auto *j = dynamic_cast<JumpCode*>(instr.get());
                std::cout << "JMP " << j->dist << "\n";
                break;
            }

            case IRKind::Label:
            {
                auto *l = dynamic_cast<LabelCode*>(instr.get());
                std::cout << l->label << ":\n";
                break;
            }

            case IRKind::Print:
            {
                auto *p = dynamic_cast<PrintCodeIR*>(instr.get());

                std::cout << "PRINT ";

                if (p->printKind == PrintKind::String)
                    std::cout << "string ";
                else
                    std::cout << "int ";

                std::cout << p->value;

                if (p->newline)
                    std::cout << " \\n";

                std::cout << "\n";
                break;
            }

        }
    }

    std::cout << "\n===== CONSTANTS =====\n";
    for (auto &kv : ir.constants)
        std::cout << kv.first << " = \"" << kv.second << "\"\n";

    std::cout << "\n===== IDENTIFIERS =====\n";
    for (auto &kv : ir.identifiers)
        std::cout << kv.first << " : " << kv.second << "\n";
}


// If Flex's buffer type isn't visible, declare it.
    typedef struct yy_buffer_state *YY_BUFFER_STATE;
    YY_BUFFER_STATE yy_scan_string(const char* str);
    void yy_delete_buffer(YY_BUFFER_STATE b);

// Provided by Bison (parser.yy)
extern int yyparse(void);
extern std::shared_ptr<Node> g_ast_root; // The global AST root

int main()
{
    while (true)
    {
        std::ifstream fin("../read.txt");
        if (!fin) { std::cerr << "Cannot open read.txt\n"; return 1; }

        std::stringstream buffer;
        buffer << fin.rdbuf();
        std::string input = buffer.str();

        try
        {
            YY_BUFFER_STATE buf = yy_scan_string(input.c_str());
            int parse_result = yyparse();
            yy_delete_buffer(buf);

            if (parse_result == 0)
            {
                std::cout << "Parsing successful!\n";
                print_ast(g_ast_root);
                auto root = g_ast_root;
                IntermediateCodeGen irgen(root);
                auto gen = irgen.get();
                print_ir(gen);

                // ===== 新增：生成 asm =====
                CodeGenerator codegen(
                    gen.code,
                    gen.identifiers,
                    gen.constants,
                    {}               // tempmap，你现在 IR 已经是最终名，可以先传空
                );

                codegen.writeAsm("../output.asm");
                std::cout << "[OK] output.asm generated.\n";
            }
            else
            {
                std::cerr << "Parsing failed.\n";
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << "\n";
        }
        std::cout << "------------------------------\n";
        std::string dummy;
        std::getline(std::cin, dummy);
    }
}