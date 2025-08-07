# swaysensor

An efficient [iio-sensor-proxy](https://gitlab.freedesktop.org/hadess/iio-sensor-proxy/) integration for window managers. Initial development stage.

## Support

- **Features**: auto-rotation, ambient light (testing needed), proximity.
- **Window managers**: Sway.

## Contributing

To build from source, please see instructions and notes in the [Makefile](./Makefile).

To report a bug or to propose a feature, please use GitHub issues.

To connect or to discuss the development, join the dedicated [Matrix room](https://matrix.to/#/#swaysensor:envs.net).

## Roadmap

- [x] ~Implement handler for proximity sensor.~
- [x] ~Implement handler for ambient light sensor.~
- [x] ~Implement handler for accelerometer.~
- [x] ~Differentiate orientation and tilt within the accel handler.~
- [ ] Implement error handling through GError.
- [ ] Make handling IPC responses asynchronous.
- [ ] Possibly add explicit user option for tilt, if so requested.
- [ ] Define and implement an action to perform on tilt updates.
- [x] ~Determine unit type within light handler.~
- [ ] Gather light metrics => someone with working AL sensor needed.
- [ ] Add and register compass (separate destination).
- [ ] Define and implement action to perform on compass updates.
- [x] ~Use common lock file instead of locked PID file.~
- [ ] Implement WM determination mechanism.
- [ ] Add support for i3.
- [ ] Make a plan on how to implement support for other WMs.
