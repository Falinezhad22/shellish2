
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h> // termios, TCSANOW, ECHO, ICANON
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#define READ_END 0
#define WRITE_END 1
//#define _POSIX_C_SOURCE 200809L

const char *sysname = "shellish";

enum return_codes {
  SUCCESS = 0,
  EXIT = 1,
  UNKNOWN = 2,
};

struct command_t {
  char *name;
  bool background;
  bool auto_complete;
  int arg_count;
  char **args;
  char *redirects[3];     // in/out redirection
  struct command_t *next; // for piping
};
static int builtin_cut(struct command_t *command);
static int run_cut_builtin(struct command_t *command);
static int parser(const char *s, int *fields, int max_fields);
static void cut_line(const char *line, char delim, const int *fields, int nfields);

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command) {
  int i = 0;
  printf("Command: <%s>\n", command->name);
  printf("\tIs Background: %s\n", command->background ? "yes" : "no");
  printf("\tNeeds Auto-complete: %s\n", command->auto_complete ? "yes" : "no");
  printf("\tRedirects:\n");
  for (i = 0; i < 3; i++)
    printf("\t\t%d: %s\n", i,
           command->redirects[i] ? command->redirects[i] : "N/A");
  printf("\tArguments (%d):\n", command->arg_count);
  for (i = 0; i < command->arg_count; ++i)
    printf("\t\tArg %d: %s\n", i, command->args[i]);
  if (command->next) {
    printf("\tPiped to:\n");
    print_command(command->next);
  }
}

/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command) {
  int i = 0;
  if (command->arg_count) {
    for (i = 0; i < command->arg_count; ++i)
      free(command->args[i]);
    free(command->args);
  }
  for (i = 0; i < 3; ++i)
    if (command->redirects[i])
      free(command->redirects[i]);
  if (command->next) {
    free_command(command->next);
    command->next = NULL;
  }
  free(command->name);
  free(command);
  return 0;
}

/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt() {
  char cwd[1024], hostname[1024];
  gethostname(hostname, sizeof(hostname));
  getcwd(cwd, sizeof(cwd));
  printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
  return 0;
}

