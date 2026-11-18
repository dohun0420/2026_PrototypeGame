#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/TextureFactory.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialEditingLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/SavePackage.h"

namespace PrototypeGround
{
static const TCHAR* TargetMapPath = TEXT("/Game/Prototype/Maps/Lvl_ApprovedLayout");
static const TCHAR* AssetRoot = TEXT("/Game/Prototype/Environment/Ground");
static const TCHAR* GroundActorLabel = TEXT("OpenField_Ground_200m");
static const TCHAR* TrackActorPrefix = TEXT("GroundLook_Track_");

enum class EGroundTextureKind
{
    Color,
    Normal,
    Mask
};

static bool SaveAsset(UObject* Asset)
{
    if (!Asset) return false;
    UPackage* Package = Asset->GetOutermost();
    Package->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    const FString Filename = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    return UPackage::SavePackage(Package, Asset, *Filename, Args);
}

static void ConfigureTexture(UTexture2D* Texture, EGroundTextureKind Kind, bool bClamp)
{
    Texture->Modify();
    Texture->LODGroup = TEXTUREGROUP_World;
    Texture->MipGenSettings = TMGS_FromTextureGroup;
    Texture->AddressX = bClamp ? TA_Clamp : TA_Wrap;
    Texture->AddressY = bClamp ? TA_Clamp : TA_Wrap;
    Texture->SRGB = Kind == EGroundTextureKind::Color;
    Texture->CompressionSettings = Kind == EGroundTextureKind::Normal
        ? TC_Normalmap
        : (Kind == EGroundTextureKind::Mask ? TC_Masks : TC_Default);
    Texture->PostEditChange();
    SaveAsset(Texture);
}

static UTexture2D* ImportTexture(
    const FString& SourceFilename,
    const FString& DestinationPath,
    const FString& AssetName,
    EGroundTextureKind Kind,
    bool bClamp)
{
    const FString ObjectPath = DestinationPath / AssetName + TEXT(".") + AssetName;
    if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, *ObjectPath))
    {
        ConfigureTexture(Existing, Kind, bClamp);
        return Existing;
    }

    if (!FPaths::FileExists(SourceFilename))
    {
        UE_LOG(LogTemp, Error, TEXT("GROUND_STYLE missing source texture: %s"), *SourceFilename);
        return nullptr;
    }

    UTextureFactory* Factory = NewObject<UTextureFactory>();
    Factory->bCreateMaterial = false;
    Factory->CompressionSettings = Kind == EGroundTextureKind::Normal
        ? TC_Normalmap
        : (Kind == EGroundTextureKind::Mask ? TC_Masks : TC_Default);
    Factory->ColorSpaceMode = Kind == EGroundTextureKind::Color
        ? ETextureSourceColorSpace::SRGB
        : ETextureSourceColorSpace::Linear;
    Factory->LODGroup = TEXTUREGROUP_World;
    Factory->MipGenSettings = TMGS_FromTextureGroup;

    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    Task->Filename = SourceFilename;
    Task->DestinationPath = DestinationPath;
    Task->DestinationName = AssetName;
    Task->bAutomated = true;
    Task->bAsync = false;
    Task->bReplaceExisting = false;
    Task->bReplaceExistingSettings = false;
    Task->bSave = false;
    Task->Factory = Factory;

    TArray<UAssetImportTask*> Tasks{Task};
    FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    AssetTools.Get().ImportAssetTasks(Tasks);

    UTexture2D* Imported = nullptr;
    for (UObject* Object : Task->GetObjects())
    {
        Imported = Cast<UTexture2D>(Object);
        if (Imported) break;
    }
    if (!Imported) Imported = LoadObject<UTexture2D>(nullptr, *ObjectPath);
    if (!Imported)
    {
        UE_LOG(LogTemp, Error, TEXT("GROUND_STYLE failed to import: %s"), *SourceFilename);
        return nullptr;
    }

    ConfigureTexture(Imported, Kind, bClamp);
    UE_LOG(LogTemp, Display, TEXT("GROUND_STYLE imported %s"), *Imported->GetPathName());
    return Imported;
}

template<typename TExpression>
static TExpression* Node(UMaterial* Material, int32 X, int32 Y)
{
    return CastChecked<TExpression>(UMaterialEditingLibrary::CreateMaterialExpression(
        Material, TExpression::StaticClass(), X, Y));
}

