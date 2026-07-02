#include <ctype.h>
#include <errno.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "log.h"
#include "swaybar/config.h"
#include "swaybar/input.h"
#include "util.h"

static char *trim(char *s) {
	while (isspace((unsigned char)*s)) {
		s++;
	}
	char *end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1])) {
		*--end = '\0';
	}
	return s;
}

static bool yes(const char *value, bool *result) {
	if (strcmp(value, "yes") == 0 || strcmp(value, "no") == 0) {
		*result = *value == 'y';
		return true;
	}
	return false;
}

static bool number(const char *value, int *result) {
	char *end;
	errno = 0;
	long parsed = strtol(value, &end, 10);
	if (errno || (*end && strcmp(end, "px") != 0)
			|| parsed < 0 || parsed > INT32_MAX) {
		return false;
	}
	*result = parsed;
	return true;
}

static void replace(char **target, const char *value) {
	free(*target);
	*target = strdup(value);
}

static bool parse_padding(struct swaybar_config *config, char *value) {
	int v[4], n = 0;
	for (char *part = strtok(value, " \t"); part && n < 4;
			part = strtok(NULL, " \t")) {
		if (!number(part, &v[n++])) {
			return false;
		}
	}
	if (n == 1) {
		v[1] = v[2] = v[3] = v[0];
	} else if (n == 2) {
		v[2] = v[0];
		v[3] = v[1];
	} else if (n == 3) {
		v[3] = v[1];
	} else if (n != 4) {
		return false;
	}
	config->gaps.top = v[0];
	config->gaps.right = v[1];
	config->gaps.bottom = v[2];
	config->gaps.left = v[3];
	return true;
}

static bool parse_colors(struct swaybar_config *config,
		const char *key, char *value) {
	uint32_t *single =
		strcmp(key, "background") == 0 ? &config->colors.background :
		strcmp(key, "statusline") == 0 ? &config->colors.statusline :
		strcmp(key, "separator") == 0 ? &config->colors.separator :
		strcmp(key, "focused_background") == 0 ? &config->colors.focused_background :
		strcmp(key, "focused_statusline") == 0 ? &config->colors.focused_statusline :
		strcmp(key, "focused_separator") == 0 ? &config->colors.focused_separator : NULL;
	if (single) {
		return parse_color(value, single);
	}
	struct box_colors *box =
		strcmp(key, "focused_workspace") == 0 ? &config->colors.focused_workspace :
		strcmp(key, "active_workspace") == 0 ? &config->colors.active_workspace :
		strcmp(key, "inactive_workspace") == 0 ? &config->colors.inactive_workspace :
		strcmp(key, "urgent_workspace") == 0 ? &config->colors.urgent_workspace :
		strcmp(key, "binding_mode") == 0 ? &config->colors.binding_mode : NULL;
	if (!box) {
		return false;
	}
	char *border = strtok(value, " \t");
	char *background = strtok(NULL, " \t");
	char *text = strtok(NULL, " \t");
	return border && background && text
		&& parse_color(border, &box->border)
		&& parse_color(background, &box->background)
		&& parse_color(text, &box->text);
}

static bool parse_binding(struct swaybar_config *config, char *value) {
	bool release = false;
	if (strncmp(value, "--release", 9) == 0
			&& isspace((unsigned char)value[9])) {
		release = true;
		value = trim(value + 9);
	}
	char *space = strpbrk(value, " \t");
	if (!space || strncmp(value, "button", 6) != 0) {
		return false;
	}
	*space++ = '\0';
	int button;
	if (!number(value + 6, &button) || button < 1) {
		return false;
	}
	static const uint32_t codes[] = {
		0, BTN_LEFT, BTN_MIDDLE, BTN_RIGHT,
		SWAY_SCROLL_UP, SWAY_SCROLL_DOWN,
		SWAY_SCROLL_LEFT, SWAY_SCROLL_RIGHT,
		BTN_SIDE, BTN_EXTRA,
	};
	struct swaybar_binding *binding = calloc(1, sizeof(*binding));
	binding->button = button < (int)(sizeof(codes) / sizeof(*codes))
		? codes[button] : BTN_LEFT + button - 1;
	binding->release = release;
	binding->command = strdup(trim(space));
	list_add(config->bindings, binding);
	return true;
}

