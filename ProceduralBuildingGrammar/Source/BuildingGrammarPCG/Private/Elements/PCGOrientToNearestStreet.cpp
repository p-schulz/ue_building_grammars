#include "Elements/PCGOrientToNearestStreet.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "Components/SplineComponent.h"
#include "Math/RotationMatrix.h"
#include "PCGBuildingGrammarDefaults.h"

namespace
{
	const FName PointsPinLabel = TEXT("Points");
	const FName StreetsPinLabel = TEXT("Streets");
}

TArray<FPCGPinProperties> UPCGOrientToNearestStreetSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(PointsPinLabel, EPCGDataType::Point);
	Pins.Emplace(StreetsPinLabel, EPCGDataType::Spline);
	return Pins;
}

TArray<FPCGPinProperties> UPCGOrientToNearestStreetSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(PointsPinLabel, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGOrientToNearestStreetSettings::CreateElement() const
{
	return MakeShared<FPCGOrientToNearestStreetElement>();
}

bool FPCGOrientToNearestStreetElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPCGOrientToNearestStreetSettings* Settings = Context->GetInputSettings<UPCGOrientToNearestStreetSettings>();
	check(Settings);

	// Gathered once (not per point/per input) -- the street network is graph-wide, not per-point
	// data -- same reasoning UPCGFacadeWindowDoorLayoutSettings' own identical gathering documents.
	TArray<FStreetSegment> StreetSegments;
	for (const FPCGTaggedData& StreetTaggedData : Context->InputData.GetInputsByPin(StreetsPinLabel))
	{
		const UPCGSplineData* StreetSpline = Cast<UPCGSplineData>(StreetTaggedData.Data.Get());
		if (!StreetSpline)
		{
			continue;
		}
		const FString StreetName = ExtractStreetNameFromTags(StreetTaggedData.Tags);
		const TArray<FSplinePoint> StreetPoints = StreetSpline->GetSplinePoints();
		for (int32 Index = 0; Index + 1 < StreetPoints.Num(); ++Index)
		{
			FStreetSegment& Segment = StreetSegments.AddDefaulted_GetRef();
			Segment.Start = StreetPoints[Index].Position;
			Segment.End = StreetPoints[Index + 1].Position;
			Segment.Name = StreetName;
		}
	}

	TArray<const FStreetSegment*> Candidates;
	Candidates.Reserve(StreetSegments.Num());
	for (const FStreetSegment& Segment : StreetSegments)
	{
		Candidates.Add(&Segment);
	}

	for (const FPCGTaggedData& PointsTaggedData : Context->InputData.GetInputsByPin(PointsPinLabel))
	{
		const UPCGPointData* InputPointData = Cast<UPCGPointData>(PointsTaggedData.Data.Get());
		if (!InputPointData)
		{
			continue;
		}

		UPCGPointData* OutputPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		OutputPointData->InitializeFromData(InputPointData);
		TArray<FPCGPoint>& OutPoints = OutputPointData->GetMutablePoints();
		OutPoints = InputPointData->GetPoints();

		for (FPCGPoint& Point : OutPoints)
		{
			const FVector Location = Point.Transform.GetLocation();
			const FVector2D PointLocation2D(Location.X, Location.Y);
			FVector2D Direction;
			if (!FindNearestStreetDirection({ PointLocation2D }, Candidates, Settings->SearchRadius, Direction))
			{
				continue;
			}

			if (Settings->AlignmentMode == EGrammarStreetAlignmentMode::Perpendicular)
			{
				Direction = FVector2D(-Direction.Y, Direction.X);
			}

			const FVector Forward(Direction.X, Direction.Y, 0.0);
			Point.Transform.SetRotation(FRotationMatrix::MakeFromXZ(Forward, FVector::UpVector).ToQuat());
		}

		FPCGTaggedData& Out = Context->OutputData.TaggedData.Emplace_GetRef();
		Out.Data = OutputPointData;
		Out.Pin = PointsPinLabel;
		Out.Tags = PointsTaggedData.Tags;
	}

	return true;
}
