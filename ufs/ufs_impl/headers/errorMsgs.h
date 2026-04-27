#ifndef ERROR_MSGS_H
#define ERROR_MSGS_H

#include "constants.h"
#include "ufs.h"

std::string errMissingFileInPath_audit(const std::string& missing, const Path& path);
std::string errMissingExecuteOnDirectory(const std::string& file, const Path& path);
std::string errCannotWriteToDirectory(const std::string& file);
std::string errUnsupportedEscapeCharacter(char character, int lineNum);
std::string errNoStreamToStreamRedirection(int lineNum);
bool checkErrLongFilenames(const Path& path);

const std::string errDeletingDirContainingCWD = 
"Trying to remove directory containing current working directory.\n Moving current and previous working directory to root.\n";

const std::string errDeletingDirContainingPrevWD = 
"Trying to remove directory containing previous working directory.\n Setting previous working directory to current.\n";

const std::string errDeletingDirNoRecurse = 
"Cannot remove directory without [-r]\nExiting.\n";

const std::string errForceRMRootDir = 
"This will bypass the regular output stream just to tell you that this is a bad idea.\
\nPlease don't ever run `rm -fr /` on a regular unix system... especially with sudo.\
\nIt does not remove the french language package.\nRoot will now be removed. Have fun!\
\n\n";

const std::string errIgnoreRMRootDir =
"Cannot remove root without truly forcing it ;)\nExiting.\n";

const std::string errTooManyFiles =
"\nError: Created too many files/directories; Killing process for safety.";

#endif
