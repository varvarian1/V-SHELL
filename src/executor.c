#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "../include/executor.h"
#include "../include/symbol.h"

/* Base value for exit status when a process is terminated by a signal.
 * Status = 128 + signal_number (as in POSIX shells).
 */
#define EXIT_SIGNAL_BASE 128

static int execute_command(ASTNode *node);
static int execute_pipeline(ASTNode *node);
static int execute_redirect(ASTNode *node);
static char *expand_variables(const char *arg);
static void free_expanded_argv(char **argv, int argc);

int execute(ASTNode *node) {
    if (!node) {
        return 0;
    }

    switch (node->type) {
        case NODE_COMMAND:
            return execute_command(node);
        case NODE_PIPELINE:
            return execute_pipeline(node);
        case NODE_REDIRECT:
            return execute_redirect(node);
        case NODE_AND: {
            int left_ret = execute(node->data.binary.left);
            if (left_ret != 0) {
                return left_ret;
            }
            return execute(node->data.binary.right);
        }
        case NODE_OR: {
            int left_ret = execute(node->data.binary.left);
            if (left_ret == 0) {
                return 0;
            }
            return execute(node->data.binary.right);
        }
        case NODE_SEMICOLON:
            execute(node->data.binary.left);
            return execute(node->data.binary.right);
        default:
            return 0;
    }
}

int execute_script(const char *filename, int argc, char **argv) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return -1;
    }

    set_positional_args(argc, argv);
    set_symbol("0", filename, 0);

    char *line = NULL;
    size_t len = 0;
    ssize_t lineLength;
    int lastStatus = 0;
    
    /* Read the next line from the script file.
     * Returns number of characters read, or -1 on EOF/error. */
    while ((lineLength = getline(&line, &len, file)) != -1) {
        /* remove trailing newline */
        if (lineLength > 0 && line[lineLength-1] == '\n') {
            line[lineLength-1] = '\0';
        }

        /* skip empty lines */
        if (line[0] == '\0') {
            continue;
        }
        
        /* skip comments */
        if (line[0] == '#') {
            continue;
        }

        ASTNode *node = parse_line(line);
        if (!node) {
            fprintf(stderr, "Syntax error is script line: %s\n", line);
            continue;
        }

        int status = execute(node);
        free_ast(node);
        if (status != 0) {
            lastStatus = status; // remember last non-zero exit code
        }
    }

    free(line);
    fclose(file);

    return lastStatus;
}

static int execute_command(ASTNode *node) {
    char **origin_argv = node->data.command.argv;
    int argc = node->data.command.argc;

    if (argc == 0) {
        return 0;
    }
    
    /* expand variables */
    char **expanded_argv = malloc((argc+1) * sizeof(char*));
    for (int i = 0; i < argc; i++) {
        expanded_argv[i] = expand_variables(origin_argv[i]);
    }
    expanded_argv[argc] = NULL;
    char **argv = expanded_argv;

    if (strcmp(argv[0], "exit") == 0) {
        int code;
        if ((argc > 1)) {
            code = atoi(argv[1]);
        } else {
            code = 0;
        }
        exit(code);
    }
    if (strcmp(argv[0], "cd") == 0) {
        const char *path = (argc > 1) ? argv[1] : getenv("HOME");
        if (chdir(path) != 0) {
            perror("cd");
            free_expanded_argv(expanded_argv, argc);
            return 1;
        }

        free_expanded_argv(expanded_argv, argc);
        return 0;
    }
    if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; i < argc; i++) {
            printf("%s", argv[i]);
            if (i + 1 < argc)
                printf(" ");
        }
        printf("\n");
        
        free_expanded_argv(expanded_argv, argc);
        return 0;
    }
    if (strcmp(argv[0], "pwd") == 0) {
        char *cwd = getcwd(NULL, 0);
        if (cwd) {
            printf("%s\n", cwd);
            free(cwd);
        } else {
            perror("pwd");
            free_expanded_argv(expanded_argv, argc);
            return 1;
        }
        
        free_expanded_argv(expanded_argv, argc);
        return 0;
    }
    if (strcmp(argv[0], "export") == 0) {
        for (int i = 1; i < argc; i++) {
            char *eq = strchr(argv[i], '=');
            if (eq) {
                *eq = '\0';
                set_symbol(argv[i], eq+1, 1);
                *eq = '=';
            } else {
                set_symbol(argv[i], get_symbol_value(argv[i]) ? get_symbol_value(argv[i]) : "", 1);
            }
        }

        free_expanded_argv(expanded_argv, argc);
        return 0;
    }
    if (strcmp(argv[0], "unset") == 0) {
        for (int i = 1; i < argc; i++) {
            unset_symbol(argv[i]);
        }

        free_expanded_argv(expanded_argv, argc);
        return 0;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        free_expanded_argv(expanded_argv, argc);
        return -1;
    }
    if (pid == 0) {
        export_environment();
        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return -1;
    }
}

