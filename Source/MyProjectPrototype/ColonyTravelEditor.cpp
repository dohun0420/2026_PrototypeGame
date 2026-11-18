#if WITH_EDITOR

#include "ColonyTravelPortal.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"

namespace ColonyTravelEditor
{
static const TCHAR* FieldMapPath = TEXT("/Game/Prototype/Maps/Lvl_ApprovedLayout");
static const TCHAR* FieldMapObjectPath = TEXT(
    "/Game/Prototype/Maps/Lvl_ApprovedLayout.Lvl_ApprovedLayout");
static const TCHAR* InteriorMapPath = TEXT("/Game/Prototype/Maps/Lvl_ColonyInterior_02");
static const TCHAR* InteriorMapObjectPath = TEXT(
    "/Game/Prototype/Maps/Lvl_ColonyInterior_02.Lvl_ColonyInterior_02");
static const TCHAR* EntranceMarkerLabel = TEXT("Colony_02_DungeonEntrance");
static const TCHAR* EntrancePortalLabel = TEXT("Colony_02_TravelEntrance");
static const TCHAR* ExitPortalLabel = TEXT("ColonyTravel_ExitToField");

static UMaterialInterface* LoadMaterial(const TCHAR* AssetPath)
{
    return LoadObject<UMaterialInterface>(nullptr, AssetPath);
}

static AStaticMeshActor* AddBlock(
    UWorld* World,
    const FString& Label,
    const FVector& Location,
    const FVector& Size,
    UMaterialInterface* Material,
    bool bCollision = true,
    float Yaw = 0.f,
    bool bCylinder = false)
{
    if (!World)
    {
        return nullptr;
    }

    AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
    Actor->SetActorLabel(Label);
    Actor->SetFolderPath(TEXT("ColonyTravel/Blockout"));
    Actor->SetMobility(EComponentMobility::Movable);
    UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
        bCylinder ? TEXT("/Engine/BasicShapes/Cylinder.Cylinder")
                  : TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetMaterial(0, Material);
    Mesh->SetCollisionProfileName(bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
    Mesh->SetCanEverAffectNavigation(bCollision);
    Actor->SetActorLocation(Location);
    Actor->SetActorRotation(FRotator(0.f, Yaw, 0.f));
    Actor->SetActorScale3D(Size / 100.f);
    Actor->SetMobility(EComponentMobility::Static);
    return Actor;
}

static void RemoveActorByLabel(UWorld* World, const FString& Label)
{
    if (!World)
    {
        return;
    }

    TArray<AActor*> Actors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
    for (AActor* Actor : Actors)
    {
        if (Actor && Actor->GetActorLabel() == Label)
        {
            Actor->Destroy();
        }
    }
}

static void RemoveInteriorStructures(UWorld* World)
{
    if (!World)
    {
        return;
    }

    TArray<AActor*> Actors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
    for (AActor* Actor : Actors)
    {
        if (!Actor)
        {
            continue;
        }

        const FString Label = Actor->GetActorLabel();
        if (Label.StartsWith(TEXT("ColonyInterior_"))
            && Label != TEXT("ColonyInterior_Floor"))
        {
            Actor->Destroy();
        }
    }
}

static void ConfigureDefaultGameMode(UWorld* World)
{
    if (!World)
    {
        return;
    }

    World->GetWorldSettings()->DefaultGameMode = LoadClass<AGameModeBase>(
        nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"));
}

static bool CreateOrUpdateInteriorMap()
{
    const bool bAlreadyExists = FPackageName::DoesPackageExist(InteriorMapPath);
    UWorld* World = bAlreadyExists
        ? UEditorLoadingAndSavingUtils::LoadMap(InteriorMapPath)
        : UEditorLoadingAndSavingUtils::NewBlankMap(false);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("COLONY_TRAVEL_BUILD failed to load/create interior map"));
        return false;
    }

    // Keep the prototype interior as a clean test arena. The travel actors,
    // player start and lights use the ColonyTravel_ prefix and remain intact.
    RemoveInteriorStructures(World);

    if (!bAlreadyExists)
    {
        UMaterialInterface* Ground = LoadMaterial(
            TEXT("/Game/Prototype/Environment/Ground/Materials/MI_SandGround.MI_SandGround"));
        if (!Ground)
        {
            Ground = LoadMaterial(
                TEXT("/Game/Prototype/Materials/MI_BlockoutSand.MI_BlockoutSand"));
        }

        AddBlock(World, TEXT("ColonyInterior_Floor"), FVector(0.f, 0.f, -50.f),
            FVector(8000.f, 6000.f, 100.f), Ground);

        APlayerStart* PlayerStart = World->SpawnActor<APlayerStart>(
            FVector(-2850.f, 0.f, 110.f), FRotator::ZeroRotator);
        PlayerStart->SetActorLabel(TEXT("ColonyTravel_PlayerStart"));
        PlayerStart->SetFolderPath(TEXT("ColonyTravel/Gameplay"));

        for (int32 Index = 0; Index < 3; ++Index)
        {
            APointLight* Light = World->SpawnActor<APointLight>(
                FVector(-1800.f + Index * 2200.f, 0.f, 650.f),
                FRotator::ZeroRotator);
            Light->SetActorLabel(FString::Printf(TEXT("ColonyTravel_Light_%02d"), Index + 1));
            Light->SetFolderPath(TEXT("ColonyTravel/Lighting"));
            Light->PointLightComponent->SetMobility(EComponentMobility::Movable);
            Light->PointLightComponent->SetIntensity(12000.f);
            Light->PointLightComponent->SetAttenuationRadius(2600.f);
            Light->PointLightComponent->SetLightColor(
                Index == 1
                    ? FLinearColor(0.8f, 0.18f, 0.08f)
                    : FLinearColor(0.18f, 0.35f, 0.8f));
        }
    }

    RemoveActorByLabel(World, TEXT("ColonyInterior_Title"));
    RemoveActorByLabel(World, ExitPortalLabel);
    AColonyTravelPortal* ExitPortal = World->SpawnActor<AColonyTravelPortal>(
        FVector(-2600.f, 0.f, 100.f), FRotator(0.f, 180.f, 0.f));
    if (!ExitPortal)
    {
        return false;
    }
    ExitPortal->SetActorLabel(ExitPortalLabel);
    ExitPortal->SetFolderPath(TEXT("ColonyTravel/Gameplay"));
    ExitPortal->ColonyId = 2;
    ExitPortal->DestinationLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(FieldMapObjectPath));
    ExitPortal->bIsExitPortal = true;
    ExitPortal->bRequiresProximity = false;
    ExitPortal->InteractionPrompt = FText::FromString(TEXT("PRESS E TO EXIT"));

