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
	void on_post_hit();
	void on_hit_goal();
	bool check_if_player_touched_last(CarWrapper caller);

	void clear_shot_stats();
	void update_shot_stats(float shotIncrement, float goalIncrement, float postIncrement);

public:
	//void RenderSettings() override; // Uncomment if you wanna render your own tab in the settings menu
	//void RenderWindow() override; // Uncomment if you want to render your own plugin window

	float num_shots = 0.f;
	float num_posts = 0.f;
	float num_goals = 0.f;
	float accuracy = 0.f;
	bool player_touched_last = false;
	float last_hit_time = 0.f;

	const float LEFT_POST = -840.f;
	const float RIGHT_POST = 840.f;
	const float CROSSBAR_HEIGHT = 2044.f;
	const float GROUND_LEVEL = 93.f;
	const float GOAL_LINE = 5120.f;
	const float BACK_OF_FIELD = 4992.f;

	const float POST_SIZE = 1000.f;
};
