#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>

#include "../include/prompt.h"

static char promptStr[PATH_MAX + 256];

/* Builds and return the shell prompt string.
 * Format: user@hostname:current_directory$ 
 */
const char* get_prompt() {
	char hostname[HOST_NAME_MAX + 1];
	char cwd[PATH_MAX];
	const char *user = getenv("USER");
	if (!user) {
		user = "user";
	}
	if (gethostname(hostname, sizeof(hostname)) != 0) {
		strncpy(hostname, "unknown_host", sizeof(hostname));
		hostname[sizeof(hostname) - 1] = '\0';
	}
	if (!getcwd(cwd, sizeof(cwd))) {
		strncpy(cwd, "unknown_dir", sizeof(cwd));
		cwd[sizeof(cwd) - 1] = '\0';
	}
	
	/* builds prompt: user@hostname:/path$  */
	snprintf(promptStr, sizeof(promptStr), "%s@%s:%s$ ", user, hostname, cwd);
	return promptStr;
}
