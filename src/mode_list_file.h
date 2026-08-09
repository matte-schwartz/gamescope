#pragma once

#include "gamescope_shared.h"

#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace gamescope
{
    // Sidecar text format: one "WxH@R" per line, connector preference order.
    inline std::string EncodeModeList( std::span<const BackendMode> modes )
    {
        std::string text;
        for ( const BackendMode &mode : modes )
        {
            char szLine[64];
            snprintf( szLine, sizeof( szLine ), "%ux%u@%u\n", mode.uWidth, mode.uHeight, mode.uRefresh );
            text += szLine;
        }
        return text;
    }

    inline std::vector<BackendMode> ParseModeList( std::string_view text )
    {
        std::vector<BackendMode> modes;
        size_t ulPos = 0;
        while ( ulPos < text.size() )
        {
            size_t ulEnd = text.find( '\n', ulPos );
            if ( ulEnd == std::string_view::npos )
                ulEnd = text.size();

            std::string line{ text.substr( ulPos, ulEnd - ulPos ) };
            ulPos = ulEnd + 1;

            BackendMode mode{};
            if ( sscanf( line.c_str(), "%ux%u@%u", &mode.uWidth, &mode.uHeight, &mode.uRefresh ) != 3 )
                continue;
            if ( !mode.uWidth || !mode.uHeight || !mode.uRefresh )
                continue;
            // Catches "%u" swallowing negatives as huge values, and other garbage.
            if ( mode.uWidth > 16384 || mode.uHeight > 16384 || mode.uRefresh > 1000 )
                continue;

            modes.push_back( mode );
        }
        return modes;
    }

    // Streaming-oriented modes offered on top of the display's own list.
    inline void AppendSyntheticModes( std::vector<BackendMode> &modes )
    {
        static constexpr uint32_t k_uSyntheticRefreshes[] = { 60, 90, 120, 144 };
        static constexpr BackendMode k_SyntheticResolutions[] =
        {
            { 1920, 1080, 0 },
            { 2560, 1440, 0 },
            { 3840, 2160, 0 },
            { 2560, 1080, 0 },
            { 3440, 1440, 0 },
            { 3840, 1080, 0 },
            { 3840, 1600, 0 },
            { 5120, 1440, 0 },
            { 5120, 2160, 0 },
            { 7680, 2160, 0 },
        };

        for ( const BackendMode &res : k_SyntheticResolutions )
        {
            for ( uint32_t uRefresh : k_uSyntheticRefreshes )
            {
                bool bHave = false;
                for ( const BackendMode &mode : modes )
                {
                    if ( mode.uWidth == res.uWidth && mode.uHeight == res.uHeight && mode.uRefresh == uRefresh )
                    {
                        bHave = true;
                        break;
                    }
                }
                if ( !bHave )
                    modes.push_back( BackendMode{ res.uWidth, res.uHeight, uRefresh } );
            }
        }
    }

    void WriteModeListFile( std::span<const BackendMode> modes );
    std::vector<BackendMode> LoadModeListFile();
}
