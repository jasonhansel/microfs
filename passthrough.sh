#!/bin/sh
set -eu

# This implements the "directory command" interface that microfs expects,
# to create a simple FS that exposes the contents of an underlying directory,
# much like fusexmp.

# If you run "./microfs ./mnt -- ./passthrough.sh ./testdir", then ./mnt should
# contain a FUSE filesystem that is a mirror of testdir.

# Note that this may break with filenames that contain spaces or newlines.

# The interface is as follows:
# ./passthrough.sh [underlying-directory] "."
#    => gives a listing of the files in underlying-directory
# ./passthrough.sh [underlying-directory] [filename]
#    => gives information about the contents of a file in underlying-directory

if test "$2" = "."
then
  echo '!listing'
  ls -1 "$1"
elif test -d "$1/$2"
then
  # Note: if we have `./passthrough.sh ./testdir subdir`, then the directory
  # command to use for that subdirectory is `./passthrough.sh ./testdir/subdir`.

  # So, to get the contents of /subdir/subdir2/newtest.txt
  # microfs must run the following series of commands:
  #   ./passthrough.sh  ./testdir                   subdir
  #   ./passthrough.sh  ./testdir/subdir            subdir2
  #   ./passthrough.sh  ./testdir/subdir/subdir2    newtest.txt
  #   cat               ./testdir/subdir/subdir2/newtest.txt
  echo '!subdir_command'
  echo "$0 $1/$2"
elif test -f "$1/$2"
then
  # When we get to an actual file (not a directory), we echo a command that
  # gives the file's contents.
  echo '!file_command'
  echo  "cat $1/$2"
else
  exit 1
fi
