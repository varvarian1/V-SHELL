#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../include/lexer.h"

CharCategory translate(int ch) {
    switch (ch) {
        case ' ':
        case '\t':
        case '\r': return CHAR_WHITESPACE;
        case '\n': return CHAR_NEWLINE;
        case '<': return CHAR_LESS;
        case '>': return CHAR_GREATER;
        case '$': return CHAR_DOLLAR;
        case '\'': return CHAR_QUOTE_SINGLE;
        case '"': return CHAR_QUOTE_DOUBLE;
        case '(': return CHAR_LPAREN;
        case ')': return CHAR_RPAREN;
        case '{': return CHAR_LBRACE;
        case '}': return CHAR_RBRACE;
        case '[': return CHAR_LBRACKET;
        case ']': return CHAR_RBRACKET;
        case '#': return CHAR_HASH;
        case '&': return CHAR_AMPERSAND;
        case '|': return CHAR_PIPE;
        case ';': return CHAR_SEMICOLON;
        case '\\': return CHAR_BACKSLASH;
        case '~': return CHAR_TILDE;
        case '*': return CHAR_STAR;
        case '?': return CHAR_QUESTION;
        case '!': return CHAR_EXCLAM;
        case '=': return CHAR_EQUALS;
        case 0: return CHAR_EOF;
        default: return CHAR_GENERAL;
    }
}

static char *safe_strdup(const char *str) {
    return str ? strdup(str) : NULL;
}

