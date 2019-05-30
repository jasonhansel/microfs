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

#include "command.h"

arg_list *word_split(char *words) {
  size_t i = 0;
  size_t wl = 0;
  char *startword = words;
  arg_list *result = NULL;
  for(;;) {
    if(words[i] == ' ' || words[i] == '\0')  {
      arg_list *newresult = malloc(sizeof(arg_list)); 
      if(words[i] == ' ') {
        *newresult = (arg_list) { .len = wl, .str = startword, .prev = result};
        startword = &words[i + 1];
      } else {
        *newresult = (arg_list) { .len = wl, .str = startword, .prev = result};
      }
      result = newresult;
      wl = 0;
    }
    if(words[i] == '\0') {
      break;
    }
    i++;
    wl++;
  }

  return result;
}

void free_arg_list(arg_list *args) {
  if(args != NULL) {
    free_arg_list(args->prev);
    free(args);
  }
}

char *cwd = NULL;
void save_cwd() { 
  cwd = (char*) calloc(sizeof(char), 512);
  if(getcwd(cwd, 512) == NULL) {
    perror("getcwd");
    exit(1);
  }
}

void exec_process(int readfd, int writefd, arg_list *args) { 
  size_t i = 0;
  arg_list *argp = args;
  while(argp != NULL) { argp = argp->prev; i++; }
  if(close(readfd) != 0) {
    perror("close");
    exit(1);
  }
  size_t size = i;
  char **command = calloc(i + 1, sizeof(char*));
  argp = args;
  while(argp != NULL) {
    i--;
    command[i] = strndup(argp->str, argp->len);
  //  printf("Running %i [%s]\n", i, command[i]);fflush(stdout);
    argp = argp->prev;
  }
  printf("Running: ");
  for(i = 0; i < size; i++) {
    printf("%s ", command[i]);
  }
  printf("\n");
  fflush(stdout);
  if(dup2(writefd, STDOUT_FILENO) != STDOUT_FILENO) {
    perror("dup2");
    exit(1);
  }

  if(chdir(cwd) != 0) {
    perror("chdir");
    exit(1);
  }
  execvp(command[0], command);
  perror("exec");
  exit(1);
}

/* If we might have a running process with
 * feof(pi->output) or ferror(pi->output),
 * this will preserve our invariants.
 */
void handle_eof(process_info *pi) {
  if(pi->state == RUNNING) {
    assert(pi->output != NULL);
    assert(!ferror(pi->output));
    assert(pi->pid > 0);
    if(feof(pi->output)) {
      // printf("Closing down...\n");fflush(stdout);
      int status;
      pid_t result = waitpid(pi->pid, &status, 0);
      if(result == -1) {
        perror("waitpid");
        exit(1); 
      }

      if(fclose(pi->output) != 0) {
        perror("fclose");
        exit(1);
      }
      pi->pid = 0;
      pi->output = NULL;
      if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        pi->state = DONE;
      } else {
        pi->state = ERROR;
      }
    }
  } else {
    assert(pi->pid == 0);
    assert(pi->output == NULL);
  }
}

void start_process(process_info **pip, arg_list *args) {
  // Refernce: https://stackoverflow.com/questions/33884291/pipes-dup2-and-exec
  int pipefd[2]; // [0] is read end; [1] is write end
  if(pipe(&pipefd[0]) != 0) {
    perror("pipe");
    exit(1);
  }
  int readfd = pipefd[0], writefd = pipefd[1];
  int pid = fork();
  if(pid == -1) {
    perror("fork");
    exit(1);
  } else if(pid == 0) {
    exec_process(readfd, writefd, args);
  } else { 
    if(close(writefd) != 0) {
      perror("close");
      exit(1);
    }
    // printf("Starting up...\n");fflush(stdout);
    *pip = (process_info*) malloc(sizeof(process_info));
    process_info *pi = *pip;
    pi->offset = 0; // Note: if you remove this line, you will get a very fun
                    // bug that appears only when you least expect it.
    pi->pid = pid;
    pi->state = RUNNING;
    FILE *output = fdopen(readfd, "r");
    if(output == NULL) {
      perror("fdopen");
      exit(1);
    }
    pi->output = output;
    handle_eof(pi);
  }
}

void get_line(char **line, process_info *pi) {
  *line = NULL;
  if(pi->state != RUNNING) {
    return;
  }
  size_t sz = 0;
  ssize_t len = getline(line, &sz, pi->output);
  if(len < 1) {
    handle_eof(pi);
    if(*line != NULL) {
      free(*line);
      *line = NULL;
    }
  } else {
    pi->offset += len;
    if((*line)[len-1] == '\n') {
      // Remove trailing newline
      (*line)[len-1] = '\0';
    }
  }
}

size_t get_bytes(process_info *pi, char *buf, size_t num_bytes) {
  if(pi->state != RUNNING) {
    return 0;
  }
  errno = 0;
  size_t sz = 0;
  ssize_t len = fread(buf, sizeof(char), num_bytes, pi->output);
  if(len < num_bytes) {
    handle_eof(pi);
  }
  pi->offset += len;
  return len;
}

void close_process(process_info *pi) { 
  if(pi->state != RUNNING) {
    return;
  }
  char buf[1024];
  while(fread(&buf[0], sizeof(char), 1024, pi->output) == 1024){}
  handle_eof(pi);
}
