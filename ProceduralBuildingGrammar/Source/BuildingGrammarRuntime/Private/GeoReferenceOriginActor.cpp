#include "GeoReferenceOriginActor.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogGeoReferenceOriginActor, Log, All);

AGeoReferenceOriginActor::AGeoReferenceOriginActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetIsSpatiallyLoaded(false);
}

bool AGeoReferenceOriginActor::FindInWorld(UWorld* World, double& OutLatitude, double& OutLongitude)
{
	if (!World)
	{
		return false;
	}

	AGeoReferenceOriginActor* Found = nullptr;
	int32 Count = 0;
	for (TActorIterator<AGeoReferenceOriginActor> It(World); It; ++It)
	{
		if (!Found)
		{
			Found = *It;
		}
		++Count;
	}
	if (!Found)
	{
		return false;
	}
	if (Count > 1)
	{
		UE_LOG(LogGeoReferenceOriginActor, Warning, TEXT("Found %d AGeoReferenceOriginActor instances in the level -- using the first one (%f, %f). Use \"Clear Level Geo Reference\" and \"Set Level Geo Reference...\" to consolidate to one."), Count, Found->OriginLatitude, Found->OriginLongitude);
	}
	OutLatitude = Found->OriginLatitude;
	OutLongitude = Found->OriginLongitude;
	return true;
}

AGeoReferenceOriginActor* AGeoReferenceOriginActor::SetInWorld(UWorld* World, double Latitude, double Longitude)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AGeoReferenceOriginActor> It(World); It; ++It)
	{
		AGeoReferenceOriginActor* Existing = *It;
		Existing->Modify();
		Existing->OriginLatitude = Latitude;
		Existing->OriginLongitude = Longitude;
		Existing->MarkPackageDirty();
		return Existing;
	}

	AGeoReferenceOriginActor* NewActor = World->SpawnActor<AGeoReferenceOriginActor>();
	if (NewActor)
	{
		NewActor->OriginLatitude = Latitude;
		NewActor->OriginLongitude = Longitude;
		NewActor->MarkPackageDirty();
	}
	return NewActor;
}

void AGeoReferenceOriginActor::ResolveOrigin(UWorld* World, double FallbackLatitude, double FallbackLongitude, double& OutLatitude, double& OutLongitude)
{
	if (FindInWorld(World, OutLatitude, OutLongitude))
	{
		return;
	}
	SetInWorld(World, FallbackLatitude, FallbackLongitude);
	OutLatitude = FallbackLatitude;
	OutLongitude = FallbackLongitude;
}