/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command) {
  const char *splitters = " \t"; // split at whitespace
  int index, len;
  len = strlen(buf);
  while (len > 0 && strchr(splitters, buf[0]) != NULL) // trim left whitespace
  {
    buf++;
    len--;
  }
  while (len > 0 && strchr(splitters, buf[len - 1]) != NULL)
    buf[--len] = 0; // trim right whitespace

  if (len > 0 && buf[len - 1] == '?') // auto-complete
    command->auto_complete = true;
  if (len > 0 && buf[len - 1] == '&') // background
    command->background = true;

  char *pch = strtok(buf, splitters);
  if (pch == NULL) {
    command->name = (char *)malloc(1);
    command->name[0] = 0;
  } else {
    command->name = (char *)malloc(strlen(pch) + 1);
    strcpy(command->name, pch);
  }
 
  command->args = (char **)malloc(sizeof(char *));

  int redirect_index;
  int arg_index = 0;
  char temp_buf[1024], *arg;
  while (1) {
    // tokenize input on splitters
    pch = strtok(NULL, splitters);
    if (!pch)
      break;
    arg = temp_buf;
    strcpy(arg, pch);
    len = strlen(arg);

    if (len == 0)
      continue; // empty arg, go for next
    while (len > 0 && strchr(splitters, arg[0]) != NULL) // trim left whitespace
    {
      arg++;
      len--;
    }
    while (len > 0 && strchr(splitters, arg[len - 1]) != NULL)
      arg[--len] = 0; // trim right whitespace
    if (len == 0)
      continue; // empty arg, go for next

    // piping to another command
    if (strcmp(arg, "|") == 0) {
      struct command_t *c =
          (struct command_t *)malloc(sizeof(struct command_t));
      int l = strlen(pch);
      pch[l] = splitters[0]; // restore strtok termination
      index = 1;
      while (pch[index] == ' ' || pch[index] == '\t')
        index++; // skip whitespaces

      parse_command(pch + index, c);
      pch[l] = 0; // put back strtok termination
      command->next = c;
      continue;
    }

    // background process
    if (strcmp(arg, "&") == 0)
      continue; // handled before

    // handle input redirection
    redirect_index = -1;
    if (arg[0] == '<')
      redirect_index = 0;
    if (arg[0] == '>') {
      if (len > 1 && arg[1] == '>') {
        redirect_index = 2;
        arg++;
        len--;
      } else
        redirect_index = 1;
    }
    if (redirect_index != -1) {
      command->redirects[redirect_index] = (char *)malloc(len);
      strcpy(command->redirects[redirect_index], arg + 1);
      continue;
    }

    // normal arguments
    if (len > 2 &&
        ((arg[0] == '"' && arg[len - 1] == '"') ||
         (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
    {
      arg[--len] = 0;
      arg++;
    }
    command->args =
        (char **)realloc(command->args, sizeof(char *) * (arg_index + 1));
    command->args[arg_index] = (char *)malloc(len + 1);
    strcpy(command->args[arg_index++], arg);
  }
  command->arg_count = arg_index;

  // increase args size by 2
  command->args = (char **)realloc(command->args,
                                   sizeof(char *) * (command->arg_count += 2));

  // shift everything forward by 1
  int i =  0;
  for (i = command->arg_count - 2; i > 0; --i)
    command->args[i] = command->args[i - 1];

  // set args[0] as a copy of name
  command->args[0] = strdup(command->name);
  // set args[arg_count-1] (last) to NULL
  command->args[command->arg_count - 1] = NULL;

  return 0;
}

void prompt_backspace() {
  putchar(8);   // go back 1
  putchar(' '); // write empty over
  putchar(8);   // go back 1 again
}

/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command) {
  int index = 0;
  char c;
  char buf[4096];
  static char oldbuf[4096];

  // tcgetattr gets the parameters of the current terminal
  // STDIN_FILENO will tell tcgetattr that it should write the settings
  // of stdin to oldt
  static struct termios backup_termios, new_termios;
  tcgetattr(STDIN_FILENO, &backup_termios);
  new_termios = backup_termios;
  // ICANON normally takes care that one line at a time will be processed
  // that means it will return if it sees a "\n" or an EOF or an EOL
  new_termios.c_lflag &=
      ~(ICANON |
        ECHO); // Also disable automatic echo. We manually echo each char.
  // Those new settings will be set to STDIN
  // TCSANOW tells tcsetattr to change attributes immediately.
  tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

  show_prompt();
  buf[0] = 0;
  while (1) {
    c = getchar();
    // printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

    if (c == 9) // handle tab
    {
      buf[index++] = '?'; // autocomplete
      break;
    }

    if (c == 127) // handle backspace
    {
      if (index > 0) {
        prompt_backspace();
        index--;
      }
      continue;
    }

    if (c == 27 || c == 91 || c == 66 || c == 67 || c == 68) {
      continue;
    }

    if (c == 65) // up arrow
    {
      while (index > 0) {
        prompt_backspace();
        index--;
      }

      char tmpbuf[4096];
      printf("%s", oldbuf);
      strcpy(tmpbuf, buf);
      strcpy(buf, oldbuf);
      strcpy(oldbuf, tmpbuf);
      index += strlen(buf);
      continue;
    }

    putchar(c); // echo the character
    buf[index++] = c;
    if (index >= sizeof(buf) - 1)
      break;
    if (c == '\n') // enter key
      break;
    if (c == 4) // Ctrl+D
      return EXIT;
  }
  if (index > 0 && buf[index - 1] == '\n') // trim newline from the end
    index--;
  buf[index++] = '\0'; // null terminate string

  strcpy(oldbuf, buf);

  parse_command(buf, command);

  // print_command(command); // DEBUG: uncomment for debugging

  // restore the old settings
  tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
  return SUCCESS;
}

static void the_reaper(void) {
  int stat;
  pid_t p;
  while ((p = waitpid(-1, &stat, WNOHANG)) > 0) {
     fprintf(stderr, "[bg done] pid %d\n", p);
  }
}
static void pathfinder(char *name, char *const argv[]) {
//for when user already gives full path
  if (strchr(name, '/')) {
    execv(name, argv);
    perror("execv");
    _exit(1);
  }
  char *path = getenv("PATH");
  if (!path) {
    fprintf(stderr, "-%s: %s: PATH not set\n", sysname, name);
    _exit(1);
  }

  char *path_copy = strdup(path); 
  if (!path_copy) {
    perror("strdup");
    _exit(1);
  }

  char full[1024];

  for (char *dir = strtok(path_copy, ":"); dir != NULL; dir = strtok(NULL, ":")) {
    int n = snprintf(full, sizeof(full), "%s/%s", dir, name);
    if (n < 0 || n >= (int)sizeof(full)) continue;

    if (access(full, X_OK) == 0) {
      execv(full, argv);
      // If execv returns, it failed
      perror("execv");
      free(path_copy);
      _exit(1);
    }
  }

  fprintf(stderr, "-%s: %s: command not found\n", sysname, name);
  free(path_copy);
  _exit(1);
}

static void redirect(const struct command_t *command) {
  int fd;
  // for input redirect
  if (command->redirects[0]) {
    fd = open(command->redirects[0], O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "-%s: cannot open input '%s': %s\n",
              sysname, command->redirects[0], strerror(errno));
      _exit(1);
    }
    if (dup2(fd, STDIN_FILENO) < 0) {
      perror("dup2 stdin");
      close(fd);
      _exit(1);
    }
    close(fd);
  }

  //truncate
  if (command->redirects[1]) {
    fd = open(command->redirects[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
      fprintf(stderr, "-%s: cannot open output '%s': %s\n",
              sysname, command->redirects[1], strerror(errno));
      _exit(1);
    }
    if (dup2(fd, STDOUT_FILENO) < 0) {
      perror("dup2 stdout");
      close(fd);
      _exit(1);
    }
    close(fd);
  }

  //append
  if (command->redirects[2]) {
    fd = open(command->redirects[2], O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) {
      fprintf(stderr, "-%s: cannot open append '%s': %s\n",
              sysname, command->redirects[2], strerror(errno));
      _exit(1);
    }
    if (dup2(fd, STDOUT_FILENO) < 0) {
      perror("dup2 stdout");
      close(fd);
      _exit(1);
    }
    close(fd);
  }
}

/*static int pipe_two(struct command_t *command) {
  int fd[2];
  pid_t pid1, pid2;

  if (pipe(fd) == -1) {
    fprintf(stderr, "Pipe fail\n");
    return SUCCESS;
  }

  //first child
  pid1 = fork();
  if (pid1 < 0) {
    perror("fork");
    close(fd[READ_END]);
    close(fd[WRITE_END]);
    return SUCCESS;
  }

  if (pid1 == 0) {
    // left command writes into pipe
    close(fd[READ_END]);
    if (dup2(fd[WRITE_END], STDOUT_FILENO) < 0) {
      perror("dup2");
      _exit(1);
    }
    close(fd[WRITE_END]);
    redirect(command);
    pathfinder(command->name, command->args);
    _exit(1);
  }

  //second child
  pid2 = fork();
  if (pid2 < 0) {
    perror("fork");
    close(fd[READ_END]);
    close(fd[WRITE_END]);
    return SUCCESS;
  }

  if (pid2 == 0) {
    // right command reads from pipe
    close(fd[WRITE_END]);
    if (dup2(fd[READ_END], STDIN_FILENO) < 0) {
      perror("dup2");
      _exit(1);
    }
    close(fd[READ_END]);

    redirect(command->next);         
    pathfinder(command->next->name, command->next->args);
    _exit(1);
  }

  //parent closing both ends
  close(fd[READ_END]);
  close(fd[WRITE_END]);

  // wait unless bg process
  if (!command->background) {
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
  } else {
    // for the last command
    printf("[bg] pid %d\n", pid2);   
  }

  return SUCCESS;
} */


static int pipe_chain(struct command_t *command) {
  struct command_t *curr = command;

  int prev_read = -1;   // previous pipe
  int fd[2];            // current pipe

  pid_t pids[256];
  int pid_count = 0;

  while (curr != NULL) {
    int has_next = (curr->next != NULL);

    // Create pipe only if there's a next command
    if (has_next) {
      if (pipe(fd) == -1) {
        fprintf(stderr, "Pipe failed\n");
        if (prev_read != -1) close(prev_read);
        return SUCCESS;
      }
    }

    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      if (has_next) {
        close(fd[READ_END]);
        close(fd[WRITE_END]);
      }
      if (prev_read != -1) close(prev_read);
      return SUCCESS;
    }

    if (pid == 0) {
      //child bit

      // if exists, stdin from previous pipe
      if (prev_read != -1) {
        if (dup2(prev_read, STDIN_FILENO) < 0) {
          perror("dup2");
          _exit(1);
        }
      }

      // stdout to next pipe, if any
      if (has_next) {
        close(fd[READ_END]);
        if (dup2(fd[WRITE_END], STDOUT_FILENO) < 0) {
          perror("dup2");
          _exit(1);
        }
        close(fd[WRITE_END]);
      }

      if (prev_read != -1) close(prev_read);

      
      redirect(curr);

      if (strcmp(curr->name, "cut") == 0) {
        builtin_cut(curr);
        _exit(0);
    }

      pathfinder(curr->name, curr->args);
      _exit(1);
    }

    //parent bit
    if (pid_count < 256) pids[pid_count++] = pid;

    // parent closes previous read end
    if (prev_read != -1) {
      close(prev_read);
      prev_read = -1;
    }

    // parent keeping present READ_END for next stage; closes WRITE_END
    if (has_next) {
      close(fd[WRITE_END]);
      prev_read = fd[READ_END];
    }

    curr = curr->next;
  }

  // Foreground waiting for all pipes
  if (!command->background) {
    for (int i = 0; i < pid_count; i++) {
      waitpid(pids[i], NULL, 0);
    }
  } else {
    if (pid_count > 0) printf("[bg] pid %d\n", pids[pid_count - 1]);
  }

  return SUCCESS;
}
static int parser(const char *s, int *fields, int max_fields) {
    //parsing and storing in an array. i need to ignore commas and space
  int count = 0;
  const char *p = s;

  while (*p && count < max_fields) {
    // ignore commas and spaces
    while(*p == ',' || *p == ' ' || *p == '\t') p++;
    if (!*p) {
      break;
  }

    char *end = NULL;
    long v = strtol(p, &end, 10);
    //invalid cases
    if (end == p || v <= 0) return -1;
    fields[count++] = (int)v;
    p = end;

    // for when expecting comma and end
    while (*p == ' ' || *p == '\t') p++;
    if (*p == ',') p++;
  }

  return count;
}
static int builtin_cut(struct command_t *command) {
  char delim = '\t';
  int fields[256];
  int nfields = 0;

  // args: [0]="cut", [1]=..., last NULL
  for (int i = 1; i < command->arg_count - 1; i++) {
    char *a = command->args[i];
    if (!a) continue;

    if (strcmp(a, "-d") == 0 || strcmp(a, "--delimiter") == 0) {
      if (i + 1 >= command->arg_count - 1 || !command->args[i + 1]) {
        fprintf(stderr, "-%s: cut: option %s requires an argument\n", sysname, a);
        return SUCCESS;
      }
      char *d = command->args[++i];
      if (strlen(d) != 1) {
        fprintf(stderr, "-%s: cut: delimiter must be a single character\n", sysname);
        return SUCCESS;
      }
      delim = d[0];
    } else if (strcmp(a, "-f") == 0 || strcmp(a, "--fields") == 0) {
      if (i + 1 >= command->arg_count - 1 || !command->args[i + 1]) {
        fprintf(stderr, "-%s: cut: option %s requires an argument\n", sysname, a);
        return SUCCESS;
      }
      char *f = command->args[++i];
      nfields = parser(f, fields, 256);
      if (nfields <= 0) {
        fprintf(stderr, "-%s: cut: invalid fields list '%s'\n", sysname, f);
        return SUCCESS;
      }
    } else {
      
      fprintf(stderr, "-%s: cut: unknown option '%s'\n", sysname, a);
      // return SUCCESS; not sure if i should add this
    }
  }

  if (nfields == 0) {
    fprintf(stderr, "-%s: cut: missing -f/--fields\n", sysname);
    return SUCCESS;
  }

  char line[8192];
  while (fgets(line, sizeof(line), stdin)) {
    cut_line(line, delim, fields, nfields);
  }

  return SUCCESS;
}
static void cut_line(const char *line, char delim, const int *fields, int nfields) {
  // Split line into fields. then print them

  int next_ind = 0;      
  int field_ind = 1;    // currently

  const char *start = line;
  const char *p = line;

  // need first per line
  int first_out = 1;

  while (1) {
    if (*p == delim || *p == '\n' || *p == '\0') {
      // current field is [start, p)
      while (next_ind < nfields && fields[next_ind] == field_ind) {
        if (!first_out) putchar(delim);
        fwrite(start, 1, (size_t)(p - start), stdout);
        first_out = 0;
        next_ind++;
      }

      if (*p == delim) {
        field_ind++;
        p++;
        start = p;
        continue;
      }
      break; // EOL
    }
    p++;
  }

  putchar('\n');
}


static int run_cut_builtin(struct command_t *command) {
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return SUCCESS;
  }

  if (pid == 0) {
    redirect(command);     // makes < > >> work for cut
    builtin_cut(command);  // reads stdin, writes stdout
    _exit(0);
  }

  if (command->background) {
    printf("[bg] pid %d\n", pid);
    return SUCCESS;
  }

  waitpid(pid, NULL, 0);
  return SUCCESS;
}

