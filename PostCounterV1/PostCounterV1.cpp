#include "pch.h"
#include "PostCounterV1.h"

BAKKESMOD_PLUGIN(PostPercentage, "Post Percentage", plugin_version, 0)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void PostPercentage::onLoad()
{
	_globalCvarManager = cvarManager;

	cvarManager->registerNotifier("ResetPostCounterStats", [this](std::vector<std::string> args) {
		clear_shot_stats();
		}, "Clears shot stats", PERMISSION_ALL);

	cvarManager->registerNotifier("post_toast_notification", [this](std::vector<std::string> args) {
		std::string insult_text = pick_toast_insult(true);
		gameWrapper->Toast("Post hit", insult_text, "default", 2.0, ToastType_Warning);
		}, "post toast notifier", PERMISSION_ALL);
	cvarManager->registerNotifier("own_goal_notification", [this](std::vector<std::string> args) {
		std::string insult_text = pick_toast_insult(false);
		gameWrapper->Toast("You own goaled", insult_text, "default", 2.0, ToastType_Error);
		}, "own goal toast notifier", PERMISSION_ALL);

	gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
		Render(canvas);
		});

#pragma region Cvars
	cvarManager->registerCvar("PP_UI_display", "1", "Toggle on/off the UI", true, true, 0, true, 1, true)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {display_text = cvar.getBoolValue(); });

	cvarManager->registerCvar("PP_abbreviated_UI", "0", "Toggle on/off the abbreviated UI", true, true, 0, true, 1, true)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {abbreviated_text = cvar.getBoolValue(); });

	cvarManager->registerCvar("PP_toast_notifications_toggle", "1", "Toggle on/off the abbreviated UI", true, true, 0, true, 1, true)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {enable_toasts = cvar.getBoolValue(); });

	cvarManager->registerCvar("PP_enable_in_training_toggle", "1", "Toggle on/off tracking in freeplay", true, true, 0, true, 1, true)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {enable_in_training = cvar.getBoolValue(); });

	cvarManager->registerCvar("PP_reset_after_every_game_toggle", "0", "Toggle on/off resetting stats after each game", true, true, 0, true, 1, true)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {reset_every_match = cvar.getBoolValue(); });

	cvarManager->registerCvar("PP_post_size", "100.f", "The size of the posts", true, true, 0.f, true, 500.f, true)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {POST_SIZE = cvar.getFloatValue(); });

#pragma endregion

#pragma region Shot Event Hooks
	gameWrapper->HookEvent("Function TAGame.Ball_TA.EventHitWorld", std::bind(&PostPercentage::on_post_hit, this));
	gameWrapper->HookEventWithCaller<TeamWrapper>("Function TAGame.Team_TA.OnScoreUpdated", [this](TeamWrapper caller, void* params, std::string eventname) {
		// if player own goaled/was scored on
		if (caller.GetTeamNum() != player_team) {
			if (!player_touched_last) return;
			num_own_goals++;
			if (enable_toasts) {
				gameWrapper->Execute([this](GameWrapper* gw) {
					cvarManager->executeCommand("own_goal_notification");
					});
			}
			player_touched_last = false;
			return;
		}
		on_goal_hit();
		});
	//gameWrapper->HookEvent("Function TAGame.Ball_TA.OnHitGoal", std::bind(&PostCounterV1::on_goal_hit, this));

	// on every ball touch, store if the player touched it most recently and which team the player is on
	gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.OnHitBall", [this](CarWrapper caller, void* params, std::string eventname) {
		if (!should_track_shots) return;
		if (!caller) return;

		player_touched_last = check_if_player_touched_last(caller);
		// if player touched last, store player team number - this catches for if the player switches teams
		if (player_touched_last) {
			PriWrapper playerPri = caller.GetPRI();
			player_team = playerPri.GetTeamNum(); // 0 for blue, 1 for orange
		}
		});

#pragma endregion

