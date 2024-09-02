#include "Config.h"
#include "Defer.h"

#include <unistd.h>
#include <algorithm>
#include <filesystem>
#include <optional>

namespace gamescope
{
    static constexpr glz::opts k_GamescopeGlzOpts =
    {
        .format = glz::json,
        .comments = true,
        .prettify = true,
    };

    static constexpr glz::opts k_GamescopeGlzMergeOpts =
    {
        .format = glz::json,
        .error_on_unknown_keys = false,
    };

    using namespace std::literals;

    static LogScope s_ConfigLog( "config" );

    ///////////
    // Helpers
    ///////////

    static glz::json_t *RemoveAppendMarkers( glz::json_t *pJson )
    {
        if ( auto *pObj = pJson->get_if<glz::json_t::object_t>() )
        {
            for ( auto &iter : *pObj )
            {
                RemoveAppendMarkers( &iter.second );

                auto node = pObj->extract( iter.first );
                if ( node.key().starts_with( '+' ) )
                {
                    node.key() = node.key().substr( 1 );
                }
                pObj->insert( std::move( node ) );
            }
        }
        else if ( auto *pArray = pJson->get_if<glz::json_t::array_t>() )
        {
            for ( auto &iter : *pArray )
            {
                RemoveAppendMarkers( &iter );
            }
        }

        return pJson;
    }
    
    static void MergeJsonObjects( glz::json_t *pDest, glz::json_t src, bool bShouldAppend = true )
    {
        auto *pDestObj = pDest->get_if<glz::json_t::object_t>();
        auto *pSrcObj = src.get_if<glz::json_t::object_t>();

        auto *pDestArray = pDest->get_if<glz::json_t::array_t>();
        auto *pSrcArray = src.get_if<glz::json_t::array_t>();

        if ( bShouldAppend && pDestObj && pSrcObj )
        {
            for ( auto &srcIter : *pSrcObj )
            {
                std::string sSrcName = srcIter.first;
                bool bShouldAppendChild = false;
                if ( srcIter.first.starts_with( '+' ) )
                {
                    bShouldAppendChild = true;
                    sSrcName = sSrcName.substr( 1 );
                }
                
                auto destIter = pDestObj->find( sSrcName );
                if ( destIter != pDestObj->end() )
                {
                    MergeJsonObjects( &destIter->second, std::move( srcIter.second ), bShouldAppendChild );
                }
                else
                {
                    ( *pDestObj )[ sSrcName ] = std::move( *RemoveAppendMarkers( &srcIter.second ) );
                }
            }
        }
        else if ( bShouldAppend && pDestArray && pSrcArray )
        {
            for ( auto &iter : *pSrcArray )
            {
                pDestArray->emplace_back( std::move( *RemoveAppendMarkers( &iter ) ));
            }
        }
        else
        {
            *pDest = std::move( *RemoveAppendMarkers( &src ) );
        }
    }

    static std::optional<std::string> OpenTextFile( const char* pszPath )
    {
        FILE* pFile = fopen( pszPath, "r" );
        if ( !pFile )
            return std::nullopt;
        defer( fclose( pFile ) );

        fseek( pFile, 0, SEEK_END );
        size_t zSize = static_cast<size_t>( ftell( pFile ) );
        fseek( pFile, 0, SEEK_SET );
        
        std::string sData;
        sData.resize( zSize );
        fread( sData.data(), 1, zSize, pFile );

        return sData;
    }

    ///////////////////
    // ConfigManager
    ///////////////////

    ConfigManager::ConfigManager()
    {
        Clear();
    }

    ConfigManager::~ConfigManager()
    {
    }

    ConfigManager &ConfigManager::Instance()
    {
        static ConfigManager s_Config;
        return s_Config;
    }

    bool ConfigManager::Init()
    {
        std::shared_ptr<Config> pNewConfig = LoadAll();

        if ( pNewConfig )
        {
            m_pConfig = pNewConfig;
            return true;
        }
        else
        {
            Clear();
            return false;
        }
    }

    void ConfigManager::Clear()
    {
        m_pConfig = std::make_shared<Config>();
    }