static bool parse_directive(struct swaybar_config *config,
		const char *key, char *value) {
	bool flag;
	int pixels;
	if (strcmp(key, "status_command") == 0) {
		replace(&config->status_command, value);
	} else if (strcmp(key, "mode") == 0) {
		replace(&config->mode, value);
	} else if (strcmp(key, "hidden_state") == 0) {
		replace(&config->hidden_state, value);
	} else if (strcmp(key, "position") == 0) {
		config->position = parse_position(value);
	} else if (strcmp(key, "output") == 0) {
		struct config_output *output = calloc(1, sizeof(*output));
		output->name = strdup(value);
		wl_list_insert(config->outputs.prev, &output->link);
	} else if (strcmp(key, "font") == 0) {
		if (strncmp(value, "pango:", 6) == 0) {
			value += 6;
		}
		pango_font_description_free(config->font_description);
		config->font_description = pango_font_description_from_string(value);
	} else if (strcmp(key, "separator_symbol") == 0) {
		replace(&config->sep_symbol, value);
	} else if (strcmp(key, "workspace_buttons") == 0 && yes(value, &flag)) {
		config->workspace_buttons = flag;
	} else if (strcmp(key, "workspace_min_width") == 0 && number(value, &pixels)) {
		config->workspace_min_width = pixels;
	} else if (strcmp(key, "strip_workspace_numbers") == 0 && yes(value, &flag)) {
		config->strip_workspace_numbers = flag;
	} else if (strcmp(key, "strip_workspace_name") == 0 && yes(value, &flag)) {
		config->strip_workspace_name = flag;
	} else if (strcmp(key, "binding_mode_indicator") == 0 && yes(value, &flag)) {
		config->binding_mode_indicator = flag;
	} else if (strcmp(key, "bindsym") == 0) {
		return parse_binding(config, value);
	} else if (strcmp(key, "padding") == 0) {
		return parse_padding(config, value);
	} else if (strcmp(key, "height") == 0 && number(value, &pixels)) {
		config->height = pixels;
	} else if (strcmp(key, "i3bar_command") != 0
			&& strcmp(key, "workspace_command") != 0
			&& strcmp(key, "modifier") != 0
			&& strcmp(key, "tray_output") != 0
			&& strcmp(key, "tray_padding") != 0
			&& strcmp(key, "id") != 0) {
		return false;
	}
	return true;
}

bool niribar_load_config(struct swaybar_config *config,
		const char *path, const char *wanted_id) {
	FILE *file = fopen(path, "r");
	if (!file) {
		sway_log_errno(SWAY_ERROR, "Unable to open config %s", path);
		return false;
	}
	char *line = NULL;
	size_t capacity = 0;
	int lineno = 0, depth = 0;
	bool selected = wanted_id == NULL, colors = false, found = false;
	while (getline(&line, &capacity, file) >= 0) {
		lineno++;
		char *text = trim(line);
		if (!*text || *text == '#') {
			continue;
		}
		if (strcmp(text, "bar {") == 0 || strcmp(text, "bar{") == 0) {
			depth = 1;
			selected = wanted_id == NULL;
			continue;
		}
		if (!depth) {
			continue;
		}
		if (strcmp(text, "colors {") == 0 || strcmp(text, "colors{") == 0) {
			colors = true;
			depth = 2;
			continue;
		}
		if (strcmp(text, "}") == 0) {
			if (--depth == 0) {
				if (selected) {
					found = true;
					break;
				}
			}
			colors = false;
			continue;
		}
		char *space = strpbrk(text, " \t");
		if (!space) {
			goto invalid;
		}
		*space++ = '\0';
		char *value = trim(space);
		if (strcmp(text, "id") == 0 && wanted_id) {
			selected = strcmp(value, wanted_id) == 0;
			continue;
		}
		if (selected && !(colors
				? parse_colors(config, text, value)
				: parse_directive(config, text, value))) {
invalid:
			sway_log(SWAY_ERROR, "%s:%d: invalid directive", path, lineno);
			free(line);
			fclose(file);
			return false;
		}
	}
	free(line);
	fclose(file);
	if (!found) {
		sway_log(SWAY_ERROR, "%s: requested bar block not found", path);
	}
	return found;
}
