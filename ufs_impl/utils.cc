#include "utils.h"
#include "ufs.h"
#include "constants.h"
#include "errorMsgs.h"

// -- Global --

std::string colour(const std::string& s, int col) {
    return s;
    // // ansi colours do not work on the online judge
    // return "\033[" + std::to_string(col) + "m" + s + "\033[0m";
}

std::ostream& operator<<(std::ostream& lhs, const std::vector<std::string>& rhs) {
    for (const auto& x : rhs) {
        lhs << "'" << x << "' ";
    }
    return lhs;
}

// normalize (compress ** -> *)
std::string compressWildcardString(std::string pattern) {
    int n = pattern.size();
    if (n == 0) return pattern;

    int sz = 1;
    for (int i = 1; i < n; i++) {
        if (pattern[i] == '*' && pattern[sz-1] == '*') continue;
        else pattern[sz++] = pattern[i];
    }
    pattern.resize(sz);
    return pattern;
}

bool wildcardMatchDPHelper(const std::string& str, std::string pattern, bool * dp) {
    int m = str.size(), n = pattern.size();
    dp[0] = dp[1] = pattern[0] == '*';
    bool prev = true;
    for (int i = 0; i < m; i++) {
        bool canTerminate = dp[0];
        for (int j = 0; j < n; j++) {
            bool next = false;
            if (pattern[j] == '*') {
                next = (prev || dp[j] || dp[j+1]);
            }
            else if (prev) {
                next = str[i] == pattern[j] || pattern[j] == '?';
            }
            prev = dp[j+1];
            dp[j+1] = next;
            if (next) canTerminate = true;
        }
        if (!canTerminate) return false;
        prev = dp[0];
    }
    return dp[n];
}

bool wildcardMatchDP(const std::string& str, std::string compressedPattern) {
    int m = str.size(), n = compressedPattern.size();
    if (n == 0) return m == 0;
    if (!m) return n == 1 && compressedPattern[0] == '*';
    if (n+1 < MAX_WILDCARD_LENGTH) {
        bool dp[MAX_WILDCARD_LENGTH] {};
        return wildcardMatchDPHelper(str, compressedPattern, dp);
    }
    else {
        bool * dp = new bool[n+1];
        for (int i = 0; i <= n; i++) dp[i] = false;
        bool res = wildcardMatchDPHelper(str, compressedPattern, dp);
        delete[] dp;
        return res;
    }
}

// Brute-force reference implementation (correct for testing)
bool wildcardMatchBrute(const std::string& str, const std::string& pattern, int i=0, int j=0) {
    if (j == pattern.size()) return i == str.size();
    if (pattern[j] == '*') {
        // match 0 or more characters
        for (int k = i; k <= str.size(); k++) {
            if (wildcardMatchBrute(str, pattern, k, j+1)) return true;
        }
        return false;
    }
    if (i >= str.size()) return false;
    if (pattern[j] == '?' || str[i] == pattern[j]) {
        return wildcardMatchBrute(str, pattern, i+1, j+1);
    }
    return false;
}

bool wildcardMatch(const std::string& str, const std::string& pattern) {
    if (pattern.size() <= 6) {
        return wildcardMatchBrute(str, pattern);
    }
    else {
        std::string compressedPattern = compressWildcardString(pattern);
        return wildcardMatchDP(str, compressedPattern);
    }
}

bool hasWildcard(const std::string& pathToken) {
    for (char c : pathToken) if (c == '*' || c == '?') return true;
    return false;
}


// -- FileSystem Node Data --

int UnixFileSystem::getPerms(Trie* curr) const {
    int perms = 0;
    if (curr->usr == currUsr) {
        perms = curr->perms>>6&0b111;
    }
    else if (curr->grp == currGrp) {
        perms = curr->perms>>3&0b111;
    }
    else {
        perms = curr->perms&0b111;
    }
    return perms;
}

std::string UnixFileSystem::getName(Trie* file) const {
    if (file == root) return "/";
    std::string usr = file->usr;
    file->usr = "~";
    Trie* parent = file->parent;
    for (auto& [child, trie] : parent->children) {
        if (trie->usr == "~") {
            file->usr = usr;
            return child;
        }
    }
    file->usr = usr;
    return ""; // Could not find
}


// -- File path validation --

