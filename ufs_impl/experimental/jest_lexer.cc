#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <stdexcept>
#include <sstream>

enum CLASSIFICATION {
    WHITESPACE,
    OPERATOR_CNTRL,
    OPERATOR_REDIR,
    DOUBLE_QUOTE_L,
    DOUBLE_QUOTE_R,
    DOUBLE_QUOTED,
    SINGLE_QUOTED,
    UNQUOTED,
    ESCAPED, // \2>out is the same as 2 > out
    NEWLINE,
    DOLLAR,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,

    IN, // unquoted 'in'
    DO, // unquoted 'for'
    DONE, // ... v
    IF,
    ELIF, // and else
    FI
};

// IGNORE: \r
// whitespaces: unescaped [\t, ' ']
// operator: [|, ||, &&, ;, <, >, >>]

// unconditional operation: [|, ||, &&, ;, <]
// conditional operation: [[Unquoted(1,2), '&'] + !Whitespace + [>, >>]]

CLASSIFICATION classify(const char c) {
    switch (c) {
        case ' ': case '\t': return WHITESPACE;
        case '|': case '&': case ';': return OPERATOR_CNTRL;
        case '>': case '<': return OPERATOR_REDIR;
        case '"': return DOUBLE_QUOTED;
        case '\'': return SINGLE_QUOTED;
        case '\\': return ESCAPED;
        
        case '\n': return NEWLINE;
        case '$': return DOLLAR;
        case '(': return LPAREN;
        case ')': return RPAREN;
        case '{': return LBRACE;
        case '}': return RBRACE;
        case '[': return LBRACKET;
        case ']': return RBRACKET;
        
        default: return UNQUOTED;
    }
}

struct Token {
    std::string lexeme;
    CLASSIFICATION id;
};

void flushToken(Token& curr, std::vector<Token>& res) {
    res.emplace_back(curr);
    curr.lexeme.clear();
    curr.id = UNQUOTED;
}

void compress(std::vector<Token>& res) {
    int sz = 0;
    int n = res.size();
    for (int i = 0; i < n; i++) {
        auto& [lexeme, id] = res[i];
        if (lexeme.empty() && (id == UNQUOTED || id == DOUBLE_QUOTED)) continue; // skip empty unquoted
        if (sz > 0 && id == WHITESPACE && res[sz-1].id == id) continue; // skip multiple whitespace
    }
}

// We will not be accepting '`' command expansion
bool isValidChar(char c) {
    if (c == '\n' || c == '\t') return true;
    return (c >= ' ' && c <= '~' && c != '`');
}

void munchSingleQuote(std::istream& fin, int& lineIdx, std::vector<Token>& res) {
    res.emplace_back("", SINGLE_QUOTED);
    auto& lexeme = res.back().lexeme;
    int startingLineIdx = lineIdx;
    char c;

    while (fin.get(c)) {
        if (c == '\r') continue;

        if (!isValidChar(c)) {
            std::ostringstream oss;
            oss << "Invalid character found on line " << lineIdx;
            throw std::runtime_error(oss.str());
        }

        if (c == '\'') return;
        
        if (c == '\n') lineIdx++;

        lexeme += c;
    }

    std::ostringstream oss;
    oss << "Error: mismatched single quotes starting from line " << startingLineIdx;
    throw std::runtime_error(oss.str());
}

bool hasChar(char c, const std::string& s) {
    return (s.find(c) != std::string::npos);
}

// no support for 
// arrays
// prefix indirection
// 
// ${var:-word}   # use word if var is unset OR empty
// ${var-word}    # use word if var is unset only

// ${var:=word}   # assign word if unset/empty
// ${var=word}    # assign if unset only

// ${var:?word}   # error if unset/empty
// ${var?word}    # error if unset only

// ${var:+word}   # use word if var is set (non-empty)
// ${var+word}    # use word if var is set at all

// ${#var} # Length

// substrings
// ${var:offset}
// ${var:offset:length}
// ${var:1}
// ${var:1:3}
// ${var: -2}   # note space before - (negative index)

// uses globs to remove from either end
// ${var#pattern}    # shortest match from front
// ${var##pattern}   # longest match from front

// ${var%pattern}    # shortest match from end
// ${var%%pattern}   # longest match from end

// it's literally sed
// pat is a **maximal glob**
// ${var/pat/repl}     # first match
// ${var//pat/repl}    # all matches
// ${var/#pat/repl}    # match at start
// ${var/%pat/repl}    # match at end

// capitlizationansnklnao
// ${var^}     # capitalize first char
// ${var^^}    # uppercase all
// ${var,}     # lowercase first
// ${var,,}    # lowercase all
// Toggle Case	${var~~}	x="aBc"; echo ${x~~}
struct Var {
    std::string id; // ${varname}
    bool isSet = false;
    bool isIndirect; // ${!varname}

};

