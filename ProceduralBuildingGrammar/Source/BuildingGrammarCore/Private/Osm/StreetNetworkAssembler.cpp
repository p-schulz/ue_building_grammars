#include "Osm/StreetNetworkAssembler.h"

namespace
{
	bool IsAddressableHighwayValue(const FString& Value)
	{
		static const TSet<FString> NonAddressable = {
			TEXT("footway"), TEXT("cycleway"), TEXT("path"), TEXT("steps"),
			TEXT("bridleway"), TEXT("corridor"), TEXT("platform"),
			TEXT("proposed"), TEXT("construction"),
		};
		return !Value.IsEmpty() && !NonAddressable.Contains(Value.ToLower());
	}

	bool ResolvePolyline(const TArray<int64>& NodeRefs, const TMap<int64, FOsmNode>& Nodes, TArray<FVector2D>& OutPoints)
	{
		OutPoints.Reset(NodeRefs.Num());
		for (const int64 Id : NodeRefs)
		{
			const FOsmNode* Node = Nodes.Find(Id);
			if (!Node)
			{
				return false;
			}
			OutPoints.Add(FVector2D(Node->Lon, Node->Lat));
		}
		return OutPoints.Num() >= 2;
	}
}

TArray<FGrammarStreetSegment> FStreetNetworkAssembler::Assemble(const FOsmDocument& Document)
{
	TArray<FGrammarStreetSegment> Result;

	for (const TPair<int64, FOsmWay>& WayPair : Document.Ways)
	{
		const FOsmWay& Way = WayPair.Value;
		const FString* HighwayValue = Way.Tags.Find(TEXT("highway"));
		if (!HighwayValue || !IsAddressableHighwayValue(*HighwayValue))
		{
			continue;
		}

		FGrammarStreetSegment Segment;
		if (!ResolvePolyline(Way.NodeRefs, Document.Nodes, Segment.Points))
		{
			continue;
		}

		if (const FString* NameValue = Way.Tags.Find(TEXT("name")))
		{
			Segment.Name = NameValue->TrimStartAndEnd();
		}

		Result.Add(MoveTemp(Segment));
	}

	return Result;
}
