#include "Elements/PCGLoadOsmPointFeatures.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Osm/BuildingGrammarOsmTypes.h"
#include "Geo/LocalTangentPlaneProjection.h"
#include "StreetFurnitureMeshSettings.h"
#include "Engine/StaticMesh.h"
#include "UObject/SoftObjectPath.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const FName FeaturesPinLabel = TEXT("Features");

	// Same meters -> UE-centimeters boundary as UPCGLoadOsmBuildingVolumesSettings/
	// UPCGGetStreetNetworkSettings -- see those elements' own comments.
	constexpr double MetersToUnrealUnits = 100.0;

	FString SerializeTagsToJson(const TMap<FString, FString>& Tags)
	{
		const TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
		for (const TPair<FString, FString>& Tag : Tags)
		{
			JsonObject->SetStringField(Tag.Key, Tag.Value);
		}

		FString Result;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
		FJsonSerializer::Serialize(JsonObject, Writer);
		return Result;
	}

	// Same OR-across-filters semantics as FGrammarStyleSelection::StyleMatchesTags' TagFilters loop
	// (BuildingGrammarCore/Private/Grammar/GrammarStyleSelection.cpp) -- copied rather than shared
	// since that function is typed to FFacadeStyleConfig specifically, unrelated to point features.
	bool CategoryMatchesTags(const FStreetFurnitureCategoryConfig& Category, const TMap<FString, FString>& Tags)
	{
		for (const TPair<FString, FGrammarStringList>& Filter : Category.TagFilters)
		{
			const FString* TagValue = Tags.Find(Filter.Key);
			if (!TagValue)
			{
				continue;
			}
			const FString NormalizedTagValue = TagValue->TrimStartAndEnd().ToLower();
			for (const FString& Allowed : Filter.Value.Values)
			{
				const FString NormalizedAllowed = Allowed.TrimStartAndEnd().ToLower();
				if (NormalizedAllowed == NormalizedTagValue || NormalizedAllowed == TEXT("*"))
				{
					return true;
				}
			}
		}
		return false;
	}
}

UPCGLoadOsmPointFeaturesSettings::UPCGLoadOsmPointFeaturesSettings()
{
	auto AddCategory = [this](const FString& Name, const FString& Key, std::initializer_list<const TCHAR*> Values)
	{
		FStreetFurnitureCategoryConfig Category;
		Category.Name = Name;
		FGrammarStringList List;
		for (const TCHAR* Value : Values)
		{
			List.Values.Add(Value);
		}
		Category.TagFilters.Add(Key, List);
		Categories.Add(MoveTemp(Category));
	};

	AddCategory(TEXT("StreetLight"), TEXT("highway"), { TEXT("street_lamp") });
	AddCategory(TEXT("TrafficSignal"), TEXT("highway"), { TEXT("traffic_signals") });
	AddCategory(TEXT("TrafficSign"), TEXT("traffic_sign"), { TEXT("*") });
	AddCategory(TEXT("StreetCabinet"), TEXT("man_made"), { TEXT("street_cabinet") });
	AddCategory(TEXT("WasteBin"), TEXT("amenity"), { TEXT("waste_basket") });
	AddCategory(TEXT("Bench"), TEXT("amenity"), { TEXT("bench") });
	AddCategory(TEXT("Billboard"), TEXT("advertising"), { TEXT("billboard") });
	AddCategory(TEXT("PostBox"), TEXT("amenity"), { TEXT("post_box") });
}

