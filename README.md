# swaysensor

An efficient [iio-sensor-proxy](https://gitlab.freedesktop.org/hadess/iio-sensor-proxy/) integration for Wayland compositors. Initial development stage.

## Support

- **Features**: auto-rotation, ambient light (testing needed), proximity.
- **Compositors**: Sway, Hyprland.

## Contributing

To build from source, please see instructions and notes in the [Makefile](./Makefile).

To report a bug or to propose a feature, please use GitHub issues.

To connect or to discuss the development, join the dedicated [Matrix room](https://matrix.to/#/#swaysensor:envs.net).

## Active tasks

- [ ] Simplify WM-related logic inside IPC client.
- [ ] Poll: next compositor to add support for.
- [ ] Poll: action to perform on tilt updates.
- [ ] Poll: gather relevant ambient light metrics.
- [ ] Planned: X window managers (i3 and others).
