#include "Osm/BuildingPartResolver.h"
#include "Geometry/GrammarGeometry2D.h"
#include "Grammar/GrammarPartTags.h"

namespace
{
	double DistanceToRingBoundary(const FVector2D& Point, const TArray<FVector2D>& Ring)
	{
		double Best = TNumericLimits<double>::Max();
		for (const FGrammarGeometry2D::FEdge& Edge : FGrammarGeometry2D::GetSegments(Ring))
		{
			Best = FMath::Min(Best, FMath::Sqrt(FGrammarGeometry2D::PointToSegmentDistanceSquared(Point, Edge.Start, Edge.End)));
		}
		return Best;
	}

	FString NormalizedTagValue(const FString& Value)
	{
		return Value.TrimStartAndEnd().ToLower();
	}

	// Explicit "which building do I belong to" hint on a part, tried in this priority order.
	FString ExplicitParentKey(const TMap<FString, FString>& PartTags)
	{
		for (const TCHAR* Key : { TEXT("building:parent"), TEXT("building:relation"), TEXT("parent"), TEXT("part_of"), TEXT("site") })
		{
			if (const FString* Value = PartTags.Find(Key))
			{
				const FString Normalized = NormalizedTagValue(*Value);
				if (!Normalized.IsEmpty())
				{
					return Normalized;
				}
			}
		}
		return FString();
	}

	// Identity strings a parent can be referred to by from a part's ExplicitParentKey.
	TArray<FString> ParentIdentityKeys(const FBuildingFootprint& Parent)
	{
		TArray<FString> Keys;
		Keys.Add(NormalizedTagValue(FString::Printf(TEXT("%s/%lld"), *Parent.SourceType, Parent.OsmId)));
		Keys.Add(NormalizedTagValue(FString::Printf(TEXT("%lld"), Parent.OsmId)));
		for (const TCHAR* Key : { TEXT("name"), TEXT("building:id"), TEXT("ref") })
		{
			if (const FString* Value = Parent.Tags.Find(Key))
			{
				const FString Normalized = NormalizedTagValue(*Value);
				if (!Normalized.IsEmpty())
				{
					Keys.Add(Normalized);
				}
			}
		}
		return Keys;
	}

	// Picks the smallest-area footprint among Candidates (most specific/nested parent wins),
	// -1 if Candidates is empty.
	int32 SmallestAreaIndex(const TArray<int32>& Candidates, const TArray<FBuildingFootprint>& Parents)
	{
		int32 BestIndex = INDEX_NONE;
		double BestArea = TNumericLimits<double>::Max();
		for (const int32 Index : Candidates)
		{
			const double Area = FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Parents[Index].OuterRing));
			if (Area < BestArea)
			{
				BestArea = Area;
				BestIndex = Index;
			}
		}
		return BestIndex;
	}

	int32 FindMatchingParent(const FBuildingFootprint& Part, const TArray<FBuildingFootprint>& Parents, double Tolerance)
	{
		const FString ExplicitKey = ExplicitParentKey(Part.Tags);
		if (!ExplicitKey.IsEmpty())
		{
			TArray<int32> Matches;
			for (int32 Index = 0; Index < Parents.Num(); ++Index)
			{
				if (ParentIdentityKeys(Parents[Index]).Contains(ExplicitKey))
				{
					Matches.Add(Index);
				}
			}
			if (Matches.Num() > 0)
			{
				return SmallestAreaIndex(Matches, Parents);
			}
		}

		const FVector2D Centroid = FGrammarGeometry2D::Centroid2D(Part.OuterRing);
		TArray<int32> Containing;
		for (int32 Index = 0; Index < Parents.Num(); ++Index)
		{
			if (FGrammarGeometry2D::PointInRing(Centroid, Parents[Index].OuterRing))
			{
				Containing.Add(Index);
			}
		}
		if (Containing.Num() > 0)
		{
			return SmallestAreaIndex(Containing, Parents);
		}

		if (Tolerance > 0.0)
		{
			TArray<int32> Near;
			for (int32 Index = 0; Index < Parents.Num(); ++Index)
			{
				if (DistanceToRingBoundary(Centroid, Parents[Index].OuterRing) <= Tolerance)
				{
					Near.Add(Index);
				}
			}
			if (Near.Num() > 0)
			{
				return SmallestAreaIndex(Near, Parents);
			}
		}

		return INDEX_NONE;
	}
}

