#include "Geo/LocalTangentPlaneProjection.h"

FLocalTangentPlaneProjection::FLocalTangentPlaneProjection(double OriginLatitudeDegrees, double OriginLongitudeDegrees)
	: OriginLatRadians(FMath::DegreesToRadians(OriginLatitudeDegrees))
	, OriginLonRadians(FMath::DegreesToRadians(OriginLongitudeDegrees))
	, CosOriginLat(FMath::Cos(FMath::DegreesToRadians(OriginLatitudeDegrees)))
{
}

FVector2D FLocalTangentPlaneProjection::ProjectToLocalMeters(const FVector2D& LonLatDegrees) const
{
	const double LatRadians = FMath::DegreesToRadians(static_cast<double>(LonLatDegrees.Y));
	const double LonRadians = FMath::DegreesToRadians(static_cast<double>(LonLatDegrees.X));

	const double EastMeters = (LonRadians - OriginLonRadians) * CosOriginLat * EarthRadiusMeters;
	const double NorthMeters = (LatRadians - OriginLatRadians) * EarthRadiusMeters;

	return FVector2D(NorthMeters, EastMeters);
}

TArray<FVector2D> FLocalTangentPlaneProjection::ProjectRing(const TArray<FVector2D>& LonLatRing) const
{
	TArray<FVector2D> Result;
	Result.Reserve(LonLatRing.Num());
	for (const FVector2D& Point : LonLatRing)
	{
		Result.Add(ProjectToLocalMeters(Point));
	}
	return Result;
}
