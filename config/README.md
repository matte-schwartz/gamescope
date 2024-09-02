# Gamescope Config Files

## Syntax

Gamescope uses JSON with comments for it's configuration syntax.

There are some nuances, such as keys with a `+` at the start, will always append to existing objects or arrays, rather than replacing them.

Scripts are also 'processed' in order from 0 onwards into what is effectively one big config file.

You can dump the current amalgamated config using `gamescopectl config_dump` which will print to stdout.

## Reserved config namespaces

The prefix `gamescope.` is a reserved prefix for default config files.

This means, modifications to config files beginning with `gamescope.` will potentially be replaced/removed in updates.

## Making modifications as a user or for testing

If you wish to make modifications to those that will persist as a user, you can re-write or append to certain objects and arrays with the `+/-` vs not `+/-` syntax.

For example, if you want to change the Steam Deck LCD (`steamdeck_lcd`) to use the colorimetry from the spec, rather than the measured colorimetry, then you could make a file with the following:

```json
{
    "+display_configuration":
    {
        "+known_displays":
        {
            "+steamdeck_lcd":
            {
                // Spec Colorimetry
                // ⬇️ Note how this does not have a + at the start, so it functions as a total replacement, rather than appending keys.
                "colorimetry": { "r": [ 0.602, 0.355 ], "g": [ 0.340, 0.574 ], "b": [ 0.164, 0.121 ], "w": [ 0.3070, 0.3220 ] }
            }
        }
    }
}
```
and save it in your `~/.config/gamescope` under `5051-frog.use-spec-colorimetry-for-steamdeck.json`
