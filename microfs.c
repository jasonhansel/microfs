#define FUSE_USE_VERSION 26

#include <fuse.h>
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

arg_list *root_command;

int get_path(process_info **pi, const char *path, arg_list *root_command);

//Assumes access is always available because we do not implement permissions
static int microfs_access(const char *path, int mask)
{
  return 0;
}

// Reads a directory in accordance with the specified script command
int microfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
		    off_t offset, struct fuse_file_info* fi) {

  process_info *pi = malloc(sizeof(process_info));
  //If it is the root directory
  if (! strcmp(path, "/")){
    arg_list args = { .prev = root_command , .str = ".", .len = strlen(".") };
    start_process(&pi, &args);
    char *line;
    get_line(&line, pi);
    if(line == NULL) { return -EACCES; }
    assert(!strcmp(line, "!listing"));
    int i = 0;
    if(i++ >= offset) {
      if(filler(buf, ".", NULL, i)) return 0;
    }
    if(i++ >= offset) {
      if(filler(buf, "..", NULL, i)) return 0;
    }
    for(;;) {
      char *name = NULL;
      get_line(&name, pi);
      if(name == NULL) break;
      if(i++ >= offset) {
	if(filler(buf, name, NULL, i)) return 0;
      }
    }
    //Otherwise check what kind of operation is being requested
  } else {
    process_info *pi_parent;
    int err = get_path(&pi_parent, path, root_command);
    if(err) {
      return err;
    }
    char *line;
    get_line(&line, pi_parent);
    assert(!strcmp(line, "!subdir_command"));
    char *cmd;
    get_line(&cmd, pi_parent);

    arg_list args = { .prev = word_split(cmd) , .str = ".", .len = strlen(".") };
    start_process(&pi, &args);
    get_line(&line, pi);
    if(line == NULL) { return -EACCES; }

    assert(!strcmp(line, "!listing"));
    int i = 0;
    if(i++ >= offset) {
      if(filler(buf, ".", NULL, i)) return 0;
    }
    if(i++ >= offset) {
      if(filler(buf, "..", NULL, i)) return 0;
    }

    for(;;) {
      char *name = NULL;
      get_line(&name, pi);
      if(name == NULL) break;
      if(i++ >= offset) {
	if(filler(buf, name, NULL, i)) return 0;
      }
    }

  }

  return 0;
}

// Find and run the command for a given path; the command info will be put in *pi.
// See passthrough.sh for the interface that this is using.
// Returns an errno if getting one of the path's ancestors failed.
int get_path_inner(process_info **pi, char *dir, char *base, arg_list *root_command) {
  *pi = NULL;
  if(!strcmp(dir, "/")) {
    arg_list args = { .prev = root_command , .str = base, .len = strlen(base) };
    start_process(pi, &args);
    return 0;
  } else {
    process_info *parent;
    int parent_err = get_path(&parent, dir, root_command) != 0;
    if(parent_err) {
      return parent_err;
    }
    char *line;
    get_line(&line, parent);
    if(parent->state == ERROR) {
      // Parent does not actually exist
      return ENOENT;
    } else if(line == NULL) {
      printf("Got no output when traversing directory: %s/%s\n", dir, base);
      fflush(stdout);
      return EIO;
    } else if (strcmp(line, "!subdir_command")) {
      return ENOTDIR;
    } else {
      char *cmd;
      get_line(&cmd, parent);
      close_process(parent);
      if(parent->state == ERROR) {
        return ENOENT;
      } else if(cmd == NULL) { 
        printf("Got no output when traversing directory: %s/%s\n", dir, base);
        fflush(stdout);
        return EIO;
      } else {
        arg_list args = { .prev = word_split(cmd) , .str = base, .len = strlen(base) };
        start_process(pi, &args);
        free_arg_list(args.prev);
        return 0;
      }
    }
  }
}


int get_path(process_info **pi, const char *path, arg_list *root_command) {
  assert(strcmp(path, "/")); // Path can't just be '/'
  char *dir_str = strdup(path);
  char *base_str = strdup(path);
  char *dir = dirname(dir_str);
  char *base = basename(base_str);
  int err = get_path_inner(pi, dir, base, root_command);
  free(dir_str);
  free(base_str);
  return err;
}

