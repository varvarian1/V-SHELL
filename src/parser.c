#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/parser.h"

static ASTNode *parse_list(ParserState *ps);
static ASTNode *parse_pipeline(ParserState *ps);
static ASTNode *parse_command(ParserState *ps);

/* returns current token without consuming it */
static Token *peek(ParserState *ps) {
    if (ps->pos < ps->count) {
        return &ps->tokens[ps->pos];
    }
    return NULL;
}

/* consumes and returns current token, advancing position */
static Token *advance(ParserState *ps) {
    if (ps->pos < ps->count) {
        return &ps->tokens[ps->pos++];
    }
    return NULL;
}

/* checks if next token matches type; consumes it if true */
static int match(ParserState *ps, TokenType type) {
    Token *tok = peek(ps);
    if (tok && tok->type == type) {
        advance(ps);
        return 1;
    }
    return 0;
}

static ASTNode *new_command_node(char **argv, int argc) {
    ASTNode *node = calloc(1, sizeof(ASTNode));
    node->type = NODE_COMMAND;
    node->data.command.argv = argv;
    node->data.command.argc = argc;
    return node;
}

static ASTNode *new_binary_node(NodeType type, ASTNode *left, ASTNode *right) {
    ASTNode *node = calloc(1, sizeof(ASTNode));
    node->type = type;
    node->data.binary.left = left;
    node->data.binary.right = right;
    return node;
}

static ASTNode *new_redirect_node(ASTNode *child, int fd, char *filename, int mode) {
    ASTNode *node = calloc(1, sizeof(ASTNode));
    node->type = NODE_REDIRECT;
    node->data.redirect.child = child;
    node->data.redirect.fd = fd;
    node->data.redirect.filename = filename;
    node->data.redirect.mode = mode;
    return node;
}

ASTNode *parse(Token *tokens, int tokenCount) {
    ParserState ps = { tokens, tokenCount, 0 };
    ASTNode *node = parse_list(&ps);
    Token *tok = peek(&ps);
    
    if (tok && tok->type != TOKEN_EOF) {
        fprintf(stderr, "Syntax error: unexpected token\n");
        free_ast(node);
        return NULL;
    }
    return node;
}

ASTNode *parse_line(const char *line) {
    int tokenCount = 0;
    Token *tokens = tokenize(line, &tokenCount);
    if (!tokens) {
        return NULL;
    }

    ASTNode *ast = parse(tokens, tokenCount);
    free_tokens(tokens, tokenCount);

    return ast;
}

static ASTNode *parse_list(ParserState *ps) {
    ASTNode *left = parse_pipeline(ps);
    if (!left) {
        return NULL;
    }
    
    while (1) {
        Token *tok = peek(ps);
        if (!tok) {
            break;
        }

        NodeType type;
        if (tok->type == TOKEN_SEMICOLON) {
            advance(ps);
            type = NODE_SEMICOLON;
        } else if (tok->type == TOKEN_AND_IF) {
            advance(ps);
            type = NODE_AND;
        } else if (tok->type == TOKEN_OR_IF) {
            advance(ps);
            type = NODE_OR;
        } else {
            break;
        }
        
        ASTNode *right = parse_pipeline(ps);
        if (!right) {
            free_ast(left);
            return NULL;
        }
        left = new_binary_node(type, left, right);
    }
    return left;
}

static ASTNode *parse_pipeline(ParserState *ps) {
    ASTNode *left = parse_command(ps);
    if (!left) {
        return NULL;
    }

    while (match(ps, TOKEN_PIPE)) {
        ASTNode *right = parse_command(ps);
        if (!right) {
            free_ast(left);
            return NULL;
        }
        left = new_binary_node(NODE_PIPELINE, left, right);
    }
    return left;
}

static ASTNode *parse_command(ParserState *ps) {
    char **argv = NULL;
    int argc = 0;
    
    /* collect all words that form the command name + arguments */
    while (1) {
        Token *tok = peek(ps);
        if (!tok || tok->type != TOKEN_WORD) {
            break;
        }
        advance(ps);
        argv = realloc(argv, (argc + 1) * sizeof(char *));
        argv[argc++] = strdup(tok->value);
    }
    
    if (argc == 0) {
        return NULL;
    }
    argv = realloc(argv, (argc + 1) * sizeof(char *));
    argv[argc] = NULL;

    /* create base command node */
    ASTNode *node = new_command_node(argv, argc);

    /* process redirections that follow the command */
    while (1) {
        Token *tok = peek(ps);
        if (!tok) {
            break;
        }

        int mode;
        if (tok->type == TOKEN_LESS) {
            mode = 0;      /* input redirection (<) */
        } else if (tok->type == TOKEN_GREATER) {
            mode = 1;      /* output redirection (>) */
        } else if (tok->type == TOKEN_DGREAT) {
            mode = 2;      /* append redirection (>>) */
        } else {
            break;
        }
        advance(ps);

        /* expect a filename after the redirection operator */
        tok = peek(ps);
        if (!tok || tok->type != TOKEN_WORD) {
            fprintf(stderr, "Syntax error: expected filename\n");
            free_ast(node);
            return NULL;
        }
        advance(ps);
        char *filename = strdup(tok->value);

        int target_fd;
        if (mode == 0) {
            target_fd = 0;
        } else {
            target_fd = 1;
        }

        /* wrap the existing node with a redirect node */
        ASTNode *new_node = new_redirect_node(node, target_fd, filename, mode);
        node = new_node;
    }

    return node;
}

void free_ast(ASTNode *node) {
    if (!node) {
        return;
    }
    
    switch (node->type) {
        case NODE_COMMAND:
            if (node->data.command.argv) {
                for (int i = 0; node->data.command.argv[i]; i++) {
                    free(node->data.command.argv[i]);
                }
                free(node->data.command.argv);
            }
            break;
            
        case NODE_PIPELINE:
        case NODE_AND:
        case NODE_OR:
        case NODE_SEMICOLON:
            free_ast(node->data.binary.left);
            free_ast(node->data.binary.right);
            break;
            
        case NODE_REDIRECT:
            free_ast(node->data.redirect.child);
            free(node->data.redirect.filename);
            break;
    }
    free(node);
}
