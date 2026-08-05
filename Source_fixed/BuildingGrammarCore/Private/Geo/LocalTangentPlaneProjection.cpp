#include "Geo/LocalTangentPlaneProjection.h"

FLocalTangentPlaneProjection::FLocalTangentPlaneProjection(double OriginLatitudeDegrees, double OriginLongitudeDegrees)
	: OriginLatRadians(FMath::DegreesToRadians(OriginLatitudeDegrees))
	, OriginLonRadians(FMath::DegreesToRadians(OriginLongitudeDegrees))
	, CosOriginLat(FMath::Cos(FMath::DegreesToRadians(OriginLatitudeDegrees)))
{
}

FVector2D FLocalTangentPlaneProjection::ProjectToUnrealCentimeters(const FVector2D& LonLatDegrees) const
{
	const double LatRadians = FMath::DegreesToRadians(static_cast<double>(LonLatDegrees.Y));
	const double LonRadians = FMath::DegreesToRadians(static_cast<double>(LonLatDegrees.X));

	const double EastMeters = (LonRadians - OriginLonRadians) * CosOriginLat * EarthRadiusMeters;
	const double NorthMeters = (LatRadians - OriginLatRadians) * EarthRadiusMeters;

	constexpr double MetersToCentimeters = 100.0;
	return FVector2D(NorthMeters * MetersToCentimeters, EastMeters * MetersToCentimeters);
}

TArray<FVector2D> FLocalTangentPlaneProjection::ProjectRing(const TArray<FVector2D>& LonLatRing) const
{
	TArray<FVector2D> Result;
	Result.Reserve(LonLatRing.Num());
	for (const FVector2D& Point : LonLatRing)
	{
		Result.Add(ProjectToUnrealCentimeters(Point));
	}
	return Result;
}
