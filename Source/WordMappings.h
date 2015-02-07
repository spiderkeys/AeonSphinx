#pragma once

// All words in vocabulary, which will be mapped to their strings
enum class EWords
{
	TAKE,
	OFF,
	LAND,
	GO,
	TO,
	TAG,
	NUMBER,
	RETURN,
	HOME,
	STOP,
	START,
	MISSION,
	ABORT,
	FOLLOW,
	ONE,
	TWO,
	THREE,
	FOUR,
	FIVE,
	SIX,
	SEVEN,
	EIGHT,
	NINE,
	TEN,
	CONNECT,
	DISCONNECT,
	EMERGENCY,
	FLIP,
	FORWARD,
	LEFT,
	RIGHT,
	BACKWARD,
	TURN,
	AROUND
};

// All command phrases, which will be defined by the presence of specific combos of EWords
enum class ECommands
{
	TAKE_OFF,
	LAND,
	GO_TO_TAG,
	RETURN_HOME,
	START_MISSION,
	PAUSE_MISSION,
	ABORT_MISSION,
	STOP,
	CONTINUE,

};
