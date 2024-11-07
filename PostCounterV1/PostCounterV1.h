#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

class PostPercentage : public BakkesMod::Plugin::BakkesModPlugin
	, public SettingsWindowBase // Uncomment if you wanna render your own tab in the settings menu
	//,public PluginWindowBase // Uncomment if you want to render your own plugin window
{
	//std::shared_ptr<bool> enabled;

	void onLoad() override;
	void on_post_hit();
	void on_goal_hit();
	void on_goal_scored();
	bool check_if_player_touched_last(CarWrapper caller);

	void clear_shot_stats();
	void update_shot_stats(float shotIncrement, float goalIncrement, float postIncrement);
public:
	void RenderSettings() override;  // Uncomment if you wanna render your own tab in the settings menu
	//void RenderWindow() override ; // Uncomment if you want to render your own plugin window
	void Render(CanvasWrapper canvas);

#pragma region Stats
	float num_shots = 0.f;
	float num_shots_in_matches = 0.f;

	float num_posts = 0.f;
	float num_posts_in_matches = 0.f;

	float num_goals = 0.f;
	float num_goals_in_matches = 0.f;
	float num_own_goals;

	float overall_accuracy = 0.f;
	float in_game_accuracy = 0.f;
	float posts_per_match = 0.f;

	float num_matches = 0.f;
	bool player_touched_last = false;
#pragma endregion

	int player_team;
	bool display_text = true;
	bool abbreviated_text = false;

	bool should_track_shots = false;
	bool is_demolished = false;

#pragma region Net Variables
	const float LEFT_POST = -1250.f;
	const float RIGHT_POST = 1250.f;

	const float CROSSBAR_HEIGHT = 750.f;
	const float GROUND_LEVEL = 95.f;

	const float GOAL_LINE_BLUE = 5120.f;
	const float GOAL_LINE_ORANGE = -5120.f;

	const float BACK_OF_FIELD_BLUE = 4990.f;
	const float BACK_OF_FIELD_ORANGE = -4990.f;

	float POST_SIZE = 100.f;
#pragma endregion
};

template <typename T, typename std::enable_if<std::is_base_of<ObjectWrapper, T>::value>::type*>
void GameWrapper::HookEventWithCaller(std::string eventName,
	std::function<void(T caller, void* params, std::string eventName)> callback)
{
	auto wrapped_callback = [callback](ActorWrapper caller, void* params, std::string eventName)
		{
			callback(T(caller.memory_address), params, eventName);
		};
	HookEventWithCaller<ActorWrapper>(eventName, wrapped_callback);
}
