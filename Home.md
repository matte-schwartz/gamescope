This is an experimental port of steamcompmgr as Wayland compositor.

## Build
Configure with meson and build with ninja.

## Run
After building you can launch it directly on a VT or from another Wayland/X11 session in nested mode. At the moment the following operation modes are supported:
* With Steam integration.
* To test other X11 clients on it.

### With Steam integration
TODO

### Testing other X11 clients
Revert commit [9c6c91d83](https://github.com/Plagman/steamos-compositor/commit/9c6c91d83) and launch other clients from a second terminal via `DISPLAY=:2 glxgears` assuming steamcompmgr's XWayland instance uses DISPLAY id 2.