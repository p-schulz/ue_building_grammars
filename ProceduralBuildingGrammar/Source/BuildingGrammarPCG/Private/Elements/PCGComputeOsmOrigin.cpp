#include "Elements/PCGComputeOsmOrigin.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Metadata/PCGMetadata.h"
#include "Osm/BuildingGrammarOsmTypes.h"
#include "GeoReferenceOriginActor.h"

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

	double FallbackLatitude = 0.0;
	double FallbackLongitude = 0.0;
	if (!Document.GetBoundsCenter(FallbackLatitude, FallbackLongitude))
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGComputeOsmOrigin", "NoBounds", "The selected file has no <bounds> element and no nodes to compute an origin from."));
		return true;
	}

	// Defers to this level's existing geo reference (AGeoReferenceOriginActor -- set via "Set Level
	// Geo Reference..." or established by an earlier import) instead of this file's own bounds, so
	// PCG-generated content stitches together with whatever else was imported into this level, the
	// same as every other OSM-driven generate/import action. Falls back to this file's own bounds-
	// center (and, since none existed yet, establishes it as the level's new reference) if
	// ExecutionSource has no World available -- SourceComponent is deprecated as of UE 5.6.
	UWorld* World = Context->ExecutionSource.IsValid() ? Context->ExecutionSource->GetExecutionState().GetWorld() : nullptr;
	double CenterLatitude = 0.0;
	double CenterLongitude = 0.0;
	AGeoReferenceOriginActor::ResolveOrigin(World, FallbackLatitude, FallbackLongitude, CenterLatitude, CenterLongitude);

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