TArray<FGrammarBuildingVolume> FBuildingPartResolver::Resolve(const TArray<FBuildingFootprint>& ProjectedFootprints, const FBuildingGrammarConfig& Config)
{
	TArray<FGrammarBuildingVolume> Volumes;

	if (!Config.bEnableBuildingParts)
	{
		Volumes.Reserve(ProjectedFootprints.Num());
		for (const FBuildingFootprint& Footprint : ProjectedFootprints)
		{
			FGrammarBuildingVolume Volume;
			Volume.Footprint = Footprint;
			Volume.SourceName = Footprint.StableSourceName();
			Volume.VolumeTags = Footprint.Tags;
			Volumes.Add(MoveTemp(Volume));
		}
		return Volumes;
	}

	TArray<FBuildingFootprint> Parents;
	TArray<FBuildingFootprint> Parts;
	for (const FBuildingFootprint& Footprint : ProjectedFootprints)
	{
		(Footprint.bIsBuildingPart ? Parts : Parents).Add(Footprint);
	}

	TArray<int32> MatchedParentIndex;
	MatchedParentIndex.SetNumUninitialized(Parts.Num());
	TSet<int32> ParentsWithMatchedParts;
	for (int32 PartIndex = 0; PartIndex < Parts.Num(); ++PartIndex)
	{
		const int32 ParentIndex = FindMatchingParent(Parts[PartIndex], Parents, Config.BuildingPartMatchTolerance);
		MatchedParentIndex[PartIndex] = ParentIndex;
		if (ParentIndex != INDEX_NONE)
		{
			ParentsWithMatchedParts.Add(ParentIndex);
		}
	}

	for (int32 ParentIndex = 0; ParentIndex < Parents.Num(); ++ParentIndex)
	{
		if (Config.bSkipParentFootprintsWithParts && ParentsWithMatchedParts.Contains(ParentIndex))
		{
			continue;
		}
		FGrammarBuildingVolume Volume;
		Volume.Footprint = Parents[ParentIndex];
		Volume.SourceName = Parents[ParentIndex].StableSourceName();
		Volume.VolumeTags = Parents[ParentIndex].Tags;
		Volumes.Add(MoveTemp(Volume));
	}

	for (int32 PartIndex = 0; PartIndex < Parts.Num(); ++PartIndex)
	{
		const FBuildingFootprint& Part = Parts[PartIndex];
		const int32 ParentIndex = MatchedParentIndex[PartIndex];

		TMap<FString, FString> MergedTags;
		if (ParentIndex != INDEX_NONE && Config.bInheritParentTagsForParts)
		{
			MergedTags = Parents[ParentIndex].Tags;
		}
		MergedTags.Append(Part.Tags);  // part's own tags win on conflict

		if (!MergedTags.Contains(TEXT("building")))
		{
			const FString* PartValue = MergedTags.Find(TEXT("building:part"));
			MergedTags.Add(TEXT("building"), PartValue ? *PartValue : TEXT("yes"));
		}
		MergedTags.Add(TEXT("grammar:is_building_part"), TEXT("yes"));

		FString ParentSourceName;
		if (ParentIndex != INDEX_NONE)
		{
			ParentSourceName = Parents[ParentIndex].StableSourceName();
			MergedTags.Add(TEXT("grammar:building_parent"), ParentSourceName);
		}

		const TPair<TMap<FString, FString>, double> VolumeTagsAndMinHeight = FGrammarPartTags::TagsForBuildingPartVolume(MergedTags, Config);

		FGrammarBuildingVolume Volume;
		Volume.Footprint = Part;
		Volume.SourceName = Part.StableSourceName();
		Volume.VolumeTags = VolumeTagsAndMinHeight.Key;
		Volume.MinHeight = VolumeTagsAndMinHeight.Value;
		Volume.bIsBuildingPart = true;
		Volume.ParentSourceName = ParentSourceName;
		Volumes.Add(MoveTemp(Volume));
	}

	return Volumes;
}
