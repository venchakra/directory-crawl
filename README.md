
dircrawl - Find total number of files under a given directory tree.

Assignment
==========
Our task is to improve the performance of the dircrawl program.

Conditions
==========

1. If you are using threads, assume the following:
  a. Threads are expensive, so don't spin up more than FIVE threads.
  b. Creation and deletion of threads are expensive.

Helper Functions
================

1. file_type_t file_type(char *path): Will tell if the given path is a regular file or directory.
2. char *next_file(char *dir_name, DIR *dir): Returns the next file path in the given directory.
