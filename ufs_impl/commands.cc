#include "errorMsgs.h"
#include "constants.h"
#include "ufs.h"
#include "utils.h"

UnixFileSystem::UnixFileSystem() 
    : root{new Trie{}}, 
      cwd{root}, 
      prev{root}, 
      currUsr{"root"}, 
      currGrp{"root"}, 
      devNullBuf{""},
      out{&devNullBuf}, 
      err{&devNullBuf},
      in{nullptr}
{
    root->parent = root;
}
UnixFileSystem::~UnixFileSystem() {
    delete root;
}

bool UnixFileSystem::mkdir(const Path& path, bool pFlag, std::string user, 
std::string group, bool addFile) {
    if (path.isDevNull() || path.asString() == "./dev/null" && getWorkingDirectory(cwd) == "/") {
        *err += "Cannot mkdir /dev/null\nExiting.";
        return false;
    }
    
    if (user.empty()) user = currUsr;
    if (group.empty()) group = currGrp;
    if (!addFile && !SETUP) {
        std::cout << "$ mkdir";
        if (pFlag) std::cout << " -p";
        std::cout << " '" << path.asString() << "'";
    }

    const auto pathStr = path.asString();
    const auto pathVec = path.asVector();
    
    if (pathStr == "/") {
        *err += "Please don't create root!\nExiting.\n";
        return false;
    }
    
    Trie* curr = path.isRelative() ? cwd : root;
    
    for (int i = 1; i < pathVec.size(); i++) {
        if (DEBUG) {
            std::cout << "[mkdir] in subdir: " << pathVec[i-1] << std::endl;
        }
        int perms = getPerms(curr);
        if (!curr->isDir) {
            *err += "Error: file [" + pathVec[i-1] + "] is not a directory in path.\nExiting.\n";
            return false;
        }
        if (!(perms&1)) {
            *err += "Error: cannot access directory: " + pathVec[i-1] + " in path.\nExiting.\n";
            return false;
        }

        if (i+1 == pathVec.size()) {
            if (curr->children.count(pathVec[i])) {
                if (pFlag) {
                    *out += "No directory created; path [" + path.asString() + "] already exists\n";
                    if (!curr->children[pathVec[i]]->isDir) {
                        *err += "Path [" + path.asString() + "] is an existing file\n";
                        return true;
                    }
                }
                else {
                    *err += "Could not create directory; path [" + path.asString() + "] already exists\n";
                    *err += "Try running with -p flag to ignore these warnings.\n Exiting.\n";
                    out->back() = '*';
                    *out += '\n';
                    return false;
                }
            }
            else if (pathVec[i] == "..") {
                *err += "[" + pathVec[i] + "] is an invalid filename.\n Exiting.\n";
                out->back() = '*';
                *out += '\n';
                return false;
            }
            else {
                if (!(perms&2)) {
                    *err += errCannotWriteToDirectory(pathVec[i-1]);
                    return false;
                }
                if (!addFile) *out += "Created file at path [" + path.asString() + "]\n";
                Trie* temp = new Trie{curr};
                temp->usr = user;
                temp->grp = group;
                if (addFile) temp->isDir = false;
                curr->children[pathVec[i]] = temp;

                if (Trie::numNodes > MAX_NODES) {
                    out->clear();
                    err->clear();
                    std::cerr << errTooManyFiles << std::endl;
                    return false;
                }
            }
        }
        else if (!curr->children.count(pathVec[i])) {
            if (pathVec[i] == "..") {
                curr = curr->parent;
                continue;
            }
            else if (pFlag) {
                if (!(perms&2)) {
                    *err += errCannotWriteToDirectory(pathVec[i-1]);
                    return false;
                }
                if (DEBUG) std::cout << "Generated subdirectory [" + pathVec[i] + "]\n";
                Trie* temp = new Trie{curr};
                temp->usr = user;
                temp->grp = group;
                if (addFile) temp->isDir = false;
                curr->children[pathVec[i]] = temp;

                if (Trie::numNodes > MAX_NODES) {
                    out->clear();
                    err->clear();
                    std::cerr << "\nError: Created too many files; Killing process for safety." << std::endl;
                    return false;
                }
            }
            else {
                *err += "Could not create directory; Missing subdirectory [" + pathVec[i] + "]\n";
                *err += "Try running with -p flag to ignore these warnings.\n Exiting.\n";
                *out += "*\n";
                return false;
            }
        }
        curr = curr->children[pathVec[i]];
    }

    return true;
}
bool UnixFileSystem::ls(const Path& path_glob, bool lFlag, bool aFlag, bool dFlag) const {
    if (!SETUP) {
        std::cout << "$ ls";
        if (lFlag || aFlag || dFlag) std::cout << " -";
        if (lFlag) std::cout << 'l';
        if (aFlag) std::cout << 'a';
        if (dFlag) std::cout << 'd';
    }
    
    std::string audit = auditPath(path_glob, dFlag);
    if (!audit.empty()) {
        *err += audit + "\n";
        return false;
    }
    const std::vector<Path> paths = expandGlob(path_glob, dFlag); 
    if (paths.empty()) return false;

    if (!SETUP) {
        for (std::string path : paths) std::cout << " '" << path << "'";
    }
    
    for (const Path& path : paths) {
        Trie* file = getFile(path, false, dFlag);
        std::string filename = path.asVector_Compressed().back();
        if (!file) return false;
        if (file->isDir && !(getPerms(file)&0b100)) {
            *err += "Error: cannot read directory '" + path.asString() + "'\n";
            continue;
        }

        *out += path.asString() + "\n";
        if (dFlag) {
            if (path.asVector_Compressed().size() == 1) {
                if (lFlag) {
                    *out += lOutput(file, path.asString());
                }
                else {
                    *out += path.asString() + "\n";
                }
            }
            else {
                if (!file->children.count(filename)) {
                    *err += "Error: Missing file '" + filename + "'.\n";
                    return false;
                }
                if (lFlag) {
                    *out += lOutput(file->children[filename], path.asString());
                }
                else {
                    *out += path.asString() + "\n";
                }
            }
        }
        else if (!file->isDir) {
            if (lFlag) {
                *out += lOutput(file);
            }
            else {
                *out += getName(file) + "\n";
            }
        }
        else {
            if (aFlag) {
                if (lFlag) {
                    *out += lOutput(file, ".");
                    if (file->parent != file) {
                        *out += lOutput(file->parent, "..");
                    }
                }
                else {
                    *out += ". ";
                    if (file->parent != file) {
                        *out += ".. ";
                    }
                }
            }
            for (auto& [child, trie] : file->children) {
                if (aFlag || child[0] != '.') {
                    if (lFlag) {
                        *out += lOutput(trie, child);
                    }
                    else {
                        *out += child + " ";
                    }
                }
            }
            if (!lFlag) *out += "\n";
        }
    }

    return true;
}
bool UnixFileSystem::cd(const Path& path_glob) {
    // std::cout << path_glob.asString() << std::endl;
    if (path_glob.asString() == "./-") {
        if (!SETUP) std::cout << "$ cd -";
        std::swap(cwd, prev);
        *out += getWorkingDirectory(cwd);
        return true;
    }
    std::string audit = auditPath(path_glob, false);
    if (!audit.empty()) {
        *err += audit + "\n";
        return false;
    }
    const std::vector<Path> paths = expandGlob(path_glob);
    if (paths.size() != 1) {
        if (DEBUG) {
            std::cerr << "cd with too many (" << paths.size() << ") arguments" << std::endl;
        }
        *err += colour("Error: ") + "`cd` called with too many arguments (" + std::to_string(paths.size()) + ")\n";
        return false;
    }
    const Path& path = paths[0];
    if (!SETUP) std::cout << "$ cd " + path.asString();
    Trie* file = getFile(path, false);
    if (!file) return false;
    if (!file->isDir) {
        *err += colour("Error: ") + "Cannot `cd` into '" + path.asString() + "'.\n Not a directory.\n";
        return false;
    }
    prev = cwd;
    cwd = file;

    return true;
}
bool UnixFileSystem::cat(const Path& path_glob) const {
    std::string audit = auditPath(path_glob, false);
    if (!SETUP) std::cout << "$ cat ";
    if (!audit.empty()) {
        *err += audit + "\n";
        return false;
    }
    const std::vector<Path> paths = expandGlob(path_glob);
    if (paths.empty()) return false;
    if (!SETUP) {
        for (std::string path : paths) std::cout << path << " ";
    }
    for (const Path& path : paths) {
        Trie* file = getFile(path, false);
        if (!file) return false;
        
        if (file->isDir) {
            *err += colour("Error: ") + "Cannot cat directory " + path.asString() + "\nExiting.\n";
            return false;
        }

        if (!(getPerms(file)&4)) {
            *err += "Missing read permissions on file " + path.asString() + "\nExiting.\n";
            return false;
        }
        *out += file->catValue;
    }
    return true;
}

