#include "Osm/BarrierNetworkAssembler.h"
#include "Grammar/GrammarTagParsing.h"

namespace
{
	bool IsLinearBarrierValue(const FString& Value)
	{
		static const TSet<FString> LinearBarriers = {
			TEXT("wall"), TEXT("fence"), TEXT("guard_rail"), TEXT("hedge"),
		};
		return LinearBarriers.Contains(Value.ToLower());
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

TArray<FGrammarBarrierSegment> FBarrierNetworkAssembler::Assemble(const FOsmDocument& Document)
{
	TArray<FGrammarBarrierSegment> Result;

	for (const TPair<int64, FOsmWay>& WayPair : Document.Ways)
	{
		const FOsmWay& Way = WayPair.Value;
		const FString* BarrierValue = Way.Tags.Find(TEXT("barrier"));
		if (!BarrierValue || !IsLinearBarrierValue(*BarrierValue))
		{
			continue;
		}

		FGrammarBarrierSegment Segment;
		if (!ResolvePolyline(Way.NodeRefs, Document.Nodes, Segment.Points))
		{
			continue;
		}

		Segment.Type = BarrierValue->TrimStartAndEnd().ToLower();
		Segment.Height = FGrammarTagParsing::ParseMeters(Way.Tags.Find(TEXT("height")));
		if (const FString* MaterialValue = Way.Tags.Find(TEXT("material")))
		{
			Segment.Material = MaterialValue->TrimStartAndEnd();
		}

		Result.Add(MoveTemp(Segment));
	}

	return Result;
}