// chatroom

static int has_slash(const char *s) {
  for (; *s; s++) if (*s == '/') return 1;
  return 0;
}
//ensure directory exists
static int ensre_room_dir(const char *room, char *out_dir, size_t out_sz) {
  if (snprintf(out_dir, out_sz, "/tmp/chatroom-%s", room) >= (int)out_sz) {
    fprintf(stderr, "-%s: chatroom: room name too long\n", sysname);
    return -1;
  }

  struct stat st;
  if (stat(out_dir, &st) == 0) {
    if (!S_ISDIR(st.st_mode)) {
      fprintf(stderr, "-%s: chatroom: %s exists but is not a directory\n", sysname, out_dir);
      return -1;
    }
    return 0; // exists
  }

  if (mkdir(out_dir, 0777) < 0 && errno != EEXIST) {
    fprintf(stderr, "-%s: chatroom: cannot create room dir %s: %s\n",
            sysname, out_dir, strerror(errno));
    return -1;
  }
  return 0;
}

static int ensre_usr_fifo(const char *dir, const char *user, char *out_fifo, size_t out_sz) {
  // building fifo
  if (snprintf(out_fifo, out_sz, "%s/%s", dir, user) >= (int)out_sz) {
    fprintf(stderr, "-%s: chatroom: username too long\n", sysname);
    return -1;
  }

  struct stat st;
  if (stat(out_fifo, &st) == 0) {
    if (!S_ISFIFO(st.st_mode)) {
      fprintf(stderr, "-%s: chatroom: %s exists but is not a fifo\n", sysname, out_fifo);
      return -1;
    }
    return 0; // already exists
  }

  if (mkfifo(out_fifo, 0666) < 0 && errno != EEXIST) {
    fprintf(stderr, "-%s: chatroom: cannot create fifo %s: %s\n",
            sysname, out_fifo, strerror(errno));
    return -1;
  }
  return 0;
}

