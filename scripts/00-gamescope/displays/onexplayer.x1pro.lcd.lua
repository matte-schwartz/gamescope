gamescope.config.known_displays.x1pro_lcd = {
    pretty_name = "OneXPlayer X1 Pro LCD",
    hdr = {
        -- Setup some fallbacks for undocking with HDR, meant
        -- for the internal panel. It does not support HDR.
        supported = false,
        force_enabled = false,
        eotf = gamescope.eotf.gamma22,
        max_content_light_level = 500,
        max_frame_average_luminance = 500,
        min_content_light_level = 0.5
    },
    orientation = gamescope.panel_orientation.upsidedown,
    matches = function(display)
        -- There is only a single panel in use on the OneXPlayer X1 Pro.
        if display.vendor == "BOE" and display.model == "YHB0AP23" and display.product == 0x0A23 then
            debug("[x1pro_lcd] Matched vendor: "..display.vendor.." model: "..display.model.." product: "..display.product)
            return 5000
        end
        return -1
    end
}
debug("Registered OneXPlayer X1 Pro LCD as a known display")
--debug(inspect(gamescope.config.known_displays.x1pro_lcd))
