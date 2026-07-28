# GMTK 2026 — Token-Directed Combat

A third-person wave-survival shooter built in **Unreal Engine 5.7 (C++)** whose core feature is a
**combat director**: a token economy that decides, every frame, which enemies are *allowed* to
attack. The result is the feel of AAA crowd combat — fights that read as overwhelming while staying
fair and readable — implemented as a single, inspectable system.

The player dual-wields pistols through timed waves of AI enemies, building kill combos that grant
escalating buffs, while the director throttles enemy aggression up and down around the player's
health, position, and camera.

---

## The headline system: the Combat Token Director

`Source/GMTK_2026/AI/Tokens/`

The design problem: with ten enemies alive, letting all ten attack at once is unwinnable, and
letting them politely take turns looks robotic. The genre solution (Doom 2016, Halo, Left 4 Dead's
director) is an attack-permission economy, and that is what this project implements:

- **Tokens as a budget, not a count.** Every attack type has a cost (`Melee` = 1,
  `RangedLaser` = 2), and the total value of outstanding tokens is capped. A laser enemy firing
  is "worth" two melee grunts — expensive attacks crowd out cheap ones automatically, with no
  special-casing anywhere else in the codebase.
- **A mercy window.** The budget shrinks as the player's health drops. At low health, fewer
  enemies are permitted to attack simultaneously — precisely when the player can least afford
  chaos — which is what stops low-health deaths from feeling cheap.
- **Forced rotation.** After an attack, an enemy enters a lockout during which it cannot
  re-request. Without this, the fastest enemy monopolizes the token and the fight collapses into
  a 1v1 with spectators.
- **Starvation priority.** Any enemy that has waited past a threshold becomes the guaranteed next
  grantee. Nobody orbits forever.
- **Directional coordination.** When there's contention, enemies *outside the player's camera
  cone* yield their request to enemies the player can see. Attacks arrive from where the player
  is looking — on-screen aggression feels fair, off-screen damage feels cheap. Starvation
  priority overrides this, so off-screen enemies are delayed, never denied.
- **Steal-on-damage.** When an enemy takes damage and holds no token, the director force-transfers
  one from an "idle" holder (one not mid-attack), preferring victims the player can't see. Shooting
  an enemy makes *that* enemy fight back, and the reshuffle is invisible.
- **Self-healing.** Tokens held past a hard cap are reclaimed with a loud log identifying the
  leak, and stale holders (dead enemies) are swept every tick. The budget cannot rot over a long
  session.

The director is a `UTickableWorldSubsystem` — one per level, zero placement or setup, resolved by
any system via `GetWorld()->GetSubsystem<UCombatTokenSubsystem>()`.

### Push, don't poll: the `ITokenHolder` interface

Tokens are keyed on **AI controllers** (the thing that decides to attack), and the subsystem pushes
grant/revoke events through the `ITokenHolder` interface rather than making holders poll. The
payoff is on forced revokes: when a token is stolen or reclaimed, the victim's controller aborts
its in-flight attack — wind-up, beam, montage and all — *the same frame*, then mirrors the change
onto its blackboard so Behavior Tree decorators react immediately. There is no window where an
enemy visibly attacks without permission.

### Watch it work

The whole economy is observable live. In PIE, open the console:

| Command | Effect |
|---|---|
| `gmtk.Tokens.Debug` | Per-enemy world-space markers — green `TOKEN` (type + hold time), yellow `WAIT` (red once starving), silver `LOCKOUT` countdown — plus a budget HUD line. |
| `gmtk.Tokens.SetMaxBudget <N>` | Retune the healthy-player budget at runtime. |
| `gmtk.Tokens.Freeze` | Refuse all new grants; watch every enemy fall into fallback behaviour as current attacks end. |
| `gmtk.Tokens.Dump` | Log every grant, waiter, and lockout. |

---

## AI architecture

`Source/GMTK_2026/AI/`, `Source/GMTK_2026/Characters/Components/`

The AI layer keeps a strict division of labour, which is what makes enemy archetypes cheap to add:

- **Behavior Trees decide *when and where*.** Custom C++ nodes handle the token handshake
  (`BTTask_RequestToken`, `BTTask_ReleaseToken`, `BTDecorator_HasToken` — an observing decorator
  that aborts an attack branch the instant permission is lost) and positioning
  (`BTTask_TeleportToVantage` samples a ring of NavMesh points around the target and keeps only
  those a chest-height beam could actually connect from).
