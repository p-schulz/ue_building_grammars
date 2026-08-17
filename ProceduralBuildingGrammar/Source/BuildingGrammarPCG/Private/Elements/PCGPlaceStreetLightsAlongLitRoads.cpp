#include "Elements/PCGPlaceStreetLightsAlongLitRoads.h"
#include "PCGContext.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Components/SplineComponent.h"
#include "Geometry/GrammarGeometry2D.h"
#include "Math/RotationMatrix.h"
#include "StreetFurnitureMeshSettings.h"
#include "Engine/StaticMesh.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FName StreetsPinLabel = TEXT("Streets");
	const FName LightsPinLabel = TEXT("Lights");

	bool IsLitTag(const TSet<FString>& Tags)
	{
		return Tags.Contains(TEXT("Lit:true"));
	}
}

TArray<FPCGPinProperties> UPCGPlaceStreetLightsAlongLitRoadsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(StreetsPinLabel, EPCGDataType::Spline);
	return Pins;
}

TArray<FPCGPinProperties> UPCGPlaceStreetLightsAlongLitRoadsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(LightsPinLabel, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGPlaceStreetLightsAlongLitRoadsSettings::CreateElement() const
{
	return MakeShared<FPCGPlaceStreetLightsAlongLitRoadsElement>();
}

bool FPCGPlaceStreetLightsAlongLitRoadsElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPCGPlaceStreetLightsAlongLitRoadsSettings* Settings = Context->GetInputSettings<UPCGPlaceStreetLightsAlongLitRoadsSettings>();
	check(Settings);

	UPCGPointData* LightData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
	UPCGMetadata* Metadata = LightData->MutableMetadata();
	FPCGMetadataAttribute<FString>* CategoryAttr = Metadata->CreateAttribute<FString>(TEXT("Category"), FString(), false, false);
	FPCGMetadataAttribute<FString>* TagsJsonAttr = Metadata->CreateAttribute<FString>(TEXT("TagsJson"), FString(), false, false);
	FPCGMetadataAttribute<int64>* SourceIdAttr = Metadata->CreateAttribute<int64>(TEXT("SourceId"), -1, false, false);
	FPCGMetadataAttribute<bool>* SyntheticAttr = Metadata->CreateAttribute<bool>(TEXT("bSynthetic"), false, false, false);
	// Same "no override" sentinel convention as UPCGLoadOsmPointFeaturesSettings' own MeshOverride
	// attribute -- see that node's own comment.
	FPCGMetadataAttribute<FSoftObjectPath>* MeshOverrideAttr = Metadata->CreateAttribute<FSoftObjectPath>(TEXT("MeshOverride"), FSoftObjectPath(), false, false);

	const UStreetFurnitureMeshSettings* MeshSettings = GetDefault<UStreetFurnitureMeshSettings>();
	TArray<FPCGPoint>& Points = LightData->GetMutablePoints();
	const double Spacing = FMath::Max(Settings->LightSpacing, 10.0);
	int32 LightIndex = 0;

	for (const FPCGTaggedData& StreetTaggedData : Context->InputData.GetInputsByPin(StreetsPinLabel))
	{
		if (!IsLitTag(StreetTaggedData.Tags))
		{
			continue;
		}
		const UPCGSplineData* StreetSpline = Cast<UPCGSplineData>(StreetTaggedData.Data.Get());
		if (!StreetSpline)
		{
			continue;
		}

		const TArray<FSplinePoint> SplinePoints = StreetSpline->GetSplinePoints();
		TArray<FVector2D> Polyline2D;
		Polyline2D.Reserve(SplinePoints.Num());
		for (const FSplinePoint& SplinePoint : SplinePoints)
		{
			Polyline2D.Add(FVector2D(SplinePoint.Position.X, SplinePoint.Position.Y));
		}

		// Half-spacing start offset centers the run of lights along the road instead of always
		// starting exactly at one end.
		const TArray<FGrammarGeometry2D::FPolylineSample> Samples = FGrammarGeometry2D::PointsAlongPolyline(Polyline2D, Spacing, Spacing * 0.5);

		bool bSideFlip = false;
		for (const FGrammarGeometry2D::FPolylineSample& Sample : Samples)
		{
			if (Sample.Tangent.IsNearlyZero())
			{
				continue;
			}

			const FVector2D Normal(-Sample.Tangent.Y, Sample.Tangent.X);
			const double SignedOffset = Settings->SideOffset * ((Settings->bAlternateSides && bSideFlip) ? -1.0 : 1.0);
			bSideFlip = !bSideFlip;

			const FVector2D OffsetPosition2D = FGrammarGeometry2D::PointOnSegment(Sample.Position, Sample.Tangent, Normal, 0.0, SignedOffset);
			const FVector Forward(Sample.Tangent.X, Sample.Tangent.Y, 0.0);
			const FQuat Rotation = FRotationMatrix::MakeFromXZ(Forward, FVector::UpVector).ToQuat();

			FPCGPoint Point;
			Point.Transform = FTransform(Rotation, FVector(OffsetPosition2D.X, OffsetPosition2D.Y, 0.0), FVector::OneVector);
			Point.Density = 1.0f;
			Point.MetadataEntry = Metadata->AddEntry();
			CategoryAttr->SetValue(Point.MetadataEntry, FString(TEXT("StreetLight")));
			TagsJsonAttr->SetValue(Point.MetadataEntry, FString());
			SourceIdAttr->SetValue(Point.MetadataEntry, static_cast<int64>(-1));
			SyntheticAttr->SetValue(Point.MetadataEntry, true);

			// Seeded by a running index, not a global RNG stream -- there's no OSM node id to seed by
			// for a synthesized point, but this still keeps the same light picking the same mesh
			// across regenerations as long as the road geometry/spacing doesn't change.
			FRandomStream MeshStream(LightIndex++);
			if (UStaticMesh* PickedMesh = MeshSettings->PickMeshForCategory(TEXT("StreetLight"), MeshStream))
			{
				MeshOverrideAttr->SetValue(Point.MetadataEntry, FSoftObjectPath(PickedMesh));
			}

			Points.Add(Point);
		}
	}

	FPCGTaggedData& Out = Context->OutputData.TaggedData.Emplace_GetRef();
	Out.Data = LightData;
	Out.Pin = LightsPinLabel;

	return true;
}
