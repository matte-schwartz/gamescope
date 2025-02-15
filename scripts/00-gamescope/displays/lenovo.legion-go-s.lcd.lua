local legion_go_s_lcd_refresh_rates = {
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
    58, 59, 60, 61, 62, 63, 64, 65, 66, 67,
    68, 69, 70, 71, 72, 73, 74, 75, 76, 77,
    78, 79, 80, 81, 82, 83, 84, 85, 86, 87,
    88, 89, 90, 91, 92, 93, 94, 95, 96, 97,
    98, 99, 100, 101, 102, 103, 104, 105, 106, 107,
    108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
    118, 119, 120
}

local legion_go_s_lcd_vfp = {
    1950,1886,1824,1764,1707,1652,1598,1548,1500,1451,
    1405,1360,1318,1272,1236,1200,1160,1122,1081,1053,
    1021, 988, 957, 925, 897, 865, 841, 812, 786, 758,
    735, 710, 686, 663, 640, 619, 597, 574, 553, 534,
    514, 494, 475, 458, 439, 422, 405, 387, 370, 355,
    338, 322, 307, 292, 277, 263, 248, 235, 217, 200,
    193, 182, 169, 157, 144, 132, 121, 108,  98,  87,
    75,  65,  54
}

gamescope.config.known_displays.legion_go_s_lcd = {
    pretty_name = "Lenovo Legion Go S LCD",
    hdr = {
        -- The Legion Go S panel does not support HDR.
        supported = false,
        force_enabled = false,
            eotf = gamescope.eotf.gamma22,
            max_content_light_level = 500,
            max_frame_average_luminance = 500,
            min_content_light_level = 0.5
    },
    
    dynamic_refresh_rates = legion_go_s_lcd_refresh_rates,
    
    -- Follow the Steam Deck OLED style for modegen by variang the VFP (Vertical Front Porch)
    
    -- TODO: confirm details of FBC
    dynamic_modegen = function(base_mode, refresh)
    debug("Generating mode "..refresh.."Hz for Legion Go S with fixed pixel clock")
    
    local index = zero_index(refresh - 48)
    local vfp = legion_go_s_lcd_vfp[index]
    if not vfp then
        warn("Couldn't do refresh "..refresh.." on Legion Go S panel!")
        return base_mode
        end
        
        local mode = base_mode
        
        gamescope.modegen.adjust_front_porch(mode, vfp)
        mode.vrefresh = gamescope.modegen.calc_vrefresh(mode)
        
        return mode
        end,
        
        matches = function(display)
        -- Table of EDID info
        local lcd_types = {
            {
                vendor  = "CSW",
                model   = "PN8007QB1-1",
                product = 0x0800, -- decimal 2048
            },
            -- TODO: More variants here
        }
        
        for _, lcd in ipairs(lcd_types) do
            if display.vendor == lcd.vendor
                and display.model == lcd.model
                and display.product == lcd.product
                then
                debug("[legion_go_s_lcd] Matched vendor: "..lcd.vendor..
                " model: "..lcd.model..
                " product:"..string.format("0x%04X", lcd.product))
                return 5000
                end
                end
                
                -- No match
                return -1
                end
}

debug("Registered Lenovo Legion Go S LCD as a known display")
--debug(inspect(gamescope.config.known_displays.legion_go_s_lcd))
