#pragma once

#include "lexer.h"

typedef struct ParserState
{
    Token *tokens;
    int count;
    int pos;
} ParserState;

typedef enum NodeType
{
    NODE_COMMAND,
    NODE_PIPELINE,
    NODE_REDIRECT,
    NODE_AND,
    NODE_OR,
    NODE_SEMICOLON,
} NodeType;

typedef struct ASTNode
{
    NodeType type;
    union
    {
        struct
        {
            char **argv;
            int argc;
        } command;
        
        struct 
        {
            struct ASTNode *left;
            struct ASTNode *right;
        } binary;

        struct
        {
            struct ASTNode *child;

            int fd;
            char *filename;
            int mode;
        } redirect;
    } data;
} ASTNode;

ASTNode *parse(Token *tokens, int tokenCount);
ASTNode *parse_line(const char *line);
void free_ast(ASTNode *node);
