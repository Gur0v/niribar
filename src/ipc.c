#include <errno.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "swaybar/bar.h"
#include "swaybar/config.h"
#include "swaybar/ipc.h"

static json_object *workspaces;
bool niribar_load_config(struct swaybar_config *config,
		const char *path, const char *bar_id);

static bool write_all(int fd, const char *data, size_t length) {
	while (length) {
		ssize_t written = write(fd, data, length);
		if (written < 0) {
			if (errno == EINTR) {
				continue;
			}
			return false;
		}
		data += written;
		length -= written;
	}
	return true;
}

static char *read_line(int fd) {
	size_t length = 0, capacity = 256;
	char *line = malloc(capacity);
	if (!line) {
		return NULL;
	}
	for (;;) {
		char c;
		ssize_t count = read(fd, &c, 1);
		if (count <= 0) {
			free(line);
			return NULL;
		}
		if (c == '\n') {
			line[length] = '\0';
			return line;
		}
		if (length + 1 == capacity) {
			capacity *= 2;
			char *larger = realloc(line, capacity);
			if (!larger) {
				free(line);
				return NULL;
			}
			line = larger;
		}
		line[length++] = c;
	}
}

static bool send_request(int fd, json_object *request) {
	const char *json = json_object_to_json_string_ext(
			request, JSON_C_TO_STRING_PLAIN);
	return write_all(fd, json, strlen(json)) && write_all(fd, "\n", 1);
}

static json_object *member(json_object *object, const char *name) {
	json_object *value = NULL;
	json_object_object_get_ex(object, name, &value);
	return value;
}

void ipc_send_workspace_command(struct swaybar *bar, const char *workspace) {
	unsigned long long id;
	if (sscanf(workspace, "id:%llu", &id) != 1) {
		return;
	}

	json_object *reference = json_object_new_object();
	json_object_object_add(reference, "Id", json_object_new_uint64(id));
	json_object *focus = json_object_new_object();
	json_object_object_add(focus, "reference", reference);
	json_object *action = json_object_new_object();
	json_object_object_add(action, "FocusWorkspace", focus);
	json_object *request = json_object_new_object();
	json_object_object_add(request, "Action", action);

	if (!send_request(bar->ipc_socketfd, request)) {
		sway_log_errno(SWAY_ERROR, "failed to focus workspace");
	} else {
		free(read_line(bar->ipc_socketfd));
	}
	json_object_put(request);
}

void ipc_execute_binding(struct swaybar *bar, struct swaybar_binding *binding) {
	(void)bar;
	const char *command = binding->command;
	if (strncmp(command, "exec ", 5) == 0) {
		command += 5;
		if (strncmp(command, "--no-startup-id ", 16) == 0) {
			command += 16;
		}
	}
	if (strcmp(command, "nop") != 0 && fork() == 0) {
		execl("/bin/sh", "sh", "-c", command, (char *)NULL);
		_exit(127);
	}
}

bool ipc_get_workspaces(struct swaybar *bar) {
	struct swaybar_output *output;
	wl_list_for_each(output, &bar->outputs, link) {
		free_workspaces(&output->workspaces);
		output->focused = false;
	}
	bar->visible_by_urgency = false;
	if (!workspaces) {
		return false;
	}

	size_t count = json_object_array_length(workspaces);
	for (size_t i = 0; i < count; i++) {
		json_object *item = json_object_array_get_idx(workspaces, i);
		const char *output_name = json_object_get_string(member(item, "output"));
		if (!output_name) {
			continue;
		}
		wl_list_for_each(output, &bar->outputs, link) {
			if (!output->name || strcmp(output_name, output->name) != 0) {
				continue;
			}
			struct swaybar_workspace *ws = calloc(1, sizeof(*ws));
			uint64_t id = json_object_get_uint64(member(item, "id"));
			int idx = json_object_get_int(member(item, "idx"));
			json_object *name = member(item, "name");
			const char *label = name && !json_object_is_type(name, json_type_null)
				? json_object_get_string(name) : NULL;
			char id_label[32];
			snprintf(id_label, sizeof(id_label), "id:%llu",
					(unsigned long long)id);
			ws->name = strdup(id_label);
			if (label) {
				ws->label = strdup(label);
			} else {
				char index_label[4];
				snprintf(index_label, sizeof(index_label), "%d", idx);
				ws->label = strdup(index_label);
			}
			if (bar->config->strip_workspace_name) {
				free(ws->label);
				snprintf(id_label, sizeof(id_label), "%d", idx);
				ws->label = strdup(id_label);
			} else if (bar->config->strip_workspace_numbers && label) {
				const char *stripped = label;
				while (*stripped >= '0' && *stripped <= '9') {
					stripped++;
				}
				if (*stripped == ':') {
					stripped++;
				}
				if (*stripped) {
					free(ws->label);
					ws->label = strdup(stripped);
				}
			}
			ws->num = idx;
			ws->visible = json_object_get_boolean(member(item, "is_active"));
			ws->focused = json_object_get_boolean(member(item, "is_focused"));
			ws->urgent = json_object_get_boolean(member(item, "is_urgent"));
			output->focused |= ws->focused;
			bar->visible_by_urgency |= ws->urgent;
			wl_list_insert(output->workspaces.prev, &ws->link);
		}
	}
	return determine_bar_visibility(bar, false);
}

