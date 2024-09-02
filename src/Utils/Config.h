#pragma once

#include "../convar.h"
#include "../color_helpers.h"
#include <string_view>
#include "glaze/glaze.hpp"

namespace gamescope
{
    class Config;

    struct ConfigSchema_t
    {
        struct DisplayConfiguration_t
        {
            struct DynamicVFPModeGeneration_t
            {
                bool dynamic_vfp; // always true. our "key" for the variant.

                uint32_t vsync = 0; // vertical sync
                uint32_t vbp = 0; // vertical back porch
                std::vector<uint32_t> vfp; // vertical front porch
            };

            struct DynamicClockModeGeneration_t
            {
                bool dynamic_clock; // always true. our "key" for the variant.

                uint32_t hdisplay = 0;
                uint32_t hfp = 0;
                uint32_t hsync = 0;
                uint32_t hbp = 0;

                uint32_t vdisplay = 0;
                uint32_t vfp = 0;
                uint32_t vsync = 0;
                uint32_t vbp = 0;
            };

            struct KnownDisplay_t
            {
                std::string pretty_name;
                std::optional<std::vector<uint32_t>> refresh_rates;
                // not hooked up yet
                //std::optional<std::variant<
                //    DynamicVFPModeGeneration_t,
                //    DynamicClockModeGeneration_t>> mode_generation;
            };

            struct DisplayMapping_t
            {
                std::optional<uint32_t> GetProduct()
                {
                    if ( !product )
                        return std::nullopt;

                    return Parse<uint32_t>( *product );
                }
                std::optional<std::string> vendor;
                std::optional<std::string> product;
                std::optional<std::string> model;

                std::string panel; // result
            };

            KnownDisplay_t *GetKnownDisplay( std::string_view psvVendor, uint32_t uProduct, std::string_view psvModel )
            {
                for ( DisplayMapping_t &mapping : display_mapping )
                {
                    bool bSatisfies = true;

                    if ( mapping.vendor && mapping.vendor != psvVendor )
                        bSatisfies = false;

                    std::optional<uint32_t> ouProduct = mapping.GetProduct();
                    if ( ouProduct && *ouProduct != uProduct )
                        bSatisfies = false;

                    if ( mapping.model && mapping.model != psvModel )
                        bSatisfies = false;

                    if ( bSatisfies )
                    {
                        auto iter = known_displays.find( mapping.panel );
                        if ( iter != known_displays.end() )
                            return &iter->second;
                    }
                }

                return nullptr;
            }

            std::unordered_map<std::string, KnownDisplay_t> known_displays;
            std::vector<DisplayMapping_t> display_mapping;
        };

        DisplayConfiguration_t display_configuration;
    };

    class ConfigManager
    {
    public:
        ~ConfigManager();

        static ConfigManager &Instance();

        std::shared_ptr<Config> GetConfig()
        {
            // Does not need to be sequentially consistent with other things.
            return m_pConfig.load( std::memory_order_relaxed );
        }

        bool Init();
        void Clear();

        std::shared_ptr<Config> LoadAll();

        std::optional<glz::json_t> LoadAllJson();
        std::optional<glz::json_t> LoadDirJson( std::string_view psvDirectory );
        std::optional<glz::json_t> LoadFileJson( std::string_view psvFileName );
    private:
        ConfigManager();

        std::atomic<std::shared_ptr<Config>> m_pConfig;
    };

    class Config : public ConfigSchema_t
    {
    public:
        static std::shared_ptr<Config> Get()
        {
            return ConfigManager::Instance().GetConfig();
        }
    };
}