static UMaterialExpressionScalarParameter* Scalar(
    UMaterial* Material,
    const TCHAR* Name,
    float Value,
    int32 X,
    int32 Y,
    float SliderMin,
    float SliderMax,
    int32 SortPriority)
{
    UMaterialExpressionScalarParameter* Result = Node<UMaterialExpressionScalarParameter>(Material, X, Y);
    Result->ParameterName = Name;
    Result->DefaultValue = Value;
    Result->Group = TEXT("Ground Look");
    Result->SortPriority = SortPriority;
    Result->SliderMin = SliderMin;
    Result->SliderMax = SliderMax;
    return Result;
}

static UMaterialExpressionTextureSampleParameter2D* TextureSample(
    UMaterial* Material,
    UTexture2D* Texture,
    const TCHAR* Name,
    int32 X,
    int32 Y,
    int32 SortPriority)
{
    UMaterialExpressionTextureSampleParameter2D* Result =
        CastChecked<UMaterialExpressionTextureSampleParameter2D>(
            UMaterialEditingLibrary::CreateMaterialExpressionEx(
                Material,
                nullptr,
                UMaterialExpressionTextureSampleParameter2D::StaticClass(),
                Texture,
                X,
                Y));
    Result->Texture = Texture;
    Result->ParameterName = Name;
    Result->Group = TEXT("Textures");
    Result->SortPriority = SortPriority;
    Result->AutoSetSampleType();
    return Result;
}

static void Connect(
    UMaterialExpression* From,
    const TCHAR* FromOutput,
    UMaterialExpression* To,
    const TCHAR* ToInput)
{
    if (!UMaterialEditingLibrary::ConnectMaterialExpressions(From, FromOutput, To, ToInput))
    {
        UE_LOG(LogTemp, Error, TEXT("GROUND_STYLE graph connection failed: %s.%s -> %s.%s"),
            *GetNameSafe(From), FromOutput, *GetNameSafe(To), ToInput);
    }
}

static void ConnectProperty(UMaterialExpression* From, const TCHAR* Output, EMaterialProperty Property)
{
    if (!UMaterialEditingLibrary::ConnectMaterialProperty(From, Output, Property))
    {
        UE_LOG(LogTemp, Error, TEXT("GROUND_STYLE material property connection failed: %s -> %d"),
            *GetNameSafe(From), static_cast<int32>(Property));
    }
}

static UMaterial* FindOrCreateMaterial(const FString& PackageName, const FString& AssetName)
{
    const FString ObjectPath = PackageName + TEXT(".") + AssetName;
    if (UMaterial* Existing = LoadObject<UMaterial>(nullptr, *ObjectPath))
    {
        Existing->Modify();
        UMaterialEditingLibrary::DeleteAllMaterialExpressions(Existing);
        return Existing;
    }

    UPackage* Package = CreatePackage(*PackageName);
    UMaterial* Result = NewObject<UMaterial>(Package, *AssetName, RF_Public | RF_Standalone);
    FAssetRegistryModule::AssetCreated(Result);
    return Result;
}

static bool CompileAndSaveMaterial(UMaterial* Material)
{
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    const TArray<FString> Errors = UMaterialEditingLibrary::RecompileMaterial(Material);
    for (const FString& Error : Errors)
    {
        UE_LOG(LogTemp, Error, TEXT("GROUND_STYLE material error in %s: %s"),
            *Material->GetName(), *Error);
    }
    return Errors.IsEmpty() && SaveAsset(Material);
}

