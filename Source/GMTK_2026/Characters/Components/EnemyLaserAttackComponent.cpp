#include "Characters/Components/EnemyLaserAttackComponent.h"

#include "Interfaces/Damageable.h"
#include "Utility/LogChannels.h"

void UEnemyLaserAttackComponent::ApplyBeamTick()
{
	AActor* BeamTarget = GetBeamTarget();
	if (!IsValid(BeamTarget))
	{
		return;
	}

	const float DamageThisTick = DamagePerSecond * DamageTickInterval;

	// Route through IDamageable rather than a health component directly - that's
	// the project convention, and it means this works on anything damageable, not
	// just characters.
	if (BeamTarget->Implements<UDamageable>())
	{
		IDamageable::Execute_ApplyDamage(
			BeamTarget, DamageThisTick, GetOwnerController(), GetOwner());
	}
	else
	{
		UE_LOG(LogGMTKCombat, Warning,
			TEXT("Laser target %s does not implement IDamageable - no damage applied."),
			*BeamTarget->GetName());
	}
}
