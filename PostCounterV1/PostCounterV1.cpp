#include "pch.h"
#include "PostCounterV1.h"

BAKKESMOD_PLUGIN(PostCounterV1, "write a plugin description here", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void PostCounterV1::onLoad()
{
	_globalCvarManager = cvarManager;

	ClearShotStats();
	if (gameWrapper->IsInFreeplay()) {
		//subscribe methods to the correct events
		gameWrapper->HookEvent("Function TAGame.Ball_TA.EventHitWorld", std::bind(&PostCounterV1::OnPostHit, this));
		gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventScore", std::bind(&PostCounterV1::OnHitGoal, this));
		gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.OnHitBall", [this](CarWrapper caller, void* params, std::string eventname) {
			playerTouchedLast = CheckIfPlayerTouchedLast(caller);
			LOG("Ball touched by player: {}", playerTouchedLast);
			});
		LOG("PostCounter Plugin loaded");
	}

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
	ClearShotStats();
}

void PostCounterV1::OnPostHit() {
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	BallWrapper ball = server.GetBall();
	if (!ball) return;
	CarWrapper car = gameWrapper->GetLocalCar();

	Vector ballHitLoc = ball.GetLocation();

	if ((ballHitLoc.Z > 93.f && ballHitLoc.Z < 2044.f)
		&& (ballHitLoc.X > -4096.f && ballHitLoc.X < 4096.f)
		&& (ballHitLoc.Y < -4992.f || ballHitLoc.Y > 4992.f)) {
		IncrementShotStats(1, 0, 1);

		LOG("Post hit, Shots:{}, Posts:{}, Miss %:{}", numShots, numPosts, missPercentage);
	}
}

void PostCounterV1::OnHitGoal() {
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	BallWrapper ball = server.GetBall();
	if (!ball) return;

	Vector ballHitLoc = ball.GetLocation();
	if ((ballHitLoc.Z > 93.f && ballHitLoc.Z < 2044.f)
		&& (ballHitLoc.X > -4096.f && ballHitLoc.X < 4096.f)) {
		IncrementShotStats(1, 1, 0);

		LOG("Goal scored, Shots: {}, Goals:{}, Accuracy:{}", numShots, numGoals, accuracy);
	}
}

bool PostCounterV1::CheckIfPlayerTouchedLast(CarWrapper callerCar) {
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return false;
	BallWrapper ball = server.GetBall();
	if (!ball) return false;
	CarWrapper playerCar = gameWrapper->GetLocalCar();

	return callerCar.GetOwnerName() == playerCar.GetOwnerName();
}

void PostCounterV1::ClearShotStats() {
	numShots = 0;
	numPosts = 0;
	numGoals = 0;
	accuracy = 0.f;
	missPercentage = 0.f;
}

void PostCounterV1::IncrementShotStats(int shotIncrement, int goalIncrement, int postIncrement) {
	numShots += shotIncrement;
	numGoals += goalIncrement;
	numPosts += postIncrement;

	accuracy = (numGoals / numShots) * 100.f;
	missPercentage = (numPosts / numShots) * 100.f;
}