#pragma once

// The full VERA grammar in GBNF, as accepted by llama.cpp.  Vendored verbatim
// from llama-cli/src/vera_grammar.cpp (kFullGbnf): it carries no EOF terminal,
// which llama.cpp's GBNF parser rejects.  Used by the harness --gbnf mode.
constexpr const char * kFullGbnf = R"GBNF(root ::= ws module
module ::= import* declaration*
import ::= "import" ws "*" ws "as" gap identifier gap "from" gap path
path ::= string-literal
declaration ::= ("export" gap)? (function-declaration | class-declaration)
function-declaration ::= ("async" gap)? "function" gap identifier signature block
signature ::= "(" ws parameter-list? ")" ws ":" ws type
parameter-list ::= parameter ("," ws parameter)*
parameter ::= identifier ":" ws type
block ::= "{" ws (local-declaration | statement)* "}" ws
local-declaration ::= "let" gap variable-declaration
variable-declaration ::= identifier ":" ws type initializer ";" ws
initializer ::= "=" ws expression
class-declaration ::= annotation* "class" gap identifier "{" ws field* "}" ws
field ::= identifier ":" ws type ";" ws
type ::= (basic-type | qualified-name | function-type | parenthesized-type) ("[" ws "]" ws)*
basic-type ::= ("int" | "number" | "boolean" | "string" | "null") ws
qualified-name ::= identifier ("." ws identifier)?
parenthesized-type ::= "(" ws type ("|" ws "null" ws)? ")" ws
function-type ::= async-function-type | non-async-function-type
async-function-type ::= "async" gap "function" ws signature
non-async-function-type ::= "function" ws signature
statement ::= if-statement | while-statement | for-of-statement | break-statement | continue-statement | return-statement | try-statement | simple-statement
if-statement ::= "if" gap "(" ws expression ")" ws block ("else" ws (if-statement | block))?
while-statement ::= "while" gap "(" ws expression ")" ws block
for-of-statement ::= "for" gap "(" ws "let" gap identifier ":" ws type "of" gap expression ")" ws block
break-statement ::= "break" ws ";" ws
continue-statement ::= "continue" ws ";" ws
return-statement ::= "return" gap expression ";" ws
try-statement ::= "try" gap block ws "catch" gap "(" ws identifier ws ")" ws block 
simple-statement ::= expression ("=" ws expression)? ";" ws
expression ::= logical-or-expression
logical-or-expression ::= logical-and-expression ("||" ws logical-and-expression)*
logical-and-expression ::= equality-expression ("&&" ws equality-expression)*
equality-expression ::= relational-expression (equality-op relational-expression)?
relational-expression ::= additive-expression (relational-op additive-expression)?
additive-expression ::= multiplicative-expression (additive-op multiplicative-expression)*
multiplicative-expression ::= unary-expression (multiplicative-op unary-expression)*
unary-expression ::= unary-op unary-expression | primary-expression
primary-expression ::= operand (arguments | index | selector | ensure-not-null)*
unary-op ::= ("-" ws | "!" ws | "await" gap)
equality-op ::= ("===" | "!==") ws
relational-op ::= ("<=" | ">=" | "<" | ">") ws
additive-op ::= ("+" | "-") ws
multiplicative-op ::= ("*" | "/" | "%") ws
operand ::= literal | identifier | array-literal | object-literal | "(" ws expression ")" ws | lambda-expression
literal ::= boolean-literal | numeric-literal | string-literal | "null" ws
boolean-literal ::= ("false" | "true") ws
lambda-expression ::= ("async" gap)? "function" ws signature block
arguments ::= "(" ws expression-list? ")" ws
expression-list ::= expression ("," ws expression)*
index ::= "[" ws expression "]" ws
selector ::= "." ws identifier
ensure-not-null ::= "!" ws
array-literal ::= "[" ws expression-list? "]" ws
object-literal ::= "{" ws property-value-list? "}" ws
property-value-list ::= property-value ("," ws property-value)*
property-value ::= identifier ":" ws expression
annotation ::= "@" identifier
identifier ::= [a-zA-Z_] [a-zA-Z_$0-9]* ws
numeric-literal ::= decimal-literal numeric-suffix? ws
numeric-suffix ::= fractional-part exponent-part? | exponent-part
decimal-literal ::= [0-9]+
fractional-part ::= "." [0-9]+
exponent-part ::= [eE] [+-]? [0-9]+
string-literal ::= "\"" double-quote-character* "\"" ws
double-quote-character ::= [^"\\\r\n] | "\\" escape-sequence
escape-sequence ::= ['"nrt0\\]
whitespace ::= ([ \t\r\n] | line-comment)
ws ::= whitespace*
line-comment ::= "//" [^\r\n]* ("\r\n" | "\n" | "\r")?
gap ::= whitespace+
)GBNF";
