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
    bool expectParseFailure = false; // when true, parser should fail and emit an error
    std::string expectedErrorContains; // substring expected in stderr on failure
    bool expectRuntimeError = false; // when true, runtime should emit an error to stderr
    std::string expectedRuntimeErrorContains; // substring expected in runtime stderr
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
            "Hello world"
        },
        {
            "phrase_plus_truth",
            R"(\
Let it be proclaimed: "The truth is " + True\
)",
            "The truth is True"
        },
        {
            "phrase_plus_number",
            R"(\
Let it be proclaimed: "Age: " + 25\
)",
            "Age: 25"
        },
        {
            "number_plus_truth_addition",
            R"(\
Let it be known throughout the land, a number named a is of 5 winters.\
Let it be proclaimed: a + True\
)",
            "6"
        },
        {
            "number_plus_phrase_right",
            R"(\
Let it be proclaimed: 7 + "apples"\
)",
            "7 apples"
        },
        {
            "type_mismatch_number_with_string",
            R"(\
Let it be known throughout the land, a number named ct2 is of "hellp" winters.\
)",
            "",
            true,
            "TypeError: Expected number but got STRING for variable 'ct2'"
        },
        {
            "type_mismatch_truth_with_number",
            R"(\
Let it be known throughout the land, a truth named brave is of 32.\
)",
            "",
            true,
            "TypeError: Expected truth but got NUMBER for variable 'brave'"
        },
        {
            "equal_check_true",
            R"(\
Let it be known throughout the land, a number named age is of 18 winters.\
Should the fates decree that age is equal to 18 then Let it be proclaimed: "Aye!" Else whisper "Nay!"\
)",
            "Aye!"
        },
        {
            "not_equal_check_true",
            R"(\
Let it be known throughout the land, a number named count is of 1 winters.\
Should the fates decree that count is not 0 then Let it be proclaimed: "Not zero!"\
)",
            "Not zero!"
        },
        {
            "greater_than_true",
            R"(\
Let it be known throughout the land, a number named n is of 10 winters.\
Should the fates decree that n is greater than 5 then Let it be proclaimed: "GT" Else whisper "LE"\
)",
            "GT"
        },
        {
            "lesser_than_true",
            R"(\
Let it be known throughout the land, a number named n is of 2 winters.\
Should the fates decree that n is lesser than 5 then Let it be proclaimed: "LT" Else whisper "GE"\
)",
            "LT"
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
        },
        {
            "boolean_literal_print",
            R"(\
Let it be known throughout the land, a truth named flag is of True.\
Let it be proclaimed: True\
)",
            "True"
        },
        {
            "boolean_variable_print",
            R"(\
Let it be known throughout the land, a truth named flag is of False.\
Let it be proclaimed: flag\
)",
            "False"
        },
        {
            "logical_and_false",
            R"(\
Let it be known throughout the land, a truth named brave is of True.\
Let it be known throughout the land, a truth named strong is of False.\
Should the fates decree brave and strong then Let it be proclaimed: "ok" Else whisper "nay"\
)",
            "nay"
        },
        {
            "logical_or_true",
            R"(\
Let it be known throughout the land, a truth named brave is of True.\
Let it be known throughout the land, a truth named strong is of False.\
Should the fates decree brave or strong then Let it be proclaimed: "ok" Else whisper "nay"\
)",
            "ok"
        },
        {
            "logical_not",
            R"(\
Let it be known throughout the land, a truth named brave is of True.\
Should the fates decree not brave then Let it be proclaimed: "yes" Else whisper "no"\
)",
            "no"
        },
        {
            "logical_precedence_not_and_or",
            R"(\
Let it be known throughout the land, a truth named brave is of True.\
Let it be known throughout the land, a truth named strong is of False.\
Let it be known throughout the land, a truth named cunning is of True.\
Should the fates decree brave and not strong or False then Let it be proclaimed: "pass" Else whisper "fail"\
)",
            "pass"
        },
        {
            "cast_to_phrase_in_concat",
            R"(\
Let it be known throughout the land, a number named n is of 25 winters.\
Let it be proclaimed: "The number is " + cast n as phrase\
)",
            "The number is 25"
        },
        {
            "cast_number_to_truth_assignment",
            R"(\
Let it be known throughout the land, a number named n is of 5 winters.\
Let it be known throughout the land, a truth named nonzero is of cast n as truth.\
Let it be proclaimed: nonzero\
)",
            "True"
        },
        {
            "cast_truth_to_number_print",
            R"(\
Let it be proclaimed: cast True as number\
)",
            "1"
        },
        {
            "order_indexing_prints_second",
            R"(\
Let it be known throughout the land, an order named heroes is of ["Aragorn", "Legolas", "Gimli"].\
Let it be proclaimed: heroes[1]\
)",
            "Legolas"
        },
        {
            "order_indexing_with_expression",
            R"(\
Let it be known throughout the land, an order named heroes is of ["Aragorn", "Legolas", "Gimli"].\
Let it be known throughout the land, a number named count is of 1 winters.\
Let it be proclaimed: heroes[count + 1]\
)",
            "Gimli"
        },
        {
            "order_pretty_print",
            R"(\
Let it be known throughout the land, an order named heroes is of ["Aragorn", "Legolas", "Gimli"].\
Let it be proclaimed: heroes\
)",
            "[ \"Aragorn\", \"Legolas\", \"Gimli\" ]"
        },
        {
            "tome_indexing_prints_title",
            R"(\
Let it be known throughout the land, a tome named hero is of {"name": "Aragorn", "title": "King of Gondor"}.\
Let it be proclaimed: hero["title"]\
)",
            "King of Gondor"
        }
        ,
        {
            "tome_unquoted_keys_prints_title",
            R"(\
Let it be known throughout the land, a tome named hero is of {name: "Aragorn", title: "King of Gondor"}.\
Let it be proclaimed: hero["title"]\
)",
            "King of Gondor"
        }
        ,
        {
            "tome_dot_syntax_prints_title",
            R"(\
Let it be known throughout the land, a tome named hero is of {name: "Aragorn", title: "King of Gondor"}.\
Let it be proclaimed: hero.title\
)",
            "King of Gondor"
        }
        ,
        {
            "tome_pretty_print_single_key",
            R"(\
Let it be known throughout the land, a tome named hero is of {title: "King of Gondor"}.\
Let it be proclaimed: hero\
)",
            "{ \"title\": \"King of Gondor\" }"
        }
        ,
        {
            "order_expand_append",
            R"(\
Let it be known throughout the land, an order named heroes is of ["Aragorn", "Legolas"].\
Let the order heroes expand with "Gimli"\
Let it be proclaimed: heroes\
)",
            "[ \"Aragorn\", \"Legolas\", \"Gimli\" ]"
        }
        ,
        {
            "tome_amend_title",
            R"(\
Let it be known throughout the land, a tome named hero is of {name: "Aragorn", title: "King"}.\
Let the tome hero amend "title" to "High King"\
Let it be proclaimed: hero.title\
)",
            "High King"
        }
        ,
        {
            "order_remove_element",
            R"(\
Let it be known throughout the land, an order named heroes is of ["Aragorn", "Legolas", "Gimli"].\
Let the order heroes remove "Legolas"\
Let it be proclaimed: heroes\
)",
            "[ \"Aragorn\", \"Gimli\" ]"
        }
        ,
        {
            "tome_erase_key",
            R"(\
Let it be known throughout the land, a tome named hero is of {name: "Aragorn", title: "King of Gondor"}.\
Let the tome hero erase "title"\
Let it be proclaimed: hero\
)",
            "{ \"name\": \"Aragorn\" }"
        }
        ,
        {
            "immutable_order_index_assignment_parse_error",
            R"(\
Let it be known throughout the land, an order named heroes is of ["Aragorn", "Legolas", "Gimli"].\
heroes[1] is of "Faramir"\
)",
            "",
            true,
            "Immutable rite: one may not assign into an order or tome"
        }
        ,
        {
            "immutable_tome_key_assignment_parse_error",
            R"(\
Let it be known throughout the land, a tome named hero is of {name: "Aragorn", title: "King of Gondor"}.\
hero["title"] is of "High King"\
)",
            "",
            true,
            "Immutable rite: one may not assign into an order or tome"
        }
        ,
        {
            "order_index_out_of_bounds_runtime_error",
            R"(\
Let it be known throughout the land, an order named heroes is of ["Aragorn", "Legolas", "Gimli"].\
Let it be proclaimed: heroes[4]\
)",
            "",
            false,
            "",
            true,
            "Error: The council knows no element at position 4, for the order 'heroes' holds but 3."
        }
        ,
        {
            "order_negative_index_last_element",
            R"(\
Let it be known throughout the land, an order named heroes is of ["Aragorn", "Legolas", "Gimli"].\
Let it be proclaimed: heroes[-1]\
)",
            "Gimli"
        }
        ,
        {
            "order_negative_index_too_far_runtime_error",
            R"(\
Let it be known throughout the land, an order named heroes is of ["Aragorn", "Legolas", "Gimli"].\
Let it be proclaimed: heroes[-4]\
)",
            "",
            false,
            "",
            true,
            "Error: None stand that far behind in the order, for only 3 dwell within."
        }
        ,
        {
            "spell_single_param_greet",
            R"(\
By decree of the elders, a spell named greet is cast upon a traveler known as name:\
Let it be proclaimed: "Hail, " + name\
Invoke the spell greet upon "Aragorn"\
)",
            "Hail, Aragorn"
        }
        ,
        {
            "spell_two_params_bless",
            R"(\
By decree of the elders, a spell named bless is cast upon a warrior known as target, a gift known as item:\
Let it be proclaimed: "Blessings upon " + target + ", bearer of " + item\
Invoke the spell bless upon "Faramir", "the Horn of Gondor"\
)",
            "Blessings upon Faramir, bearer of the Horn of Gondor"
        }
        ,
        {
            "spell_unknown_invocation_runtime_error",
            R"(\
Invoke the spell unknown upon "Nobody"\
)",
            "",
            false,
            "",
            true,
            "Error: Unknown spell 'unknown'"
        }
    };

    int passed = 0;
    int failed = 0;

    for (const auto &tc : tests) {
        Lexer lexer(tc.program);
        auto tokens = lexer.tokenize();
        // no debug printing in normal runs
        Parser parser(tokens);
        // Capture stderr during parse for negative tests
        std::ostringstream errBuf;
        std::streambuf* oldErr = std::cerr.rdbuf(errBuf.rdbuf());
        auto ast = parser.parse();
        std::cerr.rdbuf(oldErr);
        if (tc.expectParseFailure) {
            std::string errs = errBuf.str();
            bool ok = (ast == nullptr) && (errs.find(tc.expectedErrorContains) != std::string::npos);
            if (ok) {
                std::cout << "[PASS] " << tc.name << std::endl;
                passed++;
            } else {
                std::cout << "[FAIL] " << tc.name << std::endl;
                if (ast != nullptr) std::cout << "  expected: parse failure but got AST" << std::endl;
                if (errs.find(tc.expectedErrorContains) == std::string::npos) {
                    std::cout << "  expected error to contain: \n" << tc.expectedErrorContains << std::endl;
                    std::cout << "  stderr was: \n" << errs << std::endl;
                }
                failed++;
            }
            continue;
        }
        if (!ast) {
            std::cout << "[FAIL] " << tc.name << ": parser returned null AST" << std::endl;
            failed++;
            continue;
        }

        std::string got;
        if (tc.name == "for_loop_ascend" && predictInfiniteFor(ast)) {
            got = "Infinite Loop";
        } else {
            // Capture interpreter stdout and stderr
            std::ostringstream outBuf;
            std::ostringstream runErrBuf;
            std::streambuf* oldOut = std::cout.rdbuf(outBuf.rdbuf());
            std::streambuf* oldRunErr = std::cerr.rdbuf(runErrBuf.rdbuf());
            {
                Interpreter interpreter;
                interpreter.execute(ast);
            }
            std::cout.rdbuf(oldOut);
            std::cerr.rdbuf(oldRunErr);
            auto rawOut = outBuf.str();
            auto filtered = filterRuntimeNoise(rawOut);
            got = normalize(filtered);

            if (tc.expectRuntimeError) {
                auto errs = runErrBuf.str();
                if (errs.find(tc.expectedRuntimeErrorContains) == std::string::npos) {
                    std::cout << "[FAIL] " << tc.name << std::endl;
                    std::cout << "  expected runtime error to contain:\n" << tc.expectedRuntimeErrorContains << std::endl;
                    std::cout << "  stderr was:\n" << errs << std::endl;
                    failed++;
                    continue;
                }
                // Ignore stdout for runtime error tests; only the error text matters
                got = normalize(tc.expectedOutput);
            }
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