#pragma region GameState Event Hooks
	// manage for replays
	gameWrapper->HookEvent("Function GameEvent_Soccar_TA.ReplayPlayback.BeginState",
		[this](std::string eventName) {
			should_track_shots = false;
		});
	gameWrapper->HookEvent("Function GameEvent_Soccar_TA.ReplayPlayback.EndState",
		[this](std::string eventName) {
			should_track_shots = true;
		});

	// only track during the proper states/log matches. Subscribing to these events also prevents crashes at the end of matches/during replays
	gameWrapper->HookEvent("Function GameEvent_Soccar_TA.WaitingForPlayers.BeginState",
		[this](std::string eventName) {
			should_track_shots = true;
			num_matches++;
		});
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",
		[this](std::string eventName) {
			should_track_shots = false;
			if (reset_every_match) clear_shot_stats();
		});
	gameWrapper->HookEvent("Function TAGame.Mutator_Freeplay_TA.Init",
		[this](std::string eventName) {
			should_track_shots = true;
		});
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.Destroyed",
		[this](std::string eventName) {
			should_track_shots = false;
		});

	// manage for demo crashing
	gameWrapper->HookEventWithCaller<CarWrapper>(
		"Function TAGame.Car_TA.OnDemolishedGoalExplosion",
		[this](CarWrapper caller, void* params, std::string eventName)
		{
			CarWrapper player_car = gameWrapper->GetLocalCar();
			// player car will be null if demo'd, also catches for double run on demo-trade
			if (!player_car && !is_demolished) {
				//LOG("player demolished, tracking paused", caller.GetOwnerName());
				is_demolished = true;
				should_track_shots = false;
			}
		});
	gameWrapper->HookEventWithCaller<CarWrapper>(
		"Function TAGame.GameEvent_TA.AddCar",
		[this](CarWrapper caller, void* params, std::string eventName)
		{
			if (is_demolished) {
				is_demolished = false;
				should_track_shots = true;
				//LOG("player spawned tracking started again");
			}
		});
#pragma endregion

	clear_shot_stats();
	LOG("PostCounter loaded");
}

void PostPercentage::onUnload() {
	cvarManager->removeCvar("PP_UI_display");
	cvarManager->removeCvar("PP_abbreviated_UI");
	cvarManager->removeCvar("PP_toast_notifications_toggle");
	cvarManager->removeCvar("PP_enable_in_training_toggle");
	cvarManager->removeCvar("PP_reset_after_every_game_toggle");
	cvarManager->removeCvar("PP_post_size");
}

#pragma	region Shot Tracking
/*
* hooked into EventHitWorld method, will call 'on_goal_hit' if goal reset is disabled
*/
void PostPercentage::on_post_hit() {
	if (!should_track_shots) return;
	if (!player_touched_last) return;
	if (!enable_in_training && (gameWrapper->IsInFreeplay() || gameWrapper->IsInCustomTraining())) return;

	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	BallWrapper ball = server.GetBall();
	if (!ball) return;
	CarWrapper car = gameWrapper->GetLocalCar();
	if (!car) return;

	const Vector ball_hit_loc = ball.GetLocation();
	if ((ball_hit_loc.Z > GROUND_LEVEL && ball_hit_loc.Z < (CROSSBAR_HEIGHT + POST_SIZE))
		&& (ball_hit_loc.X > (LEFT_POST - POST_SIZE) && ball_hit_loc.X < (RIGHT_POST + POST_SIZE))) {
		// if in freeplay, check if ball is in goal (and if goal reset is disabled) to track goals
		if (gameWrapper->IsInFreeplay()) {
			if ((ball_hit_loc.Y <= GOAL_LINE_ORANGE && player_team == 1) || (ball_hit_loc.Y >= GOAL_LINE_BLUE && player_team == 0)) {
				on_goal_hit();
				return;
			}
		}

		// check if ball has hit the post and update stats
		if ((ball_hit_loc.Y < BACK_OF_FIELD_ORANGE && player_team == 1) || (ball_hit_loc.Y > BACK_OF_FIELD_BLUE && player_team == 0)) {
			if (enable_toasts) {
				gameWrapper->Execute([this](GameWrapper* gw) {
					cvarManager->executeCommand("post_toast_notification");
					});
			}
			update_shot_stats(1.f, 0.f, 1.f);
		}
	}
}

/**
* called by 'on_post_hit' when in freeplay and goal scoring is disabled and hooked onto scoring method to increment stats
*/
void PostPercentage::on_goal_hit() {
	if (!should_track_shots) return;
	if (!player_touched_last) return;
	if (!enable_in_training && (gameWrapper->IsInFreeplay() || gameWrapper->IsInCustomTraining())) return;
	update_shot_stats(1.f, 1.f, 0.f);
}

#pragma	endregion

#pragma region Shot Tracking Helpers
/**
* Called on every ball touch. ret true if player last touched ball, false if not
*/
bool PostPercentage::check_if_player_touched_last(CarWrapper callerCar) {
	if (!should_track_shots) return false;

	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return false;
	BallWrapper ball = server.GetBall();
	if (!ball) return false;

	CarWrapper playerCar = gameWrapper->GetLocalCar();
	if (!playerCar) return false;
	return callerCar.GetOwnerName() == playerCar.GetOwnerName();
}

