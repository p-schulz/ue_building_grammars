#include "Osm/StreetRidgeAlignment.h"
#include "Geometry/GrammarGeometry2D.h"

namespace
{
	FString NormalizedStreetName(const FString& Value)
	{
		return Value.TrimStartAndEnd().ToLower();
	}

	bool HasExplicitRidgeDirectionTag(const TMap<FString, FString>& Tags)
	{
		for (const TCHAR* Key : { TEXT("roof:orientation"), TEXT("roof:direction"), TEXT("grammar:roof:ridge_direction"), TEXT("roof:ridge:direction") })
		{
			if (const FString* Value = Tags.Find(Key))
			{
				if (!Value->TrimStartAndEnd().IsEmpty())
				{
					return true;
				}
			}
		}
		return false;
	}

	// Nearest single line segment (not just polyline endpoints) across every candidate street, with
	// that segment's own tangent as the result -- correct even for a curving street, since only the
	// segment nearest the building actually matters for "parallel to the street here".
	bool NearestSegmentDirection(const FVector2D& Point, const TArray<const FGrammarStreetSegment*>& Candidates, double& OutDistance, FVector2D& OutDirection)
	{
		bool bFound = false;
		double BestDistSq = TNumericLimits<double>::Max();
		FVector2D BestEdge = FVector2D::ZeroVector;

		for (const FGrammarStreetSegment* Segment : Candidates)
		{
			for (int32 Index = 0; Index + 1 < Segment->Points.Num(); ++Index)
			{
				const FVector2D& Start = Segment->Points[Index];
				const FVector2D& End = Segment->Points[Index + 1];
				const double DistSq = FGrammarGeometry2D::PointToSegmentDistanceSquared(Point, Start, End);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestEdge = End - Start;
					bFound = true;
				}
			}
		}

		if (!bFound)
		{
			return false;
		}

		OutDirection = FGrammarGeometry2D::Normalize2D(BestEdge);
		if (OutDirection.IsNearlyZero())
		{
			return false;
		}
		OutDistance = FMath::Sqrt(BestDistSq);
		return true;
	}
}

bool FGrammarStreetAlignment::ConfigNeedsStreetAlignment(const FBuildingGrammarConfig& Config)
{
	if (Config.Roof.RidgeAlignment == EGrammarRidgeAlignment::ClosestStreet)
	{
		return true;
	}
	for (const FFacadeStyleConfig& Style : Config.Styles)
	{
		if (Style.bOverrideRoof && Style.RoofOverride.RidgeAlignment == EGrammarRidgeAlignment::ClosestStreet)
		{
			return true;
		}
	}
	return false;
}

void FGrammarStreetAlignment::ApplyRidgeDirectionTags(TArray<FGrammarBuildingVolume>& Volumes, const TArray<FGrammarStreetSegment>& ProjectedStreets, double SearchRadius)
{
	if (ProjectedStreets.Num() == 0)
	{
		return;
	}

	TArray<const FGrammarStreetSegment*> AllStreets;
	AllStreets.Reserve(ProjectedStreets.Num());
	for (const FGrammarStreetSegment& Segment : ProjectedStreets)
	{
		AllStreets.Add(&Segment);
	}

	for (FGrammarBuildingVolume& Volume : Volumes)
	{
		if (HasExplicitRidgeDirectionTag(Volume.VolumeTags))
		{
			continue;
		}

		const FVector2D Centroid = FGrammarGeometry2D::Centroid2D(Volume.Footprint.OuterRing);

		// An addr:street match is a stronger signal than raw proximity -- e.g. a building set back
		// from its own street behind a courtyard or driveway -- so it isn't constrained by
		// SearchRadius the way the no-address proximity fallback below is.
		if (const FString* AddrStreet = Volume.VolumeTags.Find(TEXT("addr:street")))
		{
			if (!AddrStreet->IsEmpty())
			{
				const FString Normalized = NormalizedStreetName(*AddrStreet);
				TArray<const FGrammarStreetSegment*> Matching;
				for (const FGrammarStreetSegment* Segment : AllStreets)
				{
					if (!Segment->Name.IsEmpty() && NormalizedStreetName(Segment->Name) == Normalized)
					{
						Matching.Add(Segment);
					}
				}

				double MatchDistance = 0.0;
				FVector2D MatchDirection;
				if (Matching.Num() > 0 && NearestSegmentDirection(Centroid, Matching, MatchDistance, MatchDirection))
				{
					Volume.VolumeTags.Add(TEXT("grammar:roof:ridge_direction"), FString::Printf(TEXT("%f,%f"), MatchDirection.X, MatchDirection.Y));
					continue;
				}
			}
		}

		double Distance = 0.0;
		FVector2D Direction;
		if (NearestSegmentDirection(Centroid, AllStreets, Distance, Direction) && Distance <= SearchRadius)
		{
			Volume.VolumeTags.Add(TEXT("grammar:roof:ridge_direction"), FString::Printf(TEXT("%f,%f"), Direction.X, Direction.Y));
		}
	}
}
