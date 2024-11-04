#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

class PostCounterV1 : public BakkesMod::Plugin::BakkesModPlugin
	//,public SettingsWindowBase // Uncomment if you wanna render your own tab in the settings menu
	//,public PluginWindowBase // Uncomment if you want to render your own plugin window
{
	//std::shared_ptr<bool> enabled;

	//Boilerplate
	void onLoad() override;
	void onUnload() override; // Uncomment and implement if you need a unload method
	void OnPostHit();
	void OnHitGoal();
	bool CheckIfPlayerTouchedLast(CarWrapper caller);

	void ClearShotStats();
	void IncrementShotStats(int shotIncrement, int goalIncrement, int postIncrement);

public:
	//void RenderSettings() override; // Uncomment if you wanna render your own tab in the settings menu
	//void RenderWindow() override; // Uncomment if you want to render your own plugin window

	int numShots = 0;
	int numPosts = 0;
	int numGoals = 0;
	float accuracy = 0.f;
	float missPercentage = 0.f;
	bool playerTouchedLast = false;
};