static int execute_pipeline(ASTNode *node) {
    struct sigaction sa_old, sa_new;
    sa_new.sa_handler = SIG_IGN;
    sigemptyset(&sa_new.sa_mask);
    sa_new.sa_flags = 0;
    sigaction(SIGPIPE, &sa_new, &sa_old);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        sigaction(SIGPIPE, &sa_old, NULL);
        perror("pipe");

        return -1;
    }

    pid_t left_pid = fork();
    if (left_pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        
        return -1;
    }
    if (left_pid == 0) {
        /* child: execute left command with stdout -> pipe */
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        
        /* For built-in commands, we need to execute them directly */
        /* But they will write to the pipe because we redirected stdout */
        int result = execute_command(node->data.binary.left);
        exit(result);
    }

    pid_t right_pid = fork();
    if (right_pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        kill(left_pid, SIGTERM);
        waitpid(left_pid, NULL, 0);
        return -1;
    }
    if (right_pid == 0) {

        /* child: execute right command with stdin <- pipe */
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]);
        close(pipefd[0]);
        
        int result = execute_command(node->data.binary.right);
        exit(result);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    int left_status, right_status;
    waitpid(left_pid, &left_status, 0);
    waitpid(right_pid, &right_status, 0);

    sigaction(SIGPIPE, &sa_old, NULL);

    if (WIFEXITED(right_status)) {
        return WEXITSTATUS(right_status);
    }
    if (WIFSIGNALED(right_status)) {
        return EXIT_SIGNAL_BASE + WTERMSIG(right_status);
    }
    return -1;
}

static int execute_redirect(ASTNode *node) {
    int fd = node->data.redirect.fd;
    char *filename = node->data.redirect.filename;
    int mode = node->data.redirect.mode; /* 0 - <, 1 - >, 2 - >> */

    int saved_fd = dup(fd);
    if (saved_fd == -1) {
        perror("dup");
        return -1;
    }

    int flags = O_WRONLY | O_CREAT;
    if (mode == 0) {
        flags = O_RDONLY;
    } else if (mode == 2) {
        flags |= O_APPEND;
    } else {
        flags |= O_TRUNC;
    }

    int new_fd = open(filename, flags, 0644);
    if (new_fd == -1) {
        perror("open");
        dup2(saved_fd, fd);
        close(saved_fd);

        return -1;
    }

    if (dup2(new_fd, fd) == -1) {
        perror("dup2");
        close(new_fd);
        dup2(saved_fd, fd);
        close(saved_fd);

        return -1;
    }
    close(new_fd);

    int result = execute(node->data.redirect.child);

    dup2(saved_fd, fd);
    close(saved_fd);

    return result;
}

static char *expand_variables(const char *arg) {
    if (strchr(arg, '$') == NULL) {
        return strdup(arg);
    }

    char *result = strdup("");
    if (!result) {
        return NULL;
    }

    const char *position = arg;
    const char *start = position;

    while (*position) {
        if (*position == '$') {
            if (position > start) {
                size_t currentLen = strlen(result);
                size_t appendLen = position - start;
                char *newResult = realloc(result, currentLen + appendLen + 1);
                if (!newResult) {
                    free(result);
                    return NULL;
                }
                result = newResult;
                strncat(result, start, appendLen);
            }
            position++;
            
            char name[256];
            int i = 0;
            if (*position == '{') {
                position++;
                while (*position && *position != '}') {
                    if (i < 255) {
                        name[i++] = *position;
                    }
                    position++;
                }
                if (*position == '}') {
                    position++;
                }
                name[i] = '\0';
            } else {
                while (*position && (isalnum(*position) || *position == '_')) {
                    if (i < 255) {
                        name[i++] = *position;
                    }
                    position++;
                }
                name[i] = '\0';
            }

            char *val = get_symbol_value(name);
            if (!val) {
                val = "";
            }

            size_t currentLen = strlen(result);
            size_t appendLen = strlen(val);
            char *newResult = realloc(result, currentLen + appendLen + 1);
            if (!newResult) {
                free(result);
                return NULL;
            }
            result = newResult;
            strcat(result, val);
            
            start = position;
        } else {
            position++;
        }
    }

    if (position > start) {
        size_t currentLen = strlen(result);
        size_t appendLen = position - start;
        char *newResult = realloc(result, currentLen + appendLen + 1);
        if (!newResult) {
            free(result);
            return NULL;
        }
        result = newResult;
        strncat(result, start, appendLen);
    }

    return result;
}

static void free_expanded_argv(char **argv, int argc) {
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}
