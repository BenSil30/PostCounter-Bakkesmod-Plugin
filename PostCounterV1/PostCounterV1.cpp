#include "pch.h"
#include "PostCounterV1.h"

BAKKESMOD_PLUGIN(PostCounterV1, "See just how bad at shooting you are", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void PostCounterV1::onLoad()
{
	_globalCvarManager = cvarManager;
	cvarManager->registerNotifier("ResetPostCounter", [this](std::vector<std::string> args) {
		clear_shot_stats();
		}, "Clears shot stats", PERMISSION_ALL);

	clear_shot_stats();

	//subscribe methods to the correct events
	gameWrapper->HookEvent("Function TAGame.Ball_TA.EventHitWorld", std::bind(&PostCounterV1::on_post_hit, this));
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventScore", std::bind(&PostCounterV1::on_hit_goal, this));
	gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.OnHitBall", [this](CarWrapper caller, void* params, std::string eventname) {
		player_touched_last = check_if_player_touched_last(caller);

		// store the last time the ball was hit
		ServerWrapper server = gameWrapper->GetCurrentGameState();
		if (!server) return;
		last_hit_time = server.GetGameTimeRemaining();
		});
	LOG("PostCounter loaded");

	/*
	 !! Enable debug logging by setting DEBUG_LOG = true in logging.h !!
	DEBUGLOG("PostCounterV1 debug mode enabled");

	 LOG and DEBUGLOG use fmt format strings https://fmt.dev/latest/index.html

	cvarManager->registerNotifier("my_aweseome_notifier", [&](std::vector<std::string> args) {
		LOG("Hello notifier!");
	}, "", 0);

	auto cvar = cvarManager->registerCvar("template_cvar", "hello-cvar", "just a example of a cvar");
	auto cvar2 = cvarManager->registerCvar("template_cvar2", "0", "just a example of a cvar with more settings", true, true, -10, true, 10 );

	cvar.addOnValueChanged([this](std::string cvarName, CVarWrapper newCvar) {
		LOG("the cvar with name: {} changed", cvarName);
		LOG("the new value is: {}", newCvar.getStringValue());
	});

	cvar2.addOnValueChanged(std::bind(&PostCounterV1::YourPluginMethod, this, _1, _2));

	 enabled decleared in the header
	enabled = std::make_shared<bool>(false);
	cvarManager->registerCvar("TEMPLATE_Enabled", "0", "Enable the TEMPLATE plugin", true, true, 0, true, 1).bindTo(enabled);

	cvarManager->registerNotifier("NOTIFIER", [this](std::vector<std::string> params){FUNCTION();}, "DESCRIPTION", PERMISSION_ALL);
	cvarManager->registerCvar("CVAR", "DEFAULTVALUE", "DESCRIPTION", true, true, MINVAL, true, MAXVAL);//.bindTo(CVARVARIABLE);
	gameWrapper->HookEventWithCallerPost<ActorWrapper>("FUNCTIONNAME", std::bind(&PostCounterV1::FUNCTION, this, _1, _2, _3));
	gameWrapper->RegisterDrawable(bind(&TEMPLATE::Render, this, std::placeholders::_1));

	gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode", [this](std::string eventName) {
		LOG("Your hook got called and the ball went POOF");
	});
	 You could also use std::bind here
	gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode", std::bind(&PostCounterV1::YourPluginMethod, this);
	*/
}

void PostCounterV1::onUnload() {
	clear_shot_stats();
}

void PostCounterV1::on_post_hit() {
	if (!player_touched_last) return;
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	BallWrapper ball = server.GetBall();
	if (!ball) return;

	const Vector ball_hit_loc = ball.GetLocation();
	if ((ball_hit_loc.Z > GROUND_LEVEL && ball_hit_loc.Z < CROSSBAR_HEIGHT) // if ball is below crossbar and above ground
		&& (ball_hit_loc.X > (LEFT_POST - POST_SIZE) && ball_hit_loc.X < (RIGHT_POST + POST_SIZE))) { // if ball is within a certain width of the goal
		if (ball_hit_loc.Y < -GOAL_LINE || ball_hit_loc.Y > GOAL_LINE) { // if ball is in goal (for freeplay when goals are turned off)
			on_hit_goal();
			return;
		}

		if (ball_hit_loc.Y < -BACK_OF_FIELD || ball_hit_loc.Y > BACK_OF_FIELD) { // if the ball is near the goal line
			update_shot_stats(1.f, 0.f, 1.f);
			player_touched_last = false;
			LOG("Goal scored, Shots: {}, Goals:{}, Posts:{} Accuracy:{}", num_shots, num_goals, num_posts, accuracy);
		}
	}
}

void PostCounterV1::on_hit_goal() {
	if (!player_touched_last) return;
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	BallWrapper ball = server.GetBall();
	if (!ball) return;

	const Vector ball_hit_loc = ball.GetLocation();
	if ((ball_hit_loc.Z > GROUND_LEVEL && ball_hit_loc.Z < CROSSBAR_HEIGHT)
		&& (ball_hit_loc.X > (LEFT_POST - POST_SIZE) && ball_hit_loc.X > (RIGHT_POST + POST_SIZE))) {
		update_shot_stats(1.f, 1.f, 0.f);
		player_touched_last = false;
		LOG("Goal scored, Shots: {}, Goals:{}, Posts:{} Accuracy:{}", num_shots, num_goals, num_posts, accuracy);
	}
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
	last_hit_time = 0.f;
}

void PostCounterV1::update_shot_stats(float shotIncrement, float goalIncrement, float postIncrement) {
	num_shots += shotIncrement;
	num_goals += goalIncrement;
	num_posts += postIncrement;

	accuracy = (num_goals / num_shots) * 100.f;
}