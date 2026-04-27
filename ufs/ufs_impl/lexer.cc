#include "errorMsgs.h"
#include "constants.h"
#include "ufs.h"
#include "utils.h"

// flushes UP to next newline without ingesting it
void flushToNextNewline(std::string& line, int& i) {
    int sz = line.size();
    for (; i < sz && line[i] != '\n'; i++);
}

std::string getWrappedToken(std::string& line, int& i, bool& exitEarly, int& lineIdx) {
    char delim = line[i];
    std::string token = "";
    i++;
    int sz = line.size();
    int firstNewline = -1;
    while (i < sz && line[i] != delim) {
        if (delim == '"' && line[i] == '\\') {
            if (i+1 < sz) {
                if (line[i+1] == '\n' && firstNewline == -1) {
                    firstNewline = lineIdx;
                }
                if (specialEscapedDoubleQuoteCharacters.find(line[i+1]) == std::string::npos) {
                    token += '\\';
                }
                token += line[i+1];
                i += 2; 
            }
            else {
                std::cerr << "Broken escape '\\' in double quotes on line " << lineIdx << std::endl;
                exitEarly = true;
                return "";
            }
        }
        else {
            token += line[i];
            i++;
        }
        if (line[i] == '\n') {
            if (firstNewline == -1) firstNewline = lineIdx;
            lineIdx++;
        }
    }
    if (i == sz) {
        std::cerr << "Unmatched [" << delim << "] at line " << lineIdx << std::endl;
        if (firstNewline != -1) {
            std::cerr << "Potential mismatch from line " << firstNewline << std::endl;
        }
        exitEarly = true;
        return "";
    }

    return token;
}

bool flushCommand(std::vector<std::string>& tokens, std::vector<std::vector<std::string>>& commands, bool& exitEarly, int lineIdx) {
    if (tokens.empty()) return true;

    commands.push_back({});
    std::vector<std::string>& buf = commands.back();
    buf.reserve(tokens.size());

    bool isCommand = true;
    for (std::string& s : tokens) {
        if (s.empty()) continue;
        if (isCommand) {
            bool isValidCommand = false;
            if (s.find('/') == std::string::npos) {
                for (const auto& command : validCommands) {
                    if (s == command) {
                        isValidCommand = true;
                        break;
                    }
                }
            }
            else {
                isValidCommand = true;
            }
            if (!isValidCommand) {
                std::cerr << "Invalid command: " << s << " found in line " << lineIdx << std::endl;
                exitEarly = true;
                return false;
            }
            isCommand = false;

            buf.push_back(s);
        }
        else {
            if (s == "|" || s == "||" || s == "&&" || s == ";") {
                isCommand = true;
            }   
            buf.push_back(s);
        }
    }
    if (isCommand) {
        std::cerr << "Line " << lineIdx << " ends with a token that indicates continued parsing (&&, ||, |, ;)" << std::endl;
        std::cerr << "    If you wish to continuously parse with these at the end, indicate linebreaks with (\\)" << std::endl;
        exitEarly = true;
        return false;
    }

    tokens.clear();
    return true;
}

void flushToken(std::string& token, std::vector<std::string>& tokens) {
    // ignore empty strings
    if (!token.empty()) {
        // set and clear token
        tokens.emplace_back(std::move(token));
        token.clear();
    }
}

void trimStringInPlace(std::string& s) {
    int n = s.size();
    int prefixSpaces = 0;
    int suffixSpaces = 0;
    for (char c : s) {
        if (c == ' ' || c == '\t') prefixSpaces++;
        else break;
    }

    if (prefixSpaces == n) {
        s.clear();
        return;
    }
    
    for (int i = n-1; i >= 0; i--) {
        if (s[i] == ' ' || s[i] == '\t') suffixSpaces++;
        else break;
    }

    s = s.substr(prefixSpaces, n - prefixSpaces - suffixSpaces);
}

/*
 * Very naive lexer to generate tokens from a bash file
 * Inputs: 
 * - path to bash file (for ifstream)
 * - reference to vector<vector<string>> of commands 
 *     - each command is represented as a vector of tokens (vector<string>)
 */
