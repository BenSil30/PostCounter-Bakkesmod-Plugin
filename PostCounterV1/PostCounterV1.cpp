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

	//subscribe methods to the correct events
	gameWrapper->HookEvent("Function TAGame.Ball_TA.EventHitWorld", std::bind(&PostCounterV1::on_post_hit, this));
	gameWrapper->HookEvent("Function TAGame.Ball_TA.OnHitGoal", std::bind(&PostCounterV1::on_goal_scored, this));
	gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.OnHitBall", [this](CarWrapper caller, void* params, std::string eventname) {
		player_touched_last = check_if_player_touched_last(caller);
		// if player touched last, store player team number
		if (player_touched_last) {
			PriWrapper playerPri = caller.GetPRI();
			player_team = playerPri.GetTeamNum(); // 0 for blue, 1 for orange
		}
		});
	gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
		Render(canvas);
		});
	LOG("PostCounter loaded");
}

void PostCounterV1::onUnload() {
	clear_shot_stats();
}

// hooked into EventHitWorld method, will call 'on_hit_goal' if in freeplay and
void PostCounterV1::on_post_hit() {
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
	if (!player_touched_last) return;
	update_shot_stats(1.f, 1.f, 0.f);
	LOG("Goal scored, Shots: {}, Goals:{}, Posts:{} Accuracy:{}", num_shots, num_goals, num_posts, accuracy);
}

// hooked into method when goal is scored
void PostCounterV1::on_goal_scored() {
	// check if scoring team was player's team
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	TeamWrapper scoring_team = server.GetWinningTeam();
	if (!scoring_team) return;
	int team = scoring_team.GetTeamNum();
	if (team != player_team) return;

	if (!player_touched_last) return;
	LOG("nope, here");
	update_shot_stats(1.f, 1.f, 0.f);
	LOG("Goal scored, Shots: {}, Goals:{}, Posts:{} Accuracy:{}", num_shots, num_goals, num_posts, accuracy);
}

bool PostCounterV1::check_if_player_touched_last(CarWrapper callerCar) {
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return false;
	BallWrapper ball = server.GetBall();
	if (!ball) return false;
	CarWrapper playerCar = gameWrapper->GetLocalCar();

	return callerCar.GetOwnerName() == playerCar.GetOwnerName();
}

void PostCounterV1::clear_shot_stats() {
	num_shots = 0.f;
	num_posts = 0.f;
	num_goals = 0.f;
	accuracy = 0.f;
	player_touched_last = false;
}

void PostCounterV1::update_shot_stats(float shotIncrement, float goalIncrement, float postIncrement) {
	num_shots += shotIncrement;
	num_goals += goalIncrement;
	num_posts += postIncrement;

	accuracy = (num_goals / num_shots) * 100.f;
	player_touched_last = false;
}

void PostCounterV1::Render(CanvasWrapper canvas) {
	if (displayText && (gameWrapper->IsInFreeplay() || gameWrapper->IsInOnlineGame() || gameWrapper->IsInCustomTraining())) {
		canvas.SetColor(255, 255, 255, 255); // White color
		Vector2 position(100, 100);
		std::string text = "Shots: " + std::to_string(num_shots) +
			" Goals: " + std::to_string(num_goals) +
			" Posts: " + std::to_string(num_posts);
		int fontSize = 2;
		canvas.DrawString(text, fontSize, fontSize, true, true);
		//canvas.SetPosition(position);
	}
}