std::vector<UnixFileSystem::Trie*> UnixFileSystem::chmod_GetAllFiles(const Path& path_glob) {
    std::string audit = auditPath(path_glob, true);
    if (!audit.empty()) {
        *err += audit + "\n";
        return {nullptr};
    }
    
    std::vector<std::string> globVec = path_glob.asVector_Compressed();
    std::string tail = globVec.back();
    globVec.pop_back();
    if (globVec.empty()) {
        if (path_glob.isAbsolute()) {
            // is root
            return {root};
        }
        else {
            return {cwd};
        }
    }

    Path glob_noTail{globVec};
    
    const std::vector<Path> paths = expandGlob(glob_noTail, false);
    
    std::vector<Trie*> res;
    for (const Path& path : paths) {
        Trie* file = getFile(path, false);
        if (!file) continue; // ignore bad expansions
        if (tail == "..") { 
            // this is literally 1 file because of compression
            // this isn't how globbing is actually done in filesystems, but it is
            // how it is done here for efficiency
            // it uses the reduced glob rather than the full glob since it passed audit.
            // therefore if you do some chmod 000 /*/*/.., it might remove perms
            // then fail to do it the second time
            // but compression doesn't reaudit 
            res.push_back(file->parent);
        }
        else {
            for (auto& [child, trie] : file->children) {
                if (wildcardMatch(child, tail)) {
                    res.push_back(trie);
                }
            }
        }
    }
    
    return res;
}