- **Components execute *how*.** Attacks are self-contained `UActorComponent` state machines that
  own nothing but the attack itself, so any archetype gets any attack by adding a component:
  - `UEnemyBeamComponentBase` — the shared sustained-beam lifecycle: telegraphed wind-up,
    LOS-break cancellation (the player's reward for reacting to the tell), yaw tracking with an
    alignment gate so the beam never fires while pointed the wrong way, tick-based effect
    application, Niagara/montage hooks, and debug-draw fallbacks. Subclasses override exactly one
    decision — what a beam tick does:
    - `UEnemyLaserAttackComponent` → damage to the player through `IDamageable`
    - `UEnemyHealBeamComponent` → healing to the most-wounded ally
  - `UEnemyMeleeAttackComponent` — wind-up → committed swing → cooldown, with damage as a single
    range + facing-cone check at the impact frame. The swing can't turn once started, so dodging
    through it produces an honest whiff.
- **One service centralizes state.** `BTService_CombatState` computes distance, line of sight,
  melee range, a *pressure* accumulator (panic builds while the player stays close, so a
  drive-by doesn't trigger a retreat but a committed rush does), and laser-accurate vantage
  validity — once, onto the blackboard, for the whole tree to read.
- **Denied enemies get visibly hungrier.** `BTService_EscalateSpeedByWaitTime` maps token wait
  time onto movement speed (walk → jog → sprint). The crowd that *isn't* allowed to attack reads
  as a restless, tightening pack — which is the entire point of rationing attacks in the first
  place.

The controller (`AEnemyAIController`) extends `ADetourCrowdAIController` for local avoidance,
implements `IGenericTeamAgentInterface` so perception distinguishes teams (enemies never target
each other), seeds constant pursuit on possession, and wires pawn death/damage into token release
and steal-on-damage.

**Enemy archetypes are Blueprint-only.** `AEnemyCharacter` is the single C++ enemy class;
grunt/laser/healer/teleporter variants are Blueprint subclasses differing only in mesh, stats,
components, and Behavior Tree — a teammate with no C++ access can build a new enemy type.

---

## Player systems

`Source/GMTK_2026/Characters/`, `Source/GMTK_2026/Weapons/`

- **Dual-wield gunplay** with two fire disciplines: alternating hands per trigger pull (with
  empty-hand fall-through), or both-at-once — switchable at runtime because the combo system uses
  it as a reward.
- **Combo economy.** `UComboComponent` tracks a kill streak against a refreshing window. Every
  five kills grants a reward stack, and stacks walk a cycling chain — Damage → Speed → Ammo →
  Double-Fire → repeat — recomputed from zero on every change, so combo loss deterministically
  restores exact base values (no drift, no stacking bugs).
- **Weapons as data.** `AWeaponBase` covers hitscan and projectile through one class; which one a
  gun is, is a property, not a hierarchy. Ammo lives per weapon instance, so dual-wielding gives
  two independent magazines for free.
- **Risk-positioned reloading.** Reloads are incremental (one round per interval, hold-to-load,
  release-to-stop — revolver rules) and, by default, only allowed inside **reload zones** placed
  in the level: ammo is a positioning decision, not a pause. Reload endings are distinguished
  (completed / interrupted / cancelled-by-fire) so audio and UI can react to each differently.
- **Health orbs** scatter from dead enemies with a decoupled launch (spread, pop height, and fall
  speed independently tunable), rest on the ground, then magnetize to the player with
  acceleration once latched.

---

## Match flow

`Source/GMTK_2026/Core/`, `Source/GMTK_2026/Spawning/`

- **`ABaseGameMode`** owns transitions (wave events → score, survival timer, game over);
  **`ABaseGameState`** owns UI-facing data and the central `OnEnemyKilled` event that combo,
  score, and pickups all subscribe to — one kill signal, many consumers, no cross-system
  references.
- **Waves are data assets.** `UWaveDataAsset` defines enemy class/count entries; designers build
  waves in the editor without touching code. `UWaveManagerComponent` runs the state machine:
  auto-advance after a between-wave countdown, or shoot the physical wave-start trigger to begin
  early (the early start cancels the pending timer, so the two paths can't double-spawn).
- **Spawn theatrics with a safety net.** Enemies spawn with their AI paused for a Blueprint-driven
  spawn-in sequence, resumed by `FinishSpawn()` — with a failsafe timer so a missing animation can
  never permanently freeze an enemy.

---

## Engineering practices worth noting

- **Event-driven decoupling throughout.** Systems communicate by delegates and interfaces
  (`IDamageable` as the single damage path, `ITokenHolder` for director pushes, the GameState kill
  hub). Almost nothing holds a hard reference to anything it didn't create.
- **Art-optional development.** Every attack debug-draws its beam/telegraph when no VFX is
  assigned, and montage hooks are optional — the entire combat loop was tunable before any art
  existed. Montage failures (missing AnimBP slot, skeleton mismatch) are detected and logged
  instead of failing silently.
- **Designer-facing tuning.** Effectively every gameplay number — budgets, costs, lockouts,
  telegraph windows, beam ranges, combo rewards, wave pacing — is an editor-exposed `UPROPERTY`
  with a comment explaining what it trades off.
- **Debuggability as a feature.** Categorized log channels (`LogGMTKAI`, `LogGMTKCombat`,
  `LogGMTKSpawn`, `LogGMTKCore`), console commands, on-screen HUDs, and world-space state markers.
  The invisible system is the one you can't demo — so nothing here is invisible.
- **Performance hygiene.** Attack components tick only while attacking; the director's budget
  recalculation is throttled; queue and lockout maps are pruned every tick so nothing grows
  unbounded across a long match.

---

## Project layout

```
Source/GMTK_2026/
├── AI/
│   ├── Tokens/            # Combat director: subsystem, token types, ITokenHolder
│   ├── BTNodes/           # Custom Behavior Tree tasks, decorators, services
│   ├── EnemyAIController  # Perception, teams, token wiring, Detour Crowd
│   └── EnvQueryContext_*  # EQS contexts
├── Characters/
│   ├── Components/        # Health, combo, beam base + laser/heal, melee, audio
│   ├── BaseCharacter      # Shared health/death (player + enemies)
│   ├── PlayerCharacter    # Input, camera, dual-wield, combo rewards, ragdoll death
│   └── EnemyCharacter     # The one C++ enemy; archetypes are Blueprints
├── Core/                  # GameMode, GameState, PlayerController
├── Spawning/              # Wave manager, spawn points, wave-start trigger
├── Weapons/               # WeaponBase (hitscan/projectile), ProjectileBase
├── Pickups/               # Health orbs + spawner component
├── World/                 # Reload zones/points
├── Data/                  # Wave data assets
├── UI/                    # HUD / widget bases
├── Interfaces/            # IDamageable
└── Utility/               # Log channels, teams, function library
```

Built for GMTK Game Jam 2026 and expanded into a portfolio piece focused on AI combat direction.
