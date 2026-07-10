#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../include/executor.h"

/* Base value for exit status when a process is terminated by a signal.
 * Status = 128 + signal_number (as in POSIX shells).
 */
#define EXIT_SIGNAL_BASE 128

static int execute_command(ASTNode *node);
static int execute_pipeline(ASTNode *node);
static int execute_redirect(ASTNode *node);

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
        case NODE_AND:
            return execute(node->data.binary.left) == 0 && execute(node->data.binary.right) == 0;
        case NODE_OR:
            return execute(node->data.binary.left) == 0 || execute(node->data.binary.right) == 0;
        case NODE_SEMICOLON:
            execute(node->data.binary.left);
            return execute(node->data.binary.right);
        default:
            return 0;
    }
}

static int execute_command(ASTNode *node) {
    char **argv = node->data.command.argv;
    int argc = node->data.command.argc;
    if (argc == 0) {
        return 0;
    }
    if (strcmp(argv[0], "exit") == 0) {
        exit(0);
    }
    if (strcmp(argv[0], "cd") == 0) {
        const char *path = (argc > 1) ? argv[1] : getenv("HOME");
        if (chdir(path) != 0) {
            perror("cd");
            return 1;
        }

        return 0;
    }
    if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; i < argc; i++) {
            printf("%s", argv[i]);
            if (i + 1 < argc)
                printf(" ");
        }
        printf("\n");

        return 0;
    }
    if (strcmp(argv[0], "pwd") == 0) {
        char *cwd = getcwd(NULL, 0);
        if (cwd) {
            printf("%s\n", cwd);
            free(cwd);
        } else {
            perror("pwd");
            return 1;
        }

        return 0;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
            return WEXITSTATUS(status);

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