TArray<FPCGPinProperties> UPCGLoadOsmPointFeaturesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(FeaturesPinLabel, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGLoadOsmPointFeaturesSettings::CreateElement() const
{
	return MakeShared<FPCGLoadOsmPointFeaturesElement>();
}

bool FPCGLoadOsmPointFeaturesElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPCGLoadOsmPointFeaturesSettings* Settings = Context->GetInputSettings<UPCGLoadOsmPointFeaturesSettings>();
	check(Settings);

	if (Settings->OsmFilePath.FilePath.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGLoadOsmPointFeatures", "NoFile", "No OSM file path set."));
		return true;
	}

	FOsmDocument Document;
	FString ParseError;
	if (!FOsmDocument::ParseFile(Settings->OsmFilePath.FilePath, Document, ParseError))
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(NSLOCTEXT("PCGLoadOsmPointFeatures", "ParseFailed", "Failed to parse '{0}': {1}"),
			FText::FromString(Settings->OsmFilePath.FilePath), FText::FromString(ParseError)));
		return true;
	}

	const FLocalTangentPlaneProjection Projection(Settings->OriginLatitude, Settings->OriginLongitude);

	UPCGPointData* FeatureData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
	UPCGMetadata* Metadata = FeatureData->MutableMetadata();
	FPCGMetadataAttribute<FString>* CategoryAttr = Metadata->CreateAttribute<FString>(TEXT("Category"), FString(), false, false);
	FPCGMetadataAttribute<FString>* TagsJsonAttr = Metadata->CreateAttribute<FString>(TEXT("TagsJson"), FString(), false, false);
	FPCGMetadataAttribute<int64>* SourceIdAttr = Metadata->CreateAttribute<int64>(TEXT("SourceId"), 0, false, false);
	// Always false here -- every point in this pin's output corresponds to a real, explicitly-tagged
	// OSM node. Present (rather than omitted) so this pin's attribute schema matches
	// UPCGPlaceStreetLightsAlongLitRoadsSettings' "Lights" pin exactly, letting the two be merged with
	// a stock PCG points-union node and treated uniformly downstream -- see that node's own header
	// comment.
	FPCGMetadataAttribute<bool>* SyntheticAttr = Metadata->CreateAttribute<bool>(TEXT("bSynthetic"), false, false, false);
	// Filled in from UStreetFurnitureMeshSettings when the matched category has a configured mesh
	// list; left as an invalid/empty path (the established "no override" sentinel -- see
	// UPCGFacadeWindowDoorLayoutSettings' own MaterialOverride attribute) otherwise, same as if this
	// attribute weren't set at all. Wire a Static Mesh Spawner's Mesh Selector to "By Attribute
	// Override" with "MeshOverride" in its mesh-override-attribute list to use this.
	FPCGMetadataAttribute<FSoftObjectPath>* MeshOverrideAttr = Metadata->CreateAttribute<FSoftObjectPath>(TEXT("MeshOverride"), FSoftObjectPath(), false, false);

	const UStreetFurnitureMeshSettings* MeshSettings = GetDefault<UStreetFurnitureMeshSettings>();
	TArray<FPCGPoint>& Points = FeatureData->GetMutablePoints();
	for (const TPair<int64, FOsmNode>& NodePair : Document.Nodes)
	{
		const FOsmNode& Node = NodePair.Value;
		if (Node.Tags.Num() == 0)
		{
			continue;
		}

		// First matching category wins -- same "first candidate" convention used throughout this
		// pipeline (e.g. FBuildingGrammarEngine's own PrimaryStyle = Styles[0]).
		const FStreetFurnitureCategoryConfig* MatchedCategory = nullptr;
		for (const FStreetFurnitureCategoryConfig& Category : Settings->Categories)
		{
			if (CategoryMatchesTags(Category, Node.Tags))
			{
				MatchedCategory = &Category;
				break;
			}
		}
		if (!MatchedCategory)
		{
			continue;
		}

		const FVector2D LocalMeters = Projection.ProjectToLocalMeters(FVector2D(Node.Lon, Node.Lat));
		const FVector WorldPosition(LocalMeters.X * MetersToUnrealUnits, LocalMeters.Y * MetersToUnrealUnits, 0.0);

		FPCGPoint Point;
		Point.Transform = FTransform(FQuat::Identity, WorldPosition, FVector::OneVector);
		Point.Density = 1.0f;
		Point.MetadataEntry = Metadata->AddEntry();
		CategoryAttr->SetValue(Point.MetadataEntry, MatchedCategory->Name);
		TagsJsonAttr->SetValue(Point.MetadataEntry, SerializeTagsToJson(Node.Tags));
		SourceIdAttr->SetValue(Point.MetadataEntry, Node.Id);
		SyntheticAttr->SetValue(Point.MetadataEntry, false);

		// Seeded by the OSM node id, not a global RNG stream, so the same node always picks the same
		// mesh across regenerations rather than "flickering" between candidates on every regenerate.
		FRandomStream MeshStream(static_cast<int32>(Node.Id));
		if (UStaticMesh* PickedMesh = MeshSettings->PickMeshForCategory(MatchedCategory->Name, MeshStream))
		{
			MeshOverrideAttr->SetValue(Point.MetadataEntry, FSoftObjectPath(PickedMesh));
		}

		Points.Add(Point);
	}

	FPCGTaggedData& Out = Context->OutputData.TaggedData.Emplace_GetRef();
	Out.Data = FeatureData;
	Out.Pin = FeaturesPinLabel;

	return true;
}
