#include <catch2/catch_test_macros.hpp>

#include "convar.h"
#include "main.hpp"
#include "Script/Script.h"

using namespace gamescope;

TEST_CASE("ConVar", "[convar]") {
	ConVar<bool> cv_bool_var {"foobar", true, "switch on and off"};
	REQUIRE(cv_bool_var.GetName() == "foobar");
	REQUIRE(cv_bool_var.GetDescription() == "switch on and off");
	REQUIRE(cv_bool_var.Get() == true);
	cv_bool_var.SetValue(false);
	REQUIRE(cv_bool_var.Get() == false);

	{
		auto proxy = CScriptScopedLock().Manager().Gamescope().Convars.Base["foobar"];
		REQUIRE(proxy.valid() == true);
		ConVar<bool>* cv = proxy;
		REQUIRE(cv->GetName() == "foobar");
		REQUIRE(cv->GetDescription() == "switch on and off");
		REQUIRE(cv->Get() == false);
		cv->SetValue(true);
		REQUIRE(cv_bool_var.Get() == true);
	}

	{
		auto proxy = CScriptScopedLock().Manager().Gamescope().Convars.Base["nobar"];
		REQUIRE(proxy.valid() == false);
	}

	{
		auto proxy = CScriptScopedLock()->script("return gamescope.convars.foobar");
		REQUIRE(proxy.valid() == true);
		ConVar<bool>* cv = proxy;
		REQUIRE(cv->GetName() == "foobar");
		REQUIRE(cv->GetDescription() == "switch on and off");
		REQUIRE(cv->Get() == true);
		cv->SetValue(false);
		REQUIRE(cv_bool_var.Get() == false);
	}

	{
		auto proxy = CScriptScopedLock()->script("return gamescope.convars.nobar");
		REQUIRE(proxy.valid() == true);
		ConVar<bool>* cv = proxy;
		REQUIRE(cv == nullptr);
	}
}

TEST_CASE("GetUpscaleSettings", "[upscale]") {
	SECTION("a Steam focus window forces linear and fit") {
		const UpscaleSettings_t settings = GetUpscaleSettings(
			true, GamescopeUpscaleFilter::FSR, GamescopeUpscaleScaler::INTEGER );

		REQUIRE( settings.eFilter == GamescopeUpscaleFilter::LINEAR );
		REQUIRE( settings.eScaler == GamescopeUpscaleScaler::FIT );
	}

	SECTION("a non-Steam focus window keeps the wanted settings") {
		const UpscaleSettings_t settings = GetUpscaleSettings(
			false, GamescopeUpscaleFilter::FSR, GamescopeUpscaleScaler::INTEGER );

		REQUIRE( settings.eFilter == GamescopeUpscaleFilter::FSR );
		REQUIRE( settings.eScaler == GamescopeUpscaleScaler::INTEGER );
	}

	SECTION("passes through a different wanted filter and scaler pair") {
		const UpscaleSettings_t settings = GetUpscaleSettings(
			false, GamescopeUpscaleFilter::NEAREST, GamescopeUpscaleScaler::AUTO );

		REQUIRE( settings.eFilter == GamescopeUpscaleFilter::NEAREST );
		REQUIRE( settings.eScaler == GamescopeUpscaleScaler::AUTO );
	}
}
