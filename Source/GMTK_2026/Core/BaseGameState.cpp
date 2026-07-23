#include "Core/BaseGameState.h"

void ABaseGameState::AddScore(int32 Amount)
{
	Score += Amount;
	OnScoreChanged.Broadcast(Score);
}
