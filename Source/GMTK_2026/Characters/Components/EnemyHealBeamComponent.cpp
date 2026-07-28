#include "Characters/Components/EnemyHealBeamComponent.h"

#include "Characters/Components/HealthComponent.h"
#include "Utility/LogChannels.h"

void UEnemyHealBeamComponent::ApplyBeamTick()
{
	AActor* BeamTarget = GetBeamTarget();
	if (!IsValid(BeamTarget))
	{
		return;
	}

	const float HealThisTick = HealPerSecond * HealTickInterval;

	if (UHealthComponent* TargetHealth = BeamTarget->FindComponentByClass<UHealthComponent>())
	{
		TargetHealth->Heal(HealThisTick);
	}
	else
	{
		UE_LOG(LogGMTKCombat, Warning,
			TEXT("Heal target %s has no HealthComponent - no heal applied."),
			*BeamTarget->GetName());
	}
}
