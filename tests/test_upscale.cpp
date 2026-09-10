#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "main.hpp"

TEST_CASE("SGSR sharpness follows the shared slider direction", "[upscale]") {
	REQUIRE( GetSgsrSharpness( 0 ) == 1.5f );
	REQUIRE_THAT( GetSgsrSharpness( 5 ), Catch::Matchers::WithinRel( 0.375f, 1e-5f ) );
	REQUIRE_THAT( GetSgsrSharpness( 20 ), Catch::Matchers::WithinRel( 0.005859375f, 1e-5f ) );
	REQUIRE( GetSgsrSharpness( -1 ) == GetSgsrSharpness( 0 ) );
	REQUIRE( GetSgsrSharpness( 21 ) == GetSgsrSharpness( 20 ) );
	for ( int i = 1; i <= 20; i++ )
		REQUIRE( GetSgsrSharpness( i ) < GetSgsrSharpness( i - 1 ) );
}

TEST_CASE("Sharpening filters react to sharpness changes", "[upscale]") {
	REQUIRE( UpscaleFilterUsesSharpness( GamescopeUpscaleFilter::SGSR ) );
	REQUIRE( UpscaleFilterUsesSharpness( GamescopeUpscaleFilter::FSR ) );
	REQUIRE( UpscaleFilterUsesSharpness( GamescopeUpscaleFilter::NIS ) );
	REQUIRE_FALSE( UpscaleFilterUsesSharpness( GamescopeUpscaleFilter::LINEAR ) );
	REQUIRE_FALSE( UpscaleFilterUsesSharpness( GamescopeUpscaleFilter::PIXEL ) );
}

TEST_CASE("SGSR falls back for unsupported content", "[upscale]") {
	// Steam writes the raw value to GAMESCOPE_NEW_SCALING_FILTER.
	REQUIRE( uint32_t( GamescopeUpscaleFilter::SGSR ) == 5 );

	const auto sgsr = GamescopeUpscaleFilter::SGSR;
	REQUIRE( GetEffectiveUpscaleFilter( sgsr, GAMESCOPE_APP_TEXTURE_COLORSPACE_LINEAR, false, false, 1.5f, 1.5f ) == sgsr );
	REQUIRE( GetEffectiveUpscaleFilter( sgsr, GAMESCOPE_APP_TEXTURE_COLORSPACE_SRGB, false, false, 2.0f, 1.0f ) == sgsr );

	for ( auto colorspace : { GAMESCOPE_APP_TEXTURE_COLORSPACE_SCRGB, GAMESCOPE_APP_TEXTURE_COLORSPACE_HDR10_PQ, GAMESCOPE_APP_TEXTURE_COLORSPACE_PASSTHRU } )
		REQUIRE( GetEffectiveUpscaleFilter( sgsr, colorspace, false, false, 1.5f, 1.5f ) == GamescopeUpscaleFilter::LINEAR );
	REQUIRE( GetEffectiveUpscaleFilter( sgsr, GAMESCOPE_APP_TEXTURE_COLORSPACE_LINEAR, true, false, 1.5f, 1.5f ) == GamescopeUpscaleFilter::LINEAR );
	REQUIRE( GetEffectiveUpscaleFilter( sgsr, GAMESCOPE_APP_TEXTURE_COLORSPACE_LINEAR, false, true, 1.5f, 1.5f ) == GamescopeUpscaleFilter::LINEAR );
	REQUIRE( GetEffectiveUpscaleFilter( sgsr, GAMESCOPE_APP_TEXTURE_COLORSPACE_LINEAR, false, false, 1.0f, 1.0f ) == GamescopeUpscaleFilter::LINEAR );
	REQUIRE( GetEffectiveUpscaleFilter( sgsr, GAMESCOPE_APP_TEXTURE_COLORSPACE_LINEAR, false, false, 0.5f, 0.5f ) == GamescopeUpscaleFilter::LINEAR );
	REQUIRE( GetEffectiveUpscaleFilter( sgsr, GAMESCOPE_APP_TEXTURE_COLORSPACE_LINEAR, false, false, 2.0f, 0.5f ) == GamescopeUpscaleFilter::LINEAR );

	// Existing filters keep their own color and scaling policies.
	REQUIRE( GetEffectiveUpscaleFilter( GamescopeUpscaleFilter::FSR, GAMESCOPE_APP_TEXTURE_COLORSPACE_HDR10_PQ, false, true, 0.5f, 0.5f ) == GamescopeUpscaleFilter::FSR );
}

TEST_CASE("GetUpscaleSettings", "[upscale]") {
	SECTION("a Steam focus window forces linear and fit") {
		const UpscaleSettings_t settings = GetUpscaleSettings(
			true, GamescopeUpscaleFilter::FSR, GamescopeUpscaleScaler::INTEGER, 7 );

		REQUIRE( settings.eFilter == GamescopeUpscaleFilter::LINEAR );
		REQUIRE( settings.eScaler == GamescopeUpscaleScaler::FIT );
		REQUIRE( settings.nSharpness == 7 );
	}

	SECTION("a non-Steam focus window keeps the wanted settings") {
		const UpscaleSettings_t settings = GetUpscaleSettings(
			false, GamescopeUpscaleFilter::FSR, GamescopeUpscaleScaler::INTEGER, 7 );

		REQUIRE( settings.eFilter == GamescopeUpscaleFilter::FSR );
		REQUIRE( settings.eScaler == GamescopeUpscaleScaler::INTEGER );
		REQUIRE( settings.nSharpness == 7 );
	}

	SECTION("passes through a different wanted filter and scaler pair") {
		const UpscaleSettings_t settings = GetUpscaleSettings(
			false, GamescopeUpscaleFilter::NEAREST, GamescopeUpscaleScaler::AUTO, 0 );

		REQUIRE( settings.eFilter == GamescopeUpscaleFilter::NEAREST );
		REQUIRE( settings.eScaler == GamescopeUpscaleScaler::AUTO );
	}
}
