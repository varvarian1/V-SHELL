#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/executor.h"
#include "../include/prompt.h"

void handle_sigint(int sig) {
    (void)sig;
    printf("\n");
    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
}

int main(void) {
    signal(SIGINT, handle_sigint);
    using_history();

    char *input;
    while (1) {
	const char *prompt = get_prompt();
	char *input = readline(prompt);

        if (!input) {
            printf("exit\n");
            break;
        }

        if (*input) {
            add_history(input);

            int tokenCount;
            Token *tokens = tokenize(input, &tokenCount);
            if (tokens) {
                ASTNode *ast = parse(tokens, tokenCount);
                if (ast) {
                    int status = execute(ast);
                    free_ast(ast);
                } else {
                    printf("Syntax error!\n");
                }

                free_tokens(tokens, tokenCount);
            } else {
                printf("Lexical error!\n");
            }
        }

        free(input);
    }

    return 0;
}
