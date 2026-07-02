# niribar

A minimal niri port of swaybar. It keeps swaybar's Wayland layer-shell
rendering and i3bar status protocol, replacing sway IPC with niri IPC.

## Build

The `sway` checkout must be next to this directory. Required development
packages are Wayland, wayland-protocols, Cairo, Pango, and json-c.

```sh
make
```

## Run

Run it from inside a niri session:

```sh
./niribar
./niribar --status-command 'while date; do sleep 1; done'
./niribar --config ~/.config/niribar/config
```

`NIRI_SOCKET` is used by default; `--socket` overrides it. Workspace buttons
track niri's event stream and clicks focus workspaces by stable ID.

The config uses standard i3 `bar { ... }` syntax. With multiple blocks, select
one using `--bar-id ID`; otherwise the first block is used. `--status-command`
overrides the configured command. Tray directives, `workspace_command`, and
`i3bar_command` are accepted for config compatibility but have no effect.

Config lookup order is `--config`, `$NIRIBAR_CONFIG`,
`$XDG_CONFIG_HOME/niribar/config`, `~/.config/niribar/config`,
`~/.niribarrc`, then `/etc/xdg/niribar/config`.

The reused sway sources are MIT-licensed; see the adjacent sway checkout.