// idx = index of next token to process
// idx-1 = index of current node token
const std::string UnixFileSystem::auditHelper(Trie* curr, int idx, const Path& path, bool excludeTail) const {
    const auto& pathVec = path.asVector();

    if (curr->isDir && !(getPerms(curr)&1)) {
        return colour("Error:") + " Cannot access directory '" + getName(curr) + "'; Permission denied.\n";
    }
    if (idx+excludeTail >= pathVec.size()) return "";
    else if (!curr->isDir) {
        return colour("Error:") + " File '" + getName(curr) + "' in path is not a directory.\n";
    }

    if (hasWildcard(pathVec[idx])) {
        // if any path can match, then we don't care about the other branches that fail
        for (const auto& [child, trie] : curr->children) {
            if (wildcardMatch(child, pathVec[idx])) {
                if (auditHelper(trie, idx+1, path, excludeTail).empty()) return "";
            }
        }
        return colour("Error:") + " Cannot match wildcard '" + pathVec[idx] + "'.\n";
    }
    else {
        if (pathVec[idx] == "..") {
            return auditHelper(curr->parent, idx+1, path, excludeTail); 
        }
        if (curr->children.count(pathVec[idx])) {
            return auditHelper(curr->children[pathVec[idx]], idx+1, path, excludeTail);
        }
        return colour("Error:") + " Cannot match file '" + pathVec[idx] + "'.\n";
    }
}

/*
 * Checks that all directories along a path are valid and exist
 * Exclude tail: audit the final token in the path. Exclude for auditing non-existent files and creation
 */
const std::string UnixFileSystem::auditPath(const Path& path, bool excludeTail) const {
    if (path.isDevNull()) return "";
    Trie* curr = (path.isRelative()) ? cwd : root;
    return auditHelper(curr, 1, path, excludeTail);
}

void UnixFileSystem::expandGlobHelper(Trie* curr, int idx, const Path& path, 
bool excludeTail, std::vector<Path>& res, std::vector<std::string>& relativePath) const {
    std::vector<std::string> pathVec = path.asVector_Compressed();
    if (idx+excludeTail >= pathVec.size()) {
        std::string expandedPath;
        for (auto& s : relativePath) expandedPath += s + "/";
        if (excludeTail) expandedPath += pathVec.back();
        if (DEBUG) {
            std::cout << "Found expansion: [" << relativePath << "]: ";
            std::cout << expandedPath << " " << std::boolalpha << excludeTail << std::endl;
        }
        res.emplace_back(expandedPath);
    }
    else if (hasWildcard(pathVec[idx])) {
        for (const auto& [child, trie] : curr->children) {
            if (wildcardMatch(child, pathVec[idx])) {
                relativePath.push_back(child);
                expandGlobHelper(trie, idx+1, path, excludeTail, res, relativePath);
            }
        }
    }
    else {
        if (pathVec[idx] == "..") { // all frontloaded in compressed path
            relativePath.push_back(pathVec[idx]);
            expandGlobHelper(curr->parent, idx+1, path, excludeTail, res, relativePath); 
        }
        else if (curr->children.count(pathVec[idx])) {
            relativePath.push_back(pathVec[idx]);
            expandGlobHelper(curr->children[pathVec[idx]], idx+1, path, excludeTail, res, relativePath);
        }
    }
    relativePath.pop_back();
}
const std::vector<Path> UnixFileSystem::expandGlob(const Path& path, bool excludeTail) const {
    const auto audit = auditPath(path, excludeTail);
    if (!audit.empty()) return {};

    if (path.isDevNull()) return {};
    Trie* curr = (path.isRelative()) ? cwd : root;
    
    std::vector<Path> res;
    std::vector<std::string> relativePath = {path.asVector()[0]};
    expandGlobHelper(curr, 1, path, excludeTail, res, relativePath);
    
    if (DEBUG) {
        std::cout << "Expanding glob: " << path.asString() << "\nExpanded > ";
        for (auto& p : res) {
            std::cout << "'" << (std::string)p << "', ";
        }
        std::cout << std::endl;
    }

    return res;
}

