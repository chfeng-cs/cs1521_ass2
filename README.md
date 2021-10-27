This is a C program file archiver, just like some common archiver such as **tar** and **zip**. Detailed description is in [ass2.pdf](https://github.com/chfeng-cs/cs1521_ass2/blob/master/ass2.pdf).

This program is able to:

- list the contents of a *blob*,
- list the permissions of files in a *blob* ,
- list the size (number of bytes) of files in a *blob*,
- check the *blobette* magic number ,
- extract files from a *blob*,
- check *blobette* integrity (hashes),
- set the file permissions of files extracted from a *blob* ,
- create a *blob* from a list of files ,
- list, extract, and create *blobs* that include directories,
- list, extract, and create *blobs* that are compressed.

The file archives in this assignment are called ***blob***s. Each ***blob*** contains one or more ***blobette***s. Each ***blobette*** records one file system object. The initial code is in [blobby.bak.c](https://github.com/chfeng-cs/cs1521_ass2/blob/master/blobby.bak.c)   and my code is in  [blobby.c](https://github.com/chfeng-cs/cs1521_ass2/blob/master/blobby.c).