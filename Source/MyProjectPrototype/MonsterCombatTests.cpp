#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "MonsterCharacter.h"
#include "EditableDefenseBarricade.h"
#include "Components/BoxComponent.h"
#include "MyProjectPrototypeCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMonsterCombatTest,
    "Prototype.MonsterCombat.DamageEvadeWallAndDeath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMonsterCombatTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues IVS = UWorld::InitializationValues()
        .AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
        .EnableTraceCollision(true).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
    if (!TestNotNull(TEXT("Test physics world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    World->GetWorldSettings()->NotifyBeginPlay();

    UClass* PlayerClass = LoadClass<AMyProjectPrototypeCharacter>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
    if (!TestNotNull(TEXT("Existing player blueprint"), PlayerClass)) return false;
    AMyProjectPrototypeCharacter* Player = World->SpawnActorDeferred<AMyProjectPrototypeCharacter>(PlayerClass,
        FTransform(FRotator::ZeroRotator, FVector(200,0,1000)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestNotNull(TEXT("Player"), Player)) return false;
    Player->AutoPossessAI = EAutoPossessAI::Disabled;
    Player->AutoPossessPlayer = EAutoReceiveInput::Disabled;
    Player->FinishSpawning(FTransform(FRotator::ZeroRotator,FVector(200,0,1000)));
    Player->GetCharacterMovement()->DisableMovement();

    AMonsterCharacter* Monster = World->SpawnActorDeferred<AMonsterCharacter>(AMonsterCharacter::StaticClass(),
        FTransform(FRotator::ZeroRotator,FVector(0,0,1000)),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestNotNull(TEXT("Monster"), Monster)) return false;
    Monster->AutoPossessAI = EAutoPossessAI::Disabled;
    Monster->FinishSpawning(FTransform(FRotator::ZeroRotator,FVector(0,0,1000)));
    Monster->GetCharacterMovement()->DisableMovement();
    auto Advance = [World](float Seconds)
    {
        for (int32 I=0; I<FMath::CeilToInt(Seconds/.025f); ++I)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All,.025f);
        }
    };
    Advance(.05f);
    TestEqual(TEXT("Initial player HP"),Player->GetHealth(),100.f);
    TestTrue(TEXT("Start in-range attack"),Monster->TryStartMeleeAttack(Player));
    Advance(.3f);
    TestEqual(TEXT("Windup causes no damage"),Player->GetHealth(),100.f);
    Advance(.3f);
    TestEqual(TEXT("Hit window applies one hit"),Player->GetHealth(),90.f);
    Advance(.5f);
    TestEqual(TEXT("No repeated damage during hit/recovery"),Player->GetHealth(),90.f);
    TestFalse(TEXT("Cooldown blocks early restart"),Monster->TryStartMeleeAttack(Player));
    Advance(.5f);
    TestTrue(TEXT("Cooldown eventually allows attack"),Monster->TryStartMeleeAttack(Player));
    Player->SetActorLocation(FVector(1000,0,1000),false,nullptr,ETeleportType::TeleportPhysics);
    Advance(1.1f);
    TestEqual(TEXT("Moving out during windup evades"),Player->GetHealth(),90.f);

    Advance(.5f);
    Player->SetActorLocation(FVector(200,0,1000),false,nullptr,ETeleportType::TeleportPhysics);
    TestTrue(TEXT("Start attack before cover appears"),Monster->TryStartMeleeAttack(Player));
    AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>();
    Wall->SetMobility(EComponentMobility::Movable);
    Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Wall->SetActorLocation(FVector(100,0,1000));
    Wall->SetActorScale3D(FVector(.2f,3.f,4.f));
    Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    Advance(1.1f);
    TestEqual(TEXT("Cover blocks damage at impact time"),Player->GetHealth(),90.f);
    Wall->Destroy();
    Advance(.5f);

    TestTrue(TEXT("Start attack before lethal damage"),Monster->TryStartMeleeAttack(Player));
    UGameplayStatics::ApplyDamage(Monster,1000.f,nullptr,Player,UDamageType::StaticClass());
    Advance(.7f);
    TestEqual(TEXT("Attacker death cancels pending damage"),Player->GetHealth(),90.f);
    UGameplayStatics::ApplyDamage(Player,1000.f,nullptr,nullptr,UDamageType::StaticClass());
    TestEqual(TEXT("Player HP clamps to zero"),Player->GetHealth(),0.f);
    TestFalse(TEXT("Player death is exposed to targeting"),Player->IsAlive());
    TestEqual(TEXT("Dead player ignores further damage"),UGameplayStatics::ApplyDamage(Player,10.f,nullptr,nullptr,UDamageType::StaticClass()),0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScaledBarricadeCombatTest,
    "Prototype.MonsterCombat.ScaledEditableBarricade",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScaledBarricadeCombatTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Settings = UWorld::InitializationValues()
        .AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
        .EnableTraceCollision(true).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
        true, ERHIFeatureLevel::Num, &Settings);
    if (!TestNotNull(TEXT("Barricade physics world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT {
        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    };
    World->InitializeActorsForPlay(FURL());
    World->GetWorldSettings()->NotifyBeginPlay();

    // Cover both faces and the wall scales used in the saved test map.
    for (const FVector Scale : {FVector::OneVector, FVector(2.0, 1.8, 1.0), FVector(1.8, 1.8, 1.0)})
    {
        for (const float Side : {-1.f, 1.f})
        {
            const FTransform WallTransform(FRotator(0, 90, 0), FVector::ZeroVector, Scale);
            AEditableDefenseBarricade* Wall = World->SpawnActorDeferred<AEditableDefenseBarricade>(
                AEditableDefenseBarricade::StaticClass(), WallTransform, nullptr, nullptr,
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (!TestNotNull(TEXT("Editable wall"), Wall)) return false;
            Wall->FinishSpawning(WallTransform);
            const UBoxComponent* Blocker = Wall->FindComponentByClass<UBoxComponent>();
            const FVector Normal = Blocker->GetForwardVector() * Side;
            const FVector Approach = Wall->GetApproachPoint(Side);
            // Move To accepts an 80 cm radius around its goal.
            const FVector StopLocation = Approach + Normal * 80.f + FVector(0, 0, 96.f);
            AMonsterCharacter* Monster = World->SpawnActorDeferred<AMonsterCharacter>(
                AMonsterCharacter::StaticClass(), FTransform(StopLocation), nullptr, nullptr,
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (!TestNotNull(TEXT("Attacking monster"), Monster)) return false;
            Monster->AutoPossessAI = EAutoPossessAI::Disabled;
            Monster->FinishSpawning(FTransform(StopLocation));
            Monster->GetCharacterMovement()->DisableMovement();

            const bool bCanReach = Monster->CanReachMeleeTarget(Wall);
            TestTrue(FString::Printf(TEXT("Scaled wall is reachable at movement acceptance: %s side %.0f"),
                *Scale.ToString(), Side), bCanReach);
            if (bCanReach)
            {
                TestTrue(TEXT("Monster begins wall attack"), Monster->TryStartMeleeAttack(Wall));
                for (int32 Tick = 0; Tick < 28; ++Tick)
                {
                    ++GFrameCounter;
                    World->Tick(LEVELTICK_All, .025f);
                }
                TestTrue(TEXT("Melee hit damages construction-script wall"), Wall->GetHealthFraction() < 1.f);
            }
            UGameplayStatics::ApplyDamage(Wall, 1000.f, nullptr, Monster, UDamageType::StaticClass());
            TestFalse(TEXT("Lethal damage breaks wall"), Wall->IsStanding());
            Monster->SetActorLocation(Blocker->GetComponentLocation(), false, nullptr,
                ETeleportType::TeleportPhysics);
            Wall->Repair(100.f);
            TestFalse(TEXT("Occupied wall cannot rebuild"), Wall->IsStanding());
            Monster->SetActorLocation(StopLocation, false, nullptr, ETeleportType::TeleportPhysics);
            for (int32 Tick = 0; Tick < 12; ++Tick)
            {
                ++GFrameCounter;
                World->Tick(LEVELTICK_All, .025f);
            }
            TestFalse(TEXT("Leaving a repaired wall does not trigger delayed automatic rebuild"),
                Wall->IsStanding());
            Wall->Repair(1.f);
            TestTrue(TEXT("Another explicit repair rebuilds the clear wall"), Wall->IsStanding());
            Monster->Destroy();
            Wall->Destroy();
        }
    }
    return true;
}
#endif