bool foolishLexer(const std::string& bashFilename, std::vector<std::vector<std::string>>& commands) {
    std::string input = "";
    { // scoped to not clog the variable names
        std::ifstream fin{bashFilename};
        if (!fin) {
            std::cerr << "Failed to find file: '" << bashFilename << "'" << std::endl;
            return false;
        }

        std::string line;
        while (getline(fin, line)) {
            input += line;
            input += '\n';
        }
        fin.close();
    }

    // flag for exiting when any errors are detected
    // occurs after parsing entire input so that students 
    // get all syntax errors at once rather than one at a time
    bool exitEarly = false; 

    // single command
    std::vector<std::string> tokens;
    int n = input.size();

    bool flushIfNotSpecial = false;

    // manually tokenize without using sstream because whitespace matters
    // inside any "" or '' input
    std::string token = "";
    
    // we insert an "exit 1" before the file for safety, so on the Online Judge, 
    // it will show an extra line compared to what they see -> start at 0
    for (int i = 0, lineIdx = 0; i < n; i++) {
        if (input[i] == '#') {
            flushToNextNewline(input, i);
        }

        if (input[i] == '\n') {
            flushToken(token, tokens);
            if (!flushCommand(tokens, commands, exitEarly, lineIdx)) break;
            flushIfNotSpecial = false;
            lineIdx++;
            continue;
        }
        
        if (input[i] == ' ' || input[i] == '\t') {
            flushToken(token, tokens);
            flushIfNotSpecial = false;
        }
        // `<` and `;` are always single-character tokens
        else if (input[i] == '<' || input[i] == ';') {
            flushToken(token, tokens);
            tokens.emplace_back(1, input[i]);

            flushIfNotSpecial = false;
        }
        // Escaped character support for [`"`, `'`, `\`, ` `, `\n`]
        else if (input[i] == '\\') { 
            // input.back() == '\n' due to the ingestion
            if (input[i+1] == '\n') { // escaped newlines are just skipped
                lineIdx++;
                i++;
            }
            else {
                if (flushIfNotSpecial) {
                    flushToken(token, tokens);
                    flushIfNotSpecial = false;
                }
                if (input[i+1] == '\'' || input[i+1] == '"' || input[i+1] == '\\' || input[i+1] == ' ') {
                    i++;
                    token += input[i];
                }
                else {
                    std::cerr << errUnsupportedEscapeCharacter(input[i+1], lineIdx);
                    // bash seems to ignore bad escapes rather than fail
                    std::cerr << "Ignoring the `\\`" << std::endl;
                    // exitEarly = true;
                }
            }
        }   
        // pipes and ||
        else if (input[i] == '|') {
            if (token != "|") flushToken(token, tokens);
            token += '|';
            flushIfNotSpecial = true;
        }
        // &> and &&
        else if (input[i] == '&') {
            if (!token.empty() && token.back() == '>') {
                std::cerr << errNoStreamToStreamRedirection(lineIdx);
                exitEarly = true;
                flushToNextNewline(input, i);
            }
            else {
                if (token != "&") {
                    flushToken(token, tokens);
                }
                token += '&';
            }

            flushIfNotSpecial = true;
        }
        // accept >, >>, 1>, 1>>, 2>, 2>>
        else if (input[i] == '>') {
            if (token.empty() || token == "1" || token == "2" 
            || token == ">" || token == "1>" || token == "2>") {
                token += '>';
            }
            else {
                flushToken(token, tokens);
                token = ">";
            }

            flushIfNotSpecial = true;
        }
        // '', ""
        else if (input[i] == '\'' || input[i] == '"') {
            if (flushIfNotSpecial) {
                flushToken(token, tokens);
                flushIfNotSpecial = false;
            }
            token += getWrappedToken(input, i, exitEarly, lineIdx);
            if (exitEarly) break;
        }

        // accept any visible character except for ` 
        else if (input[i] >= 21 && input[i] < 127 && input[i] != '`') {
            if (flushIfNotSpecial) {
                flushToken(token, tokens);
                flushIfNotSpecial = false;
            }
            token += input[i];
        }

        else {
            std::cerr << "Error: unsupported character [" << std::hex << ((int)input[i]) << "] in line " << std::dec << lineIdx << std::endl;
            exitEarly = true;
            break;
        }
    }
    
    std::cerr << std::flush;

    if (exitEarly) {
        std::cerr << colour("Errors parsing input. Exiting.") << std::endl;
        return false;
    }
    if (!tokens.empty()) {
        std::cerr << "Unmatched escape (\\) char at final line" << std::endl;
        return false;
    }
    return true; 
}
