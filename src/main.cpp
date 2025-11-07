#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include <memory>
#include <vector>

int main() {
    std::string input =  R"ARDENT(
    Let it be proclaimed: "--- Core Demo ---"
    Let it be known throughout the land, a number named ct is of 0 winters.  
    Let it be known throughout the land, a number named count is of -3 winters.
    Let it be known throughout the land, a phrase named greeting is of "Hello, world!".  
    Let it be proclaimed: greeting + " How art thou?"
   
    Let it be known throughout the land, a truth named flag is of True.
    Let it be proclaimed: True
    Let it be proclaimed: flag
    Let it be known throughout the land, a truth named off is of False.
    Let it be proclaimed: off

    Let it be known throughout the land, a truth named brave is of True.
    Let it be known throughout the land, a truth named strong is of False.
    Should the fates decree brave and strong then Let it be proclaimed: "and-ok" Else whisper "and-nay"
    Should the fates decree brave or strong then Let it be proclaimed: "or-ok" Else whisper "or-nay"
    Should the fates decree not brave then Let it be proclaimed: "not-yes" Else whisper "not-no"
    Should the fates decree brave and not strong or False then Let it be proclaimed: "prec-pass" Else whisper "prec-fail"

    Let it be known throughout the land, a number named age is of 18 winters.
    Should the fates decree that age is equal to 18 then Let it be proclaimed: "Aye!" Else whisper "Nay!"
    Let it be known throughout the land, a number named cnt is of 0 winters.
    Should the fates decree that cnt is not 0 then Let it be proclaimed: "Not zero!" Else whisper "Zero!"
    Let it be known throughout the land, a number named x is of 7 winters.
    Should the fates decree that x is greater than 3 then Let it be proclaimed: "x>3" Else whisper "x<=3"
    Should the fates decree that x is lesser than 10 then Let it be proclaimed: "x<10" Else whisper "x>=10"


    Let it be known throughout the land, a number named n is of 25 winters.
    Let it be known throughout the land, a phrase named msg is of "The number is ".
    Let it be proclaimed: msg + cast n as phrase

    Let it be known throughout the land, a truth named nonzero is of cast n as truth.
    Let it be proclaimed: nonzero

    Let it be proclaimed: cast True as number

    Let it be known throughout the land, an order named heroes is of ["Aragorn", "Legolas", "Gimli"].
    Let it be proclaimed: heroes[1]
    Let it be proclaimed: heroes[ct+2]
    Let it be proclaimed: heroes[-1]
    Let it be proclaimed: heroes[-5]
    Let it be known throughout the land, a tome named hero is of {"name": "Aragorn", "title": "King of Gondor"}.
    Let it be proclaimed: hero["title"]
    Let it be proclaimed: hero.title

    Let it be proclaimed: "--- Collection Rites Demo ---"
    Let it be known throughout the land, an order named moreHeroes is of ["Boromir", "Frodo"].
    Let it be proclaimed: moreHeroes
    Let the order moreHeroes expand with "Sam"
    Let it be proclaimed: moreHeroes
    Let the order moreHeroes remove "Boromir"
    Let it be proclaimed: moreHeroes

    Let it be known throughout the land, a tome named realm is of {name: "Gondor", ruler: "Steward"}.
    Let it be proclaimed: realm
    Let the tome realm amend "ruler" to "Aragorn"
    Let it be proclaimed: realm.ruler
    Let the tome realm erase "name"
    Let it be proclaimed: realm

    Let it be proclaimed: "(After attempting to remove absent element)"
    Let the order moreHeroes remove "Boromir"
    Let it be proclaimed: moreHeroes
    Let it be proclaimed: "(After attempting to erase missing key)"
    Let the tome realm erase "lineage"
    Let it be proclaimed: realm

    Let it be proclaimed: "--- Spell Demo ---"
    By decree of the elders a spell named greet is cast upon a traveler known as name:
        Let it be proclaimed: "Hail, noble " + name + "!"
    Invoke the spell greet upon "Aragorn"
    By decree of the elders a spell named bless is cast upon a warrior known as name:
        Let it be proclaimed: "Blessings upon thee, " + name + "."
    Invoke the spell bless upon "Faramir"
    
    By decree of the elders, a spell named bestow is cast upon a warrior known as target, a gift known as item:
        Let it be proclaimed: "Blessings upon " + target + ", bearer of " + item
    Invoke the spell bestow upon "Faramir", "the Horn of Gondor"
    
    Let it be proclaimed: "--- Return Spell Demo ---"
    By decree of the elders, a spell named bless is cast upon a warrior known as name:
        Let it be proclaimed: "Blessing " + name
        And let it return "Blessed " + name
    Let it be proclaimed: Invoke the spell bless upon "Boromir"
    Let it be known throughout the land, a phrase named result is of Invoke the spell bless upon "Gimli".
    Let it be proclaimed: result
    )ARDENT";

    // Append Scope & Shadowing Demo (non-destructive)
    input += R"ARDENT(
    
    Let it be proclaimed: "--- Scoping & Shadowing Demo ---"
    Let it be known throughout the land, a phrase named name is of "Outer".
    By decree of the elders, a spell named echo is cast upon a traveler known as name:
        Let it be proclaimed: "Inner sees " + name
    Invoke the spell echo upon "Inner"
    Let it be proclaimed: name

    Let it be proclaimed: "--- Spell Locals Isolation Demo ---"
    By decree of the elders, a spell named forge is cast upon a traveler known as who:
        Let it be known throughout the land, a phrase named temp is of "Secret".
        Let it be proclaimed: "Crafting for " + who
    Invoke the spell forge upon "Rune"
    Let it be proclaimed: temp

    Let it be proclaimed: "--- Return Non-Effect Demo ---"
    Let it be known throughout the land, a phrase named result is of "Start".
    By decree of the elders, a spell named giver is cast upon a warrior known as result:
        And let it return "Gifted " + result
    Let it be proclaimed: Invoke the spell giver upon "Inner"
    Let it be proclaimed: result

    Let it be proclaimed: "--- Global Persistence After Loop Demo ---"
    Let it be known throughout the land, a number named outer is of 0 winters.
    Whilst the sun doth rise outer remaineth below 3 so shall these words be spoken
    outer
    let outer ascend 1
    Let it be proclaimed: outer
    )ARDENT";

    // Imported Scrolls demo (non-destructive append)
    input += R"ARDENT(

    Let it be proclaimed: "--- Imported Scrolls Demo ---"
    From the scroll of "heroes.ardent" draw all knowledge.
    Invoke the spell greet upon "Aragorn"

    Let it be proclaimed: "--- Selective Import Demo ---"
    From the scroll of "spells.ardent" take the spells bless, bestow.
    Let it be proclaimed: Invoke the spell bless upon "Boromir"

    Let it be proclaimed: "--- Alias Import Demo ---"
    From the scroll of "alchemy.ardent" draw all knowledge as alch.
    Invoke the spell alch.transmute upon "lead", "gold"

    Let it be proclaimed: "--- Unfurl Include Demo ---"
    Unfurl the scroll "legends/warriors.ardent".
    Let it be proclaimed: who
    )ARDENT";
    
    // Native Bridge demo (non-destructive append)
    input += R"ARDENT(

    Let it be proclaimed: "--- Native Bridge Demo ---"
    Let it be proclaimed: "Sum is " + Invoke the spirit of math.add upon 2, 3
    Let it be known throughout the land, a number named s is of Invoke the spirit of math.add upon 10, 20.
    Let it be proclaimed: s
    Let it be proclaimed: "Len of 'abc' is " + Invoke the spirit of system.len upon "abc"
    )ARDENT";
    
    // Exception Rites demo (non-destructive append)
    input += R"ARDENT(

    Let it be proclaimed: "--- Exception Rites Demo ---"
    Try:
    Invoke the spirit of math.divide upon 10, 0
    Catch the curse as omen:
    Let it be proclaimed: "Caught: " + omen

    Try:
    Invoke the spirit of math.add upon 2, 3
    Catch the curse as omen:
    Let it be proclaimed: "Should not happen"
    Finally:
    Let it be proclaimed: "All is well."

    Let it be proclaimed: "--- Nested Try Demo ---"
    Try:
    Try:
    Invoke the spirit of math.divide upon 1, 0
    Catch the curse as omen:
    Let it be proclaimed: "Inner: " + omen
    Catch the curse as outer:
    Let it be proclaimed: "Outer: " + outer
    )ARDENT";
    

    Lexer lexer(input);
    std::vector<Token> tokens = lexer.tokenize();

    std::cout << "=== Tokens Generated ===" << std::endl;
    for (const auto& token : tokens) {
        std::cout << "Token: " << token.value << ", Type: " << tokenTypeToString(token.type) << std::endl;
    }

    Parser parser(tokens);
    auto ast = parser.parse();
if (!ast) {
    std::cerr << "Error: Parser returned NULL AST!" << std::endl;
    return 1;
}

std::cout << "=== AST Debug Output ===" << std::endl;
std::cout << typeid(*ast).name() << std::endl;  // Print AST node type

std::cout << "Parsing complete!" << std::endl;


    Interpreter interpreter;
    interpreter.execute(ast);

    return 0;
}



