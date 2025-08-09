# swaysensor

An efficient [iio-sensor-proxy](https://gitlab.freedesktop.org/hadess/iio-sensor-proxy/) integration for window managers. Initial development stage.

## Support

- **Features**: auto-rotation, ambient light (testing needed), proximity.
- **Window managers**: Sway, i3.

## Contributing

To build from source, please see instructions and notes in the [Makefile](./Makefile).

To report a bug or to propose a feature, please use GitHub issues.

To connect or to discuss the development, join the dedicated [Matrix room](https://matrix.to/#/#swaysensor:envs.net).

## Roadmap

- [x] ~Implement handler for proximity sensor.~
- [x] ~Implement handler for ambient light sensor.~
- [x] ~Implement handler for accelerometer.~
- [x] ~Differentiate orientation and tilt within accel handler.~
- [x] ~Determine unit type within light handler.~
- [x] ~Use common lock file instead of locked PID file.~
- [x] ~Implement error handling through GError (gdbus-client).~
- [ ] Make handling IPC responses asynchronous.
- [x] ~Compass (unavailable, proxy defaults to Geoclue).~
- [ ] User polling: action to perform on tilt updates.
- [ ] User polling: gather relevant ambient light metrics.
- [x] Add support for i3.
- [ ] Add support for Hyprland.
