#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <vector>
#include <optional>
#include <ostream>

namespace ardent {

/**
 * TypeKind — Core type categories in Ardent's gradual type system.
 * 
 * Unknown  = dynamic (no type rune specified, runtime-checked)
 * Whole    = integer (i64 in LLVM)
 * Truth    = boolean (i1 in LLVM)
 * Phrase   = string
 * Order    = array/list (parametric: Order<T>)
 * Tome     = map/dictionary (parametric: Tome<K,V>)
 * Void     = no value (spell returns nothing)
 * Any      = explicitly dynamic (compatible with all types)
 * Spell    = callable type with signature
 */
enum class TypeKind {
    Unknown,    // dynamic, no rune
    Whole,      // integer
    Truth,      // boolean
    Phrase,     // string
    Order,      // array<T>
    Tome,       // map<K,V>
    Void,       // no return
    Any,        // explicitly dynamic
    Spell       // callable with signature
};

/**
 * Type — Represents a fully resolved or inferred Ardent type.
 * 
 * For simple types (Whole, Truth, Phrase, Void, Any, Unknown):
 *   - params is empty
 * 
 * For Order<T>:
 *   - params[0] = element type T
 * 
 * For Tome<K,V>:
 *   - params[0] = key type K
 *   - params[1] = value type V
 * 
 * For Spell(args...) -> ret:
 *   - params[0..n-2] = argument types
 *   - params[n-1] = return type
 *   - spellArity = number of arguments
 */
struct Type {
    TypeKind kind = TypeKind::Unknown;
    std::vector<Type> params;  // for parametric types / spell signatures
    int spellArity = -1;       // for Spell type: number of arguments (-1 = not a spell)

    // Constructors
    Type() = default;
    explicit Type(TypeKind k) : kind(k) {}
    Type(TypeKind k, std::vector<Type> p) : kind(k), params(std::move(p)) {}
    Type(TypeKind k, std::vector<Type> p, int arity) 
        : kind(k), params(std::move(p)), spellArity(arity) {}

    // Factory methods for common types
    static Type unknown() { return Type(TypeKind::Unknown); }
    static Type whole() { return Type(TypeKind::Whole); }
    static Type truth() { return Type(TypeKind::Truth); }
    static Type phrase() { return Type(TypeKind::Phrase); }
    static Type voidTy() { return Type(TypeKind::Void); }
    static Type any() { return Type(TypeKind::Any); }

    static Type order(Type elemType) {
        return Type(TypeKind::Order, {std::move(elemType)});
    }

    static Type tome(Type keyType, Type valueType) {
        return Type(TypeKind::Tome, {std::move(keyType), std::move(valueType)});
    }

    static Type spell(std::vector<Type> argTypes, Type retType) {
        int arity = static_cast<int>(argTypes.size());
        argTypes.push_back(std::move(retType));
        return Type(TypeKind::Spell, std::move(argTypes), arity);
    }

    // Type queries
    bool isUnknown() const { return kind == TypeKind::Unknown; }
    bool isKnown() const { return kind != TypeKind::Unknown; }
    bool isNumeric() const { return kind == TypeKind::Whole; }
    bool isBoolean() const { return kind == TypeKind::Truth; }
    bool isString() const { return kind == TypeKind::Phrase; }
    bool isVoid() const { return kind == TypeKind::Void; }
    bool isAny() const { return kind == TypeKind::Any; }
    bool isOrder() const { return kind == TypeKind::Order; }
    bool isTome() const { return kind == TypeKind::Tome; }
    bool isSpell() const { return kind == TypeKind::Spell; }

    // For Order<T>: get element type
    Type elementType() const {
        if (kind == TypeKind::Order && !params.empty()) return params[0];
        return Type::unknown();
    }

    // For Tome<K,V>: get key/value types
    Type keyType() const {
        if (kind == TypeKind::Tome && params.size() >= 1) return params[0];
        return Type::unknown();
    }
    Type valueType() const {
        if (kind == TypeKind::Tome && params.size() >= 2) return params[1];
        return Type::unknown();
    }

    // For Spell: get return type (last param)
    Type returnType() const {
        if (kind == TypeKind::Spell && !params.empty()) return params.back();
        return Type::unknown();
    }

    // For Spell: get argument types (all but last)
    std::vector<Type> argTypes() const {
        if (kind == TypeKind::Spell && spellArity >= 0) {
            return std::vector<Type>(params.begin(), params.begin() + spellArity);
        }
        return {};
    }

    // Equality
    bool operator==(const Type& other) const {
        if (kind != other.kind) return false;
        if (params.size() != other.params.size()) return false;
        for (size_t i = 0; i < params.size(); ++i) {
            if (!(params[i] == other.params[i])) return false;
        }
        return spellArity == other.spellArity;
    }
    bool operator!=(const Type& other) const { return !(*this == other); }
};

// Type name parsing from string (for rune syntax)
std::optional<Type> parseTypeRune(const std::string& rune);

// Type to string (for diagnostics and --dump-types)
std::string typeToString(const Type& ty);
std::string typeKindToString(TypeKind k);

// Type compatibility checks
bool isAssignableFrom(const Type& target, const Type& source);
bool isCompatible(const Type& a, const Type& b);

// Unification: find common type if possible
std::optional<Type> unifyTypes(const Type& a, const Type& b);

} // namespace ardent

// Stream output for diagnostics
inline std::ostream& operator<<(std::ostream& os, const ardent::Type& ty) {
    return os << ardent::typeToString(ty);
}

#endif // TYPES_H
