#ifndef ARDENT_AVM_COMPILER_AVM_H
#define ARDENT_AVM_COMPILER_AVM_H

#include <unordered_map>
#include <string>
#include <vector>
#include "bytecode.h"
#include "opcode.h"
#include "ast.h"

namespace avm {

// Simple symbol table for locals/globals (single flat scope for initial phase).
class SymbolTable {
public:
    uint16_t ensureSlot(const std::string& name) {
        auto it = slots_.find(name);
        if (it != slots_.end()) return it->second;
        uint16_t id = static_cast<uint16_t>(slots_.size());
        slots_[name] = id;
        order_.push_back(name);
        return id;
    }
    bool lookup(const std::string& name, uint16_t& out) const {
        auto it = slots_.find(name);
        if (it == slots_.end()) return false;
        out = it->second; return true;
    }
private:
    std::unordered_map<std::string,uint16_t> slots_;
    std::vector<std::string> order_;
};

class CompilerAVM {
public:
    CompilerAVM() = default;
    Chunk compile(std::shared_ptr<ASTNode> root) {
        // Fresh emitter per compilation but persistent symbol table for REPL
        emitter_ = BytecodeEmitter();
        patches_.clear();
        emitNode(root);
        emitter_.emit(OpCode::OP_HALT);
        return emitter_.build();
    }

private:
    BytecodeEmitter emitter_;
    SymbolTable symbols_;

    void emitNode(const std::shared_ptr<ASTNode>& n) {
        if (!n) return;
        // Dispatch based on dynamic type via casts
        if (auto lit = std::dynamic_pointer_cast<Expression>(n)) {
            emitExpression(lit);
        } else if (auto bin = std::dynamic_pointer_cast<BinaryExpression>(n)) {
            emitBinary(bin);
        } else if (auto un = std::dynamic_pointer_cast<UnaryExpression>(n)) {
            emitUnary(un);
        } else if (auto cast = std::dynamic_pointer_cast<CastExpression>(n)) {
            emitNode(cast->operand); // For now no-op conversion; placeholder
        } else if (auto blk = std::dynamic_pointer_cast<BlockStatement>(n)) {
            for (auto &s : blk->statements) emitNode(s);
        } else if (auto pr = std::dynamic_pointer_cast<PrintStatement>(n)) {
            emitNode(pr->expression);
            emitter_.emit(OpCode::OP_PRINT);
        } else if (auto ret = std::dynamic_pointer_cast<ReturnStatement>(n)) {
            emitNode(ret->expression);
            emitter_.emit(OpCode::OP_RET);
        } else if (auto ifs = std::dynamic_pointer_cast<IfStatement>(n)) {
            emitIf(ifs);
        } else {
            // Unhandled node types are ignored in initial compiler stage.
        }
    }

    void emitExpression(const std::shared_ptr<Expression>& e) {
        // Determine literal vs identifier
        if (e->token.type == TokenType::NUMBER) {
            int value = std::stoi(e->token.value);
            uint16_t idx = emitter_.addConst(value);
            emitter_.emit_push_const(idx);
        } else if (e->token.type == TokenType::STRING) {
            uint16_t idx = emitter_.addConst(e->token.value);
            emitter_.emit_push_const(idx);
        } else if (e->token.type == TokenType::BOOLEAN) {
            bool bv = (e->token.value == "True" || e->token.value == "true" || e->token.value == "TRUE");
            uint16_t idx = emitter_.addConst(bv);
            emitter_.emit_push_const(idx);
        } else if (e->token.type == TokenType::IDENTIFIER) {
            uint16_t slot;
            if (!symbols_.lookup(e->token.value, slot)) {
                slot = symbols_.ensureSlot(e->token.value);
                // Implicit default initialization: push 0 for numbers? For now treat as 0.
                uint16_t ci = emitter_.addConst(0);
                emitter_.emit_push_const(ci);
                emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(slot);
            }
            emitter_.emit(OpCode::OP_LOAD); emitter_.emit_u16(slot);
        } else {
            // Fallback: treat as string literal of its token
            uint16_t idx = emitter_.addConst(e->token.value);
            emitter_.emit_push_const(idx);
        }
    }

