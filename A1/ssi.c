#define _POSIX_C_SOURCE 200809L //https://www.ibm.com/docs/en/zos/2.5.0?topic=files-feature-test-macros
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h> 
#include <pwd.h> 
#include <stdlib.h> 
#include <limits.h> 
#include <ctype.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include <signal.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int builtin_cd(char **argv, int argc);
static int builtin_bg(char **argv, int argc);
static int builtin_bglist(char **argv, int argc);

/*
Helper function to get and return the current user's login name
(username). I have used a primary method (getlogin()) along with 
2 fallback methods (geteuid() and environment variable) to get the 
username.
*/
static const char* get_username(void){
    //https://man7.org/linux/man-pages/man0/unistd.h.0p.html

    //primary - retrieve using getlogin()
    const char *u = getlogin();
    if(u && *u){
        return u;
    }

    //secondary - using effective userid (geteuid())
    struct passwd *pw = getpwuid(geteuid()); //static buffer
    if(pw && pw->pw_name && *pw->pw_name){
        return pw->pw_name;
    }

    //using environment variable ("USER")
    const char *envu = getenv("USER");
    if(envu && *envu){
        return envu;
    }
    return "user";
}

/*
Helper function to build the shell prompt in the format:
username@hostname: current_directory > 
The function uses username, hostname and current working 
directory and returns a heap allocated string.
*/
static char* build_prompt(void){
    char host[256] = "host";
    (void)gethostname(host, sizeof(host));

    char cwd[PATH_MAX] = "/";
    if(!getcwd(cwd, sizeof(cwd))){ //absolute path of cwd
        strncpy(cwd, "?", sizeof(cwd));
        cwd[sizeof(cwd)-1] = '\0';
    }

    //Format prompt
    char buf[PATH_MAX + 256];
    int n = snprintf(buf, sizeof(buf), "%s@%s: %s > ", get_username(), host, cwd);

    if(n < 0){
        return strdup("> ");
    }
    buf[sizeof(buf)-1] = '\0'; //Terminate
    return strdup(buf);
}

/*
Helper function to remove leading and trailing whitespace characters
from readline() before tokenizing.
*/
static void trim(char *s){
    if(!s){
        return;
    }
    size_t i = 0;
    while(s[i] && isspace((unsigned char)s[i])){
        i++;
    }
    if(i > 0){
        memmove(s, s+i, strlen(s + i) + 1);
    }
    size_t n = strlen(s);
    while(n > 0 && isspace((unsigned char)s[n-1])){
        n--;
    }
    s[n] = '\0'; //Termination
}


// Helper function to tokenize input command line
static int tokenize(char *line, char ***argv_out){
    int capacity = 8;
    int argc = 0;
    char **argv = malloc(sizeof(char*) * capacity);
    if(!argv){
        perror("malloc");
        exit(1);
    }
    char *save = NULL;
    for(char *tok = strtok_r(line, " \t\r\n", &save); tok!=NULL; tok = strtok_r(NULL, " \t\r\n", &save)){
        if(argc == capacity){
            capacity = capacity * 2;
            char **temp = realloc(argv, sizeof(char*) * capacity);
            if(!temp){
                perror("realloc");
                free(argv);
                exit(1);
            }
            argv = temp;
        }
        argv[argc++] = tok;
    }
    if(argc == capacity){
        char **tmp = realloc(argv, sizeof(char*) * (capacity + 1));
        if(!tmp){
            perror("realloc");
            free(argv);
            exit(1);
        }
        argv = tmp;
    }
    argv[argc] = NULL;
    *argv_out = argv;
    return argc;
}

// Expand ~ and ~/<path> to absolute paths using $HOME
static char* my_expand(const char *arg){
    if(!arg){
        return NULL;
    }
    if(arg[0] != '~'){
        return strdup(arg);
    }
    const char *home = getenv("HOME");
    if((!home) || (!*home)){
        home = "/";
    }
    if(arg[1] == '\0'){
        return strdup(home);
    }
    else if(arg[1] == '/'){
        size_t need = strlen(home) + strlen(arg) + 1; //+1 for Null terminator
        char *out = (char*)malloc(need);
        if(!out){
            perror("malloc");
            exit(1);
        }
        snprintf(out, need, "%s%s", home, arg + 1);
        return out;
    }
    return strdup(arg);
}

static int builtin_cd(char **argv, int argc){
    char *target = NULL;
    if(argc == 1){
        const char *home = getenv("HOME");
        target = strdup((home && *home) ? home : "/");
    }
    else{
        target = my_expand(argv[1]);
    }

    if(!target){
        fprintf(stderr, "cd: invalid target\n");
        return 1;
    }
    if(chdir(target) == -1){
        perror("cd");
    }
    free(target);
    return 1;
}

static int handle_builtin(char **argv, int argc){
    if(argc == 0){
        return 1;
    }
    if(strcmp(argv[0], "cd") == 0){
        return builtin_cd(argv,argc);
    }
    if(strcmp(argv[0], "bg") == 0){
        return builtin_bg(argv, argc);
    }
    if(strcmp(argv[0], "bglist") == 0){
        return builtin_bglist(argv, argc);
    }
    return 0;
}

