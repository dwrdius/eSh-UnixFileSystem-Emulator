#!/bin/bash

# Welcome to the CTF
# You are in a filesystem filled with random files and some instructions to point you towards the correct solution
#
# You may use the following commands
# ls [-lad]
# find [-name|-type|-maxdepth]
# cd [-]
# mkdir [-p]
# cat 
# chmod [symbolic, octal]
# rm [-rf]
# echo
# pwd
# whoami
# ./executableFile (on valid files with executable functionality)

# Some behaviour may not match what you would see in a real linux filesystem
# Globbing is restricted to only '*' and '?'
# You may use '', "", |, ||, &&, ;
# However, most commands will just exit on error (since you have no immediate feedback) so only | is realistically useful
# You may also use \ to escape line breaks (but it also escapes whitespaces, so be careful)
# Variables are not implemented (including $PATH, $HOME, "$EXPANSION", "${#EXPANSION:%...}")

# There is a hard limit of 300 files/directories including root, so try to stay within the limit
# It is possible to get both keys for Part1 and Part2 from this. 

# sudo does not exist, but you can still remove the root directory if you wished.
# I don't recommend it though.
