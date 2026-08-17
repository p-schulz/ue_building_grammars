#include "PCGBuildingGrammarDefaults.h"
#include "Config/BuildingGrammarConfig.h"
#include "Config/GrammarConfigJson.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool LoadDefaultGermanBuildingGrammarConfig(FBuildingGrammarConfig& OutConfig)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ProceduralBuildingGrammar"));
	if (!Plugin.IsValid())
	{
		return false;
	}

	const FString FilePath = Plugin->GetContentDir() / TEXT("german_building_grammar_config.json");
	FString LoadError;
	if (!FGrammarConfigJson::LoadConfigFromPythonJsonFile(FilePath, OutConfig, LoadError))
	{
		UE_LOG(LogTemp, Warning, TEXT("BuildingGrammarPCG: failed to load default style config '%s': %s"), *FilePath, *LoadError);
		return false;
	}

	return true;
}

FString ExtractSourceNameFromTags(const TSet<FString>& Tags)
{
	static const FString Prefix = TEXT("SourceName:");
	for (const FString& Tag : Tags)
	{
		if (Tag.StartsWith(Prefix))
		{
			return Tag.Mid(Prefix.Len());
		}
	}
	return FString();
}

TMap<FString, FString> DeserializeTagsFromJson(const FString& Json)
{
	TMap<FString, FString> Tags;
	if (Json.IsEmpty())
	{
		return Tags;
	}
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : JsonObject->Values)
		{
			Tags.Add(Pair.Key, Pair.Value->AsString());
		}
	}
	return Tags;
}

FString ExtractStreetNameFromTags(const TSet<FString>& Tags)
{
	static const FString Prefix = TEXT("Name:");
	for (const FString& Tag : Tags)
	{
		if (Tag.StartsWith(Prefix))
		{
			return Tag.Mid(Prefix.Len());
		}
	}
	return FString();
}

namespace
{
	// Every footprint edge's midpoint, in world-space UE centimeters -- the point actually compared
	// against street geometry (an edge's own nearest-street distance is a more direct measure of
	// "does this edge face a street" than going through the building's centroid the way
	// FGrammarStreetAlignment does for its own, different, ridge-direction purpose).
	FVector EdgeMidpoint(const FPCGPoint& EdgePoint, const FPCGMetadataAttribute<double>* LengthAttr)
	{
		const FVector Start = EdgePoint.Transform.GetLocation();
		const FVector Tangent = EdgePoint.Transform.GetRotation().GetAxisX();
		const double Length = LengthAttr->GetValueFromItemKey(EdgePoint.MetadataEntry);
		return Start + Tangent * (Length * 0.5);
	}

	// Nearest EdgePoints index to the nearest of Candidates, or INDEX_NONE if Candidates is empty.
	int32 NearestEdgeToSegments(const TArray<FPCGPoint>& EdgePoints, const FPCGMetadataAttribute<double>* LengthAttr, const TArray<const FStreetSegment*>& Candidates, double& OutDistance)
	{
		int32 BestIndex = INDEX_NONE;
		double BestDistSq = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index < EdgePoints.Num(); ++Index)
		{
			const FVector Mid = EdgeMidpoint(EdgePoints[Index], LengthAttr);
			for (const FStreetSegment* Segment : Candidates)
			{
				const double DistSq = FMath::PointDistToSegmentSquared(Mid, Segment->Start, Segment->End);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestIndex = Index;
				}
			}
		}
		OutDistance = FMath::Sqrt(BestDistSq);
		return BestIndex;
	}
}

