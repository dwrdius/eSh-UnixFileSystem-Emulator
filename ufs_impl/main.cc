#include "errorMsgs.h"
#include "constants.h"
#include "ufs.h"
#include "utils.h"

int UnixFileSystem::Trie::numNodes = 0;

int main(int argc, char * argv[]) {
    if (argc <= 1) {
        std::cerr << "Please input a submission filename as argument 1" << std::endl;
        return 1;
    }
    std::string inputFile = argv[1];

    if (argc > 2 && (std::string)argv[2] == "DEBUG") {
        DEBUG = true;
    }
    else DEBUG = false;

    // artifact of judge setup
    auto cerrbuf = std::cerr.rdbuf();
    std::cerr.rdbuf(std::cout.rdbuf());

    std::vector<std::vector<std::string>> commands;
    if (!foolishLexer(inputFile, commands)) {
        std::cerr << "\nCould not parse file `" << inputFile << "`\n" << std::endl;
        return 1;
    }
    else if (DEBUG) {
        std::cout << "[DEBUG] Command parse:\n";
        for (auto& v : commands) {
            for (auto& s : v) std::cout << s << " ";
            std::cout << '\n';
        }
        std::cout << std::endl;
    }

    // ufs does not follow proper oop standards or idioms
    // but it is mainly because students never have access to it
    // many of the publically exposed functions should be made private in the future
    // but as of now, they are used in the Loader to parse the strings before sending them to the evaluators
    UnixFileSystem ufs{};

    // omitted
    SETUP = false;
    
    if (DEBUG) {
        std::cout << "FLUSH" << std::endl << std::endl;
    }
    
    int retVal = 0;
    for (auto& c : commands) {
        // for (auto s : c) std::cout << s << " ";
        // std::cout << endl;
        if (!executeCommand(c, ufs)) {
            std::cerr.rdbuf(cerrbuf);
            retVal = 1;
            break;
        }
    }

    std::cout << "\nTotal [files/directories] remaining: ";
    ufs.printNumNodes();

    std::cerr.rdbuf(cerrbuf);

    return retVal;
}