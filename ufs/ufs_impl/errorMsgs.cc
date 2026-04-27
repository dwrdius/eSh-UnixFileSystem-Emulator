#include "utils.h"
#include "errorMsgs.h"
#include "constants.h"
#include "path.h"
#include "ufs.h"

std::string errMissingFileInPath_audit(const std::string& missing, const Path& path) {
    return (std::string)"Error: Missing file '" 
            + missing 
            + "' in path '" 
            + path.asString();
}

std::string errMissingExecuteOnDirectory(const std::string& file, const Path& path) {
    return (std::string)"Cannot open directory '" 
            + file 
            + "' from path '" 
            + path.asString() 
            + "': Permission (-x) denied.\nExiting.\n";
}

std::string errCannotWriteToDirectory(const std::string& file) {
    return (std::string)"Error: cannot write to directory: '" 
            + file 
            + "'. Permission (-w) denied.\nExiting.\n";
}

std::string errUnsupportedEscapeCharacter(char character, int lineNum) {
    return (std::string)"Error: unsupported escaped character: `" 
            + character
            + "` in line "
            + std::to_string(lineNum)
            + "\nPlease limit escapes (\\) to [\\\\, \\\", \\', \\[space], \\[newline]]\n"
            + "The [] are used to represent invisible characters, not the string.\n";
}

std::string errNoStreamToStreamRedirection(int lineNum) {
    return (std::string)"Error: this shell does not support redirecting a stream to the output of another with >&. (Line " 
            + std::to_string(lineNum) 
            + ")\n";
}