static void send_to_one_fifo(const char *fifo_path, const char *msg, size_t msglen) {
  int fd = open(fifo_path, O_WRONLY | O_NONBLOCK);
  if (fd < 0) {
    // ENXIO means "no reader" (user not listening). That's fine.
    _exit(0);
  }
  // best effort write
  (void)write(fd, msg, msglen);
  close(fd);
  _exit(0);
}

static void display_message(const char *room_dir, const char *room,const char *username,  const char *line) {
  // Format: [room] user: message\n
  char msg[8192];
  int n = snprintf(msg, sizeof(msg), "[%s] %s: %s", room, username, line);
  if (n < 0) return;
  if (n >= (int)sizeof(msg)) n = (int)sizeof(msg) - 1;

  DIR *d = opendir(room_dir);
  if (!d) {
    fprintf(stderr, "-%s: chatroom: cannot open %s: %s\n", sysname, room_dir, strerror(errno));
    return;
  }

  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    const char *name = ent->d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
    if (strcmp(name, username) == 0) continue; // don't write to self

    char fifo_path[1024];
    if (snprintf(fifo_path, sizeof(fifo_path), "%s/%s", room_dir, name) >= (int)sizeof(fifo_path))
      continue;

    pid_t pid = fork();
    if (pid == 0) {
      send_to_one_fifo(fifo_path, msg, (size_t)n);
    }
    // parent continues iterating; reaper cleaning up
  }

  closedir(d);
}