Token *tokenize(const char *input, int *tokenCount) {
    Token *tokens = NULL;
    int count = 0, capacity = 0;
    char *lexem = NULL;
    int lexemLen = 0;
    State state = STATE_GENERAL;
    int line = 1, column = 1;

    /* append character to string with realloc and null-termination */
    #define APPEND(c) do {                    \
        lexem = realloc(lexem, lexemLen + 2); \
        lexem[lexemLen] = (c);                \
        lexemLen++;                           \
        lexem[lexemLen] = '\0';               \
    } while(0)

    /* adds a token to the dynamic array, resizing if necessary */
    #define ADD_TOKEN(tok_type, val) do {                       \
        if (count >= capacity) {                                \
            capacity = capacity ? capacity * 2 : 8;             \
            tokens = realloc(tokens, capacity * sizeof(Token)); \
        }                                                       \
        tokens[count].type = (tok_type);                        \
        if (val) {                                              \
            tokens[count].value = safe_strdup(val);             \
        } else {                                                \
            tokens[count].value = NULL;                         \
        }                                                       \
        count++;                                                \
    } while(0)

    /* finalizes the current word token and adds it to tokens list */
    #define FINISH_WORD() do {        \
        if (lexem && lexemLen > 0) {  \
            ADD_TOKEN(TOKEN_WORD, lexem); \
            free(lexem);              \
            lexem = NULL;             \
            lexemLen = 0;             \
        }                             \
    } while(0)

    const char *position = input;
    while (1) {
        char ch = *position;
        CharCategory category = translate(ch);
        State previousState = state;

        switch (state) {
            case STATE_GENERAL:
                if (category == CHAR_WHITESPACE || category == CHAR_NEWLINE) {
                    if (ch == '\n') {
                        line++;
                        column = 1;
                    } else {
                        column++;
                    }

                    position++;
                    continue;
                }
                lexem = NULL;
                lexemLen = 0;
                if (category == CHAR_GENERAL) {
                    APPEND(ch);
                    position++;
                    state = STATE_IN_WORD;
                } else if (category == CHAR_DOLLAR) {
                    APPEND(ch);
                    position++;
                    state = STATE_IN_WORD;
                } else if (category == CHAR_QUOTE_SINGLE) {
                    position++;
                    state = STATE_IN_SINGLE_QUOTE;
                } else if (category == CHAR_QUOTE_DOUBLE) {
                    position++;
                    state = STATE_IN_DOUBLE_QUOTE;
                } else if (category == CHAR_BACKSLASH) {
                    position++;
                    state = STATE_IN_ESCAPE;
                } else if (category == CHAR_AMPERSAND) {
                    position++;
                    state = STATE_IN_AND;
                } else if (category == CHAR_PIPE) {
                    position++;
                    state = STATE_IN_OR;
                } else if (category == CHAR_SEMICOLON) {
                    position++;
                    state = STATE_IN_SEMICOLON;
                } else if (category == CHAR_LESS) {
                    position++;
                    state = STATE_IN_LESS;
                } else if (category == CHAR_GREATER) {
                    position++;
                    state = STATE_IN_GREATER;
                } else if (category == CHAR_LPAREN) {
                    ADD_TOKEN(TOKEN_LPAREN, NULL);
                    position++;
                } else if (category == CHAR_RPAREN) {
                    ADD_TOKEN(TOKEN_RPAREN, NULL);
                    position++;
                } else if (category == CHAR_HASH) {
                    state = STATE_IN_COMMENT;
                } else if (category == CHAR_EOF) {
                    goto finish;
                } else {
                    APPEND(ch);
                    position++;
                    state = STATE_IN_WORD;
                }
                break;

            case STATE_IN_WORD:
                if (category == CHAR_GENERAL) {
                    APPEND(ch);
                    position++;
                } else if (category == CHAR_BACKSLASH) {
                    position++;
                    state = STATE_IN_ESCAPE;
                } else if (category == CHAR_QUOTE_SINGLE) {
                    position++;
                    state = STATE_IN_SINGLE_QUOTE;
                } else if (category == CHAR_QUOTE_DOUBLE) {
                    position++;
                    state = STATE_IN_DOUBLE_QUOTE;
                } else if (category == CHAR_DOLLAR) {
                    APPEND(ch);
                    position++;
                } else {
                    FINISH_WORD();
                    state = STATE_GENERAL;
                }
                break;

            case STATE_IN_SINGLE_QUOTE:
                if (category == CHAR_QUOTE_SINGLE) {
                    position++;
                    state = (previousState == STATE_GENERAL ? STATE_GENERAL : STATE_IN_WORD);
                } else {
                    APPEND(ch);
                    position++;
                }
                break;

            case STATE_IN_DOUBLE_QUOTE:
                if (category == CHAR_QUOTE_DOUBLE) {
                    position++;
                    state = (previousState == STATE_GENERAL ? STATE_GENERAL : STATE_IN_WORD);
                } else if (category == CHAR_BACKSLASH) {
                    position++;
                    state = STATE_IN_ESCAPE;
                } else if (category == CHAR_DOLLAR) {
                    APPEND(ch);
                    position++;
                } else {
                    APPEND(ch);
                    position++;
                }
                break;

            case STATE_IN_ESCAPE:
                APPEND(ch);
                position++;
                state = (previousState == STATE_GENERAL ? STATE_GENERAL : STATE_IN_WORD);
                break;

            case STATE_IN_AND:
                if (category == CHAR_AMPERSAND) {
                    ADD_TOKEN(TOKEN_AND_IF, NULL);
                    position++;
                    state = STATE_GENERAL;
                } else {
                    ADD_TOKEN(TOKEN_AMPERSAND, NULL);
                    position++;
                    state = STATE_GENERAL;
                }
                break;

            case STATE_IN_OR:
                if (category == CHAR_PIPE) {
                    ADD_TOKEN(TOKEN_OR_IF, NULL);
                    position++;
                    state = STATE_GENERAL;
                } else {
                    ADD_TOKEN(TOKEN_PIPE, NULL);
                    position++;
                    state = STATE_GENERAL;
                }
                break;

            case STATE_IN_SEMICOLON:
                if (category == CHAR_SEMICOLON) {
                    ADD_TOKEN(TOKEN_SEMICOLON, NULL);
                    position++;
                } else {
                    ADD_TOKEN(TOKEN_SEMICOLON, NULL);
                    position++;
                }
                state = STATE_GENERAL;
                break;

            case STATE_IN_LESS:
                if (category == CHAR_LESS) {
                    ADD_TOKEN(TOKEN_LESS, NULL);
                    position++;
                } else {
                    ADD_TOKEN(TOKEN_LESS, NULL);
                    position++;
                }
                state = STATE_GENERAL;
                break;

            case STATE_IN_GREATER:
                if (category == CHAR_GREATER) {
                    ADD_TOKEN(TOKEN_DGREAT, NULL);
                    position++;
                } else {
                    ADD_TOKEN(TOKEN_GREATER, NULL);
                    position++;
                }
                state = STATE_GENERAL;
                break;

            case STATE_IN_REDIRECT:
                position++;
                break;

            case STATE_IN_COMMENT:
                position++;
                break;

            case STATE_IN_HEREDOC:
            case STATE_IN_ARITHMETIC:
            default:
                position++;
                break;
        }
    }

finish:
    if (lexem && lexemLen > 0) {
        ADD_TOKEN(TOKEN_WORD, lexem);
        free(lexem);
    }
    ADD_TOKEN(TOKEN_EOF, NULL);
    *tokenCount = count;
    return tokens;

#undef APPEND
#undef ADD_TOKEN
#undef FINISH_WORD
}

void free_tokens(Token *tokens, int count) {
    if (!tokens)
        return;
    for (int i = 0; i < count; i++) {
        free(tokens[i].value);
    }
    free(tokens);
}
