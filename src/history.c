#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "../include/history.h"

static const char* get_home_dir(void);
static void get_history_path(char *buffer, size_t size);

void add_command_to_history(const char *command) {
    char filename[PATH_MAX+1];
    get_history_path(filename, sizeof(filename));

    if (filename[0] == '\0') {
        return; /* HOME not set, skip history */
    }

    FILE *filePtr = fopen(filename, "a"); // "a" - append mode
    if (filePtr) {
        fprintf(filePtr, "%s\n", command);
        fclose(filePtr);
    } else {
        fprintf(stderr, "Warning: could not write history to %s\n", filename);
    }
}

void show_shell_history(void) {
    char filename[PATH_MAX+1];
    get_history_path(filename, sizeof(filename));

    if (filename[0] == '\0') {
        fprintf(stderr, "HOME not set, cannot read history\n");
        return;
    }
    
    FILE *file = fopen(filename, "r"); /* "r" - read from file */
    if (!file) {
        fprintf(stderr, "Warning: could not open history file %s\n", filename);
        return;
    }

    char line[256];
    int commandCount = 1;
    while (fgets(line, sizeof(line), file)) {
        /* remove trailing newline for cleaner output */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        printf("%4d %s\n", commandCount++, line);
    }
    fclose(file);
}

static const char* get_home_dir(void) {
    const char *home = getenv("HOME");
    if (!home) {
        return NULL;  /* HOME not set */
    }
    return home;
}

static void get_history_path(char *buffer, size_t size) {
    const char *home = get_home_dir();
    if (home) {
        /* path to history file: ~/.v-shell_history */
        snprintf(buffer, size, "%s/.v-shell_history", home);
    } else {
        buffer[0] = '\0';
    }
}