static void receiver_loop(const char *myfifo) {
  int fd = open(myfifo, O_RDWR);
  if (fd < 0) {
    fprintf(stderr, "-%s: chatroom: cannot open fifo %s: %s\n", sysname, myfifo, strerror(errno));
    _exit(1);
  }

  char buf[4096];
  while (1) {
    ssize_t r = read(fd, buf, sizeof(buf) - 1);
    if (r > 0) {
      buf[r] = '\0';
      fputs(buf, stdout);
      fflush(stdout);
    } else if (r == 0) {
      // if no data
      continue;
    } else {
      // if error
      _exit(0);
    }
  }
}

static int builtin_chatroom(struct command_t *command) {
  // command->args: chatroom <roomname> <username>
  if (command->arg_count < 4) {
    fprintf(stderr, "-%s: chatroom: usage: chatroom <roomname> <username>\n", sysname);
    return SUCCESS;
  }

  const char *room = command->args[1];
  const char *user = command->args[2];
  if (!room || !user || room[0] == '\0' || user[0] == '\0') {
    fprintf(stderr, "-%s: chatroom: usage: chatroom <roomname> <username>\n", sysname);
    return SUCCESS;
  }
  // Basic safety: disallow slashes so people can't escape /tmp/chatroom-*
  if (has_slash(room) || has_slash(user)) {
    fprintf(stderr, "-%s: chatroom: room/user cannot contain '/'\n", sysname);
    return SUCCESS;
  }

  char room_dir[512];
  char myfifo[1024];
  if (ensre_room_dir(room, room_dir, sizeof(room_dir)) < 0) return SUCCESS;
  if (ensre_usr_fifo(room_dir, user, myfifo, sizeof(myfifo)) < 0) return SUCCESS;

  printf("Welcome to %s!\n", room);
  fflush(stdout);

  // Fork receiver
  pid_t recv_pid = fork();
  if (recv_pid < 0) {
    perror("fork");
    return SUCCESS;
  }

  if (recv_pid == 0) {
    receiver_loop(myfifo);
    _exit(0);
  }

  // Sender loop in parent (chatroom session)
  while (1) {
    printf("[%s] %s > ", room, user);
    fflush(stdout);

    char line[4096];
    if (!fgets(line, sizeof(line), stdin)) {
      break; // EOF
    }

    // allow quitting
    if (strcmp(line, "/exit\n") == 0 || strcmp(line, "/quit\n") == 0) {
      break;
    }

    // broadcast to others
    display_message(room_dir, room, user, line);
  }

  // End: kill receiver child
  kill(recv_pid, SIGTERM);
  waitpid(recv_pid, NULL, 0);
  return SUCCESS;
}

