gamescope is a port of steamcompmgr as Wayland compositor.

## Build
Configure with meson and build with ninja.

## Run
After building you can launch it directly on a VT or from another Wayland/X11 session in nested mode. At the moment the following operation modes are supported:
* With Steam integration.
* To test other X11 clients on it.

### With Steam integration
Start gamescope with the `-e` flag and start Steam with it like:
```
gamescope -e -- steam -tenfoot -steamos
```

### Testing other X11 clients
Launch other clients from a second terminal via `DISPLAY=:1 glxgears` assuming gamescope's XWayland instance uses DISPLAY id 1. You can also start Steam this way on it, but the best integration is ensured when running in integration mode.

### Using hardware planes
If you run gamescope on a VT it'll try to use hardware planes if possible. This can be disabled with the `-l` flag. For additional debugging information also issue the `-d` flag. You can also force a resolution with the `-w` and `-h` parameters. Together a command to run gamescope from a VT with layers, debugging and in 4K would be for example:

```
gamescope -e -d -w 3840 -h 2160 -- steam -tenfoot -steamos
```

You can use this command also in a nested session. The `-l`, `-d` parameters will be ignored then.