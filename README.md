# microfs: a FUSE wrapper for your favorite scripting language

*Note:* This was originally a final project for Storage Systems, a class at Williams College; the code
herein was written by @jasonhansel, @dominicchui, and @chriswu1996.

Recently, some people have used FUSE to create what one might call "utility filesystems:"
simple, single-purpose filesystems that might, for instance,
[provide a modified or filtered view of an underlying directory](http://users.softlab.ntua.gr/~thkala/projects/fuseflt/)
or [provide an FS-based interface for some other program](https://github.com/petere/postgresqlfs).
`microfs` is intended to make it much easier to construct such fileystems:
the goal is to allow even inexperienced users to create a simple filesystem
in the language of their choice.

## Implementation concept

The basic idea of `microfs` is that of a *directory command*.
A directory command will, when given `.` as its last argument, provide a listing of all the files
in a directory, in a format like:
```
user@panic6:~$ ./directory_command with_arguments .
!listing
file_one
file_two
file_three
```
But when it is given one of these filenames as its last argument, the directory command provides information
about the nature and contents of that file. For instance:
```
user@panic6:~$ ./directory_command with_arguments file_one
!run_command
echo "test"
```
This means that the file `file_one` will have as its contents the results of running the command `echo "test"`
in other words, `file_one` will just contain the string "test".

However, if the filename corresponds to a subdirectory, then the directory command will instead
provide *another directory command* to use to obtain that subdirectory's contents.
For instance:
```
user@panic6:~$ ./directory_command with_arguments file_one
!subdir_command
./other_directory_command
```
Thus, finding the contents of a path like `/file_one/file_two` will require two calls:
on to `./directory_command with_arguments file_one` and another to `./other_directory_command file_two`.

To mount `microfs`, you just need to provide the directory command for the root directory.
This command can be a bash script, a compiled executable, or any sort of script with
the appropriate Unix shebang.

But what if you want a *single* command to provide an entire directory tree.
The answer is that your script can use `$0` to construct a directory command.
So, if the root directory command is `./my_command /`, you would have:
```
user@panic6:~$ ./my_command / dir_1
!subdir_command
./my_command /dir_1
user@panic6:~$ ./my_command /dir_1 dir_2
!subdir_command
./my_command /dir_1/dir_2
user@panic6:~$ ./my_command /dir_1/dir_2 dir_3
!subdir_command
./my_command /dir_1/dir_2/dir_3
```
Thus the entire directory tree is handled by a single script (`./my_command`).

## Limitations and extensions

Some of the limitations of this project are intentional. We will **not** focus on:
* **Performance:** the performance of this project will be abysmal. Since this is only intended
  for relatively simple applications, we will largely ignore this.
* **Error handling:** we will not attempt to provide sophisticated mechanisms for error handling;
  instead, if any invoked command returns a nonzero exit code, we will just provide
  a generic error like `EACCES`.
* **Security:** we will not try to implement permissions, ACLs, *et cetera.*
* **Concurrency:** we will only allow single-threated FUSE operation (with `-s`).
  We may explore removing this limitation later, but multi-threaded C code is scary.

However, there are ways in which we might extend the basic concept:
* Provide more ways of composing and combining multiple utility filesystems together.
* Allow other ways of specifying directory/file contents (i.e. directives other than `!listing`,
   `!run_command`, and `!subdir_command`). For instance, we could use this to implement symlinks.
* Allow *seeking*. If a file's contents are given with `!run_command`, we have no way of seeking
  through the results, since we just read from the command's stdout.
  To allow seeking, we'd need to buffer the results of running the command.
* Allow *writes*. The above design is for a read-only file system, but could be extended
  to provide read/write access. For instance, a directory command could return `!use_file`
  with a filename to specify that reads/writes should be redirected there.
* Improve robustness, to ensure that (say) newlines within file names are respected. We could
  do this by allowing directory commands to use null bytes, instead of newlines, as separators
  (like `xargs -0`).

## Project deliverables
* Complete implementation of at least the basic concept in C.
  We will target the version of FUSE used on `panic6`, rather than the latest master.
* Tests, mostly consisting of shell scripts (to act as sample directory commands),
  C programs (to test the resulting filesystems), and expected outputs.
* A (brief) write-up summarizing our work.
* Some more sophisticated demos, written in various scripting languages.
  For instance, these *might* include such things as:
    * A filesystem that downloads things over HTTP. For instance, reading
      the file `/www.google.com/maps/#GET` would return the HTML contents
      of the webpage `http://www.google.com/maps`.
      (Note that trying to list a directory's contents would always fail.)
    * A filesystem that provides a view of an underlying folder in which
      all images are replaced by thumbnails.
    * A filesystem that provides a view of a C project together with all
      the targets in that project's Makefile. Trying to access one of these
      targets will automatically (lazily) invoke `make`.
    * A simple filesystem wrapper for a key-value store like LevelDB or Redis.
    * A filesystem interface for all the `man` pages available on a system,
      so that accesses to `/mount/8` would return the contents of `man 8 mount`.

## Prior art
One GitHub user has already tried to do [something similar](https://github.com/vi/execfuse).
Though I haven't looked at their code, I think that we provide a much simpler interface
for script authors. We do not intend to use any code from that project.

## Instructions for Running
We have two working demo scripts: passthrough.sh and http.sh
* passthrough.sh duplicates a given directory in the mountpoint similar to fusexmp
  * Run it with "./microfs ./mnt -- ./passthrough.sh ./testdir" where ./testdir is the directory to be duplicated
* http.sh accesses webpages and returns the HTML contents
  * Run it with ".microfs ./mnt -- ./http.sh http:/"
  * To get the contents of http://www.google.com/robots.tx
     access (e.g. call cat on) "./mnt/www.google.com/robots.txt/GET
* More scripts can be written using the same interface which has the following features
  * If the command ends with ".", it gives a listing of the files in the underlying-directory
  * If the command ends with a filename, it performs a script-specified file_command
  * If the command ends with a directory name, it performs a script-specified subdir_command

## Conclusions
There were a few main challenges for this project. One problem was the limited understanding of processes and bash scripts for some of our members made the task conceptually hard to understand. However, once we understood the the problems in enough detail, we had the difficulty of parsing the command line arguments. FUSE allows for simplified parsing but the limited and confusing documentation meant we chose to forgo that option and instead opted for a manual parsing technique. Another issue was time: we did not foresee the problems that were hard to understand and debug. A contributing factor was the difficulty in pinpointing the location of a bug because a specific system call, e.g. "ls", would involve many FUSE functions, and it was also sometimes unclear whether the parsing itself had fully worked. However, we were succesfully able to complete the project such that it correctly runs. Given more time, we would streamline the code such that it 1) does not have memory leaks, and 2) is more elegantly written. Overall, we can can the project a success.