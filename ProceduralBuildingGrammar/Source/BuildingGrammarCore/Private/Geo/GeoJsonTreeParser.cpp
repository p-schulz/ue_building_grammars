#include "Geo/GeoJsonTreeParser.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

EGrammarTreeType FGeoJsonTreeParser::ParseTreeType(const FString& BaumartAllgemein)
{
	const FString Normalized = BaumartAllgemein.TrimStartAndEnd();
	if (Normalized.Equals(TEXT("Obstbaum"), ESearchCase::IgnoreCase))
	{
		return EGrammarTreeType::FruitTree;
	}
	if (Normalized.Equals(TEXT("Laubbaum"), ESearchCase::IgnoreCase))
	{
		return EGrammarTreeType::Broadleaf;
	}
	return EGrammarTreeType::Unknown;
}

bool FGeoJsonTreeParser::ParseFile(const FString& FilePath, TArray<FGrammarTreePoint>& OutTrees, FString& OutError)
{
	OutTrees.Reset();

	FString FileText;
	if (!FFileHelper::LoadFileToString(FileText, *FilePath))
	{
		OutError = FString::Printf(TEXT("Could not read '%s'"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Malformed JSON");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
	if (!Root->TryGetArrayField(TEXT("features"), Features) || !Features)
	{
		OutError = TEXT("No 'features' array found -- not a GeoJSON FeatureCollection");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& FeatureValue : *Features)
	{
		const TSharedPtr<FJsonObject>* FeatureObject = nullptr;
		if (!FeatureValue.IsValid() || !FeatureValue->TryGetObject(FeatureObject) || !FeatureObject)
		{
			continue;
		}

		const TSharedPtr<FJsonObject>* GeometryObject = nullptr;
		if (!(*FeatureObject)->TryGetObjectField(TEXT("geometry"), GeometryObject) || !GeometryObject)
		{
			continue;
		}
		FString GeometryType;
		if (!(*GeometryObject)->TryGetStringField(TEXT("type"), GeometryType) || GeometryType != TEXT("Point"))
		{
			continue;
		}
		const TArray<TSharedPtr<FJsonValue>>* Coordinates = nullptr;
		if (!(*GeometryObject)->TryGetArrayField(TEXT("coordinates"), Coordinates) || !Coordinates || Coordinates->Num() < 2)
		{
			continue;
		}

		// GeoJSON's own coordinate order is always [longitude, latitude] (RFC 7946 section 3.1.1),
		// matching FLocalTangentPlaneProjection::ProjectToLocalMeters' own (X=Lon, Y=Lat) input
		// convention exactly -- no axis swap needed at the call site.
		FGrammarTreePoint TreePoint;
		TreePoint.Longitude = (*Coordinates)[0]->AsNumber();
		TreePoint.Latitude = (*Coordinates)[1]->AsNumber();

		const TSharedPtr<FJsonObject>* PropertiesObject = nullptr;
		if ((*FeatureObject)->TryGetObjectField(TEXT("properties"), PropertiesObject) && PropertiesObject)
		{
			FString BaumartAllgemein;
			(*PropertiesObject)->TryGetStringField(TEXT("baumart_allgemein"), BaumartAllgemein);
			TreePoint.Type = ParseTreeType(BaumartAllgemein);
		}

		OutTrees.Add(TreePoint);
	}

	return true;
}
