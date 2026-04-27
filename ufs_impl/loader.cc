#include "errorMsgs.h"
#include "constants.h"
#include "ufs.h"
#include "utils.h"

bool executeCommand(std::vector<std::string>& command, UnixFileSystem& ufs) {
    std::vector<std::vector<std::string>> munch;
    if (command[0] == "exit") return true;
    
    bool isCommand = true;
    std::cout << "<RawCommand> ' ";
    for (std::string& s : command) {
        std::cout << s << " ";
        if (isCommand) {
            isCommand = false;
            munch.push_back({});
        }
        if (find(commandDelims.begin(), commandDelims.end(), s) != commandDelims.end()) {
            isCommand = true;
            munch.push_back({s});
            /*
                ls -la | echo "hello" && echo "world" ; echo "hi"
                ->
                    ls -la
                    |
                    echo "hello"
                    &&
                    echo "world"
                    ;
                    echo "hi"
            */
        }
        else {
            munch.back().push_back(s);
        }
    }
    std::cout << '\'' << std::endl;
    std::string input;

    bool failed = false;
    for (int i = 0; i < munch.size(); i++) {
        std::string output, error, fakeDevNull;
        ufs.setOutString(&output);
        ufs.setErrString(&error);
        
        if (munch[i][0] == "||") { // we never use || because the program just dies if it fails
            if (failed) i++; // so we just skip it
            continue;
        }
        else if (munch[i][0] == ";") continue;
        else if (munch[i][0] == "&&") {
            if (!failed) i++;
            continue;
        }
        else if (munch[i][0] == "|") {
            continue;
        }
        
        std::vector<std::string>& curr = munch[i];
        bool hasIOredir = false;
        int sz = curr.size();
        int IOredirsz = sz;
        std::string ifile = "", ofile = "", efile = "";
        bool ofileAppend = false, efileAppend = false;

        bool exitEarly = false;
        bool nextVerse = false;

        for (int j = 1; j < sz; j++) {
            if (curr[j] == "<" || curr[j].back() == '>') {
                if (!hasIOredir) {
                    hasIOredir = true;
                    IOredirsz = j;
                }
                if (j+1 >= sz) {
                    std::cerr << "Missing IO redirection parameter after keyword '" << curr[j] << "'.\nExiting" << std::endl;
                    return false;
                }

                if (curr[j] == "<") {
                    ifile = curr[j+1];

                    const std::vector<Path> paths = ufs.expandGlob(ifile, false);
                    if (paths.size() != 1) {
                        std::cerr << "Error: input stream redirection expands to invalid path" << std::endl;
                        return false;
                    }
                    auto* file = ufs.getFile(paths[0]);
                    if (!file) return false;

                    if (!(ufs.getPerms(file) & 4)) {
                        std::cerr << "Error: cannot read from input stream '" << ifile << "'" << std::endl;
                        return false;
                    }

                    input = file->catValue;
                }
                else if (curr[j][0] == '1' || curr[j][0] == '>') {
                    // 1> > 1>> >>
                    ofile = curr[j+1];
                    if (curr[j] == ">>" || curr[j] == "1>>") {
                        ofileAppend = true;
                    }
                    else {
                        ofileAppend = false;
                    }
                }
                else if (curr[j][0] == '2') {
                    // 2> 2>>
                    efile = curr[j+1];
                    if (curr[j] == "2>>") {
                        efileAppend = true;
                    }
                    else {
                        efileAppend = false;
                    }
                }
                else if (curr[j][0] == '&') {
                    ofile = efile = curr[j+1];
                    if (curr[j] == "&>>") {
                        ofileAppend = efileAppend = true;
                    }
                    else {
                        ofileAppend = efileAppend = false;
                    }
                }
                else {
                    std::cerr << "Unrecognized redirection token '" << curr[j] << "'.\nExiting" << std::endl;
                    return false;
                }
                j++;
            }
            else if (hasIOredir) {
                std::cerr << "Invalid order of redirection operations in subcommand: ' ";
                for (std::string s : curr) std::cerr << s << " ";
                std::cerr << "' (from full command ' ";
                for (std::string s : command) std::cerr << s << " ";
                std::cerr << "')\nPlease place all IO redirections at the end of a command.\nExiting." << std::endl;
                return false;
            }
        }

        if (hasIOredir) {
            std::cout << '"';
        }
        std::string base = curr[0];
        if (base == "exit") {
            std::cout << "$ exit 1" << std::endl;
            std::cout << ">> Ignore this Exit is ignored and used as a safety feature.\n\n" << std::flush;
            continue;
        }
        else if (base == "mkdir") {
            bool pFlag = false;
            for (int j = 1; j < IOredirsz; j++) {
                if (curr[j] == "-p") {
                    pFlag = true;
                }
                else {
                    Path path{curr[j]};
                    if (!ufs.mkdir(path, pFlag)) {
                        exitEarly = true;
                        break;
                    }
                }
            }
            failed = false;
        }
        else if (base == "ls") {
            bool lFlag = false;
            bool aFlag = false;
            bool dFlag = false;
            bool specifiesFile = false;
            for (int j = 1; j < IOredirsz; j++) {
                if (curr[j][0] == '-') {
                    for (int k = 1; k < curr[j].size(); k++) {
                        switch (curr[j][k]) {
                            case 'l':
                                lFlag = true;
                                break;
                            case 'a':
                                aFlag = true;
                                break;
                            case 'd':
                                dFlag = true;
                                break;
                            default:
                                std::cerr << "ls with invalid flags. Ignoring..." << std::endl;
                        }
                    }
                }
                else {
                    specifiesFile = true;
                    Path path{curr[j]};
                    if (!ufs.ls(path, lFlag, aFlag, dFlag)) {
                        exitEarly = true;
                        break;
                    }
                }
            }
            if (!specifiesFile) {
                if (!ufs.ls((std::string)".", lFlag, aFlag, dFlag)) {
                    exitEarly = true;
                    break;
                }
            }
            failed = false;
        }
        else if (base == "rm") {
            bool rFlag = false;
            bool fFlag = false;
            bool npr = false; // --no-preserve-root
            
            std::vector<Path> removals{};
            for (int j = 1; j < IOredirsz; j++) {
                if (curr[j][0] == '-') {
                    if (curr[j] == "--no-preserve-root") {
                        npr = true;
                    }
                    else {
                        for (int k = 1; k < curr[j].size(); k++) {
                            switch (curr[j][k]) {
                                case 'r':
                                    rFlag = true;
                                    break;
                                case 'f':
                                    fFlag = true;
                                    break;
                                default:
                                    std::cerr << "rm with invalid flags. Exiting.\n" << std::endl;
                                    return false;
                            }
                        }
                    }
                }
                else {
                    removals.emplace_back(curr[j]);
                }
            }
            
            std::cout << "$ rm";
            if (rFlag || fFlag) std::cout << " -";
            if (rFlag) std::cout << "r";
            if (fFlag) std::cout << "f";
            if (npr) std::cout << " --no-preserve-root";        
            for (const Path& path : removals) {
                if(!ufs.rm(path, rFlag, fFlag, npr)) {
                    exitEarly = true;
                    break;
                }
            }
            failed = false;
        }
        else if (base == "cat") {
            for (int j = 1; j < IOredirsz; j++) {
                Path path{curr[j]};
                
                if (!ufs.cat(path)) {
                    exitEarly = true;
                    break;
                }
            }
            failed = false;
        }
        else if (base == "echo") {
            std::string echoArgs = "";
            for (int j = 1; j < IOredirsz; j++) {
                echoArgs += curr[j] + " ";
            }
            if (!echoArgs.empty()) echoArgs.pop_back();
            ufs.echo(echoArgs);
            failed = false;
        }
        else if (base == "cd") {
            if (IOredirsz > 2) {
                std::cerr << colour("Warning:", 35) << " Ignoring extra terms after first in cd command ' ";
                for (int k = 0; k < IOredirsz; k++) {
                    std::cerr << curr[k] << " ";
                }
                std::cerr << "'" << std::endl;
            }
            if (!ufs.cd(curr[1])) exitEarly = true;
            failed = false;
        }
        else if (base == "pwd") {
            ufs.pwd();
            failed = false;
        }
        else if (base == "chmod") {
            if (IOredirsz < 3) {
                std::cerr << "Chmod with invalid number of args ' ";
                for (int j = 0; j < IOredirsz; j++) std::cerr << curr[j] << " ";
                std::cerr << " '.\nProper usage: chmod <perms> <file1> [file2... filen]\nExiting";
                return false;
            }

            std::cout << "$ chmod ";
            for (int j = 1; j+1 < IOredirsz; j++) std::cout << curr[j] << " ";
            std::cout << curr[IOredirsz-1];

            if (isOctal(curr[1])) {
                int perms = ((curr[1][0]-'0')<<6) + ((curr[1][1]-'0')<<3) + ((curr[1][2]-'0'));
                for (int j = 2; j < IOredirsz; j++) {
                    Path path{curr[j]};
                    if (!ufs.chmodOctal(path, perms)) exitEarly = true;
                }
            }
            else {
                std::vector<std::string> split = {""};
                for (char c : curr[1]) {
                    if (c == ',') {
                        if (!split.back().empty()) split.push_back("");
                    }
                    else {
                        split.back() += c;
                    }
                }
                if (split.back().empty()) split.pop_back();
                std::vector<std::tuple<int,int,char>> symbolicSequence;
                symbolicSequence.reserve(split.size());
                for (std::string& s : split) {
                    if (!isSymbolic(s)) {
                        std::cerr << "Could not parse chmod " + curr[i] << ". Exiting." << std::endl;
                        return false;
                    }

                    int mask = 0;
                    char op = ' ';
                    int permbits = 0;
                    for (char c : s) {
                        if (c == 'a') mask = 0777;
                        else if (c == 'u') mask |= 0700;
                        else if (c == 'g') mask |= 0070;
                        else if (c == 'o') mask |= 0007;
                        else {
                            op = c;
                            if (!mask) mask = 0777;
                            break;
                        }
                    }
                    for (int k = (int)s.size()-1; s[k] != '+' && s[k] != '-' && s[k] != '='; k--) {
                        if (s[k] == 'r') permbits |= 0444;
                        else if (s[k] == 'w') permbits |= 0222;
                        else if (s[k] == 'x') permbits |= 0111;
                    }
                    symbolicSequence.emplace_back(permbits, mask, op);
                }
                for (int j = 2; j < IOredirsz; j++) {
                    Path path{curr[j]};
                    if (!ufs.chmodSymbolic(path, symbolicSequence)) {
                        exitEarly = true;
                        break;
                    }
                }
            }
            failed = false;
        }
        else if (base == "find") {
            std::string name = "*";
            std::string type = "f,d";
            int numPaths = 0;
            std::vector<Path> paths;
            
            std::map<std::string,std::string> flags;
            flags["-name"] = "*";
            flags["-type"] = "f,d";
            flags["-maxdepth"] = "1";
            
            for (int j = 1; j < IOredirsz; j++) {
                if (find(findFlags.begin(), findFlags.end(), curr[j]) == findFlags.end()) {
                    numPaths++;
                }
                else break;
            }
            if (!numPaths) {
                paths.emplace_back(".");
            }
            for (int j = 1+numPaths; j < IOredirsz; j++) {
                for (int k = 0; k < findFlags.size(); k++) {
                    if (curr[j] == findFlags[k]) {
                        if (j+1 < IOredirsz) {
                            flags[curr[j]] = curr[j+1];
                            j++;
                            break;
                        }
                        else {
                            std::cerr << "`find` command missing parameter after '" << curr[j] << "' flag.\nExiting." << std::endl;
                            return false;
                        }
                    }
                }
            }
            for (int j = 0; j < numPaths; j++) {
                const auto& expansion = ufs.expandGlob(curr[j+1]);
                paths.insert(paths.end(), expansion.begin(), expansion.end());
            }

            if (DEBUG) {
                std::cout << "[find] ";
                for (std::string s : paths) std::cout << s << ", ";
                std::cout << std::endl;
                std::cout << flags["-name"] << ", " << flags["-type"] << ", " << flags["-maxdepth"] << std::endl;
            }
            if (!ufs.find(paths, flags)) {
                exitEarly = true;
            }
            failed = false;
        }
        else if (base == "whoami") {
            ufs.whoami();
            failed = false;
        }
        else {
            std::vector<Path> paths = ufs.expandGlob(base);
            if (paths.size() != 1) {
                std::cerr << "Error: unrecognised command '" << base << "'" << std::endl;
                return false;
            }

            auto file = ufs.getFile(paths[0]);
            if (!file) {
                std::cerr << "Error: unrecognised command '" << base << "'" << std::endl;
                return false;
            }
            else if (file->isDir || !ufs.hasExec(file)) {
                std::cerr << "Error: file '" << base << "' is not an executable command" << std::endl;
                return false;
            }

            std::cerr << "This path is a standard text file with no executable implementation.\nExiting.\n";
            exitEarly = true;
        }

        ufs.setOutString(&fakeDevNull);
        ufs.setErrString(&fakeDevNull);

        if (!ofile.empty()) {
            const std::vector<Path> paths = ufs.expandGlob(ofile, true);
            if (paths.size() != 1) {
                std::cerr << "Error: output stream redirection expands to invalid path" << std::endl;
                return false;
            }
            auto* file = ufs.getFile(paths[0], false, true);
            if (!file) return false;

            std::string filename = paths[0].asVector().back();
            // if file is missing and there's an error creating a new output file then
            if (!file->children.count(filename) && (!ufs.hasWrite(file) || !ufs.addFile(paths[0], ""))) {
                std::cerr << "Error: cannot create output file '" << ofile << "'" << std::endl;
                return false;
            }
            file = file->children[filename];

            if (file->isDir) {
                std::cerr << "Error: cannot redirect output to directory '" << ofile << "'" << std::endl;
                return false;
            }

            if (!ufs.hasWrite(file)) {
                std::cerr << "Error: cannot write to output file '" << ofile << "'" << std::endl;
                return false;
            }
            
            if (ofileAppend) {
                file->catValue += output;
            }
            else {
                file->catValue = output;
            }

            output.clear();
        }
        
        if (!efile.empty()) {
            const std::vector<Path> paths = ufs.expandGlob(efile, true);
            if (paths.size() != 1) {
                std::cerr << "Error: error stream redirection expands to invalid path" << std::endl;
                return false;
            }
            auto* file = ufs.getFile(paths[0], false, true);
            if (!file) return false;

            std::string filename = paths[0].asVector().back();
            // if file is missing and there's an error creating a new output file then
            if (!file->children.count(filename) && (!ufs.hasWrite(file) || !ufs.addFile(paths[0], ""))) {
                std::cerr << "Error: cannot create error file '" << ofile << "'" << std::endl;
                return false;
            }
            file = file->children[filename];

            if (file->isDir) {
                std::cerr << "Error: cannot redirect error to directory '" << efile << "'" << std::endl;
                return false;
            }

            if (!ufs.hasWrite(file)) {
                std::cerr << "Error: cannot write to error file '" << efile << "'" << std::endl;
                return false;
            }
            
            if (efileAppend) {
                file->catValue += error;
            }
            else {
                file->catValue = error;
            }

            error.clear();
        }

        if (i+1 < munch.size() && munch[i+1][0] == "|") {
            input = output;
            output.clear();
        }
        else {
            input.clear();
        }

        if (hasIOredir) std::cout << '"';
        if (!ifile.empty()) std::cout << " < '" << ifile << "'";
        if (!ofile.empty()) std::cout << " 1> '" << ofile << "'";
        if (!efile.empty()) std::cout << " 2> '" << efile << "'";
        std::cout << std::endl;

        std::cout << "Out:\n" << output << std::endl;
        std::cerr << "Err:\n" << error << std::endl;
        fakeDevNull.clear();

        output = error = "";
        
        if (exitEarly) return false;
    }
    return true;
}
