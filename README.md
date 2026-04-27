Last Updated: April 27, 2026 by E.S.

UPDATE:
- The filesystem is missing significant functionality relating to its online judge test setup and its ctf filestructure due to confidentiality. The core lexer is undergoing signficant restructuring, and will be updated under `ufs_impl/experimental/`

Welcome to my fake unix filesystem!

---

### File structure
There are 2 folders packaged with this program:
- ctf_submission_files
    - These contain the instructions and submission files that the students need to use for the assignment
    - *Currently, the submission file is named `ctf.sh` and was made for the W26 final review session

- ufs_test_setup
    - **Omitted**

---

### UFS Implementation
I implemented UFS to have a `main`, a `lexer`, a `loader`, and a class that executes everything.
- Ideally, the `loader` should be integrated into the `UnixFileSystem` class so that various exposed methods can be internalized, but I have left this separation in so that the commands can simply handle functionality while the loader handles parsing flags and munching chained commands [||, &&, ;, |]

#### Lexer
The lexer does the first parse of the student submission shell file. \
It sends all lines from the input file to a single string, delimited by \n. This can probably be avoided if the we use `ifstream.get(char)` or `std::noskipws`. This is done because BASH will consume every token wrapped in quotes, including invisible and newline characters. This lexer will modify a `commands` vector of vectors of strings, with each element containing a fully tokenized command. These are grouped by the command chaining mentioned above and separated by regular, non-escaped newline delimiters.
```bash
echo hello
echo "world\
.txt" | cat
```
will lex into
```cpp
vector<vector<string>> commands = {{"echo", "hello"}, {"echo", "world.txt", "|", "cat"}} 
```

#### Loader
Each command is then fed into `executeCommand`, implemented in `Loader` \
Loader will perform `munching` first to separate each command into their subcommands and then separate them with the chaining parameters:
```cpp
vector<vector<string>> munch = {{"echo", "hello"}} // 1 
vector<vector<string>> munch = {{"echo", "world.txt"}, {"|"}, {"cat"}} // 2 
```
For each subcommand, it will try to parse the first argument as a known command and default to interpreting it as an executable path. These are individually implemented for each command and most output stream information is computed here. IORedirection is also computed here.

#### Execution
When the commands are run (`commands.cc`), they typically print out `$ commandname <parsed args/flags>` and then attempt to execute them, returning `true` and `false` depending on whether it succeeded.

Since we want to flush output and error streams before exiting, these all activate an `earlyExit` flag that does not return from execution until after the IO flushing has been performed in the loader.

**Omitted**

#### Filesystem Representation
Internally, the filesystem is just a Trie. This Trie holds the `catValue`, which is text shown when `cat` is called, as well its children, its parent, permissions, and an isDirectory flag.
- It might be worth removing the `parent` field and replacing it with a `..` child in the children class, but this would require modification to check for `..` on root since it is currently configured such that `root->parent = root` for this traversal. I think it is simply removing this one parameter from the `ls -a` output IF AND ONLY IF the path being evaluated is root.

#### Utilities
There are several utilities, such as `expandGlob`, `auditPath`, and `getFile`, which will help safely get and modify parameters. `auditPath` returns an error message if it fails to validate the integrity of the path (missing files, no execute in directory), and `expandGlob` will only return a vector of paths when it is able to both `auditPath` and wildcard match the expansion. `getFile` is not a safe getter, but it will return false if it is unable to find a single path expansion of the input. 
- `getFile` should probably be updated to be a purely unsafe getter that follows from `expandGlob` outputs since there can be files with wildcard characters in their name in POSIX compliant systems.

Wildcards:
- Globbing will correctly match for `*` and `?`
- `[]` and `()` are not implemented, as they would require a separate parser altogether, but in BASH, this can probably be supplmented with `#include <regex>`
- The `wildcardMatch` function dispatches to 3 separate matchers based on the application:
    - brute force: for small inputs, brute force is the fastest matcher I could make
    - DP-static array size: this uses dynamic programming to match the wildcard in $O(mn)$ time on a statically allocated array of `MAX_WILDCARD_LENGTH` (`constants.h`) length. It is faster than a dynamically allocated array by about 3x but only works if the wildcard is strictly less than $MAX_WILDCARD_LENGTH-1$ since the DP needs +1 element to deal with the empty string.
    - DP-dynamic array size: this is the same as above, but it dynamically allocates a large enough array to handle the input.

---

### Additional Details on Implementation Choices
`constants.h` is actually `constants and globals` because there are a few global counters and flags that are used to control the state of the system. It is possible to remove these, but it would either require muddying the return values of the functions, or it would require integrating all functionality into the core `UnixFileSystem` class, which is already quite bloated.

You can configure your own file system in main before declaring the global `SETUP` flag as false to change up the filesystem if you wish. The interface is just the public interface of `UnixFileSystem`, which mostly comprises of the base commands, a few privileged getters/setters (change user, group), and some utilities (get file nodes, etc.)
- If we want to make the executable unique to the file and different across years, we should add a privileged parameter to the `UnixFileSystem::Trie` class, which 


---

### Future Updates (?)

I believe most bugs have been resolved, but for future use, implement the following features:
- wc
- head
- tail
- uniq
- sort
- *grep
- touch
- man
- su

As well as input redirection support for each of the commands \
Currently, the loader will properly redirect inputs and outputs for the basic 1/2 streams (out/err) and input
- The main BASH commands support piping into one another, but UFS currently only allows piping into executables
- It is simply that the other commands do not use the input stream
- I believe `cat` will just treat stdin as args in the command line and the others all ignore it though


Variables + logical control flow would also be nice to add, but it is really difficult to parse BASH and zsh's [if/for/while] clauses
- You might need to make the parser accept `;` as a universal line break rather than as part of a "munched" command
- Currently, all commands separated by [|, ||, &&, ;] are treated as a single large command that is individually broken into smaller components, but I think `;` might override that and deserve to be treated as a new command altogether.
- Variables are difficult because `$VAR`, `"$VAR"`, `"${VAR}"`, `"${#VAR%%-*}"` among countless others are POSIX compliant uses of bash variables. They also can be used as commands themselves after expansion

Support for commands that call other commands
- Currently, core utils have hardcoded functionality
- They are capable of directly calling one another, but they are not capable of expanding to several commands because the entire file is parsed one time at the beginning. If you can make the parser ONLY parse a single command (by munching until the next delimiter `;` or unquoted `\n`), then it is possible to parse as you evaluate, but this sacrifices syntactical error logging for all lines simulataneously and would require students to submit much more often.

---

### Setting up Online Judge
**Omitted**