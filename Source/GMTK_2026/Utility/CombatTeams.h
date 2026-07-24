#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"

/**
 * Team IDs used by the AI perception system to decide who is hostile to whom.
 *
 * Perception's DetectionByAffiliation flags (bDetectEnemies / bDetectFriendlies /
 * bDetectNeutrals) are meaningless unless something actually implements
 * IGenericTeamAgentInterface and hands out these IDs. Without them every actor is
 * team 255 (neutral) and enemies happily perceive - and target - each other.
 */
namespace CombatTeams
{
	/** The player and anything friendly to the player. */
	static const FGenericTeamId Player(0);

	/** All AI enemies. */
	static const FGenericTeamId Enemy(1);
}
