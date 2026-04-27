#ifndef PATH_H
#define PATH_H

#include <string>
#include <vector>

class Path {
private:
    std::vector<std::string> filepath; 
    const std::string toString(const std::vector<std::string>& pathVec) const;
public:
    Path(const std::string& pathStr);
    Path(const std::vector<std::string>& pathVec);
    Path(const char* pathCStr);
    Path(const Path& path) = default;
    Path& operator=(const Path& rhs) = default;
    ~Path() = default;

    const std::vector<std::string>& asVector() const;
    const std::vector<std::string> asVector_Compressed() const;
    const std::string asString() const;
    const std::string asString_Compressed() const;

    bool isRelative() const { return filepath[0] == "."; }
    bool isAbsolute() const { return !isRelative(); }
    bool isDevNull() const;
    bool hasLongToken() const;

    operator std::string() const { return asString(); }
    operator std::vector<std::string>() const { return asVector(); }
};

#endif