#include <assert.h>
#include <stdbool.h>

bool niribar_handle_event(const char *);

int main(void) {
	assert(!niribar_handle_event("null"));
	assert(!niribar_handle_event("{}"));
	assert(!niribar_handle_event(
			"{\"WorkspacesChanged\":{\"workspaces\":null}}"));
	assert(niribar_handle_event(
			"{\"WorkspacesChanged\":{\"workspaces\":[]}}"));
	assert(!niribar_handle_event(
			"{\"WorkspaceActivated\":{\"id\":\"1\",\"focused\":true}}"));
	assert(niribar_handle_event(
			"{\"WorkspaceActivated\":{\"id\":1,\"focused\":true}}"));
	assert(niribar_handle_event(
			"{\"WorkspaceUrgencyChanged\":{\"id\":1,\"urgent\":false}}"));
}
