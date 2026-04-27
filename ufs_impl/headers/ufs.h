#ifndef UFS_H
#define UFS_H

#include <iostream>
#include <fstream>
#include <cassert>
#include <vector>
#include <map>
#include <algorithm>
#include "path.h"

class UnixFileSystem {
private:
    struct Trie {
        static int numNodes;
        Trie* parent;
        std::map<std::string,Trie*> children;
        std::string catValue;
        std::string usr, grp;
        int perms;
        bool isDir;

        Trie(Trie* parent, const std::string& catValue) 
            : parent{parent}, 
              children{}, 
              catValue{catValue}, 
              usr{"root"},
              grp{"root"},
              perms{0777},
              isDir{true} 
              {
                numNodes++;
              }
        Trie(Trie* parent) : Trie{parent, "."} {}
        Trie() : Trie{nullptr, "."} {}
        ~Trie() {
            for (auto& [_, trie] : children) {
                delete trie;
            }
            numNodes--;
        }
    };

    Trie* root;
    Trie* cwd;
    Trie* prev;

    std::string currUsr, currGrp, devNullBuf;
    std::string* out;
    std::string* err;
    std::string* in;


    bool canRemoveDir(Trie* node);
    bool removeNode(Trie* node, bool recursive);
    std::string lOutput(Trie* file, std::string filename = "") const;
    const std::string auditHelper(Trie* curr, int idx, const Path& path, bool excludeTail = true) const;
    void expandGlobHelper(Trie* curr, int idx, const Path& path, bool excludeTail, std::vector<Path>& res, std::vector<std::string>& relativePath) const;
    bool findHelper_dfs(Trie* curr, std::vector<Path>& res, 
        std::vector<std::string>& relativePath, const std::string& wildcard, 
        const std::string& type, const int remainingDepth, bool noExec) const;
    
    std::vector<Trie*> chmod_GetAllFiles(const Path& path_glob);

    
public:
    bool hasRead(Trie* node) const;
    bool hasWrite(Trie* node) const;
    bool hasExec(Trie* node) const;

    void setUser(const std::string user) { currUsr = user; }
    void setGroup(const std::string group) { currGrp = group; }
    void setInput(std::string* input) { in = input; }

    void setOutString(std::string* output);
    void setErrString(std::string* error);
    std::string* getOutString();
    std::string* getErrString();
    void printNumNodes();
    void printAll();
    UnixFileSystem();
    ~UnixFileSystem();
    int getPerms(Trie* curr) const;
    // Trie* findFile(const std::string& path, int isDir, bool isChmod = false) const;
    Trie* changeFile(const Path& path_glob, const std::string& contents, bool append = false);
    Trie* addFile(const Path& path_glob, const std::string& contents);
    std::string getName(Trie* file) const;
    std::string getWorkingDirectory(Trie* curr) const;
    
    bool mkdir(const Path& path_glob, bool pFlag, std::string user = "", 
    std::string group = "", bool addFile = false);
    bool ls(const Path& path_glob, bool lFlag, bool aFlag, bool dFlag) const;
    bool cd(const Path& path_glob);
    bool cat(const Path& path_glob) const;
    bool chmodOctal(const Path& path_glob, int newPerms);
    bool chmodSymbolic(const Path& path_glob, const std::vector<std::tuple<int,int,char>>& seq);
    bool rm(const Path& path_glob, bool rFlag, bool fFlag, bool npr);
    bool echo(const std::string& args);
    bool pwd() const;
    bool find(const std::vector<Path>& paths, const std::map<std::string,std::string>& flags) const;
    bool whoami() const;
    
    // the string is the error message
    const std::string auditPath(const Path& path, bool excludeTail = false) const;

    const std::vector<Path> expandGlob(const Path& path, bool excludeTail = false) const;

    Trie* getFile(const Path& path, bool audit = true, bool excludeTail = false) const;
};

std::string getWrappedToken(std::string& line, int& i, bool& exitEarly, int& lineIdx);
void checkAndPush(std::vector<std::string>& v, std::vector<std::vector<std::string>>& commands, bool& exitEarly, int lineIdx);
bool foolishLexer(const std::string& shellscript, std::vector<std::vector<std::string>>& commands);
bool isOctal(const std::string& s);
bool isSymbolic(const std::string& s);
bool executeCommand(std::vector<std::string>& command, UnixFileSystem& ufs);

#endif