#include <string>
#include <iostream>

#include "path.h"
#include "utils.h"
#include "constants.h"
/*
 * Takes in any string file path and converts it into directory tokens in a "Path"
 * Assumes the empty string refers to the cwd
 * Does not perform compression of ./path/.. -> ./
 * Returns a path starting with '.' or '/'
 */
Path::Path(const std::string& pathStr) {
    if (pathStr.empty()) {
        filepath = {"."};
        return;
    }

    filepath = {(pathStr[0] == '/') ? "/" : "."};

    std::string token;
    for (char c : pathStr) {
        if (c == '/') {
            if (!token.empty() && token != ".") {
                filepath.emplace_back(std::move(token));
            }
            token.clear();
        }
        else token += c;
    }
    if (!token.empty() && token != ".") {
        filepath.emplace_back(std::move(token));
    }
    
    if (DEBUG) {
        std::cout << "[DEBUG] Vectorizing: [" << pathStr << "]\n" 
                  << "[DEBUG] Vectorized:  [" << filepath << "]\n"
                  << std::flush;
    }
}

Path::Path(const std::vector<std::string>& pathVec) {
    std::string pathStr = "";
    for (const auto& s : pathVec) pathStr += s + "/";
    filepath = Path{pathStr}.asVector();
}
Path::Path(const char* pathCStr) : Path{(std::string)pathCStr} {}
const std::vector<std::string>& Path::asVector() const {
    return filepath;
}

/*
 * Compresses redundant traversals
 * Does not audit path 
 * For faster access and visual outputs (if tokens are limited)
*/
const std::vector<std::string> Path::asVector_Compressed() const {
    if (DEBUG) {
        std::cout << "[DEBUG]\nCompressing [" << filepath << "]\n";
    }

    int n = filepath.size();
    int sz = 1;
    
    std::vector<std::string> res(n);
    res[0] = filepath[0];

    for (int i = 1; i < n; i++) {
        const std::string& curr = filepath[i];
        if (curr == "..") {
            if (sz > 1 && res[sz-1] != "..") {
                sz--;
            }
            else if (res[0] != "/") {
                res[sz] = "..";
                sz++;
            }
        }
        else {
            res[sz++] = curr;
        }
    }

    res.resize(sz);
    if (DEBUG) {
        std::cout << "Compressed  [" << res << "]\n"
                  << std::flush;
    }
    return res;
}

const std::string Path::toString(const std::vector<std::string>& pathVec) const {
    std::string res = "/";
    if (pathVec[0] == ".") res = "./";
    for (int i = 1; i < pathVec.size(); i++) res += pathVec[i] + '/';
    if (pathVec.size() > 1) res.pop_back();
    return res;
}
const std::string Path::asString() const {
    return toString(filepath);
}
const std::string Path::asString_Compressed() const {
    return toString(asVector_Compressed());
}

bool Path::isDevNull() const {
    if (filepath.size() != 3) return false;
    for (int i = 0; i < 3; i++) if (filepath[i] != devNull[i]) return false;
    return true;
}
bool Path::hasLongToken() const {
    for (auto& token : filepath) {
        auto compressed = compressWildcardString(token);
        if (hasWildcard(compressed) && compressed.size() >= MAX_WILDCARD_LENGTH) return true; 
        else if (compressed.size() > MAX_FILENAME_LENGTH) return true;
    }
    return false;
}