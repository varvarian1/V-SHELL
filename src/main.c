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
#include "../include/symbol.h"

void handle_sigint(int sig) {
    (void)sig;
    printf("\n");
    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
}

int main(int argc, char **argv) {
    import_environment();

    /* if a script file is given, execute it and exit */
    if (argc > 1) {
        return execute_script(argv[1], argc-2, &argv[2]);
    }
    
    signal(SIGINT, handle_sigint);
    using_history();

    char *input;
    while (1) {
	    const char *prompt = get_prompt();
	    input = readline(prompt);

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
                    (void)execute(ast);
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
