#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "swaybar/bar.h"

static struct swaybar bar;

static void stop(int signal) {
	(void)signal;
	bar.running = false;
}

static bool use_config(const char *directory, const char *name) {
	if (!directory) {
		return false;
	}
	size_t length = strlen(directory) + strlen(name) + 2;
	char *path = malloc(length);
	snprintf(path, length, "%s/%s", directory, name);
	if (access(path, R_OK) == 0) {
		setenv("NIRIBAR_CONFIG", path, 1);
		free(path);
		return true;
	}
	free(path);
	return false;
}

int main(int argc, char **argv) {
	const char *socket_path = NULL;
	bool debug = false;
	static const struct option options[] = {
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{"socket", required_argument, NULL, 's'},
		{"status-command", required_argument, NULL, 'c'},
		{"config", required_argument, NULL, 'f'},
		{"bar-id", required_argument, NULL, 'b'},
		{"debug", no_argument, NULL, 'd'},
		{0},
	};

	for (;;) {
		int c = getopt_long(argc, argv, "hvs:c:f:b:d", options, NULL);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 's':
			socket_path = optarg;
			break;
		case 'c':
			setenv("NIRIBAR_STATUS_COMMAND", optarg, 1);
			break;
		case 'f':
			setenv("NIRIBAR_CONFIG", optarg, 1);
			break;
		case 'b':
			setenv("NIRIBAR_BAR_ID", optarg, 1);
			break;
		case 'd':
			debug = true;
			break;
		case 'v':
			puts("niribar 0.1");
			return 0;
		default:
			puts("Usage: niribar [-d] [-s NIRI_SOCKET] [-f CONFIG] "
					"[-b BAR_ID] [-c STATUS_COMMAND]");
			return c == 'h' ? 0 : 1;
		}
	}

	if (!getenv("NIRIBAR_CONFIG")
			&& !use_config(getenv("XDG_CONFIG_HOME"), "niribar/config")
			&& !use_config(getenv("HOME"), ".config/niribar/config")
			&& !use_config(getenv("HOME"), ".niribarrc")) {
		use_config("/etc/xdg", "niribar/config");
	}
	if (!socket_path) {
		socket_path = getenv("NIRI_SOCKET");
	}
	if (!socket_path) {
		fputs("niribar: NIRI_SOCKET is not set\n", stderr);
		return 1;
	}

	sway_log_init(debug ? SWAY_DEBUG : SWAY_INFO, NULL);
	if (!bar_setup(&bar, socket_path)) {
		return 1;
	}

	struct sigaction action = {.sa_handler = stop};
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);
	struct sigaction reap = {.sa_handler = SIG_IGN};
	sigaction(SIGCHLD, &reap, NULL);
	bar.running = true;
	bar_run(&bar);
	bar_teardown(&bar);
	return 0;
}