bool UnixFileSystem::chmodOctal(const Path& path_glob, int newPerms) {
    auto fileList = chmod_GetAllFiles(path_glob);
    if (!fileList.size()) {
        *err += "Could not find any files in the glob.\n";
        return false;
    }
    else if (fileList[0] == nullptr) {
        return false; // error message audit already in stream
    }
    *out += std::to_string(fileList.size()) + " file" + ((fileList.size() == 1) ? "" : "s") + " modified\n";
    
    for (auto& file : fileList) {
        if (!file) return false;
        
        file->perms = newPerms;
    }
    return true;
}
bool UnixFileSystem::chmodSymbolic(const Path& path_glob, const std::vector<std::tuple<int,int,char>>& seq) {
    auto fileList = chmod_GetAllFiles(path_glob);
    if (!fileList.size()) {
        *err += "Could not find any files in the glob.\n";
        return false;
    }
    else if (fileList[0] == nullptr) {
        return false; // error message audit already in stream
    }
    *out += std::to_string(fileList.size()) + " file" + ((fileList.size() == 1) ? "" : "s") + " modified\n";
    
    for (auto& file :fileList) {
        if (!file) return false;
        
        int& perms = file->perms;
        for (auto& [permbits, mask, op] : seq) {
            if (op == '+') perms |= (permbits & mask);
            else if (op == '-') perms &= ~(permbits & mask);
            else if (op == '=') {
                perms &= ~mask;
                perms |= (permbits & mask);
            }
        }
    }

    return true;
}
bool UnixFileSystem::rm(const Path& path_glob, bool rFlag, bool fFlag, bool npr) {    
    std::string audit = auditPath(path_glob, false);
    if (!audit.empty()) {
        *err += audit + "\n";
        return false;
    }
    const std::vector<Path> paths = expandGlob(path_glob); 
    if (paths.empty()) return false;
    if (!SETUP) {
        for (std::string path : paths) std::cout << " '" << path << "'";
    }
    
    for (const Path& path : paths) {
        Trie* file = getFile(path, true);
        if (!file) continue;
        if (file == root) {
            if (rFlag && fFlag && npr) {
                std::cerr << errForceRMRootDir << std::endl;
                *err += errForceRMRootDir;
                removeNode(root, rFlag);
                return true;
            }
            else {
                *err += errIgnoreRMRootDir;
                return false;
            }
        }
        else {
            if (!removeNode(file, rFlag)) {
                return false;
            }
        }
    }
    return true;
}
bool UnixFileSystem::echo(const std::string& args) {
    if (!SETUP) std::cout << "$ echo '" + args + "'";
    *out += args + "\n";
    return true;
}
bool UnixFileSystem::pwd() const {
    if (!SETUP) std::cout << "$ pwd";
    *out += getWorkingDirectory(cwd);
    return true;
}

