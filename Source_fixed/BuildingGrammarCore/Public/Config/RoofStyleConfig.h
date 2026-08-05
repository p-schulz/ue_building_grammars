#pragma once

#include "CoreMinimal.h"
#include "RoofStyleConfig.generated.h"

UENUM(BlueprintType)
enum class EGrammarRoofType : uint8
{
	Flat UMETA(DisplayName = "Flat"),
	Gabled UMETA(DisplayName = "Gabled"),
	Hipped UMETA(DisplayName = "Hipped"),
	Pyramid UMETA(DisplayName = "Pyramid")
};

UENUM(BlueprintType)
enum class EGrammarRidgeAlignment : uint8
{
	ClosestStreet UMETA(DisplayName = "Closest Street"),
	LongestAxis UMETA(DisplayName = "Longest Axis")
};

// Port of config.py's RoofStyleConfig. Defaults match the Python dataclass exactly.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FRoofStyleConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof")
	EGrammarRoofType Type = EGrammarRoofType::Flat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof")
	double Height = 1.6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof")
	double Overhang = 0.25;

	// Only consulted for gabled roofs when no explicit OSM roof:orientation tag is present.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof")
	EGrammarRidgeAlignment RidgeAlignment = EGrammarRidgeAlignment::ClosestStreet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof")
	FString Material = TEXT("Grammar Roof");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof")
	FLinearColor Color = FLinearColor(0.34, 0.08, 0.06, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof")
	FString TexturePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof")
	double TextureScale = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Edge")
	bool bEdgeEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Edge")
	double EdgeWidth = 0.28;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Edge")
	double EdgeHeight = 0.35;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Edge")
	double SurfaceInset = 0.08;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Edge")
	FString EdgeMaterial = TEXT("Grammar Roof Edge");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Edge")
	FLinearColor EdgeColor = FLinearColor(0.22, 0.22, 0.2, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Edge")
	double CornerCapSize = 0.42;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Tiles")
	int32 TileRows = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Tiles")
	double TileDepth = 0.035;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Tiles")
	double TileSpacing = 0.55;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Tiles")
	FString TileMaterial = TEXT("Grammar Roof Tile Bands");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Tiles")
	FLinearColor TileColor = FLinearColor(0.28, 0.07, 0.045, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Dormers")
	int32 DormerCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Dormers")
	double DormerWidth = 1.35;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Dormers")
	double DormerDepth = 0.9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Dormers")
	double DormerHeight = 0.9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Dormers")
	FString DormerMaterial = TEXT("Grammar Dormer Cladding");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Dormers")
	FLinearColor DormerColor = FLinearColor(0.62, 0.58, 0.5, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Roof Windows")
	int32 RoofWindowCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Roof Windows")
	double RoofWindowWidth = 0.75;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Roof Windows")
	double RoofWindowHeight = 1.05;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Roof Windows")
	FString RoofWindowMaterial = TEXT("Grammar Roof Window Glass");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Roof Windows")
	FLinearColor RoofWindowColor = FLinearColor(0.08, 0.16, 0.2, 0.86);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Chimneys")
	int32 ChimneyCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Chimneys")
	double ChimneyWidth = 0.45;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Chimneys")
	double ChimneyDepth = 0.38;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Chimneys")
	double ChimneyHeight = 1.15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Chimneys")
	FString ChimneyMaterial = TEXT("Grammar Brick Chimney");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof|Chimneys")
	FLinearColor ChimneyColor = FLinearColor(0.42, 0.16, 0.1, 1.0);
};