/*
* clears all shot stats AND resets last touch
*/
void PostPercentage::clear_shot_stats() {
	num_shots = 0.f;
	num_shots_in_matches = 0.f;

	num_posts = 0.f;
	num_posts_in_matches = 0.f;

	num_goals = 0.f;
	num_goals_in_matches = 0.f;
	num_own_goals = 0.f;

	overall_accuracy = 0.f;
	in_game_accuracy = 0.f;
	posts_per_match = 0.f;
	num_matches = 0.f;

	player_touched_last = false;
}

/*
* Increments shot stats by the parameter specified amounts AND resets last touch on ball
*/
void PostPercentage::update_shot_stats(float shotIncrement, float goalIncrement, float postIncrement) {
	if (!should_track_shots) return;
	num_shots += shotIncrement;
	if (gameWrapper->IsInOnlineGame()) num_shots_in_matches += shotIncrement;
	num_goals += goalIncrement;
	if (gameWrapper->IsInOnlineGame()) num_goals_in_matches += goalIncrement;
	num_posts += postIncrement;
	if (gameWrapper->IsInOnlineGame()) num_posts_in_matches += postIncrement;

	overall_accuracy = (num_goals / num_shots) * 100.f;
	if (num_matches > 0) {
		in_game_accuracy = (num_goals_in_matches / num_shots_in_matches) * 100.f;
		posts_per_match = num_posts_in_matches / num_matches;
	}
	player_touched_last = false;
}

#pragma endregion

#pragma region GUI
void PostPercentage::RenderSettings()
{
#pragma region Stat indicators
	ImGui::Columns(2, nullptr, true);
	ImGui::SetColumnWidth(0, 200.f);
	ImGui::SetColumnWidth(1, 200.f);
	ImGui::Text("Shots: %.f", num_shots);
	ImGui::Text("Shots in games: %.f", num_shots_in_matches);
	float shots_per_match = 0.f;
	if (num_matches > 0) shots_per_match = num_shots / num_matches;
	ImGui::Text("Shots per game: %.1f", shots_per_match);
	ImGui::TextUnformatted("----------------");

	ImGui::Text("Posts: %.f", num_posts);
	ImGui::Text("Posts in games: %.f", num_posts_in_matches);
	ImGui::Text("Posts per game: %.1f", posts_per_match);
	ImGui::TextUnformatted("----------------");

	ImGui::Text("Goals: %.f", num_goals);
	ImGui::Text("Goals in games: %.f", num_goals_in_matches);
	float goals_per_match = 0.f;
	if (num_matches > 0) goals_per_match = num_goals_in_matches / num_matches;
	ImGui::Text("Goals per game: %.1f", goals_per_match);

	ImGui::NextColumn(); // Move to the second column

	ImGui::Text("Own Goals: %.f", num_own_goals);
	float own_goals_per_match = 0.f;
	if (num_matches > 0) own_goals_per_match = num_own_goals / num_matches;
	ImGui::Text("Own Goals per game: %.1f", own_goals_per_match);
	ImGui::TextUnformatted("----------------");

	ImGui::Text("Overall Accuracy: %.1f%%", overall_accuracy);
	ImGui::Text("In Game Accuracy: %.1f%%", in_game_accuracy);
	ImGui::Text("Games Played: %.f", num_matches);

	ImGui::Columns(1); // End columns
	ImGui::Separator();
#pragma endregion

#pragma region Plugin Setting Controls
	CVarWrapper ui_cvar = _globalCvarManager->getCvar("PP_UI_display");
	if (!ui_cvar) return;
	display_text = ui_cvar.getBoolValue();
	if (ImGui::Checkbox("Onscreen UI", &display_text)) {
		ui_cvar.setValue(display_text);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Disables/Enables the onscreen UI");
	}

	CVarWrapper abbr_ui_cvar = _globalCvarManager->getCvar("PP_abbreviated_UI");
	if (!abbr_ui_cvar) return;
	abbreviated_text = abbr_ui_cvar.getBoolValue();
	if (ImGui::Checkbox("Abbreviated UI", &abbreviated_text)) {
		abbr_ui_cvar.setValue(abbreviated_text);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Enables/disables the abbreviated onscreen UI");
	}

	CVarWrapper toast_cvar = _globalCvarManager->getCvar("PP_toast_notifications_toggle");
	if (!toast_cvar) return;
	enable_toasts = toast_cvar.getBoolValue();
	if (ImGui::Checkbox("Toast Notifications", &enable_toasts)) {
		toast_cvar.setValue(enable_toasts);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Disables/Enables the onscreen toast notifications. You MUST have notifications enabled in BakkesMod settings under 'misc'");
	}

	CVarWrapper training_cvar = _globalCvarManager->getCvar("PP_enable_in_training_toggle");
	if (!training_cvar) return;
	enable_in_training = training_cvar.getBoolValue();
	if (ImGui::Checkbox("Enable in training", &enable_in_training)) {
		training_cvar.setValue(enable_in_training);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Enables/disables tracking in freeplay/custom training");
	}

	CVarWrapper post_size_cvar = _globalCvarManager->getCvar("PP_post_size");
	if (!post_size_cvar) return;
	POST_SIZE = post_size_cvar.getFloatValue();
	if (ImGui::SliderFloat("Post Size", &POST_SIZE, 0.0f, 500.f)) {
		post_size_cvar.setValue(POST_SIZE);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Controls the area that the plugin sees as the post. This does not effect the crossbar height");
	}
	if (ImGui::Button("Reset Post Size")) {
		post_size_cvar.setValue(100.f);
		POST_SIZE = post_size_cvar.getFloatValue();
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Resets the post size to the default of 100uu");
	}
	if (ImGui::Button("Reset All Stats")) {
		gameWrapper->Execute([this](GameWrapper* gw) {
			cvarManager->executeCommand("ResetPostCounterStats");
			});
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Resets ALL stats for the session. This cannot be undone");
	}

	CVarWrapper reset_cvar = _globalCvarManager->getCvar("PP_reset_after_every_game_toggle");
	if (!reset_cvar) return;
	reset_every_match = reset_cvar.getBoolValue();
	if (ImGui::Checkbox("Reset after every match", &reset_every_match)) {
		reset_cvar.setValue(reset_every_match);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Toggles resetting stats after each match. This will effectively disable the per-game stats");
	}
#pragma endregion
}