bool UnixFileSystem::findHelper_dfs(Trie* curr, std::vector<Path>& res, 
std::vector<std::string>& relativePath, const std::string& wildcard, 
const std::string& type, const int remainingDepth, bool noExec) const {
    bool isDir = false;
    bool isFile = false;
    if (type == "d") isDir = true;
    else if (type == "f") isFile = true;
    else if (type == "d,f" || type == "f,d") {
        isDir = isFile = true;
    }
    else {
        return false;
    }

    Path pathObj{relativePath};
    if ((curr->isDir && isDir || !curr->isDir && isFile) 
    && wildcardMatch(relativePath.back(), wildcard)) {
        res.push_back(pathObj);
    }

    if (remainingDepth <= 0 || !curr->isDir) return true;
    if (noExec) return false;

    if (getPerms(curr) & 4) {
        bool exec = getPerms(curr) & 1;
        for (auto [child, trie] : curr->children) {
            relativePath.push_back(child);
            findHelper_dfs(trie, res, relativePath, wildcard, type, remainingDepth-1, !exec);
            relativePath.pop_back();
        }
    }
    else {
        *err += "find: '" + pathObj.asString() + "': Cannot read directory.\n";
        return false;
    }

    return true;
}
bool UnixFileSystem::find(const std::vector<Path>& paths, const std::map<std::string,std::string>& flags) const {
    if (!SETUP) {
        std::cout << "$ find";
        for (std::string s : paths) std::cout << " " << s;
        if (flags.at("-name") != "*") std::cout << " -name '" << flags.at("-name") << "'";
        if (flags.at("-type") != "f,d") std::cout << " -type '" << flags.at("-type") << "'";
        if (flags.at("-maxdepth") != "1") std::cout << " -maxdepth '" << flags.at("-maxdepth") << "'";
        else std::cout << "\n Default maxdepth hardcoded to 1. Change with [-maxdepth] flag";
    }

    for (char c : flags.at("-maxdepth")) {
        if (c < '0' || c > '9') {
            *err += "`find`: Specified non-positive-integer maxdepth parameter.\nExiting.\n";
            return false;
        }
    }
    std::vector<Path> res {};
    for (const Path& path : paths) {
        std::string audit = auditPath(path);
        if (!audit.empty()) {
            *err += audit;
            return false;
        }
        std::vector<std::string> relative = path.asVector_Compressed();
        if (!findHelper_dfs(getFile(path, false, false), res, relative, flags.at("-name"), flags.at("-type"), stoi(flags.at("-maxdepth")), false)) {
            *err += "find: error in path " + path.asString() + "\n";
        }
    }
    for (std::string s : res) {
        *out += s + "\n";
    }
    return true;
}

bool UnixFileSystem::whoami() const {
    if (!SETUP) std::cout << "$ whoami";
    *out += "[u:" + currUsr + ", g:" + currGrp + "]";
    return true;
}
