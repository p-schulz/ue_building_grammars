#include "Elements/PCGComputeOsmOrigin.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Metadata/PCGMetadata.h"
#include "Osm/OsmTypes.h"
#include "Osm/BuildingFootprintAssembler.h"

namespace
{
	const FName OriginPinLabel = TEXT("Origin");
}

TArray<FPCGPinProperties> UPCGComputeOsmOriginSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(OriginPinLabel, EPCGDataType::Param);
	return Pins;
}

FPCGElementPtr UPCGComputeOsmOriginSettings::CreateElement() const
{
	return MakeShared<FPCGComputeOsmOriginElement>();
}

bool FPCGComputeOsmOriginElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPCGComputeOsmOriginSettings* Settings = Context->GetInputSettings<UPCGComputeOsmOriginSettings>();
	check(Settings);

	if (Settings->OsmFilePath.FilePath.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGComputeOsmOrigin", "NoFile", "No OSM file path set."));
		return true;
	}

	FOsmDocument Document;
	FString ParseError;
	if (!FOsmDocument::ParseFile(Settings->OsmFilePath.FilePath, Document, ParseError))
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(NSLOCTEXT("PCGComputeOsmOrigin", "ParseFailed", "Failed to parse '{0}': {1}"),
			FText::FromString(Settings->OsmFilePath.FilePath), FText::FromString(ParseError)));
		return true;
	}

	const TArray<FBuildingFootprint> Footprints = FBuildingFootprintAssembler::Assemble(Document);
	double CenterLatitude = 0.0;
	double CenterLongitude = 0.0;
	if (!FBuildingFootprintAssembler::ComputeFootprintBoundsCenter(Footprints, CenterLatitude, CenterLongitude))
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGComputeOsmOrigin", "NoFootprints", "The selected file has no building footprints to compute an origin from."));
		return true;
	}

	UPCGParamData* OriginData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
	UPCGMetadata* Metadata = OriginData->MutableMetadata();
	FPCGMetadataAttribute<double>* LatAttr = Metadata->CreateAttribute<double>(TEXT("OriginLatitude"), 0.0, false, false);
	FPCGMetadataAttribute<double>* LonAttr = Metadata->CreateAttribute<double>(TEXT("OriginLongitude"), 0.0, false, false);

	const int64 EntryKey = OriginData->FindOrAddMetadataKey(TEXT("Origin"));
	LatAttr->SetValue(EntryKey, CenterLatitude);
	LonAttr->SetValue(EntryKey, CenterLongitude);

	FPCGTaggedData& Out = Context->OutputData.TaggedData.Emplace_GetRef();
	Out.Data = OriginData;
	Out.Pin = OriginPinLabel;

	return true;
}
