#pragma once

#include "CoreMinimal.h"
#include "GrammarParcelTypes.generated.h"

// Which of the four subdivision strategies FGrammarParcelSubdivision::Subdivide uses to carve a
// block boundary into parcels -- see GrammarParcelSubdivision.h for what each one actually does.
// Ported from the user-supplied parcel_subdivision_algorithms.hpp reference implementation (itself
// a dependency-free approximation of Vanegas et al.-style parcel subdivision).
UENUM(BlueprintType)
enum class EGrammarParcelSubdivisionMethod : uint8
{
	// Recursive oriented-bounding-box splitting until area/width/depth constraints are satisfied.
	// The only one of the four that always fully partitions its input with no separate fallback --
	// used as the fallback target for the other three when their own inner-contour/strip generation
	// can't be validated.
	Obb UMETA(DisplayName = "OBB Recursive Split"),
	// Frontage strips between the block boundary and a small collapsed inner contour -- approximates
	// the reference paper's d_offset -> infinity case (rear lot lines collapse toward the interior
	// rather than a single point).
	SkeletonNoOffset UMETA(DisplayName = "Skeleton (No Offset)"),
	// Frontage strips between the block boundary and a larger inner contour, blending a
	// depth-driven scale (Config.PerimeterDepth) with Config.OffsetRatio -- optionally emits the
	// leftover inner region as a "patio" parcel (no street access, not built on).
	SkeletonWithOffset UMETA(DisplayName = "Skeleton (With Offset)"),
	// SkeletonWithOffset's perimeter frontage strips, then OBB-subdivides whatever inner region is
	// left over instead of leaving it as one big patio parcel.
	Hybrid UMETA(DisplayName = "Hybrid (Perimeter + Inner OBB)")
};

// Direct port of parcel_subdivision_algorithms.hpp's Config struct -- same fields, same defaults,
// same meaning. See GrammarParcelSubdivision.h for how each field is actually used.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FGrammarParcelConfig
{
	GENERATED_BODY()

	// Square meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Subdivision", meta = (ClampMin = "1.0"))
	double MinArea = 1250.0;

	// Square meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Subdivision", meta = (ClampMin = "1.0"))
	double MaxArea = 4400.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Subdivision", meta = (ClampMin = "1.0", Units = "m"))
	double MinWidth = 28.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Subdivision", meta = (ClampMin = "1.0", Units = "m"))
	double MaxWidth = 58.0;

	// Target frontage-strip depth for the skeleton variants (SkeletonNoOffset/SkeletonWithOffset/
	// Hybrid); ignored by Obb.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Subdivision", meta = (ClampMin = "1.0", Units = "m"))
	double PerimeterDepth = 88.0;

	// 0..1. How much random jitter is applied to OBB split positions and frontage-strip widths.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Subdivision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Irregularity = 0.24;

	// 0..1. When an OBB split would leave one side with no street frontage, this is the probability
	// of retrying the split along the other axis instead (favoring street-accessible parcels).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Subdivision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double StreetAccessPreference = 0.72;

	// 0..1. Only used by SkeletonWithOffset/Hybrid -- blended with a depth-driven scale to produce
	// the inner contour's final scale factor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Subdivision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double OffsetRatio = 0.35;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Subdivision", meta = (ClampMin = "1"))
	int32 MaxDepth = 9;

	// Maximum long:short OBB-side ratio a parcel may keep without being forced to split further,
	// independent of area/width -- ShouldSplitObb (GrammarParcelSubdivision.cpp) previously only
	// checked the SHORT obb side against MaxWidth, so a naturally-narrow block that was also long
	// (short side already under MaxWidth, total area under MaxArea) could pass every existing check
	// and stay as one giant unsplit sliver spanning almost the whole block -- confirmed against a real
	// generated building doing exactly that with a chaotic multi-hip roof as a result (many short roof
	// facets from a straight-skeleton roof applied to a very elongated footprint). Also loosely clamps
	// the skeleton frontage-strip methods' per-strip width relative to local depth (GenerateFrontageStrips)
	// for the same reason. Lower values push toward squarer parcels; 1.0 pushes hardest toward square.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Subdivision", meta = (ClampMin = "1.0"))
	double MaxAspectRatio = 2.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Subdivision")
	int32 Seed = 4127;
};

// One generated parcel -- mirrors parcel_subdivision_algorithms.hpp's Parcel struct. Polygon is
// meters, matching BuildingGrammarCore's working space throughout (same convention as
// FBuildingFootprint::OuterRing once projected).
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FGrammarParcel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel")
	int32 Id = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel")
	int32 BlockId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel")
	TArray<FVector2D> Polygon;

	// "obb" / "skeleton-no-offset" / "skeleton-offset" / "hybrid-inner-obb" / "patio" / any
	// "*-fallback-obb" variant -- mirrors the reference implementation's plain string tag exactly,
	// since it's diagnostic/debug information, not something callers branch on except by exact
	// string match (patio parcels specifically, see GenerateBuildingsFromBlocks).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel")
	FString Method;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel", meta = (Units = "m"))
	double Frontage = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel")
	bool bStreetAccess = false;

	// Comma-separated constraint violations (area/width/no street access) -- empty means no warning.
	// "unsplit" means an OBB split couldn't be validated and this parcel was left as-is, oversized.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel")
	FString Warning;
};

// One debug line (split ray, rejected frontage strip, ...) -- purely for an eventual viewport
// visualization, never consumed by generation itself.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FGrammarParcelDebugLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Debug")
	FVector2D A = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Debug")
	FVector2D B = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Debug")
	FString Kind;
};

// One debug polygon (an OBB box, a collapsed/offset inner contour) -- same purely-diagnostic role
// as FGrammarParcelDebugLine.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FGrammarParcelDebugPolygon
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Debug")
	TArray<FVector2D> Polygon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Debug")
	FString Kind;
};

USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FGrammarParcelSubdivisionResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel")
	TArray<FGrammarParcel> Parcels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Debug")
	TArray<FGrammarParcelDebugLine> Lines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Debug")
	TArray<FGrammarParcelDebugPolygon> Polygons;
};

// One road-network block's boundary plus everything FGrammarParcelSubdivision::Subdivide produced
// for it, kept around purely for an eventual viewport visualization (e.g. FBuildingPickEdMode::
// Render) -- never consumed by generation itself, same role FGrammarParcelDebugLine/DebugPolygon
// already play at the single-block level. Optionally captured by
// UBuildingGenerationLibrary::GenerateBuildingsFromBlocks (BuildingGrammarRuntime) via an out-param,
// since that's the only place a block's boundary and its Subdivide() result are ever both in scope
// at once.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FGrammarBlockDebugData
{
	GENERATED_BODY()

	// Meters, closed ring, no repeated first point -- same convention as FGrammarBlock::Polygon.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Debug")
	TArray<FVector2D> BlockBoundary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Debug")
	int32 BlockId = INDEX_NONE;

	// Mirrors FGrammarBlockInput::DominantRoadTagHint -- kept here so a later single-block regenerate
	// (e.g. an interactive pick tool) can fully reconstruct that struct from just this debug data,
	// without re-running block extraction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Debug")
	FString DominantRoadTagHint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Debug")
	FGrammarParcelSubdivisionResult Subdivision;

	// UE world centimeters, sampled once per block (a single ground trace at its centroid) at
	// generation time -- the subdivision geometry itself is flat 2D meters with no elevation, so this
	// is what lets a later viewport draw flatten it onto roughly the right height instead of Z=0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Debug")
	double WorldZ = 0.0;
};
