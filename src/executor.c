#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include "../include/executor.h"
#include "../include/symbol.h"
#include "../include/history.h"

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
        case NODE_COMMAND: { return execute_command(node); }
        case NODE_PIPELINE: { return execute_pipeline(node); }
        case NODE_REDIRECT: { return execute_redirect(node); }

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
        case NODE_IF: {
            int conditionResult = execute(node->data.ifNode.condition);
            if (conditionResult == 0) {
                return execute(node->data.ifNode.thenBranch);
            } else if (node->data.ifNode.elseBranch) {
                return execute(node->data.ifNode.elseBranch);
            }
            return conditionResult;
        }
        case NODE_SEMICOLON: {
            execute(node->data.binary.left);
            return execute(node->data.binary.right);
        }
        case NODE_FOR: {
            ASTNode *forNode = node;
            int status = 0;

            /* if no word list, iterate over positional parameters ($@) */
            if (forNode->data.forNode.wordCount == 0) {
                int count = get_positional_argc();
                for (int i = 1; i <= count; i++) {
                    char *val = get_positional_arg(i);
                    set_symbol(forNode->data.forNode.varName, val ? val : "", 0);
                    status = execute(forNode->data.forNode.body);
                    if (status != 0) {
                        break;
                    }
                }
            } else {
                for (int i = 0; i < forNode->data.forNode.wordCount; i++) {
                    set_symbol(forNode->data.forNode.varName,
                               forNode->data.forNode.wordList[i], 0);
                    status = execute(forNode->data.forNode.body);
                    if (status != 0) {
                        break;
                    }
                }
            }

            return status;
        }
        
        case NODE_WHILE: {
            int status = 0;
            while (1) {
                int condition = execute(node->data.whileNode.condition);
                if (condition != 0) {
                    break;
                }

                status = execute(node->data.whileNode.body);
                if (status != 0) {
                    break;
                }
            }

            return status;
        }

        default: {
            return 0;
        }
    }
}

static int is_executable_script(const char* path) {
    if (!strchr(path, '/')) {
        return 0;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR);
}

int execute_script(const char *filename, int argc, char **argv) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return -1;
    }

    set_positional_args(argc, argv);
    set_symbol("0", filename, 0);

    char *content = NULL;
    size_t contentSize = 0;
    char *line = NULL;
    size_t lineLen = 0;
    ssize_t line_read;

    while ((line_read = getline(&line, &lineLen, file)) != -1) {
        char *new_content = realloc(content, contentSize + line_read + 1);
        if (!new_content) {
            perror("realloc");
            free(content);
            free(line);
            fclose(file);
            return -1;
        }
        content = new_content;
        memcpy(content + contentSize, line, line_read);
        contentSize += line_read;
        content[contentSize] = '\0';
    }

    free(line);
    fclose(file);

    if (!content || contentSize == 0) {
        free(content);
        return 0;
    }

    /* skip shebang line if present */
    char *start = content;
    if (contentSize >= 2 && content[0] == '#' && content[1] == '!') {
        start = strchr(content, '\n');
        if (start) {
            start++;  /* skip the newline character */
        } else {
            /* only shebang line, no actual script content */
            free(content);
            return 0;
        }
    }

    if (!start || *start == '\0') {
        free(content);
        return 0;
    }

    int tokenCount = 0;
    Token *tokens = tokenize(start, &tokenCount);
    free(content);

    if (!tokens || tokenCount == 0) {
        return 0;
    }

    ASTNode *ast = parse(tokens, tokenCount);
    if (!ast) {
        fprintf(stderr, "Syntax error in script: %s\n", filename);
        free_tokens(tokens, tokenCount);
        return -1;
    }

    int status = execute(ast);
    
    free_ast(ast);
    free_tokens(tokens, tokenCount);

    return status;
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

    if (is_executable_script(argv[0])) {
        int status = execute_script(argv[0], argc-1, &argv[1]);
        free_expanded_argv(expanded_argv, argc);
        return status;
    }

    if (strcmp(argv[0], "exit") == 0) {
        int code;
        if ((argc > 1)) {
            code = atoi(argv[1]);
        } else {
            code = 0;
        }
        free_expanded_argv(expanded_argv, argc);
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
    if (strcmp(argv[0], "history") == 0) {
        show_shell_history();
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

        export_environment();
        free_expanded_argv(expanded_argv, argc);
        return 0;
    }
    if (strcmp(argv[0], "unset") == 0) {
        for (int i = 1; i < argc; i++) {
            unset_symbol(argv[i]);
        }

        export_environment();
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
