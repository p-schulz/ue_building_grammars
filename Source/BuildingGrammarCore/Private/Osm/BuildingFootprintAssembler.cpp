#include "Osm/BuildingFootprintAssembler.h"
#include "Geometry/GrammarGeometry2D.h"

namespace
{
	bool IsTruthyTagValue(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		const FString Lower = Value.ToLower();
		return Lower != TEXT("no") && Lower != TEXT("false") && Lower != TEXT("0");
	}

	bool HasTruthyBuildingTag(const TMap<FString, FString>& Tags)
	{
		if (const FString* Building = Tags.Find(TEXT("building")))
		{
			if (IsTruthyTagValue(*Building))
			{
				return true;
			}
		}
		if (const FString* Part = Tags.Find(TEXT("building:part")))
		{
			if (IsTruthyTagValue(*Part))
			{
				return true;
			}
		}
		return false;
	}

	bool IsBuildingPartTags(const TMap<FString, FString>& Tags)
	{
		const FString* Part = Tags.Find(TEXT("building:part"));
		return Part && IsTruthyTagValue(*Part);
	}

	// Stitches open node-id polylines into closed rings by matching shared endpoint node ids,
	// trying both forward and reversed continuation at each step. A relation's outer or inner
	// member set can require multiple stitched rings (multiple disjoint outer boundaries per
	// relation), each returned separately. Topologically invalid input (a polyline that never
	// closes) is still returned as a best-effort open ring rather than silently dropped.
	TArray<TArray<int64>> StitchPolylinesIntoRings(TArray<TArray<int64>> Polylines)
	{
		TArray<TArray<int64>> Rings;
		TArray<bool> Used;
		Used.Init(false, Polylines.Num());

		for (int32 StartIndex = 0; StartIndex < Polylines.Num(); ++StartIndex)
		{
			if (Used[StartIndex] || Polylines[StartIndex].Num() == 0)
			{
				continue;
			}
			Used[StartIndex] = true;
			TArray<int64> Ring = Polylines[StartIndex];

			bool bExtended = true;
			while (bExtended && (Ring.Num() < 2 || Ring.Last() != Ring[0]))
			{
				bExtended = false;
				for (int32 Index = 0; Index < Polylines.Num(); ++Index)
				{
					if (Used[Index] || Polylines[Index].Num() == 0)
					{
						continue;
					}
					const TArray<int64>& Candidate = Polylines[Index];
					if (Candidate[0] == Ring.Last())
					{
						for (int32 PointIndex = 1; PointIndex < Candidate.Num(); ++PointIndex)
						{
							Ring.Add(Candidate[PointIndex]);
						}
						Used[Index] = true;
						bExtended = true;
						break;
					}
					if (Candidate.Last() == Ring.Last())
					{
						for (int32 PointIndex = Candidate.Num() - 2; PointIndex >= 0; --PointIndex)
						{
							Ring.Add(Candidate[PointIndex]);
						}
						Used[Index] = true;
						bExtended = true;
						break;
					}
				}
			}

			if (Ring.Num() >= 4 && Ring.Last() == Ring[0])
			{
				Ring.Pop(EAllowShrinking::No);  // drop the duplicate closing vertex
			}
			if (Ring.Num() >= 3)
			{
				Rings.Add(MoveTemp(Ring));
			}
		}
		return Rings;
	}

	bool ResolveRing(const TArray<int64>& NodeIds, const TMap<int64, FOsmNode>& Nodes, TArray<FVector2D>& OutPoints)
	{
		OutPoints.Reset(NodeIds.Num());
		for (const int64 Id : NodeIds)
		{
			const FOsmNode* Node = Nodes.Find(Id);
			if (!Node)
			{
				return false;
			}
			OutPoints.Add(FVector2D(Node->Lon, Node->Lat));
		}
		return true;
	}
}

