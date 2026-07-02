#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swaybar/config.h"

bool niribar_load_config(struct swaybar_config *, const char *, const char *);

int main(void) {
	char path[] = "/tmp/niribar-config-XXXXXX";
	int fd = mkstemp(path);
	assert(fd >= 0);
	FILE *file = fdopen(fd, "w");
	assert(file);
	fputs(
		"bar {\n"
		" id other\n"
		" position bottom\n"
		"}\n"
		"bar {\n"
		" id main\n"
		" status_command exec date\n"
		" position top\n"
		" font pango:DejaVu Sans Mono 11\n"
		" separator_symbol :|:\n"
		" workspace_buttons no\n"
		" workspace_min_width 40px\n"
		" strip_workspace_numbers yes\n"
		" padding 2px 6px 4px 1px\n"
		" bindsym --release button3 exec --no-startup-id true\n"
		" colors {\n"
		"  background #01020304\n"
		"  focused_workspace #111111 #222222 #333333\n"
		" }\n"
		"}\n", file);
	assert(fclose(file) == 0);

	struct swaybar_config *config = init_config();
	assert(niribar_load_config(config, path, "main"));
	assert(strcmp(config->status_command, "exec date") == 0);
	assert(config->workspace_buttons == false);
	assert(config->workspace_min_width == 40);
	assert(config->strip_workspace_numbers);
	assert(config->gaps.top == 2 && config->gaps.right == 6
			&& config->gaps.bottom == 4 && config->gaps.left == 1);
	assert(config->colors.background == 0x01020304);
	assert(config->bindings->length == 1);
	free_config(config);
	unlink(path);
}
