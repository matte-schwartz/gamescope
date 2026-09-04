#include <catch2/catch_test_macros.hpp>

#include "mode_list_file.h"

using namespace gamescope;

TEST_CASE("Mode list round-trip", "[mode_list]") {
	std::vector<BackendMode> modes = {
		{ 3840, 2160, 120 },
		{ 3840, 2160, 60 },
		{ 1920, 1080, 60 },
	};

	std::string text = EncodeModeList( modes );
	REQUIRE( text == "3840x2160@120\n3840x2160@60\n1920x1080@60\n" );

	std::vector<BackendMode> parsed = ParseModeList( text );
	REQUIRE( parsed.size() == 3 );
	for ( size_t i = 0; i < parsed.size(); i++ )
	{
		REQUIRE( parsed[i].uWidth == modes[i].uWidth );
		REQUIRE( parsed[i].uHeight == modes[i].uHeight );
		REQUIRE( parsed[i].uRefresh == modes[i].uRefresh );
	}
}

TEST_CASE("Mode list parse skips malformed lines", "[mode_list]") {
	std::string text = "1920x1080@60\ngarbage\n0x0@0\n1280x720@\n\n2560x1440@165\n";
	std::vector<BackendMode> parsed = ParseModeList( text );
	REQUIRE( parsed.size() == 2 );
	REQUIRE( parsed[0].uWidth == 1920 );
	REQUIRE( parsed[0].uHeight == 1080 );
	REQUIRE( parsed[0].uRefresh == 60 );
	REQUIRE( parsed[1].uWidth == 2560 );
	REQUIRE( parsed[1].uRefresh == 165 );
}

TEST_CASE("Mode list parse handles missing trailing newline", "[mode_list]") {
	std::vector<BackendMode> parsed = ParseModeList( "1920x1080@60" );
	REQUIRE( parsed.size() == 1 );
	REQUIRE( parsed[0].uRefresh == 60 );
}

TEST_CASE("Empty mode list encodes to empty text", "[mode_list]") {
	REQUIRE( EncodeModeList( {} ) == "" );
	REQUIRE( ParseModeList( "" ).empty() );
}

TEST_CASE("Mode list parse rejects out-of-range values", "[mode_list]") {
	std::string text = "-1x-1@-1\n20000x20000@60\n1920x1080@2000\n1920x1080@60\n";
	std::vector<BackendMode> parsed = ParseModeList( text );
	REQUIRE( parsed.size() == 1 );
	REQUIRE( parsed[0].uWidth == 1920 );
	REQUIRE( parsed[0].uHeight == 1080 );
	REQUIRE( parsed[0].uRefresh == 60 );
}

TEST_CASE("Synthetic modes skip entries the display provides", "[modelist]") {
	std::vector<BackendMode> modes = {
		{ 5120, 2160, 165 },
		{ 5120, 2160, 120 },
		{ 3440, 1440, 144 },
		{ 1920, 1080, 60 },
	};

	AppendSyntheticModes( modes );

	// 10 resolutions x 4 refreshes, minus the three exact pairs already present.
	REQUIRE( modes.size() == 4 + 40 - 3 );

	// The display's own modes stay first and untouched.
	REQUIRE( modes[0].uWidth == 5120 );
	REQUIRE( modes[0].uRefresh == 165 );

	int nCount5120x2160at120 = 0;
	int nCount3440x1440at144 = 0;
	int nCount5120x2160at90 = 0;
	for ( const BackendMode &mode : modes )
	{
		if ( mode.uWidth == 5120 && mode.uHeight == 2160 && mode.uRefresh == 120 )
			nCount5120x2160at120++;
		if ( mode.uWidth == 3440 && mode.uHeight == 1440 && mode.uRefresh == 144 )
			nCount3440x1440at144++;
		if ( mode.uWidth == 5120 && mode.uHeight == 2160 && mode.uRefresh == 90 )
			nCount5120x2160at90++;
	}
	REQUIRE( nCount5120x2160at120 == 1 );
	REQUIRE( nCount3440x1440at144 == 1 );
	REQUIRE( nCount5120x2160at90 == 1 );
}