static UMaterial* CreateSandMaterial(
    UTexture2D* BaseColor,
    UTexture2D* Normal,
    UTexture2D* Roughness,
    UTexture2D* AO)
{
    const FString AssetName = TEXT("M_SandGround");
    const FString PackageName = FString(AssetRoot) / TEXT("Materials") / AssetName;
    UMaterial* Material = FindOrCreateMaterial(PackageName, AssetName);
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);

    UMaterialExpressionWorldPosition* WorldPosition = Node<UMaterialExpressionWorldPosition>(Material, -1200, 0);
    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    UMaterialExpressionComponentMask* WorldXY = Node<UMaterialExpressionComponentMask>(Material, -1020, 0);
    WorldXY->R = true;
    WorldXY->G = true;
    // These inputs use generated/shortened pin names in UE 5.8, so connect them directly.
    WorldXY->Input.Connect(0, WorldPosition);

    UMaterialExpressionScalarParameter* DetailScale = Scalar(
        Material, TEXT("Detail Scale (tiles per cm)"), 0.005f, -1020, -250, 0.001f, 0.02f, 0);
    UMaterialExpressionMultiply* DetailUV = Node<UMaterialExpressionMultiply>(Material, -780, -30);
    Connect(WorldXY, TEXT(""), DetailUV, TEXT("A"));
    Connect(DetailScale, TEXT(""), DetailUV, TEXT("B"));

    UMaterialExpressionScalarParameter* MacroScale = Scalar(
        Material, TEXT("Macro Scale (tiles per cm)"), 0.00055f, -1020, 330, 0.0001f, 0.002f, 1);
    UMaterialExpressionMultiply* MacroUV = Node<UMaterialExpressionMultiply>(Material, -780, 300);
    Connect(WorldXY, TEXT(""), MacroUV, TEXT("A"));
    Connect(MacroScale, TEXT(""), MacroUV, TEXT("B"));

    UMaterialExpressionTextureSampleParameter2D* DetailColor = TextureSample(
        Material, BaseColor, TEXT("Sand Base Color"), -520, -100, 0);
    UMaterialExpressionTextureSampleParameter2D* MacroColor = TextureSample(
        Material, BaseColor, TEXT("Sand Macro Source"), -520, 300, 1);
    DetailColor->Coordinates.Connect(0, DetailUV);
    MacroColor->Coordinates.Connect(0, MacroUV);

    UMaterialExpressionScalarParameter* MacroStrength = Scalar(
        Material, TEXT("Macro Variation Strength"), 0.22f, -240, 370, 0.f, 0.5f, 2);
    UMaterialExpressionMultiply* MacroContrast = Node<UMaterialExpressionMultiply>(Material, 0, 280);
    Connect(MacroColor, TEXT("R"), MacroContrast, TEXT("A"));
    Connect(MacroStrength, TEXT(""), MacroContrast, TEXT("B"));
    UMaterialExpressionScalarParameter* MacroBias = Scalar(
        Material, TEXT("Macro Variation Bias"), 0.89f, -240, 520, 0.5f, 1.2f, 3);
    UMaterialExpressionAdd* MacroFactor = Node<UMaterialExpressionAdd>(Material, 210, 280);
    Connect(MacroContrast, TEXT(""), MacroFactor, TEXT("A"));
    Connect(MacroBias, TEXT(""), MacroFactor, TEXT("B"));

    UMaterialExpressionMultiply* VariedColor = Node<UMaterialExpressionMultiply>(Material, 210, -50);
    Connect(DetailColor, TEXT("RGB"), VariedColor, TEXT("A"));
    Connect(MacroFactor, TEXT(""), VariedColor, TEXT("B"));

    UMaterialExpressionVectorParameter* SandTint = Node<UMaterialExpressionVectorParameter>(Material, 210, -230);
    SandTint->ParameterName = TEXT("Sand Tint");
    SandTint->DefaultValue = FLinearColor(1.00f, 0.96f, 0.90f, 1.f);
    SandTint->Group = TEXT("Ground Look");
    SandTint->SortPriority = 4;
    UMaterialExpressionMultiply* FinalColor = Node<UMaterialExpressionMultiply>(Material, 470, -50);
    Connect(VariedColor, TEXT(""), FinalColor, TEXT("A"));
    Connect(SandTint, TEXT("RGB"), FinalColor, TEXT("B"));

    UMaterialExpressionTextureSampleParameter2D* NormalSample = TextureSample(
        Material, Normal, TEXT("Sand Normal"), -420, -430, 2);
    UMaterialExpressionTextureSampleParameter2D* RoughnessSample = TextureSample(
        Material, Roughness, TEXT("Sand Roughness"), -420, -650, 3);
    UMaterialExpressionTextureSampleParameter2D* AOSample = TextureSample(
        Material, AO, TEXT("Sand Ambient Occlusion"), -420, -870, 4);
    NormalSample->Coordinates.Connect(0, DetailUV);
    RoughnessSample->Coordinates.Connect(0, DetailUV);
    AOSample->Coordinates.Connect(0, DetailUV);

    UMaterialExpressionSubstrateSlabBSDF* SandSlab =
        Node<UMaterialExpressionSubstrateSlabBSDF>(Material, 760, -100);
    SandSlab->SubSurfaceType = EMaterialSubSurfaceType::MSS_None;
    SandSlab->DiffuseAlbedo.Connect(0, FinalColor);
    SandSlab->Roughness.Connect(1, RoughnessSample);
    SandSlab->Normal.Connect(0, NormalSample);
    ConnectProperty(SandSlab, TEXT(""), MP_FrontMaterial);
    ConnectProperty(AOSample, TEXT("R"), MP_AmbientOcclusion);

    return CompileAndSaveMaterial(Material) ? Material : nullptr;
}

static UMaterialInstanceConstant* CreateMaterialInstance(
    const TCHAR* AssetName,
    UMaterial* Parent)
{
    const FString PackageName = FString(AssetRoot) / TEXT("Materials") / AssetName;
    const FString ObjectPath = PackageName + TEXT(".") + AssetName;
    UMaterialInstanceConstant* Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *ObjectPath);
    if (!Instance)
    {
        Instance = NewObject<UMaterialInstanceConstant>(
            CreatePackage(*PackageName), AssetName, RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Instance);
    }
    Instance->Modify();
    Instance->ClearParameterValuesEditorOnly();
    Instance->SetParentEditorOnly(Parent);
    Instance->PostEditChange();
    return SaveAsset(Instance) ? Instance : nullptr;
}