    ConfigureDefaultGameMode(World);
    World->MarkPackageDirty();
    return UEditorLoadingAndSavingUtils::SaveMap(World, InteriorMapPath);
}

static bool InstallFieldEntrance()
{
    UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(FieldMapPath);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("COLONY_TRAVEL_BUILD failed to load field map"));
        return false;
    }

    AActor* EntranceMarker = nullptr;
    TArray<AActor*> Actors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
    for (AActor* Actor : Actors)
    {
        if (Actor && Actor->GetActorLabel() == EntranceMarkerLabel)
        {
            EntranceMarker = Actor;
            break;
        }
    }
    if (!EntranceMarker)
    {
        UE_LOG(LogTemp, Error, TEXT("COLONY_TRAVEL_BUILD missing marker %s"), EntranceMarkerLabel);
        return false;
    }

    RemoveActorByLabel(World, EntrancePortalLabel);
    const FVector PortalLocation = EntranceMarker->GetActorLocation();
    const FRotator PortalRotation =
        (FVector(0.f, 5300.f, PortalLocation.Z) - PortalLocation).Rotation();
    AColonyTravelPortal* EntrancePortal = World->SpawnActor<AColonyTravelPortal>(
        PortalLocation, PortalRotation);
    if (!EntrancePortal)
    {
        return false;
    }
    EntrancePortal->SetActorLabel(EntrancePortalLabel);
    EntrancePortal->SetFolderPath(TEXT("Gameplay/ColonyTravel"));
    EntrancePortal->ColonyId = 2;
    EntrancePortal->DestinationLevel = TSoftObjectPtr<UWorld>(
        FSoftObjectPath(InteriorMapObjectPath));
    EntrancePortal->bIsExitPortal = false;
    EntrancePortal->bRequiresProximity = true;
    EntrancePortal->InteractionRadius = 550.f;
    EntrancePortal->InteractionPrompt = FText::FromString(TEXT("COLONY 2: PRESS E TO ENTER"));

    World->MarkPackageDirty();
    return UEditorLoadingAndSavingUtils::SaveMap(World, FieldMapPath);
}

static void BuildTravelPrototype()
{
    if (!GEditor || GEditor->PlayWorld)
    {
        UE_LOG(LogTemp, Error, TEXT("COLONY_TRAVEL_BUILD unavailable during PIE"));
        return;
    }

    const bool bInteriorBuilt = CreateOrUpdateInteriorMap();
    const bool bEntranceInstalled = bInteriorBuilt && InstallFieldEntrance();
    UE_LOG(LogTemp, Display,
        TEXT("COLONY_TRAVEL_BUILD COMPLETE interior=%d entrance=%d interior_map=%s field_map=%s"),
        bInteriorBuilt,
        bEntranceInstalled,
        InteriorMapPath,
        FieldMapPath);

    if (FParse::Param(FCommandLine::Get(), TEXT("PrototypeColonyBatch")))
    {
        FPlatformMisc::RequestExit(false);
    }
}

static FAutoConsoleCommand BuildTravelCommand(
    TEXT("PrototypeColony.BuildTravelPrototype"),
    TEXT("Create Colony 2 interior and install the E-key travel entrance in the approved field map."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([](float)
            {
                BuildTravelPrototype();
                return false;
            }),
            2.f);
    }));
}

#endif
