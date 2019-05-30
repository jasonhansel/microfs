#pragma once 
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <libgen.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>

// A linked list of command-line arguments, in reverse order.
typedef struct arg_list_t {
  struct arg_list_t *prev;
  char *str;
  size_t len;
} arg_list;
// Convert a string like "a b c" to a linked list like "c" -> "b" -> "a"
arg_list *word_split(char *words);
// Free a linked list
void free_arg_list(arg_list *args);


// The state of a child process:
typedef enum process_state_t {
  RUNNING, // Still running (or a zombie)
  DONE, // Done, exited normally with zero exit code
  ERROR // Done, exited abnormally or with nonzero exit code
} process_state;

// All the information needed to keep track of a process.
// After each call, EXACTLY ONE of the following is true:
// - pid == 0, output == NULL, state != RUNNING
// - pid != 0, output != NULL, !feof(output), !ferror(output), state == RUNNING
typedef struct process_info_t {
  pid_t pid;
  process_state state;
  FILE *output;
  size_t offset;
} process_info;

// The current working directory.
void save_cwd(); // Save in *cwd.

// Start a process, placing its information in **pip.
// Note that "args" contains both the name of the command and the arguments
// to be passed to it.
void start_process(process_info **pip, arg_list *args);

// Read a line of data from a process's stdout, maintaining the above invariants.
// *line will be NULL if no line is available.
void get_line(char **line, process_info *pi);
void close_process(process_info *pi);
size_t get_bytes(process_info *pi, char *buf, size_t num_bytes);