static int32 RemoveTrackDecals(UWorld* World)
{
    TArray<AActor*> ExistingTracks;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel().StartsWith(TrackActorPrefix)) ExistingTracks.Add(*It);
    }
    for (AActor* Actor : ExistingTracks)
    {
        Actor->Modify();
        Actor->Destroy();
    }
    return ExistingTracks.Num();
}

static bool Apply()
{
    if (!GEditor || GEditor->PlayWorld)
    {
        UE_LOG(LogTemp, Error, TEXT("GROUND_STYLE requires an idle editor world"));
        return false;
    }

    const FString Downloads = FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT("Downloads"));
    const FString SandSource = Downloads / TEXT("sand_sgfodj1a_2k");
    const FString SandDestination = FString(AssetRoot) / TEXT("Textures/Sand");

    UTexture2D* SandBaseColor = ImportTexture(
        SandSource / TEXT("Sand_sgfodj1a_2K_BaseColor.jpg"), SandDestination,
        TEXT("T_Sand_BaseColor"), EGroundTextureKind::Color, false);
    UTexture2D* SandNormal = ImportTexture(
        SandSource / TEXT("Sand_sgfodj1a_2K_Normal.jpg"), SandDestination,
        TEXT("T_Sand_Normal"), EGroundTextureKind::Normal, false);
    UTexture2D* SandRoughness = ImportTexture(
        SandSource / TEXT("Sand_sgfodj1a_2K_Roughness.jpg"), SandDestination,
        TEXT("T_Sand_Roughness"), EGroundTextureKind::Mask, false);
    UTexture2D* SandAO = ImportTexture(
        SandSource / TEXT("Sand_sgfodj1a_2K_AO.jpg"), SandDestination,
        TEXT("T_Sand_AO"), EGroundTextureKind::Mask, false);

    if (!SandBaseColor || !SandNormal || !SandRoughness || !SandAO)
    {
        UE_LOG(LogTemp, Error, TEXT("GROUND_STYLE aborted because one or more sand textures are unavailable"));
        return false;
    }
    UMaterial* SandMaterial = CreateSandMaterial(SandBaseColor, SandNormal, SandRoughness, SandAO);
    if (!SandMaterial) return false;

    UMaterialInstanceConstant* SandInstance = CreateMaterialInstance(TEXT("MI_SandGround"), SandMaterial);
    if (!SandInstance) return false;

    UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(TargetMapPath);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("GROUND_STYLE could not load %s"), TargetMapPath);
        return false;
    }

    AStaticMeshActor* Ground = nullptr;
    int32 GroundMatches = 0;
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == GroundActorLabel)
        {
            Ground = *It;
            ++GroundMatches;
        }
    }
    if (GroundMatches != 1 || !Ground)
    {
        UE_LOG(LogTemp, Error, TEXT("GROUND_STYLE expected one %s actor; found %d"),
            GroundActorLabel, GroundMatches);
        return false;
    }

    Ground->Modify();
    UStaticMeshComponent* GroundMesh = Ground->GetStaticMeshComponent();
    GroundMesh->Modify();
    GroundMesh->SetMaterial(0, SandInstance);
    Ground->PostEditMove(true);
    UE_LOG(LogTemp, Display, TEXT("GROUND_STYLE ground slot0=%s"),
        *GetNameSafe(GroundMesh->GetMaterial(0)));

    const int32 RemovedTracks = RemoveTrackDecals(World);

    World->MarkPackageDirty();
    const bool bSaved = UEditorLoadingAndSavingUtils::SaveMap(World, TargetMapPath);
    UE_LOG(LogTemp, Display,
        TEXT("GROUND_STYLE COMPLETE success=%d ground=%s material=%s tracks=0 removed_tracks=%d map=%s"),
        bSaved, GroundActorLabel, *SandInstance->GetPathName(), RemovedTracks, TargetMapPath);
    return bSaved;
}

static FAutoConsoleCommand ApplyCommand(
    TEXT("PrototypeGround.Apply"),
    TEXT("Import the approved sand texture set, apply it to the ground, and remove tire-track decals."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
        {
            Apply();
            if (FParse::Param(FCommandLine::Get(), TEXT("PrototypeGroundBatch")))
            {
                FPlatformMisc::RequestExit(false);
            }
            return false;
        }), 2.f);
    }));

}

#endif
