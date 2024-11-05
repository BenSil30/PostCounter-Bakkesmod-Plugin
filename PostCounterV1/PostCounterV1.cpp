#include "pch.h"
#include "PostCounterV1.h"

BAKKESMOD_PLUGIN(PostCounterV1, "See just how bad at shooting you are", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void PostCounterV1::onLoad()
{
	_globalCvarManager = cvarManager;
	cvarManager->registerNotifier("ResetPostCounterStats", [this](std::vector<std::string> args) {
		clear_shot_stats();
		}, "Clears shot stats", PERMISSION_ALL);

	clear_shot_stats();

#pragma region Shot Event Hooks
	gameWrapper->HookEvent("Function TAGame.Ball_TA.EventHitWorld", std::bind(&PostCounterV1::on_post_hit, this));
	gameWrapper->HookEvent("Function TAGame.Ball_TA.OnHitGoal", std::bind(&PostCounterV1::on_goal_scored, this));
	gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.OnHitBall", [this](CarWrapper caller, void* params, std::string eventname) {
		if (!should_track_shots) return;
		player_touched_last = check_if_player_touched_last(caller);
		// if player touched last, store player team number
		if (player_touched_last) {
			PriWrapper playerPri = caller.GetPRI();
			player_team = playerPri.GetTeamNum(); // 0 for blue, 1 for orange
		}
		});
#pragma endregion

	gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
		Render(canvas);
		});

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

	// only track during the proper states/log matches
	gameWrapper->HookEvent("Function GameEvent_Soccar_TA.WaitingForPlayers.BeginState",
		[this](std::string eventName) {
			should_track_shots = true;
			if (gameWrapper->IsInOnlineGame()) num_matches++;
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
#pragma endregion

	cvarManager->registerCvar("display_UI", "1", "Toggle on/off the UI")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {display_text = cvar.getBoolValue(); });

	LOG("PostCounter loaded");
}

void PostCounterV1::onUnload() {
	clear_shot_stats();
}

#pragma	region Shot Tracking
// hooked into EventHitWorld method, will call 'on_hit_goal' if goal reset is disabled
void PostCounterV1::on_post_hit() {
	if (!should_track_shots) return;
	if (!player_touched_last) return;
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	BallWrapper ball = server.GetBall();
	if (!ball) return;

	const Vector ball_hit_loc = ball.GetLocation();
	if ((ball_hit_loc.Z > GROUND_LEVEL && ball_hit_loc.Z < CROSSBAR_HEIGHT)
		&& (ball_hit_loc.X > (LEFT_POST - POST_SIZE) && ball_hit_loc.X < (RIGHT_POST + POST_SIZE))) {
		// if in freeplay, check if ball is in goal (and if goal reset is disabled) to track goals
		if (gameWrapper->IsInFreeplay()) {
			if ((ball_hit_loc.Y <= GOAL_LINE_ORANGE && player_team == 1) || (ball_hit_loc.Y >= GOAL_LINE_BLUE && player_team == 0)) {
				on_hit_goal();
				return;
			}
		}

		if ((ball_hit_loc.Y < BACK_OF_FIELD_ORANGE && player_team == 1) || (ball_hit_loc.Y > BACK_OF_FIELD_BLUE && player_team == 0)) {
			update_shot_stats(1.f, 0.f, 1.f);
			LOG("Post hit, Shots: {}, Goals:{}, Posts:{} Accuracy:{}", num_shots, num_goals, num_posts, accuracy);
		}
	}
}

/**
* called by 'on_post_hit' when in freeplay and goal scoring is disabled
*/
void PostCounterV1::on_hit_goal() {
	if (!should_track_shots) return;
	if (!player_touched_last) return;
	update_shot_stats(1.f, 1.f, 0.f);
	LOG("Goal scored, Shots: {}, Goals:{}, Posts:{} Accuracy:{}", num_shots, num_goals, num_posts, accuracy);
}

// hooked into method when goal is scored
void PostCounterV1::on_goal_scored() {
	if (!should_track_shots) return;
	if (!player_touched_last) return;
	// check if scoring team was player's team
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	//todo: fix call here - ignores first goal because there is no winning team
	TeamWrapper scoring_team = server.GetWinningTeam();
	if (!scoring_team) return;
	int team = scoring_team.GetTeamNum();
	if (team != player_team) return;

	update_shot_stats(1.f, 1.f, 0.f);
	LOG("Goal scored, Shots: {}, Goals:{}, Posts:{} Accuracy:{}", num_shots, num_goals, num_posts, accuracy);
}