    std::shared_ptr<Config> ConfigManager::LoadAll()
    {
        std::optional<glz::json_t> oJsonConfig = LoadAllJson();
        if ( !oJsonConfig )
            return nullptr;

        //std::vector<uint8_t> mergedConfig;
        std::string sMergedConfig;
        {
            //auto ec = glz::write_binary( *oJsonConfig, mergedConfig );
            auto ec = glz::write<k_GamescopeGlzMergeOpts>( *oJsonConfig, sMergedConfig );
            if ( ec )
            {
                std::string_view psvEnumName = glz::nameof( ec.ec );
                s_ConfigLog.errorf( "Failed to merge configs: %.*s",
                    int ( psvEnumName.size() ), psvEnumName.data() );
                return nullptr;
            }
        }

        std::shared_ptr<Config> pNewConfig = std::make_shared<Config>();
        {
            //auto ec = glz::read_binary( *pNewConfig, std::span<uint8_t>{ mergedConfig } );
            auto ec = glz::read<k_GamescopeGlzMergeOpts>( *pNewConfig, sMergedConfig );
            if ( ec )
            {
                std::string_view psvEnumName = glz::nameof( ec.ec );
                s_ConfigLog.errorf( "Failed to deserialize configs: %.*s",
                    int ( psvEnumName.size() ), psvEnumName.data() );
                return nullptr;
            }
        }

        return pNewConfig;
    }

    std::optional<glz::json_t> ConfigManager::LoadAllJson()
    {
        static constexpr std::string_view k_psvConfigDirectory = "/etc/gamescope.d";

        return LoadDirJson( k_psvConfigDirectory );
    }

    std::optional<glz::json_t> ConfigManager::LoadDirJson( std::string_view psvDirectory )
    {
        std::filesystem::path dirConfig = std::filesystem::path{ psvDirectory };
        if ( !std::filesystem::is_directory( dirConfig ) )
        {
            s_ConfigLog.warnf( "Config directory '%.*s' does not exist",
                int( psvDirectory.size() ), psvDirectory.data() );
            return false;
        }

        if ( access( dirConfig.c_str(), R_OK | X_OK ) != 0 )
        {
            s_ConfigLog.warnf( "Cannot open directory '%.*s'",
                int( psvDirectory.size() ), psvDirectory.data() );
            return false;
        }

        std::vector<std::string> sFiles;
        for ( const auto &iter : std::filesystem::directory_iterator( dirConfig ) )
        {
            const std::filesystem::path &pathFile = iter.path();
            // XXX: is_regular_file -> What about symlinks?
            if ( std::filesystem::is_regular_file( iter.status() ) && pathFile.extension() == ".json"sv )
            {
                sFiles.push_back( pathFile );
            }
        }

        std::sort( sFiles.begin(), sFiles.end() );

        glz::json_t jsonMerged;
        for ( const auto &sPath : sFiles )
        {
            s_ConfigLog.infof( "Loading config file '%.*s'",
                int( sPath.length() ), sPath.data() );
            std::optional<glz::json_t> oFileJson = LoadFileJson( sPath );
            if ( oFileJson )
            {
                MergeJsonObjects( &jsonMerged, *oFileJson );
            }
        }

        return jsonMerged;
    }

    std::optional<glz::json_t> ConfigManager::LoadFileJson( std::string_view psvFileName )
    {
        std::string sFileName = std::string( psvFileName );

        std::optional<std::string> osData = OpenTextFile( sFileName.c_str() );
        if ( !osData )
        {
            s_ConfigLog.errorf_errno( "Failed to open config file '%.*s'",
                int( sFileName.size() ), sFileName.data() );

            return std::nullopt;
        }

        glz::json_t obj;
        glz::error_ctx ec = glz::read<k_GamescopeGlzOpts>( obj, *osData );
        if ( ec )
        {
            std::string_view psvEnumName = glz::nameof( ec.ec );
            s_ConfigLog.errorf( "Failed to parse config file '%.*s': %.*s",
                int( sFileName.size() ), sFileName.data(),
                int ( psvEnumName.size() ), psvEnumName.data() );
            return std::nullopt;
        }

        return obj;
    }

    ///////////////////////////////////////////
    // Console interaction for config stuff
    ///////////////////////////////////////////

#if 0
    static ConCommand cc_config_merge( "config_dump", "Load and merge all configs to a single .json",
    []( std::span<std::string_view> args )
    {
        std::optional<glz::json_t> oJsonConfig = ConfigManager::Get().LoadAll();
        if ( !oJsonConfig )
            return;

        std::string sMyConfig;
        glz::error_ctx ec = glz::write<k_GamescopeGlzOpts>( *oJsonConfig, sMyConfig );
        if ( ec )
            return;

        console_log.infof( "%.*s", int( sMyConfig.size() ), sMyConfig.data() );
    });

    static ConCommand cc_config_merge( "config_merge", "Load and merge all configs to a single .json",
    []( std::span<std::string_view> args )
    {
        std::optional<glz::json_t> oJsonConfig = ConfigManager::Get().LoadAll();
        if ( !oJsonConfig )
            return;

        std::string sMyConfig;
        glz::error_ctx ec = glz::write<k_GamescopeGlzOpts>( *oJsonConfig, sMyConfig );
        if ( ec )
            return;

        console_log.infof( "%.*s", int( sMyConfig.size() ), sMyConfig.data() );
    });
#endif

}