bool ipc_initialize(struct swaybar *bar) {
	const char *config = getenv("NIRIBAR_CONFIG");
	if (config && !niribar_load_config(
				bar->config, config, getenv("NIRIBAR_BAR_ID"))) {
		return false;
	}
	const char *status = getenv("NIRIBAR_STATUS_COMMAND");
	if (status) {
		bar->config->status_command = strdup(status);
	}

	json_object *request = json_object_new_string("EventStream");
	bool sent = send_request(bar->ipc_event_socketfd, request);
	json_object_put(request);
	char *reply = sent ? read_line(bar->ipc_event_socketfd) : NULL;
	if (!reply) {
		sway_log_errno(SWAY_ERROR, "failed to start niri event stream");
		return false;
	}
	free(reply);
	return true;
}

static void update_workspace_flag(uint64_t id, const char *flag, bool value) {
	if (!workspaces) {
		return;
	}
	size_t count = json_object_array_length(workspaces);
	for (size_t i = 0; i < count; i++) {
		json_object *item = json_object_array_get_idx(workspaces, i);
		if (json_object_get_uint64(member(item, "id")) == id) {
			json_object_object_add(item, flag, json_object_new_boolean(value));
			return;
		}
	}
}

bool handle_ipc_readable(struct swaybar *bar) {
	char *line = read_line(bar->ipc_event_socketfd);
	if (!line) {
		return false;
	}
	json_object *event = json_tokener_parse(line);
	free(line);
	if (!event) {
		return false;
	}

	json_object *payload;
	if ((payload = member(event, "WorkspacesChanged"))) {
		json_object *next = member(payload, "workspaces");
		json_object_get(next);
		json_object_put(workspaces);
		workspaces = next;
	} else if ((payload = member(event, "WorkspaceUrgencyChanged"))) {
		update_workspace_flag(
				json_object_get_uint64(member(payload, "id")),
				"is_urgent",
				json_object_get_boolean(member(payload, "urgent")));
	} else if ((payload = member(event, "WorkspaceActivated"))) {
		uint64_t id = json_object_get_uint64(member(payload, "id"));
		bool focused = json_object_get_boolean(member(payload, "focused"));
		json_object *target_output = NULL;
		if (workspaces) {
			size_t count = json_object_array_length(workspaces);
			for (size_t i = 0; i < count; i++) {
				json_object *item = json_object_array_get_idx(workspaces, i);
				if (json_object_get_uint64(member(item, "id")) == id) {
					target_output = member(item, "output");
				}
			}
			for (size_t i = 0; i < count; i++) {
				json_object *item = json_object_array_get_idx(workspaces, i);
				if (target_output && json_object_equal(
							member(item, "output"), target_output)) {
					json_object_object_add(item, "is_active",
							json_object_new_boolean(
								json_object_get_uint64(member(item, "id")) == id));
				}
				if (focused) {
					json_object_object_add(item, "is_focused",
							json_object_new_boolean(
								json_object_get_uint64(member(item, "id")) == id));
				}
			}
		}
	} else {
		json_object_put(event);
		return false;
	}

	json_object_put(event);
	ipc_get_workspaces(bar);
	return true;
}