#pragma	endregion

#pragma region Shot Tracking Helpers
/**
* Called on every ball touch. ret true if player last touched ball, false if not
*/
bool PostCounterV1::check_if_player_touched_last(CarWrapper callerCar) {
	if (!should_track_shots) return false;
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return false;
	BallWrapper ball = server.GetBall();
	if (!ball) return false;
	CarWrapper playerCar = gameWrapper->GetLocalCar();

	return callerCar.GetOwnerName() == playerCar.GetOwnerName();
}

void PostCounterV1::clear_shot_stats() {
	num_shots = 0.f;
	num_shots_in_matches = 0.f;
	num_posts = 0.f;
	num_posts_in_matches = 0.f;
	num_goals = 0.f;
	num_goals_in_matches = 0.f;
	accuracy = 0.f;
	posts_per_match = 0.f;
	num_matches = 0.f;
	player_touched_last = false;
}

void PostCounterV1::update_shot_stats(float shotIncrement, float goalIncrement, float postIncrement) {
	if (!should_track_shots) return;
	num_shots += shotIncrement;
	if (gameWrapper->IsInOnlineGame()) num_shots_in_matches += shotIncrement;
	num_goals += goalIncrement;
	if (gameWrapper->IsInOnlineGame()) num_goals_in_matches += goalIncrement;
	num_posts += postIncrement;
	if (gameWrapper->IsInOnlineGame()) num_posts_in_matches += postIncrement;

	accuracy = (num_goals / num_shots) * 100.f;
	if (num_matches > 0) posts_per_match = (num_posts_in_matches / num_matches) * 100.f;
	player_touched_last = false;
}

#pragma endregion

#pragma region GUI
void PostCounterV1::RenderSettings()
{
	ImGui::TextUnformatted("Post Counter Plugin v1");

	ImGui::Text("Number of Shots: %.1f", num_shots);
	ImGui::Text("Shots in Matches: %.1f", num_shots_in_matches);
	ImGui::TextUnformatted("----------------");
	ImGui::Text("Number of Posts: %.1f", num_posts);
	ImGui::Text("Posts in Matches: %.1f", num_posts_in_matches);
	ImGui::TextUnformatted("----------------");
	ImGui::Text("Number of Goals: %.1f", num_goals);
	ImGui::Text("Goals in Matches: %.1f", num_goals_in_matches);
	ImGui::TextUnformatted("----------------");
	ImGui::Text("Accuracy: %.1f%%", accuracy);
	ImGui::Text("Posts per Match: %.1f", posts_per_match);
	ImGui::Text("Number of Matches: %.1f", num_matches);
	ImGui::TextUnformatted("----------------");
	ImGui::Text("Player Touched Last: %s", player_touched_last ? "Yes" : "No");
	ImGui::Text("Player Team: %d", player_team);
	ImGui::Text("Is Tracking Shots: %s", should_track_shots ? "Yes" : "No");
	ImGui::TextUnformatted("----------------");

	CVarWrapper ui_cvar = _globalCvarManager->getCvar("display_UI");
	if (!ui_cvar) return;
	display_text = ui_cvar.getBoolValue();
	if (ImGui::Checkbox("Enable UI", &display_text)) {
		ui_cvar.setValue(display_text);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Disables/Enables the onscreen UI");
	}
	if (ImGui::Button("Reset All Stats")) {
		gameWrapper->Execute([this](GameWrapper* gw) {
			cvarManager->executeCommand("ResetPostCounterStats");
			});
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Resets ALL stats. This cannot be undone");
	}
}

void PostCounterV1::Render(CanvasWrapper canvas) {
	if (display_text && should_track_shots) {
		canvas.SetColor(255, 255, 255, 255);
		std::string text = "Shots: " + std::to_string(static_cast<int>(num_shots)) +
			" Goals: " + std::to_string(static_cast<int>(num_goals)) +
			" Posts: " + std::to_string(static_cast<int>(num_posts));
		int fontSize = 2;
		canvas.DrawString(text, fontSize, fontSize, true, true);
		canvas.SetPosition(gameWrapper->GetScreenSize());
	}
}
#pragma endregion