UnixFileSystem::Trie* UnixFileSystem::getFile(const Path& path, bool audit, bool excludeTail) const {
    // const auto& pathVec = path.asVector();
    // for (int i = pathVec.size() - excludeTail - 1; i >= 0; i--) {
    //     if (hasWildcard(pathVec[i])) {
    //         std::cerr << "you called getfile wrong bro. expand yo globs first xddd" << std::endl;
    //         return nullptr;
    //     }
    // }
    if (audit) {
        const auto auditOutput = auditPath(path, excludeTail);
        if (!auditOutput.empty()) {
            *err += "Failed to audit path " + path.asString() + ":\n    " + auditOutput;
            return nullptr;
        }
    }

    Trie* curr = (path.isRelative()) ? cwd : root;
    const auto pathVec = path.asVector_Compressed();
    for (int i = 1; i+excludeTail < pathVec.size(); i++) {
        if (pathVec[i] == "..") curr = curr->parent;
        else curr = curr->children[pathVec[i]];
    }
    return curr;
}

/*
 * Checks all subdirectories for WX perms
 * If any are missing, then exit
 */
bool UnixFileSystem::canRemoveDir(Trie* node) {
    if (!node->isDir) return true;
    if ((getPerms(node)&0b011) != 0b011) return false;
    for (auto& [child, trie] : node->children) {
        if (!canRemoveDir(trie)) return false;
    }
    return true;
}
bool UnixFileSystem::removeNode(Trie* node, bool recursive) {
    if (node->isDir) {
        if (!recursive) {
            *err += errDeletingDirNoRecurse;
            return false;
        }
        Trie* p = cwd;
        while (p != root) {
            if (p == node) {
                *err += errDeletingDirContainingCWD;
                cwd = prev = root;
                break;
            }
            p = p->parent;
        }

        if (prev != cwd) {
            p = prev;
            while (p != root) {
                if (p == node) {
                    *err += errDeletingDirContainingPrevWD;
                    prev = cwd;
                    break;
                }
                p = p->parent;
            }
        }
    }
    if ((getPerms(node->parent)&0b011) != 0b011) {
        *err += "Cannot remove file without -wx perms in parent directory.\nExiting.\n";
        return false;
    }
    if (node->isDir && !canRemoveDir(node)) {
        *err += "Cannot remove directory " + getName(node) + ". Contains unremovable subdirectories.\nExiting.\n";
        return false;
    }
    if (node == root) {
        delete node;
        root = new Trie{};
        root->parent = root;
        cwd = prev = root;
    }
    else {
        std::string name = getName(node);
        node->parent->children.erase(name);
        delete node;
    }
    return true;
}

void UnixFileSystem::setOutString(std::string* output) { out = output; }
void UnixFileSystem::setErrString(std::string* error) { err = error; }
std::string* UnixFileSystem::getOutString() { return out; }
std::string* UnixFileSystem::getErrString() { return err; }

void UnixFileSystem::printNumNodes() { std::cout << Trie::numNodes << std::endl; }
void UnixFileSystem::printAll() {
    if (DEBUG) {
        std::cout << "[DEBUG FLUSH]\n";
        std::cout << "out: " << *out << std::endl << std::endl;
        std::cout << "err: " << *err << std::endl << std::endl;
    }
    *out = "";
    *err = "";
}

// UnixFileSystem::Trie* UnixFileSystem::findFile(const std::string& path, int isDir, bool isChmod) const {
//     Trie* curr = cwd;
//     int start = 0;
//     if (path[0] == '/') {
//         curr = root;
//         start = 1;
//     }

//     int sz = path.size();
//     while (sz > 1 && path[sz-1] == '/') sz--;
//     for (int i = start; i < sz; i++) {
//         int perms = getPerms(curr);
//         if (!curr->isDir) {
//             *err += "Error: file [" + getName(curr) + "] is not a directory in path.\nExiting.\n";
//             return nullptr;
//         }
//         if (!(perms&1)) {
//             *err += "Error: cannot access directory: " + getName(curr) + " in path.\nExiting.\n";
//             return nullptr;
//         }

        
//         std::string sub = "";
//         while (i < sz && path[i] != '/') {
//             sub += path[i];
//             i++;
//         }
//         if (sub.empty()) continue;
//         if (sub == ".") continue;
//         if (sub == "..") {
//             curr = curr->parent;
//             continue;
//         }
//         if (!curr->children.count(sub)) {
//             *err += "Could not find file [" + path + "]. Died at [" + sub + "]\n Exiting.\n";
//             return nullptr;
//         }
//         curr = curr->children[sub];
//     }