TArray<FBuildingFootprint> FBuildingFootprintAssembler::Assemble(const FOsmDocument& Document)
{
	TArray<FBuildingFootprint> Result;
	TSet<int64> ConsumedWayIds;

	for (const TPair<int64, FOsmRelation>& RelationPair : Document.Relations)
	{
		const FOsmRelation& Relation = RelationPair.Value;
		const FString* TypeTag = Relation.Tags.Find(TEXT("type"));
		if (!TypeTag || *TypeTag != TEXT("multipolygon") || !HasTruthyBuildingTag(Relation.Tags))
		{
			continue;
		}

		TArray<TArray<int64>> OuterPolylines;
		TArray<TArray<int64>> InnerPolylines;
		for (const FOsmRelationMember& Member : Relation.Members)
		{
			if (Member.Type != TEXT("way"))
			{
				continue;
			}
			const FOsmWay* Way = Document.Ways.Find(Member.Ref);
			if (!Way)
			{
				continue;
			}
			ConsumedWayIds.Add(Way->Id);
			if (Member.Role == TEXT("inner"))
			{
				InnerPolylines.Add(Way->NodeRefs);
			}
			else
			{
				OuterPolylines.Add(Way->NodeRefs);
			}
		}

		TArray<FBuildingFootprint> RelationFootprints;
		for (const TArray<int64>& RingIds : StitchPolylinesIntoRings(OuterPolylines))
		{
			TArray<FVector2D> Points;
			if (!ResolveRing(RingIds, Document.Nodes, Points) || Points.Num() < 3)
			{
				continue;
			}
			FBuildingFootprint Footprint;
			Footprint.OsmId = Relation.Id;
			Footprint.SourceType = TEXT("relation");
			Footprint.OuterRing = MoveTemp(Points);
			Footprint.Tags = Relation.Tags;
			Footprint.bIsBuildingPart = IsBuildingPartTags(Relation.Tags);
			RelationFootprints.Add(MoveTemp(Footprint));
		}

		for (const TArray<int64>& InnerRingIds : StitchPolylinesIntoRings(InnerPolylines))
		{
			TArray<FVector2D> InnerPoints;
			if (!ResolveRing(InnerRingIds, Document.Nodes, InnerPoints) || InnerPoints.Num() < 3)
			{
				continue;
			}
			int32 BestIndex = INDEX_NONE;
			double BestArea = TNumericLimits<double>::Max();
			for (int32 Index = 0; Index < RelationFootprints.Num(); ++Index)
			{
				if (!FGrammarGeometry2D::PointInRing(InnerPoints[0], RelationFootprints[Index].OuterRing))
				{
					continue;
				}
				const double Area = FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(RelationFootprints[Index].OuterRing));
				if (Area < BestArea)
				{
					BestArea = Area;
					BestIndex = Index;
				}
			}
			if (BestIndex != INDEX_NONE)
			{
				FGrammarRing Hole;
				Hole.Points = MoveTemp(InnerPoints);
				RelationFootprints[BestIndex].Holes.Add(MoveTemp(Hole));
			}
		}

		Result.Append(MoveTemp(RelationFootprints));
	}

	for (const TPair<int64, FOsmWay>& WayPair : Document.Ways)
	{
		const FOsmWay& Way = WayPair.Value;
		if (ConsumedWayIds.Contains(Way.Id) || !HasTruthyBuildingTag(Way.Tags))
		{
			continue;
		}

		TArray<FVector2D> Points;
		if (!ResolveRing(Way.NodeRefs, Document.Nodes, Points) || Points.Num() < 3)
		{
			continue;
		}

		FBuildingFootprint Footprint;
		Footprint.OsmId = Way.Id;
		Footprint.SourceType = TEXT("way");
		Footprint.OuterRing = MoveTemp(Points);
		Footprint.Tags = Way.Tags;
		Footprint.bIsBuildingPart = IsBuildingPartTags(Way.Tags);
		Result.Add(MoveTemp(Footprint));
	}

	return Result;
}
