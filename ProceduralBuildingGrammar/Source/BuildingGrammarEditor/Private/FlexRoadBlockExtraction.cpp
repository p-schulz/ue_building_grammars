#include "FlexRoadBlockExtraction.h"
#include "BuildingGenerationLibrary.h"
#include "Parcel/GrammarBlockExtraction.h"
#include "FlexNetworkSubsystem.h"
#include "FlexRoadSegment.h"
#include "RoadTypeProfile.h"
#include "FlexNetworkTypes.h"
#include "Math/FlexBezierMath.h"
#include "Engine/World.h"

namespace
{
	// Secondary/Tertiary/Unclassified read as plausible "high street"/commercial-fronting roads;
	// Residential/Service read as ordinary residential streets. Motorway/Trunk/Primary get no hint
	// at all -- not a plausible parcel frontage (this plugin doesn't attempt to keep buildings off
	// arterial/highway frontage beyond that; block extraction still traces the block, it just won't
	// tag it either way).
	FString RoadDominanceLevelToTagHint(EFlexRoadDominanceLevel Level)
	{
		switch (Level)
		{
		case EFlexRoadDominanceLevel::Secondary:
		case EFlexRoadDominanceLevel::Tertiary:
		case EFlexRoadDominanceLevel::Unclassified:
			return TEXT("commercial");
		case EFlexRoadDominanceLevel::Residential:
		case EFlexRoadDominanceLevel::Service:
			return TEXT("residential");
		default:
			return FString();
		}
	}

	// FGrammarBlock::BoundingRoadIds carries one of the strings above (or empty) per bounding edge --
	// this picks whichever non-empty hint appears most often, i.e. "what kind of street does most of
	// this block actually front."
	FString MostCommonNonEmpty(const TArray<FString>& Values)
	{
		TMap<FString, int32> Counts;
		for (const FString& Value : Values)
		{
			if (!Value.IsEmpty())
			{
				Counts.FindOrAdd(Value)++;
			}
		}
		FString Best;
		int32 BestCount = 0;
		for (const TPair<FString, int32>& Pair : Counts)
		{
			if (Pair.Value > BestCount)
			{
				BestCount = Pair.Value;
				Best = Pair.Key;
			}
		}
		return Best;
	}
}

TArray<FGrammarBlockInput> FFlexRoadBlockExtraction::ExtractBlockInputsFromFlexNetwork(UWorld* World)
{
	TArray<FGrammarBlockInput> Result;
	if (!World)
	{
		return Result;
	}

	UFlexNetworkSubsystem* Subsystem = World->GetSubsystem<UFlexNetworkSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("FFlexRoadBlockExtraction::ExtractBlockInputsFromFlexNetwork: no UFlexNetworkSubsystem on this world -- returning zero blocks."));
		return Result;
	}

	constexpr double CmToM = 100.0;

	const int32 TotalSegmentCount = Subsystem->GetAllSegments().Num();
	int32 SkippedInvalidArcLengthTable = 0;
	int32 SkippedTooFewPoints = 0;

	TArray<FGrammarRoadPolyline> Polylines;
	Polylines.Reserve(TotalSegmentCount);

	for (const TPair<FFlexSegmentId, FFlexRoadSegment>& Pair : Subsystem->GetAllSegments())
	{
		const FFlexRoadSegment& Segment = Pair.Value;
		if (!Segment.ArcLengthTable.IsValid())
		{
			++SkippedInvalidArcLengthTable; // Not yet rebuilt (bDirty) or a genuinely zero-length segment -- nothing to tessellate.
			continue;
		}

		FGrammarRoadPolyline Polyline;
		Polyline.Points.Reserve(Segment.ArcLengthTable.Samples.Num());
		for (const FFlexArcLengthSample& Sample : Segment.ArcLengthTable.Samples)
		{
			const FVector WorldPos = FFlexBezierMath::Evaluate(Segment.Curve, Sample.T);
			// FlexNetwork nodes/curves have no separate meters-projection layer (unlike OSM's
			// lon/lat -> FLocalTangentPlaneProjection path) -- they're already plain UE world
			// centimeters, so this is the only conversion needed.
			Polyline.Points.Add(FVector2D(WorldPos.X, WorldPos.Y) / CmToM);
		}
		if (Polyline.Points.Num() < 2)
		{
			++SkippedTooFewPoints;
			continue;
		}

		if (Segment.Profile)
		{
			Polyline.InsetDistance = Segment.Profile->GetOuterExtent() / CmToM;
			Polyline.RoadId = RoadDominanceLevelToTagHint(Segment.Profile->RoadDominanceLevel);
		}

		Polylines.Add(MoveTemp(Polyline));
	}

	UE_LOG(LogTemp, Log, TEXT("FFlexRoadBlockExtraction::ExtractBlockInputsFromFlexNetwork: %d segment(s) on subsystem, %d skipped (invalid/not-yet-rebuilt ArcLengthTable), %d skipped (fewer than 2 tessellated points), %d road polyline(s) handed to ExtractBlocks."),
		TotalSegmentCount, SkippedInvalidArcLengthTable, SkippedTooFewPoints, Polylines.Num());

	const TArray<FGrammarBlock> Blocks = FGrammarBlockExtraction::ExtractBlocks(Polylines);
	Result.Reserve(Blocks.Num());
	for (int32 Index = 0; Index < Blocks.Num(); ++Index)
	{
		FGrammarBlockInput Input;
		Input.Boundary = Blocks[Index].Polygon;
		Input.BlockId = Index;
		Input.DominantRoadTagHint = MostCommonNonEmpty(Blocks[Index].BoundingRoadIds);
		Result.Add(MoveTemp(Input));
	}
	return Result;
}