// fork() to create a child
// Child --> execvp(argv[0], argv)
// Parent --> waitpid() for the child to finish 
static void run_foreground(char **argv){
    if(!argv || !argv[0]){
        return;
    }

    // parent temporarily ignores SIGINT so Ctrl-C goes to child only
    struct sigaction oldint, ign;
    ign.sa_handler = SIG_IGN;
    sigemptyset(&ign.sa_mask);
    ign.sa_flags = 0;
    sigaction(SIGINT, &ign, &oldint);

    pid_t pid = fork();
    if(pid < 0){
        perror("fork");
        sigaction(SIGINT, &oldint, NULL);
        return;
    }
    else if(pid == 0){
        //Child
        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);

        execvp(argv[0], argv);
        fprintf(stderr, "%s: No such file or directory\n", argv[0]);
        _exit(127);
    }
    else{
        //Parent
        int status;
        while(waitpid(pid, &status, 0) == -1){
            if(errno == EINTR){ //Interrupt
                continue;
            }
            perror("waitpid");
            break;
        }
        sigaction(SIGINT, &oldint, NULL);
    }
}

//Background Jobs
typedef struct Job{
    pid_t pid;
    char *cmdline;
    struct Job *next;
} Job;

static Job *jobs_head = NULL;

//Helper to add a background job
static void add_job(pid_t pid, const char *cmdline){
    Job *j = (Job*)malloc(sizeof(Job));
    if(!j){
        perror("malloc");
        exit(1);
    }
    j->pid = pid;
    j->cmdline = strdup(cmdline ? cmdline : "");
    j->next = jobs_head;
    jobs_head = j;
}

//Helper to remove a background job
static int remove_job(pid_t pid, char **out_cmd){
    Job **pp = &jobs_head;
    while(*pp){
        if((*pp)->pid == pid){
            Job *removed_job = *pp;
            *pp = removed_job->next;
            if(out_cmd){
                *out_cmd = removed_job->cmdline;
            }
            else{
                free(removed_job->cmdline);
            }
            free(removed_job);
            return 1;
        }
        pp = &(*pp)->next;
    }
    return 0;
}

static void print_bglist(void){
    int count = 0;
    for(Job *j = jobs_head; j; j = j->next){
        printf("%d: %s\n", (int)j->pid, j->cmdline);
        count++;
    }
    printf("Total Background jobs: %d\n", count);
}

static char *join_argv(char **argv, int start, int argc){
    size_t len = 0;
    for(int i = start; i < argc; i++){
        len += strlen(argv[i]) + 1;
    }
    char *s = (char*)malloc(len + 1);
    if(!s){
        perror("malloc");
        exit(1);
    }
    s[0] = '\0';
    for(int i = start; i <argc; i++){
        strcat(s, argv[i]);
        if(i+1 < argc){
            strcat(s, " ");
        }
    }
    return s;
}

static void reap_background(void){
    int status;
    pid_t pid;
    while((pid = waitpid(-1, &status, WNOHANG)) > 0){
        char *cmd = NULL;
        if(remove_job(pid, &cmd)){
            printf("%d: %s has terminated.\n", (int)pid, cmd ? cmd : "");
            fflush(stdout);
            free(cmd);
        }
    }
}

static int builtin_bg(char **argv, int argc){
    if(argc < 2){
        return 1;
    }
    char *cmdline = join_argv(argv, 1, argc);
    pid_t pid = fork();
    if(pid < 0){
        perror("fork");
        free(cmdline);
        return 1;
    }
    else if(pid == 0){
        //Child
        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
        execvp(argv[1], &argv[1]);
        fprintf(stderr, "%s: No such file or directory\n", argv[1]);
        _exit(127);
    }
    else{ 
        //Parent
        add_job(pid, cmdline);
        free(cmdline);
    }
    return 1;
}

static int builtin_bglist(char **argv, int argc){
    (void)argv;
    (void)argc;
    print_bglist();
    return 1;
}

static void sigint_prompt_handler(int signum){
    (void)signum;
    rl_replace_line("", 0);
    write(STDOUT_FILENO, "\n", 1);
    rl_on_new_line();
    rl_redisplay();
}


int main(void){
    using_history(); //initialize history library

    //CTRL + C
    struct sigaction sa;
    sa.sa_handler = sigint_prompt_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  // restart readline on signals
    sigaction(SIGINT, &sa, NULL);

    for (;;) {
        // Reap any finished background jobs before showing the prompt
        reap_background();

        char *prompt = build_prompt();
        char *line = readline(prompt);
        free(prompt);

        // Ctrl+D (EOF) exits
        if (line == NULL) {
            printf("\n");
            break;
        }

        trim(line);
        if (*line == '\0') { // ignore empty lines
            free(line);
            continue;
        }

        add_history(line);

        // Tokenize into argv[]
        char **argv = NULL;
        int argc = tokenize(line, &argv);

        // Builtins first; otherwise run in foreground
        if (!handle_builtin(argv, argc)) {
            run_foreground(argv);
        }

        free(argv); // free argv array (tokens live inside 'line')
        free(line);
    }
    return 0;
}