// Run chatroom in a separate process so your shell doesn't get stuck inside it.
static int run_chatroom_builtin(struct command_t *command) {
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return SUCCESS;
  }

  if (pid == 0) {
    // chatroom is interactive; we usually do NOT apply redirect() here.
    // If you *want* < > >> to affect chatroom, you could add redirect(command).
    builtin_chatroom(command);
    _exit(0);
  }

  if (command->background) {
    printf("[bg] pid %d\n", pid);
    return SUCCESS;
  }

  waitpid(pid, NULL, 0);
  return SUCCESS;
}

int process_command(struct command_t *command) {
  //int r;
  the_reaper();
  if (strcmp(command->name, "") == 0)
    return SUCCESS;

  if (strcmp(command->name, "exit") == 0)
    return EXIT;
  
  if (strcmp(command->name, "cd") == 0) {
    char *dest = command->args[1];

    if (dest == NULL)
      dest = getenv("HOME");

    if (dest == NULL || chdir(dest) == -1)
      fprintf(stderr, "-%s: cd: %s\n", sysname, strerror(errno));

    return SUCCESS;
  }
  if (strcmp(command->name, "cut") == 0) {
    return run_cut_builtin(command);
}
if (strcmp(command->name, "chatroom") == 0) {
    return run_chatroom_builtin(command);
  }


  if (command->next != NULL) {
    return pipe_chain(command);
  }


  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return SUCCESS;
  }
  if (pid == 0) // child 
  {
    redirect(command);
    pathfinder(command->name, command->args);
    _exit(1); 
    /// This shows how to do exec with environ (but is not available on MacOs)
    // extern char** environ; // environment variables
    // execvpe(command->name, command->args, environ); // exec+args+path+environ

    /// This shows how to do exec with auto-path resolve
    // add a NULL argument to the end of args, and the name to the beginning
    // as required by exec

    // TODO: do your own exec with path resolving using execv()
    // do so by replacing the execvp call below
    //execvp(command->name, command->args); // exec+args+path
    //printf("-%s: %s: command not found\n", sysname, command->name);
    //exit(127);
  } 
  else{ // parent
    if (command->background) {
      printf("[bg] pid %d\n", pid);
      return SUCCESS;
    }


    if (waitpid(pid, NULL, 0) < 0) {
      perror("waitpid");
    }
    return SUCCESS;              
  }
}



int main() {
  while (1) {
    struct command_t *command =
        (struct command_t *)malloc(sizeof(struct command_t));
    memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

    int code;
    code = prompt(command);
    if (code == EXIT)
      break;

    code = process_command(command);
    if (code == EXIT)
      break;

    free_command(command);
  }

  printf("\n");
  return 0;
}