gamescope is a port of steamcompmgr as Wayland compositor.

### With Steam integration
Start gamescope with the `-e` flag and start Steam with it like:
```
gamescope -e -- steam -tenfoot -steamos3
```

### Testing other X11 clients
Launch other clients from a second terminal via `DISPLAY=:1 glxgears` assuming gamescope's XWayland instance uses DISPLAY id 1. You can also start Steam this way on it, but the best integration is ensured when running in integration mode.

### Using hardware planes
If you run gamescope on a VT it'll try to use hardware planes if possible. This can be disabled with the `--disable-layers` flag, and you can get debugging logs for libliftoff with `--debug-layers`. You can also force a resolution with the `-w` and `-h` parameters. Together a command to run gamescope from a VT with layers, debugging and in 4K would be for example:

```
gamescope -e --debug-layers -w 3840 -h 2160 -- steam -tenfoot -steamos3
```

You can use this command also in a nested session, although `--debug-layers` will be ignored as no hardware planes are used outside of the DRM backend.