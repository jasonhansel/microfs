#!/bin/sh

# This implments the microfs interface. It creates a simple filesystem
# that can be used to access files (e.g. webpages) over HTTP.

# If you run "./microfs ./mnt -- ./http.sh http:/", then accessing
# "./mnt/www.google.com/robots.txt/GET" will give you the results of an
# HTTP GET request for http://www.google.com/robots.txt". Conveniently,
# this happens to be in plain text.

if test "$2" = "."
then
  # We can't get directory listings, so just give up.
  exit 1
elif test "$2" = "GET"
then
  echo '!file_command'
  echo "curl $1"
else
  echo '!subdir_command'
  echo "$0 $1/$2"
fi
