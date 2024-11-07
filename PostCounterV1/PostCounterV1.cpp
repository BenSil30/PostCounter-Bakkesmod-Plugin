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
	gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
		Render(canvas);
		});

#pragma region Cvars
	cvarManager->registerCvar("display_UI", "1", "Toggle on/off the UI")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {display_text = cvar.getBoolValue(); });
	cvarManager->registerCvar("abbreviated_UI", "0", "Toggle on/off the abbreviated UI")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {abbreviated_text = cvar.getBoolValue(); });
	cvarManager->registerCvar("post_size", "100.f", "The size of the posts")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {POST_SIZE = cvar.getBoolValue(); });
#pragma endregion

#pragma region Shot Event Hooks
	gameWrapper->HookEvent("Function TAGame.Ball_TA.EventHitWorld", std::bind(&PostPercentage::on_post_hit, this));
	gameWrapper->HookEventWithCaller<TeamWrapper>("Function TAGame.Team_TA.OnScoreUpdated", [this](TeamWrapper caller, void* params, std::string eventname) {
		// if player own goaled/was scored on
		if (caller.GetTeamNum() != player_team) {
			if (!player_touched_last) return;
			num_own_goals++;
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

#pragma	region Shot Tracking
/*
* hooked into EventHitWorld method, will call 'on_goal_hit' if goal reset is disabled
*/
void PostPercentage::on_post_hit() {
	if (!should_track_shots) return;
	if (!player_touched_last) return;

	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	BallWrapper ball = server.GetBall();
	if (!ball) return;
	CarWrapper car = gameWrapper->GetLocalCar();
	if (!car) return;

	const Vector ball_hit_loc = ball.GetLocation();
	if ((ball_hit_loc.Z > GROUND_LEVEL && ball_hit_loc.Z < CROSSBAR_HEIGHT)
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
	ImGui::Text("Shots: %.f", num_shots);
	ImGui::Text("Shots in games: %.f", num_shots_in_matches);
	float shots_per_match = 0.f;
	if (num_matches > 0) shots_per_match = num_shots / num_matches;
	ImGui::Text("Shots per game: %.f", shots_per_match);
	ImGui::TextUnformatted("----------------");

	ImGui::Text("Posts: %.f", num_posts);
	ImGui::Text("Posts in games: %.f", num_posts_in_matches);
	ImGui::Text("Posts per game: %.f", posts_per_match);
	ImGui::TextUnformatted("----------------");

	ImGui::Text("Goals: %.f", num_goals);
	ImGui::Text("Goals in games: %.f", num_goals_in_matches);
	float goals_per_match = 0.f;
	if (num_matches > 0) goals_per_match = num_goals_in_matches / num_matches;
	ImGui::Text("Goals per game: %.f", goals_per_match);
	ImGui::TextUnformatted("----------------");

	ImGui::Text("Own Goals: %.f", num_own_goals);
	float own_goals_per_match = 0.f;
	if (num_matches > 0) own_goals_per_match = num_own_goals / num_matches;
	ImGui::Text("Own Goals per game: %.f", own_goals_per_match);
	ImGui::TextUnformatted("----------------");

	ImGui::Text("Overall Accuracy: %.f%%", overall_accuracy);
	ImGui::Text("In Game Accuracy: %.f%%", in_game_accuracy);
	ImGui::Text("Games Played: %.f", num_matches);
	ImGui::TextUnformatted("----------------");
	//ImGui::Text("Player Touched Last: %s", player_touched_last ? "Yes" : "No");
	//ImGui::Text("Player Team: %d", player_team);
	//ImGui::Text("Is Tracking Shots: %s", should_track_shots ? "Yes" : "No");
	//ImGui::Text("Is Player Demolished: %s", is_demolished ? "Yes" : "No");
	//ImGui::TextUnformatted("----------------");

	CVarWrapper ui_cvar = _globalCvarManager->getCvar("display_UI");
	if (!ui_cvar) return;
	display_text = ui_cvar.getBoolValue();
	if (ImGui::Checkbox("Enable UI", &display_text)) {
		ui_cvar.setValue(display_text);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Disables/Enables the onscreen UI");
	}

	CVarWrapper abbr_ui_cvar = _globalCvarManager->getCvar("abbreviated_UI");
	if (!abbr_ui_cvar) return;
	abbreviated_text = abbr_ui_cvar.getBoolValue();
	if (ImGui::Checkbox("Enable abbreviated UI", &abbreviated_text)) {
		abbr_ui_cvar.setValue(abbreviated_text);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Enables/disables the abbreviated onscreen UI");
	}

	CVarWrapper post_size_cvar = _globalCvarManager->getCvar("post_size");
	if (!post_size_cvar) return;
	POST_SIZE = post_size_cvar.getFloatValue();
	if (ImGui::SliderFloat("Post size", &POST_SIZE, 0.0f, 500.f)) {
		post_size_cvar.setValue(POST_SIZE);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Controls the area that the plugin sees as the post. This does not effect the crossbar height");
	}
	if (ImGui::Button("Reset post size")) {
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
}

void PostPercentage::Render(CanvasWrapper canvas) {
	if (display_text && should_track_shots) {
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
		canvas.SetPosition(gameWrapper->GetScreenSize());
	}
}
#pragma endregion