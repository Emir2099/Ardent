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
        } 
        // ============================================================
        // Collection Operations (Ardent 3.1+)
        // ============================================================
        else if (auto forEach = std::dynamic_pointer_cast<ForEachStmt>(n)) {
            emitForEach(forEach);
        } else if (auto contains = std::dynamic_pointer_cast<ContainsExpr>(n)) {
            emitContains(contains);
        } else if (auto where = std::dynamic_pointer_cast<WhereExpr>(n)) {
            emitWhere(where);
        } else if (auto transform = std::dynamic_pointer_cast<TransformExpr>(n)) {
            emitTransform(transform);
        } else if (auto idxAssign = std::dynamic_pointer_cast<IndexAssignStmt>(n)) {
            emitIndexAssign(idxAssign);
        } else if (auto arrLit = std::dynamic_pointer_cast<ArrayLiteral>(n)) {
            emitArrayLiteral(arrLit);
        } else if (auto objLit = std::dynamic_pointer_cast<ObjectLiteral>(n)) {
            emitObjectLiteral(objLit);
        } else if (auto idx = std::dynamic_pointer_cast<IndexExpression>(n)) {
            emitIndexExpression(idx);
        } else if (auto varDecl = std::dynamic_pointer_cast<VariableDeclaration>(n)) {
            emitVariableDeclaration(varDecl);
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

    // ============================================================
    // Collection Operations (Ardent 3.1+ - Forged Verses 3.2 VM)
    // ============================================================
    
    void emitForEach(const std::shared_ptr<ForEachStmt>& forEach) {
        // Emit collection expression, then create iterator
        emitNode(forEach->collection);
        emitter_.emit(OpCode::OP_ITER_INIT);
        
        // Store iterator in a temporary slot
        uint16_t iterSlot = symbols_.ensureSlot("__iter__" + std::to_string(emitter_.codeSize()));
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(iterSlot);
        
        // Loop start position
        size_t loopStart = emitter_.codeSize();
        
        // Load iterator and get next
        emitter_.emit(OpCode::OP_LOAD); emitter_.emit_u16(iterSlot);
        
        OpCode nextOp = forEach->hasTwoVars ? OpCode::OP_ITER_KV_NEXT : OpCode::OP_ITER_NEXT;
        emitter_.emit(nextOp);
        size_t exitJumpOperand = emitter_.codeSize();
        emitter_.emit_u16(0); // placeholder for exit jump
        patches_.push_back({exitJumpOperand});
        size_t exitPatchIdx = patches_.size() - 1;
        
        // Store updated iterator
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(iterSlot);
        
        if (forEach->hasTwoVars) {
            // Stack has: key, value - store value first, then key
            uint16_t valSlot = symbols_.ensureSlot(forEach->valueVar);
            emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(valSlot);
            uint16_t keySlot = symbols_.ensureSlot(forEach->iterVar);
            emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(keySlot);
        } else {
            // Stack has: value - store it
            uint16_t varSlot = symbols_.ensureSlot(forEach->iterVar);
            emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(varSlot);
        }
        
        // Emit body
        emitNode(forEach->body);
        
        // Jump back to loop start
        emitter_.emit(OpCode::OP_JMP);
        // Compute relative offset (backward jump)
        size_t currentPos = emitter_.codeSize();
        int32_t backOffset = -static_cast<int32_t>(currentPos + 2 - loopStart);
        emitter_.emit_u16(static_cast<uint16_t>(backOffset & 0xFFFF));
        
        // Patch exit jump
        patchOperand(exitPatchIdx);
    }
    
    void emitContains(const std::shared_ptr<ContainsExpr>& contains) {
        // Push needle, then haystack, then CONTAINS
        emitNode(contains->needle);
        emitNode(contains->haystack);
        emitter_.emit(OpCode::OP_CONTAINS);
    }
    
    void emitWhere(const std::shared_ptr<WhereExpr>& where) {
        // Desugar to: create empty result order, iterate source, filter
        // Push empty order
        emitter_.emit(OpCode::OP_MAKE_ORDER); emitter_.emit_u16(0);
        uint16_t resultSlot = symbols_.ensureSlot("__where_result__" + std::to_string(emitter_.codeSize()));
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(resultSlot);
        
        // Initialize iterator on source
        emitNode(where->source);
        emitter_.emit(OpCode::OP_ITER_INIT);
        uint16_t iterSlot = symbols_.ensureSlot("__where_iter__" + std::to_string(emitter_.codeSize()));
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(iterSlot);
        
        // Loop start
        size_t loopStart = emitter_.codeSize();
        emitter_.emit(OpCode::OP_LOAD); emitter_.emit_u16(iterSlot);
        emitter_.emit(OpCode::OP_ITER_NEXT);
        size_t exitJumpOperand = emitter_.codeSize();
        emitter_.emit_u16(0);
        patches_.push_back({exitJumpOperand});
        size_t exitPatchIdx = patches_.size() - 1;
        
        // Store iterator
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(iterSlot);
        
        // Store current element in iter variable
        uint16_t elemSlot = symbols_.ensureSlot(where->iterVar);
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(elemSlot);
        
        // Evaluate predicate
        emitNode(where->predicate);
        
        // If false, jump to loop continue
        emitter_.emit(OpCode::OP_JMP_IF_FALSE);
        size_t skipPushOperand = emitter_.codeSize();
        emitter_.emit_u16(0);
        patches_.push_back({skipPushOperand});
        size_t skipPatchIdx = patches_.size() - 1;
        
        // Predicate true: append element to result
        emitter_.emit(OpCode::OP_LOAD); emitter_.emit_u16(resultSlot);
        emitter_.emit(OpCode::OP_LOAD); emitter_.emit_u16(elemSlot);
        emitter_.emit(OpCode::OP_ORDER_PUSH);
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(resultSlot);
        
        // Patch skip jump
        patchOperand(skipPatchIdx);
        
        // Jump back to loop start
        emitter_.emit(OpCode::OP_JMP);
        size_t currentPos = emitter_.codeSize();
        int32_t backOffset = -static_cast<int32_t>(currentPos + 2 - loopStart);
        emitter_.emit_u16(static_cast<uint16_t>(backOffset & 0xFFFF));
        
        // Patch exit jump
        patchOperand(exitPatchIdx);
        
        // Load result onto stack
        emitter_.emit(OpCode::OP_LOAD); emitter_.emit_u16(resultSlot);
    }
    
    void emitTransform(const std::shared_ptr<TransformExpr>& transform) {
        // Similar to where, but always push transformed value
        emitter_.emit(OpCode::OP_MAKE_ORDER); emitter_.emit_u16(0);
        uint16_t resultSlot = symbols_.ensureSlot("__transform_result__" + std::to_string(emitter_.codeSize()));
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(resultSlot);
        
        emitNode(transform->source);
        emitter_.emit(OpCode::OP_ITER_INIT);
        uint16_t iterSlot = symbols_.ensureSlot("__transform_iter__" + std::to_string(emitter_.codeSize()));
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(iterSlot);
        
        size_t loopStart = emitter_.codeSize();
        emitter_.emit(OpCode::OP_LOAD); emitter_.emit_u16(iterSlot);
        emitter_.emit(OpCode::OP_ITER_NEXT);
        size_t exitJumpOperand = emitter_.codeSize();
        emitter_.emit_u16(0);
        patches_.push_back({exitJumpOperand});
        size_t exitPatchIdx = patches_.size() - 1;
        
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(iterSlot);
        
        uint16_t elemSlot = symbols_.ensureSlot(transform->iterVar);
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(elemSlot);
        
        // Evaluate transform expression
        emitNode(transform->transform);
        
        // Stack now has: [..., transformedValue]
        // Need to push to result order: ORDER_PUSH expects [order, value] and pushes new order
        uint16_t tempSlot = symbols_.ensureSlot("__temp__" + std::to_string(emitter_.codeSize()));
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(tempSlot); // store transformed value
        emitter_.emit(OpCode::OP_LOAD); emitter_.emit_u16(resultSlot); // load result order
        emitter_.emit(OpCode::OP_LOAD); emitter_.emit_u16(tempSlot);   // load transformed value
        emitter_.emit(OpCode::OP_ORDER_PUSH); // push value to order
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(resultSlot); // store updated order
        
        emitter_.emit(OpCode::OP_JMP);
        size_t currentPos = emitter_.codeSize();
        int32_t backOffset = -static_cast<int32_t>(currentPos + 2 - loopStart);
        emitter_.emit_u16(static_cast<uint16_t>(backOffset & 0xFFFF));
        
        patchOperand(exitPatchIdx);
        emitter_.emit(OpCode::OP_LOAD); emitter_.emit_u16(resultSlot);
    }
    
    void emitIndexAssign(const std::shared_ptr<IndexAssignStmt>& idxAssign) {
        // Get target variable name
        auto targetExpr = std::dynamic_pointer_cast<Expression>(idxAssign->target);
        if (!targetExpr) return;
        std::string varName = targetExpr->token.value;
        uint16_t slot;
        if (!symbols_.lookup(varName, slot)) {
            slot = symbols_.ensureSlot(varName);
        }
        
        // Load target, push index, push value, ORDER_SET, store back
        emitter_.emit(OpCode::OP_LOAD); emitter_.emit_u16(slot);
        emitNode(idxAssign->index);
        emitNode(idxAssign->value);
        emitter_.emit(OpCode::OP_ORDER_SET);
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(slot);
    }
    
    void emitArrayLiteral(const std::shared_ptr<ArrayLiteral>& arr) {
        // Push all elements, then MAKE_ORDER
        for (const auto& elem : arr->elements) {
            emitNode(elem);
        }
        emitter_.emit(OpCode::OP_MAKE_ORDER);
        emitter_.emit_u16(static_cast<uint16_t>(arr->elements.size()));
    }
    
    void emitObjectLiteral(const std::shared_ptr<ObjectLiteral>& obj) {
        // Push key-value pairs, then MAKE_TOME
        for (const auto& [key, val] : obj->entries) {
            uint16_t keyIdx = emitter_.addConst(key);
            emitter_.emit_push_const(keyIdx);
            emitNode(val);
        }
        emitter_.emit(OpCode::OP_MAKE_TOME);
        emitter_.emit_u16(static_cast<uint16_t>(obj->entries.size()));
    }
    
    void emitIndexExpression(const std::shared_ptr<IndexExpression>& idx) {
        emitNode(idx->target);
        emitNode(idx->index);
        // Determine if target is Order or Tome at compile time (or emit generic GET)
        emitter_.emit(OpCode::OP_ORDER_GET); // Default to order; runtime will handle tome
    }
    
    void emitVariableDeclaration(const std::shared_ptr<VariableDeclaration>& decl) {
        uint16_t slot = symbols_.ensureSlot(decl->varName);
        if (decl->initializer) {
            emitNode(decl->initializer);
        } else {
            emitter_.emit_push_const(emitter_.addConst(0));
        }
        emitter_.emit(OpCode::OP_STORE); emitter_.emit_u16(slot);
        // Leave value on stack as expression result
        emitter_.emit(OpCode::OP_LOAD); emitter_.emit_u16(slot);
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