void munchDoubleQuote(std::istream& fin, int& lineIdx, std::vector<Token>& res) {
    res.emplace_back("", DOUBLE_QUOTE_L);
    res.emplace_back("", DOUBLE_QUOTED);
    int startingLineIdx = lineIdx;
    char c;

    while (fin.get(c)) {
        OverMunched: 
        if (c == '\r') continue;

        if (!isValidChar(c)) {
            std::ostringstream oss;
            oss << "Invalid character found on line " << lineIdx;
            throw std::runtime_error(oss.str());
        }

        auto& lexeme = res.back().lexeme;

        if (c == '"') return;
        else if (c == '\\') {
            if (fin.get(c)) {
                if (c != '\n') {
                    if (!hasChar(c, "\\$\"")) {
                        lexeme += '\\';
                    }
                    lexeme += c;
                }
            }
        }
        else if (c == '$') {
            if (fin.peek() == '{')
            res.emplace_back("$", DOLLAR);
            res.emplace_back("", DOUBLE_QUOTED);
        }
        else if (c == '{') {
        }

        lexeme += c;
    }

    std::ostringstream oss;
    oss << "Error: mismatched double quotes starting from line " << startingLineIdx;
    throw std::runtime_error(oss.str());
}

std::vector<Token> munchOnce(std::istream& fin, int& lineIdx) {
    std::vector<Token> res{};
    Token curr;
    char c;
    while (fin.get(c)) {
        if (c == '\r') continue;

        auto classed = classify(c);
        if (classed != curr.id) {
            flushToken(curr, res);
            curr.id = classed;
        }
        
        switch (classed) {
            case ESCAPED:
                if (fin.get(c)) {
                    curr.lexeme += c;
                }
                else {
                    curr.lexeme += '\\';
                }
                break;

            case WHITESPACE:
                flushToken(curr, res);

            
        }
    }

    compress(res);

    return res;
}

// // t/f succeed, fail
// bool lexer(std::string filename, std::vector<Term>& lexed_output) {
//     std::ifstream input{filename};
//     if (!input) {
//         std::cerr << "Failed to find file: '" << filename << "'" << std::endl;
//         return false;
//     }

//     int lineIdx = 1;
//     char c;
//     std::string lexeme;
//     Term term {};
//     bool isEscaped = false;
//     while (input.get(c)) {
//         if (isEscaped) {
//             isEscaped = false;
//             if (c != '\n') {
//                 lexeme += c;
//                 term.isWord = true;
//             }

//             continue;
//         }
        
//         if (c == '#') {
//             flushToken(lexeme, UNQUOTED, term);
//             flushTerm(term, lexed_output);
//             while (input.get(c) && c != '\n') {}
//             continue;
//         }

//         if (c == '"' || c == '\'') {
//             if (!term.isWord) {
//                 handleOperators(lexeme, lexed_output);
//             }
//             flushToken(lexeme, UNQUOTED, term);
//             if (c == '"') munchDoubleQuote(input, lineIdx, lexeme);
//             else munchSingleQuote(input, lineIdx, lexeme);
//             flushToken(lexeme, (c == '"') ? DOUBLE_QUOTE : SINGLE_QUOTE, term);

//             term.isWord = true;

//             continue;
//         }

//         if (c == '\\') {
//             isEscaped = true;
//         }
//         else if (c == ';' || c == '<') {
//             flushToken(lexeme, UNQUOTED, term);
//             flushTerm(term, lexed_output);
//             lexed_output.emplace_back(Token{std::string(1, c), OPERATOR}, false);
//         }
//         else if (c == '|' || c == '&') {
//             // |, &, ||, &&
//             if (lexeme != std::string(1, c)) {
//                 // & can be flushed but that will be removed when sanitizing the lexed output
//                 if ()
//                 flushToken(lexeme, UNQUOTED, term);
//                 flushTerm(term, lexed_output);
//             }
//             lexeme += c;
//         }
//         else if (c == '>') {
//             if (term.isWord || !isOutputRedirRoot(lexeme)) {
//                 flushToken(lexeme, UNQUOTED, term);
//                 flushTerm(term, lexed_output);
//             }
            
//             lexeme += '>';
//         }
//     }
// }


// void flushToken(std::string& lexeme, ID state, Term& term) {
//     if (!lexeme.empty()) {
//         term.segments.emplace_back(lexeme, state);
//         lexeme.clear();
//     }
// }

// void flushTerm(Term& term, std::vector<Term>& lexed_output) {
//     if (!term.segments.empty()) lexed_output.emplace_back(term);
//     term.segments.clear();
//     term.isWord = false;
// }

// // We will not be accepting '`' command expansion
// bool isValidChar(char c) {
//     if (c == '\n' || c == '\t') return true;
//     return (c >= ' ' && c <= '~' && c != '`');
// }