void PostPercentage::Render(CanvasWrapper canvas) {
	if (display_text) {
		canvas.SetColor(255, 255, 255, 255);
		std::string text = "";
		switch (abbreviated_text)
		{
		default:
			break;
		case true:
			text = "S: " + std::to_string(static_cast<int>(num_shots)) +
				" G: " + std::to_string(static_cast<int>(num_goals)) +
				" P: " + std::to_string(static_cast<int>(num_posts));
			break;
		case false:
			text = "Shots: " + std::to_string(static_cast<int>(num_shots)) +
				" Goals: " + std::to_string(static_cast<int>(num_goals)) +
				" Posts: " + std::to_string(static_cast<int>(num_posts));
			break;
		}
		int fontSize = 2;
		canvas.DrawString(text, fontSize, fontSize, true, true);
		Vector2 screenSize = gameWrapper->GetScreenSize();

		// Adjust the position to be 150 pixels above the bottom-right corner
		Vector2 position(screenSize.X, screenSize.Y - 150);

		// Set the position and draw the text
		canvas.SetPosition(position);
	}
}

std::string PostPercentage::pick_toast_insult(bool isPost) {
	std::random_device rd;  // Obtain a seed from the operating system
	std::mt19937 gen(rd()); // Standard mersenne_twister_engine seeded with rd()
	std::uniform_int_distribution<int> dis(1, 100); // Define the distribution
	int rand = dis(gen); // Generate a random number in the range [1, 10]

	std::string ret = "";
	switch (isPost)
	{
	default:
		break;
	case true: // hit post
		if (rand <= 10) {
			ret = "I'd forfeit honestly";
		}
		if (rand > 10 && rand <= 25) {
			ret = "It was wide open, cmon";
		}
		else if (rand > 25 && rand <= 50) {
			ret = "Are you blind?";
		}
		else if (rand > 50 && rand <= 75) {
			ret = "You must have been trying to miss";
		}
		else if (rand > 75 && rand <= 90) {
			ret = "Nice shot! Nice shot! Nice shot!";
		}
		else if (rand > 90 && rand <= 95) {
			ret = "Quick, blame lag";
		}
		else {
			ret = "Maybe try another game";
		}
		break;
	case false: // own goal
		if (rand <= 10) {
			ret = "Nice shot. Wrong goal, but nice shot.";
		}
		if (rand > 10 && rand <= 25) {
			ret = "What are you, colorblind?";
		}
		else if (rand > 25 && rand <= 50) {
			ret = "Should have aimed for the post";
		}
		else if (rand > 50 && rand <= 75) {
			ret = "So... don't do that maybe?";
		}
		else if (rand > 75 && rand <= 90) {
			ret = "You should have to watch the replay";
		}
		else {
			ret = "Quick, blame lag";
		}
		break;
	}
	return ret;
}
#pragma endregion