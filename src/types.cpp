#include "types.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace ardent {

// Convert TypeKind to human-readable string
std::string typeKindToString(TypeKind k) {
    switch (k) {
        case TypeKind::Unknown: return "unknown";
        case TypeKind::Whole:   return "whole";
        case TypeKind::Truth:   return "truth";
        case TypeKind::Phrase:  return "phrase";
        case TypeKind::Order:   return "order";
        case TypeKind::Tome:    return "tome";
        case TypeKind::Void:    return "void";
        case TypeKind::Any:     return "any";
        case TypeKind::Spell:   return "spell";
    }
    return "unknown";
}

// Convert Type to diagnostic string
std::string typeToString(const Type& ty) {
    switch (ty.kind) {
        case TypeKind::Unknown: return "unknown";
        case TypeKind::Whole:   return "whole";
        case TypeKind::Truth:   return "truth";
        case TypeKind::Phrase:  return "phrase";
        case TypeKind::Void:    return "void";
        case TypeKind::Any:     return "any";

        case TypeKind::Order: {
            std::ostringstream os;
            os << "order<";
            if (!ty.params.empty()) {
                os << typeToString(ty.params[0]);
            } else {
                os << "unknown";
            }
            os << ">";
            return os.str();
        }

        case TypeKind::Tome: {
            std::ostringstream os;
            os << "tome<";
            if (ty.params.size() >= 2) {
                os << typeToString(ty.params[0]) << ", " << typeToString(ty.params[1]);
            } else if (ty.params.size() == 1) {
                os << typeToString(ty.params[0]) << ", unknown";
            } else {
                os << "unknown, unknown";
            }
            os << ">";
            return os.str();
        }

        case TypeKind::Spell: {
            std::ostringstream os;
            os << "(";
            auto args = ty.argTypes();
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) os << ", ";
                os << typeToString(args[i]);
            }
            os << ") → " << typeToString(ty.returnType());
            return os.str();
        }
    }
    return "unknown";
}

// Parse type rune from identifier string
// Supports: whole, truth, phrase, void, any, order, tome
// Future: order<whole>, tome<phrase, whole>, etc.
std::optional<Type> parseTypeRune(const std::string& rune) {
    // Normalize to lowercase
    std::string lower = rune;
    std::transform(lower.begin(), lower.end(), lower.begin(), 
                   [](unsigned char c) { return std::tolower(c); });

    // Simple types
    if (lower == "whole" || lower == "number" || lower == "integer" || lower == "int") {
        return Type::whole();
    }
    if (lower == "truth" || lower == "boolean" || lower == "bool") {
        return Type::truth();
    }
    if (lower == "phrase" || lower == "string" || lower == "str") {
        return Type::phrase();
    }
    if (lower == "void" || lower == "nothing") {
        return Type::voidTy();
    }
    if (lower == "any" || lower == "dynamic") {
        return Type::any();
    }
    if (lower == "order" || lower == "array" || lower == "list") {
        // Unparameterized order → Order<unknown>
        return Type::order(Type::unknown());
    }
    if (lower == "tome" || lower == "map" || lower == "dict") {
        // Unparameterized tome → Tome<unknown, unknown>
        return Type::tome(Type::unknown(), Type::unknown());
    }

    // Parametric types: order<T>, tome<K, V>
    // Look for '<' 
    auto angleBracket = lower.find('<');
    if (angleBracket != std::string::npos) {
        std::string base = lower.substr(0, angleBracket);
        std::string rest = lower.substr(angleBracket + 1);
        // Remove trailing '>'
        if (!rest.empty() && rest.back() == '>') {
            rest.pop_back();
        }

        if (base == "order" || base == "array" || base == "list") {
            auto elemTy = parseTypeRune(rest);
            if (elemTy) {
                return Type::order(*elemTy);
            }
            return Type::order(Type::unknown());
        }

        if (base == "tome" || base == "map" || base == "dict") {
            // Split by comma
            auto commaPos = rest.find(',');
            if (commaPos != std::string::npos) {
                std::string keyPart = rest.substr(0, commaPos);
                std::string valPart = rest.substr(commaPos + 1);
                // Trim whitespace
                while (!keyPart.empty() && std::isspace(keyPart.back())) keyPart.pop_back();
                while (!valPart.empty() && std::isspace(valPart.front())) valPart.erase(valPart.begin());

                auto keyTy = parseTypeRune(keyPart);
                auto valTy = parseTypeRune(valPart);
                return Type::tome(keyTy.value_or(Type::unknown()), valTy.value_or(Type::unknown()));
            }
            // Single param: treat as value type, key = phrase (default for tomes)
            auto valTy = parseTypeRune(rest);
            return Type::tome(Type::phrase(), valTy.value_or(Type::unknown()));
        }
    }

    // Unknown type rune
    return std::nullopt;
}

// Check if 'source' can be assigned to a variable of type 'target'
bool isAssignableFrom(const Type& target, const Type& source) {
    // Unknown target accepts anything (dynamic behavior)
    if (target.isUnknown()) return true;
    // Any target accepts anything
    if (target.isAny()) return true;
    // Unknown source can be assigned to any target (runtime check)
    if (source.isUnknown()) return true;
    // Any source is compatible with any target
    if (source.isAny()) return true;

    // Same kind check
    if (target.kind != source.kind) return false;

    // For parametric types, check params recursively
    if (target.kind == TypeKind::Order) {
        // Order<unknown> accepts any Order
        if (target.elementType().isUnknown()) return true;
        return isAssignableFrom(target.elementType(), source.elementType());
    }

    if (target.kind == TypeKind::Tome) {
        if (target.keyType().isUnknown() && target.valueType().isUnknown()) return true;
        return isAssignableFrom(target.keyType(), source.keyType()) &&
               isAssignableFrom(target.valueType(), source.valueType());
    }

    if (target.kind == TypeKind::Spell) {
        // Spell arity must match
        if (target.spellArity != source.spellArity) return false;
        // Contravariant params, covariant return
        auto targetArgs = target.argTypes();
        auto sourceArgs = source.argTypes();
        for (size_t i = 0; i < targetArgs.size(); ++i) {
            // Contravariance: source param must be assignable from target param
            if (!isAssignableFrom(sourceArgs[i], targetArgs[i])) return false;
        }
        // Covariance: target return must accept source return
        return isAssignableFrom(target.returnType(), source.returnType());
    }

    // Simple types: exact match (already checked kind equality)
    return true;
}

// Check if two types are compatible (either direction)
bool isCompatible(const Type& a, const Type& b) {
    return isAssignableFrom(a, b) || isAssignableFrom(b, a);
}

// Unify two types into a common supertype
std::optional<Type> unifyTypes(const Type& a, const Type& b) {
    // Same type
    if (a == b) return a;

    // Unknown unifies with anything
    if (a.isUnknown()) return b;
    if (b.isUnknown()) return a;

    // Any unifies to Any
    if (a.isAny() || b.isAny()) return Type::any();

    // Different kinds → can't unify (would need union types)
    if (a.kind != b.kind) return std::nullopt;

    // Parametric types: unify params
    if (a.kind == TypeKind::Order) {
        auto unified = unifyTypes(a.elementType(), b.elementType());
        if (unified) return Type::order(*unified);
        return std::nullopt;
    }

    if (a.kind == TypeKind::Tome) {
        auto keyUnified = unifyTypes(a.keyType(), b.keyType());
        auto valUnified = unifyTypes(a.valueType(), b.valueType());
        if (keyUnified && valUnified) return Type::tome(*keyUnified, *valUnified);
        return std::nullopt;
    }

    // Same simple type
    return a;
}

} // namespace ardent