// // munch it first, eval and escape later
// bool munchDoubleQuote(std::istream& input, int& lineIdx, std::string& lexeme) {
//     lexeme.clear();
//     int startingLineIdx = lineIdx;
//     char c;
//     bool isEscaped = false;
//     while (input.get(c)) {
//         // windows users may accidentally leave \r rather than just \n
//         if (c == 'r') continue;
//         if (!isValidChar(c)) {
//             std::cerr << "Invalid character found on line " << lineIdx << std::endl;
//             return false;
//         }

//         if (!isEscaped && c == '"') return true;
        
//         if (c == '\n') lineIdx++;
//         if (isEscaped) isEscaped = false;
//         else if (c == '\\') isEscaped = true;
//         lexeme += c;
//     }

//     std::cerr << "Error: mismatched double quotes starting from line " << startingLineIdx << std::endl;
//     return false;
// }

// bool munchSingleQuote(std::istream& input, int& lineIdx, std::string& lexeme) {
//     lexeme.clear();
//     int startingLineIdx = lineIdx;
//     char c;
//     while (input.get(c)) {
//         // windows users may accidentally leave \r rather than just \n
//         if (c == 'r') continue;
//         if (!isValidChar(c)) {
//             std::cerr << "Invalid character found on line " << lineIdx << std::endl;
//             return false;
//         }

//         if (c == '\'') return true;
        
//         lexeme += c;
//     }

//     std::cerr << "Error: mismatched single quotes starting from line " << startingLineIdx << std::endl;
//     return false;
// }

// inline const std::vector<std::string> OPERATORS = {
//     "<", 
//     ">", ">>", 
//     "1>", "1>>", 
//     "2>", "2>>", 
//     "&>", "&>>",
//     "|",
//     ";",
//     "||",
//     "&&"
// };

// bool isOperator(const std::string& lexeme) {
//     for (const std::string& s : OPERATORS) {
//         if (s == lexeme) {
//             return true;
//         }
//     }
//     return false;
// }

// void handleOperators(std::string& lexeme, std::vector<Term>& lexed_output) {
//     if (isOperator(lexeme)) {
//         lexed_output.emplace_back(Token{lexeme, OPERATOR}, false);
//         lexeme.clear();
//     }
// }

// inline const std::vector<std::string> OUTPUT_REDIR_ROOTS = {
//     "", ">", 
//     "1", "1>", 
//     "2", "2>", 
//     "&", "&>"
// };

// bool isOutputRedirRoot(const std::string& lexeme) {
//     for (const std::string& s : OUTPUT_REDIR_ROOTS) {
//         if (s == lexeme) {
//             return true;
//         }
//     }
//     return false;
// }

// // // t/f succeed, fail
// // bool lexer(std::string filename, std::vector<Term>& lexed_output) {
// //     std::ifstream input{filename};
// //     if (!input) {
// //         std::cerr << "Failed to find file: '" << filename << "'" << std::endl;
// //         return false;
// //     }

// //     int lineIdx = 1;
// //     char c;
// //     std::string lexeme;
// //     Term term {};
// //     bool isEscaped = false;
// //     while (input.get(c)) {
// //         if (isEscaped) {
// //             isEscaped = false;
// //             if (c != '\n') {
// //                 lexeme += c;
// //                 term.isWord = true;
// //             }

// //             continue;
// //         }
        
// //         if (c == '#') {
// //             flushToken(lexeme, UNQUOTED, term);
// //             flushTerm(term, lexed_output);
// //             while (input.get(c) && c != '\n') {}
// //             continue;
// //         }

// //         if (c == '"' || c == '\'') {
// //             if (!term.isWord) {
// //                 handleOperators(lexeme, lexed_output);
// //             }
// //             flushToken(lexeme, UNQUOTED, term);
// //             if (c == '"') munchDoubleQuote(input, lineIdx, lexeme);
// //             else munchSingleQuote(input, lineIdx, lexeme);
// //             flushToken(lexeme, (c == '"') ? DOUBLE_QUOTE : SINGLE_QUOTE, term);

// //             term.isWord = true;

// //             continue;
// //         }

// //         if (c == '\\') {
// //             isEscaped = true;
// //         }
// //         else if (c == ';' || c == '<') {
// //             flushToken(lexeme, UNQUOTED, term);
// //             flushTerm(term, lexed_output);
// //             lexed_output.emplace_back(Token{std::string(1, c), OPERATOR}, false);
// //         }
// //         else if (c == '|' || c == '&') {
// //             // |, &, ||, &&
// //             if (lexeme != std::string(1, c)) {
// //                 // & can be flushed but that will be removed when sanitizing the lexed output
// //                 if ()
// //                 flushToken(lexeme, UNQUOTED, term);
// //                 flushTerm(term, lexed_output);
// //             }
// //             lexeme += c;
// //         }
// //         else if (c == '>') {
// //             if (term.isWord || !isOutputRedirRoot(lexeme)) {
// //                 flushToken(lexeme, UNQUOTED, term);
// //                 flushTerm(term, lexed_output);
// //             }
            
// //             lexeme += '>';
// //         }
// //     }
// // }
