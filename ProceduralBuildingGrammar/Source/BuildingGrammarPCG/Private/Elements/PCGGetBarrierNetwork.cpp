#include "Elements/PCGGetBarrierNetwork.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGSplineData.h"
#include "Metadata/PCGMetadata.h"
#include "Components/SplineComponent.h"
#include "Osm/OsmTypes.h"
#include "Osm/BarrierNetworkAssembler.h"
#include "Geo/LocalTangentPlaneProjection.h"

namespace
{
	const FName BarriersPinLabel = TEXT("Barriers");
	const FName BarrierInfoPinLabel = TEXT("BarrierInfo");

	// Same meters -> UE-centimeters boundary as UPCGLoadOsmBuildingVolumesSettings/
	// UPCGGetStreetNetworkSettings -- see those elements' own comments.
	constexpr double MetersToUnrealUnits = 100.0;

	UPCGSplineData* MakeBarrierSplineData(FPCGContext* Context, const FGrammarBarrierSegment& Segment)
	{
		if (Segment.Points.Num() < 2)
		{
			return nullptr;
		}

		TArray<FSplinePoint> SplinePoints;
		SplinePoints.Reserve(Segment.Points.Num());
		for (int32 Index = 0; Index < Segment.Points.Num(); ++Index)
		{
			const FVector Position(Segment.Points[Index].X * MetersToUnrealUnits, Segment.Points[Index].Y * MetersToUnrealUnits, 0.0);
			SplinePoints.Add(FSplinePoint(static_cast<float>(Index), Position, ESplinePointType::Linear));
		}

		UPCGSplineData* SplineData = FPCGContext::NewObject_AnyThread<UPCGSplineData>(Context);
		SplineData->Initialize(SplinePoints, /*bInClosedLoop=*/false, FTransform::Identity);
		return SplineData;
	}
}

TArray<FPCGPinProperties> UPCGGetBarrierNetworkSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(BarriersPinLabel, EPCGDataType::Spline);
	Pins.Emplace(BarrierInfoPinLabel, EPCGDataType::Param);
	return Pins;
}

FPCGElementPtr UPCGGetBarrierNetworkSettings::CreateElement() const
{
	return MakeShared<FPCGGetBarrierNetworkElement>();
}

bool FPCGGetBarrierNetworkElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPCGGetBarrierNetworkSettings* Settings = Context->GetInputSettings<UPCGGetBarrierNetworkSettings>();
	check(Settings);

	if (Settings->OsmFilePath.FilePath.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGGetBarrierNetwork", "NoFile", "No OSM file path set."));
		return true;
	}

	FOsmDocument Document;
	FString ParseError;
	if (!FOsmDocument::ParseFile(Settings->OsmFilePath.FilePath, Document, ParseError))
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(NSLOCTEXT("PCGGetBarrierNetwork", "ParseFailed", "Failed to parse '{0}': {1}"),
			FText::FromString(Settings->OsmFilePath.FilePath), FText::FromString(ParseError)));
		return true;
	}

	const FLocalTangentPlaneProjection Projection(Settings->OriginLatitude, Settings->OriginLongitude);
	TArray<FGrammarBarrierSegment> Barriers = FBarrierNetworkAssembler::Assemble(Document);
	for (FGrammarBarrierSegment& Barrier : Barriers)
	{
		Barrier.Points = Projection.ProjectRing(Barrier.Points);
	}

	UPCGParamData* BarrierInfo = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
	UPCGMetadata* InfoMetadata = BarrierInfo->MutableMetadata();
	FPCGMetadataAttribute<FString>* TypeAttr = InfoMetadata->CreateAttribute<FString>(TEXT("Type"), FString(), false, false);
	FPCGMetadataAttribute<double>* HeightAttr = InfoMetadata->CreateAttribute<double>(TEXT("Height"), 0.0, false, false);
	FPCGMetadataAttribute<FString>* MaterialAttr = InfoMetadata->CreateAttribute<FString>(TEXT("Material"), FString(), false, false);

	int32 BarrierIndex = 0;
	for (const FGrammarBarrierSegment& Barrier : Barriers)
	{
		UPCGSplineData* SplineData = MakeBarrierSplineData(Context, Barrier);
		if (!SplineData)
		{
			continue;
		}

		const double HeightUnrealUnits = Barrier.Height.IsSet() ? Barrier.Height.GetValue() * MetersToUnrealUnits : 0.0;

		FPCGTaggedData& BarrierData = Context->OutputData.TaggedData.Emplace_GetRef();
		BarrierData.Data = SplineData;
		BarrierData.Pin = BarriersPinLabel;
		BarrierData.Tags.Add(FString::Printf(TEXT("Type:%s"), *Barrier.Type));
		if (Barrier.Height.IsSet())
		{
			BarrierData.Tags.Add(FString::Printf(TEXT("Height:%f"), HeightUnrealUnits));
		}
		if (!Barrier.Material.IsEmpty())
		{
			BarrierData.Tags.Add(FString::Printf(TEXT("Material:%s"), *Barrier.Material));
		}

		const int64 EntryKey = BarrierInfo->FindOrAddMetadataKey(FName(*FString::Printf(TEXT("Barrier_%d"), BarrierIndex)));
		TypeAttr->SetValue(EntryKey, Barrier.Type);
		HeightAttr->SetValue(EntryKey, HeightUnrealUnits);
		MaterialAttr->SetValue(EntryKey, Barrier.Material);
		++BarrierIndex;
	}

	FPCGTaggedData& BarrierInfoData = Context->OutputData.TaggedData.Emplace_GetRef();
	BarrierInfoData.Data = BarrierInfo;
	BarrierInfoData.Pin = BarrierInfoPinLabel;

	return true;
}