int32 DetermineStreetFacingSideIndex(const TMap<FString, FString>& Tags, const TArray<FPCGPoint>& EdgePoints, const FPCGMetadataAttribute<double>* LengthAttr, const TArray<FStreetSegment>& StreetSegments, double SearchRadius)
{
	FString PointValue;
	if (const FString* P1 = Tags.Find(TEXT("grammar:street:point")))
	{
		PointValue = *P1;
	}
	else if (const FString* P2 = Tags.Find(TEXT("grammar:street_point")))
	{
		PointValue = *P2;
	}

	if (!PointValue.IsEmpty())
	{
		TArray<FString> Parts;
		PointValue.Replace(TEXT(";"), TEXT(",")).ParseIntoArray(Parts, TEXT(","), true);
		if (Parts.Num() >= 2)
		{
			const FString XStr = Parts[0].TrimStartAndEnd();
			const FString YStr = Parts[1].TrimStartAndEnd();
			if (FCString::IsNumeric(*XStr) && FCString::IsNumeric(*YStr) && EdgePoints.Num() > 0 && LengthAttr)
			{
				const FVector Point(FCString::Atod(*XStr) * 100.0, FCString::Atod(*YStr) * 100.0, 0.0);
				int32 BestIndex = 0;
				double BestDistSq = TNumericLimits<double>::Max();
				for (int32 Index = 0; Index < EdgePoints.Num(); ++Index)
				{
					const FPCGPoint& EdgePoint = EdgePoints[Index];
					const FVector Start = EdgePoint.Transform.GetLocation();
					const FVector Tangent = EdgePoint.Transform.GetRotation().GetAxisX();
					const double Length = LengthAttr->GetValueFromItemKey(EdgePoint.MetadataEntry);
					const FVector End = Start + Tangent * Length;
					const double DistSq = FMath::PointDistToSegmentSquared(Point, Start, End);
					if (DistSq < BestDistSq)
					{
						BestDistSq = DistSq;
						BestIndex = Index;
					}
				}
				return BestIndex;
			}
		}
	}

	if (const FString* SideValue = Tags.Find(TEXT("grammar:street_facing_side")))
	{
		const FString Trimmed = SideValue->TrimStartAndEnd();
		if (FCString::IsNumeric(*Trimmed) && EdgePoints.Num() > 0)
		{
			const int32 Value = FCString::Atoi(*Trimmed);
			const int32 NumEdges = EdgePoints.Num();
			return ((Value % NumEdges) + NumEdges) % NumEdges;
		}
		// Present but unusable (non-numeric, or no edges) -- fall through to real street geometry
		// below rather than defaulting straight to edge 0.
	}

	if (StreetSegments.Num() > 0 && EdgePoints.Num() > 0 && LengthAttr)
	{
		// addr:street name match, regardless of SearchRadius -- see this function's own comment.
		if (const FString* AddrStreet = Tags.Find(TEXT("addr:street")))
		{
			if (!AddrStreet->IsEmpty())
			{
				const FString Normalized = AddrStreet->TrimStartAndEnd().ToLower();
				TArray<const FStreetSegment*> Matching;
				for (const FStreetSegment& Segment : StreetSegments)
				{
					if (!Segment.Name.IsEmpty() && Segment.Name.TrimStartAndEnd().ToLower() == Normalized)
					{
						Matching.Add(&Segment);
					}
				}
				if (Matching.Num() > 0)
				{
					double MatchDistance = 0.0;
					const int32 MatchIndex = NearestEdgeToSegments(EdgePoints, LengthAttr, Matching, MatchDistance);
					if (MatchIndex != INDEX_NONE)
					{
						return MatchIndex;
					}
				}
			}
		}

		// Nearest edge to the nearest street among ALL of them, only if within SearchRadius.
		TArray<const FStreetSegment*> AllSegments;
		AllSegments.Reserve(StreetSegments.Num());
		for (const FStreetSegment& Segment : StreetSegments)
		{
			AllSegments.Add(&Segment);
		}
		double NearestDistance = 0.0;
		const int32 NearestIndex = NearestEdgeToSegments(EdgePoints, LengthAttr, AllSegments, NearestDistance);
		if (NearestIndex != INDEX_NONE && NearestDistance <= SearchRadius)
		{
			return NearestIndex;
		}
	}

	return 0;
}

bool FindNearestStreetDirection(const TArray<FVector2D>& Footprint, const TArray<const FStreetSegment*>& Candidates, double SearchRadius, FVector2D& OutDirection)
{
	if (Candidates.Num() == 0 || Footprint.Num() == 0)
	{
		return false;
	}

	FVector2D Centroid = FVector2D::ZeroVector;
	for (const FVector2D& Point : Footprint)
	{
		Centroid += Point;
	}
	Centroid /= static_cast<double>(Footprint.Num());
	const FVector CentroidWorld(Centroid.X, Centroid.Y, 0.0);

	const FStreetSegment* BestSegment = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (const FStreetSegment* Segment : Candidates)
	{
		const double DistSq = FMath::PointDistToSegmentSquared(CentroidWorld, Segment->Start, Segment->End);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestSegment = Segment;
		}
	}

	if (!BestSegment)
	{
		return false;
	}
	if (SearchRadius >= 0.0 && BestDistSq > SearchRadius * SearchRadius)
	{
		return false;
	}

	const FVector2D Direction = FVector2D(BestSegment->End.X - BestSegment->Start.X, BestSegment->End.Y - BestSegment->Start.Y).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return false;
	}
	OutDirection = Direction;
	return true;
}
