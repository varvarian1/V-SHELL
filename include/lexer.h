#pragma once

#include <stddef.h>

typedef enum State
{
    STATE_GENERAL,
    STATE_IN_WORD,
    STATE_IN_FILENAME,
    STATE_IN_VARIABLE,
    STATE_IN_BRACE,
    STATE_IN_ARITHMETIC,
    STATE_IN_SINGLE_QUOTE,
    STATE_IN_DOUBLE_QUOTE,
    STATE_IN_ESCAPE,
    STATE_IN_AND,
    STATE_IN_OR,
    STATE_IN_SEMICOLON,
    STATE_IN_LESS,
    STATE_IN_GREATER,
    STATE_IN_REDIRECT,
    STATE_IN_COMMENT,
    STATE_IN_HEREDOC,
} State;

typedef enum CharCategory
{
    CHAR_GENERAL,
    CHAR_WHITESPACE,
    CHAR_LESS,
    CHAR_GREATER,
    CHAR_DOLLAR,
    CHAR_QUOTE_SINGLE,
    CHAR_QUOTE_DOUBLE,
    CHAR_LPAREN,
    CHAR_RPAREN,
    CHAR_LBRACE,
    CHAR_RBRACE,
    CHAR_LBRACKET,
    CHAR_RBRACKET,
    CHAR_HASH,
    CHAR_AMPERSAND,
    CHAR_PIPE,
    CHAR_SEMICOLON,
    CHAR_BACKSLASH,
    CHAR_TILDE,
    CHAR_STAR,
    CHAR_QUESTION,
    CHAR_EXCLAM,
    CHAR_EQUALS,	
    CHAR_NEWLINE,
    CHAR_EOF,
} CharCategory;

typedef enum TokenType
{
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_AND_IF,
    TOKEN_OR_IF,
    TOKEN_SEMICOLON,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_DGREAT,
    TOKEN_LESSAND,
    TOKEN_HEREDOC,
    TOKEN_AMPERSAND,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_EOF,
} TokenType;

typedef struct Token
{
    TokenType type;
    char *value;
} Token;

CharCategory translate(int ch);
Token *tokenize(const char *input, int *tokenCount);
void free_tokens(Token *tokens, int count);