//     if (isDir != -1 && isDir != curr->isDir) {
//         *err += "Path to [" + path + "] is the wrong type (dir/file).\n Exiting.\n";
//         return nullptr;
//     }

//     if (!isChmod && curr->isDir && !(getPerms(curr)&1)) {
//         *err += errMissingExecuteOnDirectory(getName(curr), path);
//         return nullptr;
//     }

//     return curr;
// }

UnixFileSystem::Trie* UnixFileSystem::changeFile(const Path& path_glob, const std::string& contents, bool append) {
    const std::vector<Path> paths = expandGlob(path_glob);
    if (paths.size() != 1) {
        if (DEBUG) {
            std::cerr << "[DEBUG] Changefile called on multiple files:" << std::endl;
            std::cerr << path_glob.asString() << std::endl << std::endl;
            for (std::string s : paths) std::cerr << s << ";; " << std::endl;
        }
        return nullptr;
    }

    Trie* file = getFile(paths[0], false);
    if (file) {
        if (append) file->catValue += contents;
        else file->catValue = contents;
    }
    return file;
}
UnixFileSystem::Trie* UnixFileSystem::addFile(const Path& path_glob, const std::string& contents) {
    const std::vector<Path> paths = expandGlob(path_glob, true);
    if (paths.size() != 1) {
        if (DEBUG) {
            std::cerr << "[DEBUG] Changefile called on multiple files:" << std::endl;
            std::cerr << path_glob.asString() << std::endl << std::endl;
            for (std::string s : paths) std::cerr << s << ";; " << std::endl;
        }
        return nullptr;
    }

    if (!mkdir(paths[0], false, currUsr, currGrp, true)) {
        if (DEBUG) {
            std::cerr << "[DEBUG] Failed to add file" << paths[0].asString() << std::endl;
        }
    }
    
    Trie* file = getFile(paths[0], false);
    if (file) {
        file->catValue = contents;
        file->isDir = false;
    }
    return file;
}

std::string UnixFileSystem::getWorkingDirectory(Trie* curr) const {
    std::vector<std::string> pathToRoot;
    while (curr != root) {
        pathToRoot.push_back(getName(curr));
        curr = curr->parent;
    }
    std::string res = "/";
    for (auto i = pathToRoot.rbegin(); i != pathToRoot.rend(); i++) {
        res += *i + "/";
    }
    if (!pathToRoot.empty()) res.pop_back();
    return res;
}

std::string UnixFileSystem::lOutput(Trie* file, std::string filename) const {
    std::string lout = (file->isDir) ? "d" : "-";
    for (int shift = 6, mask = 0b111; shift >= 0; shift -= 3) {
        int masked = (file->perms>>shift) & mask;
        lout += (masked & 04) ? "r" : "-";
        lout += (masked & 02) ? "w" : "-";
        lout += (masked & 01) ? "x" : "-";
    }
    lout += " X " + file->usr + " " + file->grp + " X DateDoesNotMatter ";
    if (filename.empty()) lout += getName(file);
    else lout += "'" + filename;
    lout += "'\n";
    return lout;
}

bool isOctal(const std::string& s) {
    if (s.size() > 3) return false;
    for (char c : s) {
        if ('0' > c || c > '7') return false;
    }
    return true;
}

bool isSymbolic(const std::string& s) {
    bool hasOp = false;
    bool hasPerms = false;
    std::string valid = "ugoarwx+-=";
    for (char c : s) {
        if (find(valid.begin(), valid.end(), c) == valid.end()) {
            return false;
        }
        
        if (c == '+' || c == '-' || c == '=') {
            if (hasOp) return false;
            hasOp = true;
        }
        else if (!hasPerms && (c == 'r' || c == 'w' || c == 'x')) {
            hasPerms = true;
        }
    }
    return hasOp && hasPerms;
}

bool UnixFileSystem::hasRead(Trie* node) const {
    return getPerms(node) & 4;
}
bool UnixFileSystem::hasWrite(Trie* node) const {
    return getPerms(node) & 2;
}
bool UnixFileSystem::hasExec(Trie* node) const {
    return getPerms(node) & 1;
}
