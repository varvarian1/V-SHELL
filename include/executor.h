#pragma once
#include "parser.h"

int execute(ASTNode *node);
int execute_script(const char *filename, int argc, char **argv);