//Gets the information about the file given the path. Checks whether it is root
//or whether the output is "!file_command" for a file or "!subdir_command" for a
//subdirectory
static int microfs_getattr(const char *path, struct stat *stbuf) {
  if(!strcmp(path, "/")) {
  stbuf->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
  } else {
     process_info *pi_parent = malloc(sizeof(process_info));
    int err = get_path(&pi_parent, path, root_command);
    if(err) { return -ENOENT; }
    assert(pi_parent->state != ERROR);
    char *line = NULL;
    get_line(&line, pi_parent);
    if(line == NULL) {
      return -ENOENT;
    }
    if (!strcmp(line,"!file_command")) {
      stbuf->st_mode = S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO;  
    } else if(!strcmp(line, "!subdir_command")) {
      stbuf->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
    } else {
      return -ENOENT;
    }
  }
  stbuf->st_nlink = 1; // No hard link support
  stbuf->st_size = 1; // Seems to work with most programs
  stbuf->st_blocks = 20;
  stbuf->st_blksize = 20;

  return 0;
}

// Opens the file by performing the script specified file_command
static int innermicrofs_open(const char* path, struct fuse_file_info* fi, process_info *pi_parent, process_info *pi) {
  int err = get_path(&pi_parent, path, root_command);
  char *line; get_line(&line, pi_parent);
  if(pi_parent->state == ERROR) { return -ENOENT; }
  if(strcmp(line, "!file_command")) {
    return -ENOENT;
  }
  char *cmd; get_line(&cmd, pi_parent);
  
  if(pi_parent->state == ERROR) { return -ENOENT; }
  start_process(&pi, word_split(cmd));
  if (err)
    return -err;
  fi->fh = (size_t) pi;
  return 0;
}

static int microfs_open(const char* path, struct fuse_file_info* fi) {
  process_info *pi_parent = malloc(sizeof(process_info));
  process_info *pi = malloc(sizeof(process_info));
  int result = innermicrofs_open(path, fi, pi_parent, pi);
  close_process(pi_parent);
  close_process(pi);
  free(pi_parent);
  free(pi);
  return result;
}

// Reads the contents of a file in accordance with the script file command
static int microfs_read(const char* path, char *buf, size_t size, off_t _off, struct fuse_file_info* fi) {  
  process_info *pi = (process_info*) fi->fh;
  if(_off != pi->offset) {
    printf("Seek error: %d %d %lx\n", _off, pi->offset, fi->fh); fflush(stdout);
    return -ESPIPE; // test with 'tail' command -- "Illegal seek"
  }
  assert(pi->state != ERROR);
  int len = get_bytes(pi, buf, size);
  assert(pi->state != ERROR);
  fflush(stdout);
  
  return len;
}

// Both functions should return 0 because nothing needs to be done
static int microfs_flush(const char* path, struct fuse_file_info* fi) { return 0; }
static int microfs_release(const char* path, struct fuse_file_info *fi) { return 0; }


static struct fuse_operations microfs_operations = {
  .getattr= microfs_getattr,
  .access= microfs_access,
  .readdir= microfs_readdir,
  .release = microfs_release,
  .open= microfs_open,
  .read= microfs_read,
  .flush = microfs_flush
};


int main(int argc, char **argv) { 
  save_cwd();

  //Look at the command line arguments
  //The argument immediately after "--" is the bash script
  //All arguments after that are arguments for the script
  int commandset=0;
  arg_list *command = malloc(sizeof(arg_list));
  arg_list *argumentlist = malloc(sizeof(arg_list));
  char *mount_dir = NULL;
  for (int i=1; i < argc; i++) {
    if (!strcmp(argv[i],"--")){
      *command = (arg_list) {.len = strlen(argv[i+1]), .str = argv[i+1], .prev = NULL};
      i++;
      argumentlist = command;
      commandset = 1;
    } else if (commandset){
      arg_list *argument = malloc(sizeof(arg_list));
      *argument = (arg_list) {.len = strlen(argv[i]), .str = argv[i], .prev = argumentlist};
      argumentlist = argument;      
    } else if(argv[i][0] == '\0' || argv[i][0] == '-' || mount_dir != NULL) {
      printf("Invalid arguments! This program does not accept normal FUSE options.");
      exit(1);
    } else {
      mount_dir = argv[i];
    }
  }
  if(mount_dir == NULL) {
    printf("No mount directory specified.");
    exit(1);
  }

  //update the parameters to mount and run
  char **new_argv = malloc(sizeof(char*) * 8);
  new_argv[0] = argv[0];
  new_argv[1] = "-f";
  new_argv[2] = "-s";
  new_argv[3] = "-o";
  // Note: setting max_background = 0 makes things more reliable, for some reason
  new_argv[4] = "sync_read,auto_unmount,attr_timeout=0,entry_timeout=0,direct_io,noauto_cache,max_read=512,max_readahead=0,max_background=0";
  new_argv[5] = mount_dir;
  new_argv[6] = NULL;
  int new_argc=6;
  root_command = argumentlist;
    
  return fuse_main(new_argc, new_argv, &microfs_operations, NULL);
}
