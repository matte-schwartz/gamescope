#pragma once

#include <cstdio>
#include <cstring>
#include <string_view>

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

namespace gamescope
{
    // A /proc/<pid>/maps line names vrclient if the mapped file's basename
    // contains "vrclient" (vrclient.so, vrclient_x64.so, Proton builds).
    inline bool MapsLineNamesVRClient( std::string_view line )
    {
        size_t slash = line.rfind( '/' );
        if ( slash == std::string_view::npos )
            return false;

        return line.substr( slash + 1 ).find( "vrclient" ) != std::string_view::npos;
    }

    // Plain read()-based scan, procfs files cannot be sized up front.
    inline bool ProcMapsHasVRClient( const char *path )
    {
        int fd = open( path, O_RDONLY | O_CLOEXEC );
        if ( fd < 0 )
            return false;

        bool bFound = false;
        char buf[16384];
        size_t unCarry = 0;
        for ( ;; )
        {
            ssize_t nRead = read( fd, buf + unCarry, sizeof( buf ) - unCarry );
            if ( nRead <= 0 )
            {
                // Trailing line without a newline.
                bFound = unCarry && MapsLineNamesVRClient( { buf, unCarry } );
                break;
            }

            size_t unLength = unCarry + (size_t)nRead;
            size_t unLineStart = 0;
            for ( size_t i = 0; i < unLength && !bFound; i++ )
            {
                if ( buf[i] != '\n' )
                    continue;

                bFound = MapsLineNamesVRClient( { buf + unLineStart, i - unLineStart } );
                unLineStart = i + 1;
            }

            if ( bFound )
                break;

            unCarry = unLength - unLineStart;
            // Drop lines longer than the buffer instead of stalling.
            if ( unCarry == sizeof( buf ) )
                unCarry = 0;
            else
                memmove( buf, buf + unLineStart, unCarry );
        }

        close( fd );
        return bFound;
    }

    inline bool ProcessHasVRClientMapped( pid_t pid )
    {
        char path[64];
        snprintf( path, sizeof( path ), "/proc/%d/maps", (int)pid );
        return ProcMapsHasVRClient( path );
    }
}
