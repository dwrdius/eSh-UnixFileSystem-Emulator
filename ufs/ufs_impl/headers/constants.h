// And globals. Sorry about the global usage :(

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <string>
#include <vector>

inline const std::vector<std::string> validCommands = {
    "exit",  // this is fake and only used as a safety feature to prevent the accidental execution of this file with a real BASH parser
    "mkdir", 
    "ls", 
    "cd", 
    "cat", 
    "chmod", 
    "rm", 
    "find", 
    "echo", 
    "pwd", 
    "whoami"
};
inline const std::vector<std::string> commandDelims = {"|", "||", "&&", ";"};
inline const std::vector<std::string> devNull = {"/", "dev", "null"};
inline const std::vector<std::string> findFlags = {"-name", "-type", "-maxdepth"};

inline const std::string specialEscapedDoubleQuoteCharacters = "$\\\"`\n"; // [$, \, ", \n]

inline const std::string validCharacters = 
"!#$%()+,-.0123456789:;=?@qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM[]^_{}~";

inline const int MAX_FILENAME_LENGTH = 32;
inline const int MAX_WILDCARD_LENGTH = 64;
inline const int MAX_NODES = 300;

// flags for controlling user experience
inline bool DEBUG = false;
inline bool SETUP = true;

#endif
