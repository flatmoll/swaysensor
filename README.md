# swaysensor

An efficient [iio-sensor-proxy](https://gitlab.freedesktop.org/hadess/iio-sensor-proxy/) integration for window managers. Initial development stage.

## Support

- **Features**: auto-rotation, ambient light (testing needed), proximity.
- **Window managers**: Sway, i3, Hyprland.

## Contributing

To build from source, please see instructions and notes in the [Makefile](./Makefile).

To report a bug or to propose a feature, please use GitHub issues.

To connect or to discuss the development, join the dedicated [Matrix room](https://matrix.to/#/#swaysensor:envs.net).

## Active tasks

- [ ] Simplify WM-related logic inside IPC client.
- [ ] User polling: action to perform on tilt updates.
- [ ] User polling: gather relevant ambient light metrics.
