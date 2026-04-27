#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

std::string colour(const std::string& s, int col = 31);

std::ostream& operator<<(std::ostream& lhs, const std::vector<std::string>& rhs);

bool wildcardMatch(const std::string& str, const std::string& pattern);
std::string compressWildcardString(std::string pattern);
bool hasWildcard(const std::string& pathToken);

#endif