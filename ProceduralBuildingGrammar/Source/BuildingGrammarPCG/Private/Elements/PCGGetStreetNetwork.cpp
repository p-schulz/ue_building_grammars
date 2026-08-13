#include "Elements/PCGGetStreetNetwork.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGSplineData.h"
#include "Metadata/PCGMetadata.h"
#include "Components/SplineComponent.h"
#include "Osm/OsmTypes.h"
#include "Osm/StreetNetworkAssembler.h"
#include "Geo/LocalTangentPlaneProjection.h"

namespace
{
	const FName StreetsPinLabel = TEXT("Streets");
	const FName StreetInfoPinLabel = TEXT("StreetInfo");

	// Same meters -> UE-centimeters boundary as UPCGLoadOsmBuildingVolumesSettings -- see that
	// element's own comment.
	constexpr double MetersToUnrealUnits = 100.0;

	UPCGSplineData* MakeStreetSplineData(FPCGContext* Context, const FGrammarStreetSegment& Segment)
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

TArray<FPCGPinProperties> UPCGGetStreetNetworkSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(StreetsPinLabel, EPCGDataType::Spline);
	Pins.Emplace(StreetInfoPinLabel, EPCGDataType::Param);
	return Pins;
}

FPCGElementPtr UPCGGetStreetNetworkSettings::CreateElement() const
{
	return MakeShared<FPCGGetStreetNetworkElement>();
}

bool FPCGGetStreetNetworkElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPCGGetStreetNetworkSettings* Settings = Context->GetInputSettings<UPCGGetStreetNetworkSettings>();
	check(Settings);

	if (Settings->OsmFilePath.FilePath.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGGetStreetNetwork", "NoFile", "No OSM file path set."));
		return true;
	}

	FOsmDocument Document;
	FString ParseError;
	if (!FOsmDocument::ParseFile(Settings->OsmFilePath.FilePath, Document, ParseError))
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(NSLOCTEXT("PCGGetStreetNetwork", "ParseFailed", "Failed to parse '{0}': {1}"),
			FText::FromString(Settings->OsmFilePath.FilePath), FText::FromString(ParseError)));
		return true;
	}

	const FLocalTangentPlaneProjection Projection(Settings->OriginLatitude, Settings->OriginLongitude);
	TArray<FGrammarStreetSegment> Streets = FStreetNetworkAssembler::Assemble(Document);
	for (FGrammarStreetSegment& Street : Streets)
	{
		Street.Points = Projection.ProjectRing(Street.Points);
	}

	UPCGParamData* StreetInfo = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
	UPCGMetadata* InfoMetadata = StreetInfo->MutableMetadata();
	FPCGMetadataAttribute<FString>* NameAttr = InfoMetadata->CreateAttribute<FString>(TEXT("Name"), FString(), false, false);

	int32 StreetIndex = 0;
	for (const FGrammarStreetSegment& Street : Streets)
	{
		UPCGSplineData* SplineData = MakeStreetSplineData(Context, Street);
		if (!SplineData)
		{
			continue;
		}

		FPCGTaggedData& StreetData = Context->OutputData.TaggedData.Emplace_GetRef();
		StreetData.Data = SplineData;
		StreetData.Pin = StreetsPinLabel;
		if (!Street.Name.IsEmpty())
		{
			StreetData.Tags.Add(FString::Printf(TEXT("Name:%s"), *Street.Name));
		}

		const int64 EntryKey = StreetInfo->FindOrAddMetadataKey(FName(*FString::Printf(TEXT("Street_%d"), StreetIndex)));
		NameAttr->SetValue(EntryKey, Street.Name);
		++StreetIndex;
	}

	FPCGTaggedData& StreetInfoData = Context->OutputData.TaggedData.Emplace_GetRef();
	StreetInfoData.Data = StreetInfo;
	StreetInfoData.Pin = StreetInfoPinLabel;

	return true;
}