    void emitBinary(const std::shared_ptr<BinaryExpression>& b) {
        // For now, detect assignment vs arithmetic by operator token type.
        if (b->op.type == TokenType::IS_OF) {
            // Declaration/assignment: left must be identifier token expression.
            if (auto lhs = std::dynamic_pointer_cast<Expression>(b->left)) {
                std::string name = lhs->token.value;
                uint16_t slot = symbols_.ensureSlot(name);
                emitNode(b->right);
                emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(slot);
                // Push stored value as expression result (load slot)
                emitter_.emit(OpCode::OP_LOAD); emitter_.emit_u16(slot);
            } else {
                emitNode(b->right); // degrade: still evaluate RHS
            }
            return;
        }
        // Otherwise treat as arithmetic / logical / comparison
        emitNode(b->left);
        emitNode(b->right);
        if (b->op.type == TokenType::OPERATOR) {
            const std::string& op = b->op.value;
            if (op == "+") emitter_.emit(OpCode::OP_ADD);
            else if (op == "-") emitter_.emit(OpCode::OP_SUB);
            else if (op == "*") emitter_.emit(OpCode::OP_MUL);
            else if (op == "/") emitter_.emit(OpCode::OP_DIV);
            else emitter_.emit(OpCode::OP_POP);
        } else if (b->op.type == TokenType::AND) {
            emitter_.emit(OpCode::OP_AND);
        } else if (b->op.type == TokenType::OR) {
            emitter_.emit(OpCode::OP_OR);
        } else if (b->op.type == TokenType::EQUAL) {
            emitter_.emit(OpCode::OP_EQ);
        } else if (b->op.type == TokenType::NOT_EQUAL) {
            emitter_.emit(OpCode::OP_NE);
        } else if (b->op.type == TokenType::GREATER || b->op.type == TokenType::SURPASSETH) {
            emitter_.emit(OpCode::OP_GT);
        } else if (b->op.type == TokenType::LESSER || b->op.type == TokenType::REMAINETH) {
            emitter_.emit(OpCode::OP_LT);
        } else {
            // Fallback for unhandled
            emitter_.emit(OpCode::OP_POP);
        }
    }

    void emitUnary(const std::shared_ptr<UnaryExpression>& u) {
        emitNode(u->operand);
        if (u->op.type == TokenType::NOT) {
            emitter_.emit(OpCode::OP_NOT);
        }
    }

    void emitIf(const std::shared_ptr<IfStatement>& ifs) {
        // condition
        emitNode(ifs->condition);
        // Emit conditional jump placeholder
        emitter_.emit(OpCode::OP_JMP_IF_FALSE);
        size_t operandPosFalse = emitter_.codeSize(); // location where operand starts
        emitter_.emit_u16(0); // placeholder
        patches_.push_back({operandPosFalse});
        size_t patchIndexFalse = patches_.size() - 1;
        // then branch
        emitNode(ifs->thenBranch);
        // Jump over else
        emitter_.emit(OpCode::OP_JMP);
        size_t operandPosEnd = emitter_.codeSize();
        emitter_.emit_u16(0);
        patches_.push_back({operandPosEnd});
        size_t patchIndexEnd = patches_.size() - 1;
        // Patch false jump target to current position (start of else)
        patchOperand(patchIndexFalse);
        // else branch
        emitNode(ifs->elseBranch);
        // Patch end jump target
        patchOperand(patchIndexEnd);
    }

    size_t emitterPosition() const { return emitter_.codeSize(); }
    struct PendingPatch { size_t operandOffset; }; // location of first byte of u16 operand
    std::vector<PendingPatch> patches_;

    void patchOperand(size_t patchVectorIndex) {
        if (patchVectorIndex >= patches_.size()) return;
        auto off = patches_[patchVectorIndex].operandOffset;
        uint16_t rel = static_cast<uint16_t>(emitter_.codeSize() - (off + 2)); // distance from end of operand to target
        auto &code = const_cast<std::vector<uint8_t>&>(emitter_.codeRef());
        code[off] = static_cast<uint8_t>(rel & 0xFF);
        code[off+1] = static_cast<uint8_t>((rel >> 8) & 0xFF);
    }
};

} // namespace avm

#endif // ARDENT_AVM_COMPILER_AVM_H
