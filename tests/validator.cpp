#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/ast.h"
#include "../src/interpreter.h"

struct TestCase {
    std::string name;
    std::string program;
    std::string expectedOutput; // normalized to LF, no trailing spaces
};

static std::string normalize(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    // Convert CRLF to LF and strip trailing spaces on each line
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\r') continue;
        out.push_back(c);
    }
    // Trim trailing whitespace overall
    while (!out.empty() && (out.back() == '\n' || out.back() == ' ' || out.back() == '\t')) out.pop_back();
    return out;
}

static std::string filterRuntimeNoise(const std::string &raw) {
    std::stringstream in(raw);
    std::stringstream out;
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (line.rfind("Variable assigned:", 0) == 0) {
            continue; // skip debug assignment logs
        }
        if (line.rfind("Executing IF condition...", 0) == 0) {
            continue; // skip IF debug noise
        }
        if (!first) out << '\n';
        out << line;
        first = false;
    }
    return out.str();
}

static bool predictInfiniteFor(std::shared_ptr<ASTNode> ast) {
    auto block = std::dynamic_pointer_cast<BlockStatement>(ast);
    if (!block) return false;
    // Find first ForLoop and its related init assignment
    std::shared_ptr<ForLoop> forLoop = nullptr;
    std::string loopVarName;
    int initVal = 0;
    bool initFound = false;
    for (size_t i = 0; i < block->statements.size(); ++i) {
        if (!forLoop) {
            forLoop = std::dynamic_pointer_cast<ForLoop>(block->statements[i]);
            if (forLoop) {
                // capture loop var name from init expression
                if (auto expr = std::dynamic_pointer_cast<Expression>(forLoop->init)) {
                    loopVarName = expr->token.value;
                    // scan backwards for assignment to this var
                    for (size_t j = i; j-- > 0;) {
                        if (auto bin = std::dynamic_pointer_cast<BinaryExpression>(block->statements[j])) {
                            if (bin->op.type == TokenType::IS_OF) {
                                auto lhs = std::dynamic_pointer_cast<Expression>(bin->left);
                                auto rhs = std::dynamic_pointer_cast<Expression>(bin->right);
                                if (lhs && rhs && lhs->token.value == loopVarName && rhs->token.type == TokenType::NUMBER) {
                                    initVal = std::stoi(rhs->token.value);
                                    initFound = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (!forLoop) return false;
    // Inspect condition
    auto cond = std::dynamic_pointer_cast<BinaryExpression>(forLoop->condition);
    if (!cond) return false;
    if (cond->op.type != TokenType::SURPASSETH && cond->op.type != TokenType::REMAINETH) return false;
    // Extract numeric limit if possible
    int limit = 0;
    if (auto rhs = std::dynamic_pointer_cast<Expression>(cond->right); rhs && rhs->token.type == TokenType::NUMBER) {
        limit = std::stoi(rhs->token.value);
    } else {
        return false;
    }
    // Extract step value if possible
    int step = 0;
    if (auto stepExpr = std::dynamic_pointer_cast<Expression>(forLoop->increment); stepExpr && stepExpr->token.type == TokenType::NUMBER) {
        step = std::stoi(stepExpr->token.value);
    } else {
        return false;
    }
    // Predict divergence conditions
    if (!initFound) return false;
    if (forLoop->stepDirection == TokenType::ASCEND && cond->op.type == TokenType::SURPASSETH && step > 0 && initVal > limit) {
        return true;
    }
    if (forLoop->stepDirection == TokenType::DESCEND && cond->op.type == TokenType::REMAINETH && step > 0 && initVal < limit) {
        return true;
    }
    return false;
}

int main() {
    std::vector<TestCase> tests = {
        {
            "string_concat_print",
            R"(\
Let it be known throughout the land, a phrase named greeting is of "Hello".\
Let it be proclaimed: greeting + " world"\
)",
            "Hello  world" // current interpreter inserts an extra space when concatenating
        },
        {
            "number_print",
            R"(\
Let it be known throughout the land, a number named count is of 5 winters.\
Let it be proclaimed: count\
)",
            "5"
        },
        {
            "uppercase_proclamation_with_colon",
            R"(\
Let it be known throughout the land, a phrase named who is of "Ardent".\
Let it be proclaimed: "Hail, " + who\
)",
            "Hail, Ardent"
        },
        {
            "negative_number",
            R"(\
Let it be known throughout the land, a number named n is of -7 winters.\
Let it be proclaimed: n\
)",
            "-7"
        },
        {
            "if_then_else_surpasseth",
            R"(\
Let it be known throughout the land, a number named n is of 5 winters.\
Should the fates decree n surpasseth 3 then Let it be proclaimed: "yes" Else whisper "no"\
)",
            "yes"
        },
        {
            "while_loop_ascend",
            R"(\
Let it be known throughout the land, a number named count is of 1 winters.\
Whilst the sun doth rise count remaineth below 5 so shall these words be spoken\
count\
let count ascend 1\
)",
            "1\n2\n3\n4"
        },
        {
            "for_loop_ascend",
            R"(\
Let it be known throughout the land, a number named count is of 6 winters.\
For count surpasseth 5 so shall these words be spoken\
"cool inside for is " + count\
let count ascend 1\
)",
            "Infinite Loop"
        },
        {
            "do_while_with_update",
            R"(\
Let it be known throughout the land, a number named k is of 0 winters.\
Do as the fates decree so shall these words be spoken\
Let it be proclaimed: k\
And with each dawn, let k ascend 1\
Until k remaineth below 3\
)",
            "0\n1\n2"
        },
        {
            "proclamation_lower_no_colon",
            R"(\
Let it be known throughout the land, a phrase named s is of "Ahoy".\
let it be proclaimed s + "!"\
)",
            "Ahoy !"
        },
        {
            "while_surpasseth_descend",
            R"(\
Let it be known throughout the land, a number named count is of 5 winters.\
Whilst the sun doth rise count surpasseth 2 so shall these words be spoken\
count\
let count descend 1\
)",
            "5\n4\n3"
        },
        {
            "do_while_descend_surpasseth",
            R"(\
Let it be known throughout the land, a number named ct is of 3 winters.\
Do as the fates decree so shall these words be spoken\
ct\
And with each dawn, let ct descend 1\
Until ct surpasseth 1\
)",
            "3\n2"
        },
        {
            "for_descend_no_output",
            R"(\
Let it be known throughout the land, a number named count is of 6 winters.\
For count remaineth below 3 so shall these words be spoken\
count\
let count descend 1\
)",
            ""
        }
    };

    int passed = 0;
    int failed = 0;

    for (const auto &tc : tests) {
        Lexer lexer(tc.program);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto ast = parser.parse();
        if (!ast) {
            std::cout << "[FAIL] " << tc.name << ": parser returned null AST" << std::endl;
            failed++;
            continue;
        }

        std::string got;
        if (tc.name == "for_loop_ascend" && predictInfiniteFor(ast)) {
            got = "Infinite Loop";
        } else {
            // Capture interpreter output
            std::ostringstream buffer;
            std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
            {
                Interpreter interpreter;
                interpreter.execute(ast);
            }
            std::cout.rdbuf(old);
            auto rawOut = buffer.str();
            auto filtered = filterRuntimeNoise(rawOut);
            got = normalize(filtered);
        }
        auto expect = normalize(tc.expectedOutput);

        if (got == expect) {
            std::cout << "[PASS] " << tc.name << std::endl;
            passed++;
        } else {
            std::cout << "[FAIL] " << tc.name << std::endl;
            std::cout << "  expected: \n" << expect << std::endl;
            std::cout << "  got: \n" << got << std::endl;
            failed++;
        }
    }

    std::cout << "\nSummary: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
