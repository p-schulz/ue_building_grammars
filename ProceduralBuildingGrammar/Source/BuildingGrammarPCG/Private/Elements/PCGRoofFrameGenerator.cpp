#include "Elements/PCGRoofFrameGenerator.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGDynamicMeshData.h"
#include "Geometry/GrammarRoofFrame.h"
#include "Geometry/GrammarPolygonTriangulator.h"
#include "Geometry/GrammarFace.h"
#include "Geometry/GrammarRoofSkeleton.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Algo/Reverse.h"
#include "GrammarKitResolver.h"
#include "PCGBuildingGrammarDefaults.h"

namespace
{
	const FName FootprintPinLabel = TEXT("Footprint");
	const FName StyleInfoPinLabel = TEXT("StyleInfo");
	const FName BuildingInfoPinLabel = TEXT("BuildingInfo");
	const FName StreetsPinLabel = TEXT("Streets");
	const FName RoofPinLabel = TEXT("Roof");

	// Reverse counterpart of PCGSelectFacadeStyle.cpp's RoofTypeToString -- spelling only needs to
	// match between the two, not any UI-facing text.
	EGrammarRoofType RoofTypeFromString(const FString& Value)
	{
		if (Value == TEXT("Gabled")) return EGrammarRoofType::Gabled;
		if (Value == TEXT("Hipped")) return EGrammarRoofType::Hipped;
		if (Value == TEXT("Pyramid")) return EGrammarRoofType::Pyramid;
		return EGrammarRoofType::Flat;
	}

	// Reverse counterpart of PCGSelectFacadeStyle.cpp's RidgeAlignmentToString.
	EGrammarRidgeAlignment RidgeAlignmentFromString(const FString& Value)
	{
		return (Value == TEXT("ClosestStreet")) ? EGrammarRidgeAlignment::ClosestStreet : EGrammarRidgeAlignment::LongestAxis;
	}

	FString GetTrimmedLower(const TMap<FString, FString>& Tags, const TCHAR* Key)
	{
		const FString* Value = Tags.Find(Key);
		return Value ? Value->TrimStartAndEnd().ToLower() : FString();
	}

	// Port of GrammarRoofDirection::DirectionFromCardinalOrAngle (GrammarRoofDirection.cpp:26-70,
	// private/non-exported -- ported by value, not called directly, same convention as
	// GrammarEngineInternal's own porting elsewhere in this module). X=North, Y=East, matching
	// FLocalTangentPlaneProjection's axis convention that every footprint coordinate in this pipeline
	// is already expressed in -- do not swap without also flipping the projection (classic's own
	// comment documents the exact regression this caused once: every roof:orientation/roof:direction/
	// ridge_direction-tagged building's ridge silently rotated 90 degrees from what the tag specified).
	FVector2D DirectionFromCardinalOrAngle(const FString& Value)
	{
		const FString Normalized = Value.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT("-"));
		static const TMap<FString, FVector2D> Cardinal = {
			{ TEXT("n"), FVector2D(1.0, 0.0) }, { TEXT("s"), FVector2D(1.0, 0.0) },
			{ TEXT("north"), FVector2D(1.0, 0.0) }, { TEXT("south"), FVector2D(1.0, 0.0) },
			{ TEXT("e"), FVector2D(0.0, 1.0) }, { TEXT("w"), FVector2D(0.0, 1.0) },
			{ TEXT("east"), FVector2D(0.0, 1.0) }, { TEXT("west"), FVector2D(0.0, 1.0) },
			{ TEXT("n-s"), FVector2D(1.0, 0.0) }, { TEXT("north-south"), FVector2D(1.0, 0.0) }, { TEXT("s-n"), FVector2D(1.0, 0.0) },
			{ TEXT("e-w"), FVector2D(0.0, 1.0) }, { TEXT("east-west"), FVector2D(0.0, 1.0) }, { TEXT("w-e"), FVector2D(0.0, 1.0) },
			{ TEXT("ne-sw"), FVector2D(1.0, 1.0).GetSafeNormal() },
			{ TEXT("sw-ne"), FVector2D(1.0, 1.0).GetSafeNormal() },
			{ TEXT("nw-se"), FVector2D(1.0, -1.0).GetSafeNormal() },
			{ TEXT("se-nw"), FVector2D(1.0, -1.0).GetSafeNormal() },
		};
		if (const FVector2D* Found = Cardinal.Find(Normalized))
		{
			return *Found;
		}

		FString AngleString = Normalized;
		if (AngleString.EndsWith(TEXT("deg")))
		{
			AngleString.LeftChopInline(3);
		}
		AngleString = AngleString.Replace(TEXT("°"), TEXT(""));
		if (AngleString.IsEmpty() || !FCString::IsNumeric(*AngleString))
		{
			return FVector2D::ZeroVector;
		}
		const double AngleDegrees = FCString::Atod(*AngleString);
		const double AngleRadians = FMath::DegreesToRadians(AngleDegrees);
		// Compass bearing convention (0deg = North = +X, 90deg = East = +Y here) -- see this
		// function's Cardinal table comment for the axis convention this must match.
		return FVector2D(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians)).GetSafeNormal();
	}

	// Port of GrammarRoofDirection::RoofOrientationDirection (tier 1) -- explicit roof:orientation/
	// roof:direction tag: "along"/"across" relative to the footprint's own longest edge, or a
	// cardinal/compass value via DirectionFromCardinalOrAngle above. Zero vector if neither tag is
	// present (or the tag couldn't be parsed).
	FVector2D RoofOrientationDirection(const FVector2D& LongestAxis, const TMap<FString, FString>& Tags)
	{
		FString Orientation = GetTrimmedLower(Tags, TEXT("roof:orientation"));
		if (Orientation.IsEmpty())
		{
			Orientation = GetTrimmedLower(Tags, TEXT("roof:direction"));
		}
		if (Orientation.IsEmpty())
		{
			return FVector2D::ZeroVector;
		}
		if (Orientation == TEXT("along") || Orientation == TEXT("parallel") || Orientation == TEXT("longitudinal"))
		{
			return LongestAxis;
		}
		if (Orientation == TEXT("across") || Orientation == TEXT("perpendicular") || Orientation == TEXT("transverse"))
		{
			return FVector2D(-LongestAxis.Y, LongestAxis.X);
		}
		return DirectionFromCardinalOrAngle(Orientation);
	}

	// Port of GrammarRoofDirection::RidgeDirectionFromTags (tier 2) -- explicit "X,Y" ridge-direction
	// tag, only consulted (by this node's own caller below) when the resolved style's RidgeAlignment
	// is ClosestStreet.
	FVector2D RidgeDirectionFromTags(const TMap<FString, FString>& Tags)
	{
		for (const TCHAR* Key : { TEXT("grammar:roof:ridge_direction"), TEXT("roof:ridge:direction") })
		{
			const FString* Value = Tags.Find(Key);
			if (!Value || Value->IsEmpty())
			{
				continue;
			}
			TArray<FString> Parts;
			Value->Replace(TEXT(";"), TEXT(",")).ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() < 2)
			{
				continue;
			}
			const FString XStr = Parts[0].TrimStartAndEnd();
			const FString YStr = Parts[1].TrimStartAndEnd();
			if (!FCString::IsNumeric(*XStr) || !FCString::IsNumeric(*YStr))
			{
				continue;
			}
			const FVector2D Direction = FVector2D(FCString::Atod(*XStr), FCString::Atod(*YStr)).GetSafeNormal();
			if (!Direction.IsNearlyZero())
			{
				return Direction;
			}
		}
		return FVector2D::ZeroVector;
	}

	// Port of StreetRidgeAlignment.cpp's HasExplicitRidgeDirectionTag -- gates the street-alignment
	// tier below exactly the way FGrammarStreetAlignment::ApplyRidgeDirectionTags gates its own tag
	// injection: presence alone (even an unparseable value) is enough to skip straight to the
	// footprint fallback rather than trying the street, matching classic's OSM-load-time behavior.
	bool HasExplicitRidgeDirectionTag(const TMap<FString, FString>& Tags)
	{
		for (const TCHAR* Key : { TEXT("roof:orientation"), TEXT("roof:direction"), TEXT("grammar:roof:ridge_direction"), TEXT("roof:ridge:direction") })
		{
			if (const FString* Value = Tags.Find(Key))
			{
				if (!Value->TrimStartAndEnd().IsEmpty())
				{
					return true;
				}
			}
		}
		return false;
	}

	// Footprint's own longest-edge direction -- tier 4 fallback, and also the "along" reference axis
	// for tier 1's along/across resolution. Self-contained (not a call into BuildingGrammarCore's
	// FGrammarGeometry2D::LongestAxisDirection) -- see this node's own header comment.
	FVector2D LongestFootprintEdgeDirection(const TArray<FVector2D>& Footprint)
	{
		FVector2D Direction(1.0, 0.0);
		double BestLengthSquared = -1.0;
		for (int32 Index = 0; Index < Footprint.Num(); ++Index)
		{
			const FVector2D Edge = Footprint[(Index + 1) % Footprint.Num()] - Footprint[Index];
			const double LengthSquared = Edge.SizeSquared();
			if (LengthSquared > BestLengthSquared)
			{
				BestLengthSquared = LengthSquared;
				Direction = Edge;
			}
		}
		if (!Direction.IsNearlyZero())
		{
			Direction.Normalize();
		}
		return Direction;
	}

	// Winding (which vertex order is actually appended per triangle -- controls visibility/culling)
	// and the stored shading normal are fully independent controls -- see
	// UPCGExtrudeFootprintToWallsSettings' equivalent helper for why. The normal is always computed
	// from the fixed (A,B,C) order regardless of bFlipWinding, so the two never interact.
	void AppendTriangleWithComputedNormal(UE::Geometry::FDynamicMesh3& Mesh, UE::Geometry::FDynamicMeshNormalOverlay* Normals, UE::Geometry::FDynamicMeshUVOverlay* UVs,
		const FVector& A, const FVector& B, const FVector& C, double TextureScale, bool bFlipWinding, bool bFlipNormal)
	{
		using namespace UE::Geometry;

		FVector3f Normal(-FVector::CrossProduct(B - A, C - A).GetSafeNormal());
		if (bFlipNormal)
		{
			Normal = -Normal;
		}

		const int32 IA = Mesh.AppendVertex(A);
		const int32 IB = Mesh.AppendVertex(B);
		const int32 IC = Mesh.AppendVertex(C);
		const int32 TriID = bFlipWinding ? Mesh.AppendTriangle(IA, IC, IB) : Mesh.AppendTriangle(IA, IB, IC);
		if (TriID < 0)
		{
			return;
		}

		const int32 NA = Normals->AppendElement(Normal);
		const int32 NB = Normals->AppendElement(Normal);
		const int32 NC = Normals->AppendElement(Normal);
		Normals->SetTriangle(TriID, bFlipWinding ? FIndex3i(NA, NC, NB) : FIndex3i(NA, NB, NC));

		// Flat planar UV (world XY / TextureScale) -- correct texel density for a horizontal (Flat)
		// roof; foreshortens along the slope direction for a Gabled roof's sloped side faces, since
		// this doesn't project onto each face's own plane -- see this node's header comment.
		const int32 UA = UVs->AppendElement(FVector2f(A.X / TextureScale, A.Y / TextureScale));
		const int32 UB = UVs->AppendElement(FVector2f(B.X / TextureScale, B.Y / TextureScale));
		const int32 UC = UVs->AppendElement(FVector2f(C.X / TextureScale, C.Y / TextureScale));
		UVs->SetTriangle(TriID, bFlipWinding ? FIndex3i(UA, UC, UB) : FIndex3i(UA, UB, UC));
	}
}

TArray<FPCGPinProperties> UPCGRoofFrameGeneratorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(FootprintPinLabel, EPCGDataType::Spline);
	// Neither is a required pin -- this node works fine without them, falling back to its own
	// Material/EaveHeight.
	Pins.Emplace(StyleInfoPinLabel, EPCGDataType::Param);
	Pins.Emplace(BuildingInfoPinLabel, EPCGDataType::Param);
	Pins.Emplace(StreetsPinLabel, EPCGDataType::Spline);
	return Pins;
}

TArray<FPCGPinProperties> UPCGRoofFrameGeneratorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(RoofPinLabel, EPCGDataType::DynamicMesh);
	return Pins;
}

FPCGElementPtr UPCGRoofFrameGeneratorSettings::CreateElement() const
{
	return MakeShared<FPCGRoofFrameGeneratorElement>();
}

bool FPCGRoofFrameGeneratorElement::ExecuteInternal(FPCGContext* Context) const
{
	using namespace UE::Geometry;

	const UPCGRoofFrameGeneratorSettings* Settings = Context->GetInputSettings<UPCGRoofFrameGeneratorSettings>();
	check(Settings);

	// At most one StyleInfo connection is expected (see UPCGFacadeWindowDoorLayoutSettings' own
	// identical pattern).
	const UPCGParamData* StyleInfo = nullptr;
	for (const FPCGTaggedData& StyleData : Context->InputData.GetInputsByPin(StyleInfoPinLabel))
	{
		if (const UPCGParamData* Param = Cast<UPCGParamData>(StyleData.Data.Get()))
		{
			StyleInfo = Param;
			break;
		}
	}
	const UPCGMetadata* StyleMetadata = StyleInfo ? StyleInfo->ConstMetadata() : nullptr;
	const FPCGMetadataAttribute<FString>* StyleNameAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("StyleName")) : nullptr;
	const FPCGMetadataAttribute<FString>* RoofTypeAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("RoofType")) : nullptr;
	const FPCGMetadataAttribute<FString>* RoofMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("RoofMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* RoofColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("RoofColor")) : nullptr;
	const FPCGMetadataAttribute<FString>* RidgeAlignmentAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("RidgeAlignment")) : nullptr;

	// At most one BuildingInfo connection is expected (see StyleInfo's identical pattern above).
	const UPCGParamData* BuildingInfo = nullptr;
	for (const FPCGTaggedData& InfoData : Context->InputData.GetInputsByPin(BuildingInfoPinLabel))
	{
		if (const UPCGParamData* Param = Cast<UPCGParamData>(InfoData.Data.Get()))
		{
			BuildingInfo = Param;
			break;
		}
	}
	const UPCGMetadata* InfoMetadata = BuildingInfo ? BuildingInfo->ConstMetadata() : nullptr;
	const FPCGMetadataAttribute<double>* TotalHeightAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<double>(TEXT("TotalHeight")) : nullptr;
	const FPCGMetadataAttribute<double>* MinHeightAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<double>(TEXT("MinHeight")) : nullptr;
	const FPCGMetadataAttribute<FString>* TagsJsonAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<FString>(TEXT("TagsJson")) : nullptr;
	// A parent building whose footprint has one or more building:part children (see
	// UPCGLoadOsmBuildingVolumesSettings' own header comment on Config.bSkipParentFootprintsWithParts
	// -- PCG defaults this to false so UPCGExtrudeFootprintToWallsSettings' overlap suppression can
	// reconcile parent-vs-part walls) gets no roof of its own here -- each part still gets its own,
	// at its own height. Nothing reconciles overlapping ROOF geometry the way that node's wall
	// suppression does, so generating the parent's roof in full here would render it overlapping
	// every part's own roof wherever they coincide in plan view.
	const FPCGMetadataAttribute<bool>* HasBuildingPartsAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<bool>(TEXT("HasBuildingParts")) : nullptr;

	// Gathered once (not per building) -- see UPCGFacadePatternStreetDetailLayoutSettings' identical
	// gathering for why (the street network is shared graph-wide, not per-building data).
	TArray<FStreetSegment> StreetSegments;
	for (const FPCGTaggedData& StreetData : Context->InputData.GetInputsByPin(StreetsPinLabel))
	{
		const UPCGSplineData* StreetSpline = Cast<UPCGSplineData>(StreetData.Data.Get());
		if (!StreetSpline)
		{
			continue;
		}
		const FString StreetName = ExtractStreetNameFromTags(StreetData.Tags);
		const TArray<FSplinePoint> StreetPoints = StreetSpline->GetSplinePoints();
		for (int32 Index = 0; Index + 1 < StreetPoints.Num(); ++Index)
		{
			FStreetSegment& Segment = StreetSegments.AddDefaulted_GetRef();
			Segment.Start = StreetPoints[Index].Position;
			Segment.End = StreetPoints[Index + 1].Position;
			Segment.Name = StreetName;
		}
	}

	UMaterialInterface* FallbackMaterial = Settings->Material.LoadSynchronous();

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(FootprintPinLabel);
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSplineData* SplineData = Cast<UPCGSplineData>(Input.Data.Get());
		if (!SplineData)
		{
			continue;
		}

		TArray<FSplinePoint> SplinePoints = SplineData->GetSplinePoints();
		const int32 Count = SplinePoints.Num();
		if (Count < 3)
		{
			continue;
		}

		// Normalize to CCW (viewed from above) so both the flat-cap fan triangulation and the gabled
		// side quads produce consistently outward/upward-facing normals -- see this node's own
		// header comment; this pipeline doesn't reuse BuildingGrammarCore's OrientFootprintCCW.
		double SignedArea = 0.0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector& A = SplinePoints[Index].Position;
			const FVector& B = SplinePoints[(Index + 1) % Count].Position;
			SignedArea += (A.X * B.Y - B.X * A.Y);
		}
		if (SignedArea < 0.0)
		{
			Algo::Reverse(SplinePoints);
		}

		TArray<FVector2D> Footprint2D;
		Footprint2D.Reserve(Count);
		for (const FSplinePoint& Point : SplinePoints)
		{
			Footprint2D.Add(FVector2D(Point.Position.X, Point.Position.Y));
		}

		const FString SourceName = ExtractSourceNameFromTags(Input.Tags);

		// Resolved once per building, before any mesh-building math -- this building's own StyleInfo
		// row (if present) supplies its actual roof Type, not just Material/Color, so two buildings
		// with different resolved styles (Flat vs Gabled) render with genuinely different roof shapes
		// instead of every building always matching whatever this node's own flat Settings say. See
		// UPCGSelectFacadeStyleSettings' own header comment for how RoofType is resolved
		// (bOverrideRoof-else-DefaultRoof, same as RoofMaterial/RoofColor).
		const int64 StyleEntryKey = StyleInfo ? StyleInfo->FindMetadataKey(FName(*SourceName)) : INDEX_NONE;
		EGrammarRoofType EffectiveRoofType = Settings->RoofType;
		if (StyleEntryKey != INDEX_NONE && RoofTypeAttr)
		{
			EffectiveRoofType = RoofTypeFromString(RoofTypeAttr->GetValueFromItemKey(StyleEntryKey));
		}

		// No Settings-level fallback (mirrors UPCGFacadeWindowDoorLayoutSettings' own Ledge/Balcony/
		// Shutter fields) -- defaults to ClosestStreet, matching FRoofStyleConfig's own default,
		// whenever StyleInfo is unconnected or has no row for this building.
		EGrammarRidgeAlignment EffectiveRidgeAlignment = EGrammarRidgeAlignment::ClosestStreet;
		if (StyleEntryKey != INDEX_NONE && RidgeAlignmentAttr)
		{
			EffectiveRidgeAlignment = RidgeAlignmentFromString(RidgeAlignmentAttr->GetValueFromItemKey(StyleEntryKey));
		}

		// This building's own OSM-derived TotalHeight (see UPCGLoadOsmBuildingVolumesSettings'
		// header comment) if BuildingInfo is connected and has a usable row, else this node's own
		// flat EaveHeight -- see this class's header comment. MinHeight is added on top regardless
		// (0 if unavailable) -- unlike TotalHeight/Footprint2D (which only ever reads the footprint's
		// X/Y, never baked-in Z), EaveHeight is this node's own independent absolute Z, so it needs
		// this explicit addition to sit on top of a building part's own elevated base the same way
		// UPCGExtrudeFootprintToWallsSettings' walls now do via the footprint spline's own Z.
		double EffectiveEaveHeight = Settings->EaveHeight;
		const int64 InfoEntryKey = BuildingInfo ? BuildingInfo->FindMetadataKey(FName(*SourceName)) : INDEX_NONE;

		// See this element's own comment on HasBuildingPartsAttr above -- a parent with building:part
		// children gets no roof of its own; each part still gets one, at its own height.
		if (InfoEntryKey != INDEX_NONE && HasBuildingPartsAttr && HasBuildingPartsAttr->GetValueFromItemKey(InfoEntryKey))
		{
			continue;
		}

		if (InfoEntryKey != INDEX_NONE && TotalHeightAttr)
		{
			const double TotalHeight = TotalHeightAttr->GetValueFromItemKey(InfoEntryKey);
			if (TotalHeight > 0.0)
			{
				EffectiveEaveHeight = TotalHeight;
			}
			if (MinHeightAttr)
			{
				EffectiveEaveHeight += MinHeightAttr->GetValueFromItemKey(InfoEntryKey);
			}
		}

		// This building's raw OSM tags -- only actually needed for the gabled ridge-direction tiers
		// below, but resolved unconditionally alongside the other per-building lookups above.
		TMap<FString, FString> Tags;
		if (InfoEntryKey != INDEX_NONE && TagsJsonAttr)
		{
			Tags = DeserializeTagsFromJson(TagsJsonAttr->GetValueFromItemKey(InfoEntryKey));
		}

		const TArray<FVector> Base = FGrammarRoofFrameMath::RoofBaseVertices(Footprint2D, EffectiveEaveHeight, Settings->Overhang);

		FDynamicMesh3 RoofMesh;
		RoofMesh.EnableAttributes();
		FDynamicMeshNormalOverlay* Normals = RoofMesh.Attributes()->PrimaryNormals();
		FDynamicMeshUVOverlay* UVs = RoofMesh.Attributes()->PrimaryUV();

		const double TextureScale = FMath::Max(Settings->TextureScale, 1.0);

		// Ridge direction -- port of GrammarRoofDirection::RidgeDirection, see this node's own header
		// comment for the full 4-tier priority this reproduces. Only actually evaluated below for
		// Gabled, and for Hipped when it doesn't fall back to Pyramid (see that branch).
		const FVector2D LongestAxis = LongestFootprintEdgeDirection(Footprint2D);
		auto ResolveRidgeDirection = [&]() -> FVector2D
		{
			FVector2D Direction = RoofOrientationDirection(LongestAxis, Tags);
			if (!Direction.IsNearlyZero())
			{
				return Direction;
			}
			if (EffectiveRidgeAlignment == EGrammarRidgeAlignment::ClosestStreet)
			{
				Direction = RidgeDirectionFromTags(Tags);
				if (!Direction.IsNearlyZero())
				{
					return Direction;
				}
				if (!HasExplicitRidgeDirectionTag(Tags) && StreetSegments.Num() > 0)
				{
					FVector2D StreetDirection = FVector2D::ZeroVector;
					bool bFoundStreetDirection = false;

					// addr:street name match ignores StreetSearchRadius (SearchRadius < 0 disables the
					// check) -- same priority as FGrammarStreetAlignment::ApplyRidgeDirectionTags' own
					// addr:street tier.
					if (const FString* AddrStreet = Tags.Find(TEXT("addr:street")))
					{
						if (!AddrStreet->IsEmpty())
						{
							const FString Normalized = AddrStreet->TrimStartAndEnd().ToLower();
							TArray<const FStreetSegment*> Matching;
							for (const FStreetSegment& Segment : StreetSegments)
							{
								if (!Segment.Name.IsEmpty() && Segment.Name.TrimStartAndEnd().ToLower() == Normalized)
								{
									Matching.Add(&Segment);
								}
							}
							if (Matching.Num() > 0)
							{
								bFoundStreetDirection = FindNearestStreetDirection(Footprint2D, Matching, -1.0, StreetDirection);
							}
						}
					}

					if (!bFoundStreetDirection)
					{
						TArray<const FStreetSegment*> AllSegments;
						AllSegments.Reserve(StreetSegments.Num());
						for (const FStreetSegment& Segment : StreetSegments)
						{
							AllSegments.Add(&Segment);
						}
						bFoundStreetDirection = FindNearestStreetDirection(Footprint2D, AllSegments, Settings->StreetSearchRadius, StreetDirection);
					}

					if (bFoundStreetDirection)
					{
						Direction = StreetDirection;
					}
				}
			}
			return Direction.IsNearlyZero() ? LongestAxis : Direction;
		};

		// Port of PyramidRoofMesh -- also HippedRoofMesh's own fallback for near-square footprints
		// (GrammarRoof.cpp:74-77), so shared here as a lambda rather than a separate Pyramid-only
		// code path.
		auto AppendPyramidRoof = [&]()
		{
			FVector2D Center = FVector2D::ZeroVector;
			for (const FVector& Point : Base)
			{
				Center += FVector2D(Point.X, Point.Y);
			}
			Center /= static_cast<double>(FMath::Max(Base.Num(), 1));
			const FVector Peak(Center.X, Center.Y, EffectiveEaveHeight + Settings->RidgeHeight);
			for (int32 Index = 0; Index < Base.Num(); ++Index)
			{
				const int32 NextIndex = (Index + 1) % Base.Num();
				AppendTriangleWithComputedNormal(RoofMesh, Normals, UVs, Base[Index], Base[NextIndex], Peak, TextureScale, Settings->bFlipWinding, Settings->bFlipNormals);
			}
		};

		switch (EffectiveRoofType)
		{
		case EGrammarRoofType::Gabled:
		{
			const FVector2D RidgeDirection = ResolveRidgeDirection();
			const FGrammarRoofFrame Frame = FGrammarRoofFrameMath::BuildFrame(Base, RidgeDirection, EffectiveEaveHeight, EffectiveEaveHeight + Settings->RidgeHeight);

			TArray<FVector> Ridge;
			Ridge.Reserve(Base.Num());
			for (const FVector& Point : Base)
			{
				Ridge.Add(FGrammarRoofFrameMath::RidgeProjection(FVector2D(Point.X, Point.Y), Frame));
			}

			for (int32 Index = 0; Index < Base.Num(); ++Index)
			{
				const int32 NextIndex = (Index + 1) % Base.Num();
				AppendTriangleWithComputedNormal(RoofMesh, Normals, UVs, Base[Index], Base[NextIndex], Ridge[NextIndex], TextureScale, Settings->bFlipWinding, Settings->bFlipNormals);
				AppendTriangleWithComputedNormal(RoofMesh, Normals, UVs, Base[Index], Ridge[NextIndex], Ridge[Index], TextureScale, Settings->bFlipWinding, Settings->bFlipNormals);
			}
			break;
		}
		case EGrammarRoofType::Hipped:
		{
			// Straight-skeleton hip roof (FGrammarRoofSkeleton) -- see GrammarRoof.cpp's
			// HippedRoofMesh (BuildingGrammarCore, this branch's shared counterpart, called
			// identically from the classic engine) for the full algorithm description. A true
			// frustum: every footprint edge recedes inward at unit speed until it either converges
			// to a ridge/apex point or is capped at Settings->RidgeHeight (unit pitch: 1 unit of
			// inward recession = 1 unit of height), robustly handling non-convex (L/T/U-shaped)
			// footprints via the skeleton's own split-event handling.
			TArray<FVector2D> BaseXY;
			BaseXY.Reserve(Base.Num());
			for (const FVector& Point : Base)
			{
				BaseXY.Add(FVector2D(Point.X, Point.Y));
			}

			FGrammarRoofSkeleton::FResult Skeleton;
			if (!FGrammarRoofSkeleton::Build(BaseXY, Settings->RidgeHeight, Skeleton) || Skeleton.Faces.Num() == 0)
			{
				// Degenerate footprint (skeleton couldn't be built at all) -- fall back to Pyramid
				// rather than emitting no roof, same fallback GrammarRoof.cpp's HippedRoofMesh uses.
				AppendPyramidRoof();
				break;
			}

			TArray<FVector> SkeletonVertices;
			SkeletonVertices.Reserve(Skeleton.Nodes.Num());
			for (const FGrammarRoofSkeleton::FNode& Node : Skeleton.Nodes)
			{
				SkeletonVertices.Add(FVector(Node.Position.X, Node.Position.Y, EffectiveEaveHeight + Node.Distance));
			}

			auto AppendSkeletonFace = [&](const TArray<int32>& NodeIndices)
			{
				const TArray<int32> Triangles = FGrammarPolygonTriangulator::Triangulate(SkeletonVertices, FGrammarFace(NodeIndices));
				for (int32 TriIndex = 0; TriIndex + 2 < Triangles.Num(); TriIndex += 3)
				{
					AppendTriangleWithComputedNormal(RoofMesh, Normals, UVs, SkeletonVertices[Triangles[TriIndex]], SkeletonVertices[Triangles[TriIndex + 1]], SkeletonVertices[Triangles[TriIndex + 2]], TextureScale, Settings->bFlipWinding, Settings->bFlipNormals);
				}
			};

			for (const FGrammarRoofSkeleton::FFace& Face : Skeleton.Faces)
			{
				AppendSkeletonFace(Face.NodeIndices);
			}
			for (const TArray<int32>& TopRing : Skeleton.TopRings)
			{
				AppendSkeletonFace(TopRing);
			}
			break;
		}
		case EGrammarRoofType::Pyramid:
			AppendPyramidRoof();
			break;
		case EGrammarRoofType::Flat:
		default:
		{
			// Port of FlatRoofMesh, which emits the whole footprint as a single N-gon face and relies
			// on FGrammarPolygonTriangulator's ear-clipping to triangulate it (GrammarRoof.cpp:113-126,
			// GrammarPolygonTriangulator.h) -- a plain fan from vertex 0 (this node's own approach
			// before this fix) produces triangles that cross outside the outline for any non-convex
			// footprint (L-shaped/U-shaped buildings are common in OSM data), which is exactly what
			// was reported as roof mesh "hanging over" the footprint at inward corners.
			TArray<int32> Indices;
			Indices.Reserve(Base.Num());
			for (int32 Index = 0; Index < Base.Num(); ++Index)
			{
				Indices.Add(Index);
			}
			const TArray<int32> Triangles = FGrammarPolygonTriangulator::Triangulate(Base, FGrammarFace(MoveTemp(Indices)));
			for (int32 TriIndex = 0; TriIndex + 2 < Triangles.Num(); TriIndex += 3)
			{
				AppendTriangleWithComputedNormal(RoofMesh, Normals, UVs, Base[Triangles[TriIndex]], Base[Triangles[TriIndex + 1]], Base[Triangles[TriIndex + 2]], TextureScale, Settings->bFlipWinding, Settings->bFlipNormals);
			}
			break;
		}
		}

		// Every triangle uses material slot 0 -- see UPCGExtrudeFootprintToWallsSettings' equivalent
		// setup for why this is needed at all (no MaterialID attribute at all otherwise, which
		// renders with the engine's default material regardless of what's passed to Initialize).
		RoofMesh.Attributes()->EnableMaterialID();
		FDynamicMeshMaterialAttribute* MaterialIDs = RoofMesh.Attributes()->GetMaterialID();
		for (const int32 TriangleID : RoofMesh.TriangleIndicesItr())
		{
			MaterialIDs->SetValue(TriangleID, 0);
		}

		// Resolve this building's own roof material from StyleInfo's row -- see
		// UPCGExtrudeFootprintToWallsSettings' identical wall-material resolution for why.
		UMaterialInterface* RoofMaterial = FallbackMaterial;
		if (StyleEntryKey != INDEX_NONE && RoofMaterialAttr && RoofColorAttr)
		{
			const FString StyleName = StyleNameAttr ? StyleNameAttr->GetValueFromItemKey(StyleEntryKey) : FString();
			const FString MaterialName = RoofMaterialAttr->GetValueFromItemKey(StyleEntryKey);
			const FVector4 ColorValue = RoofColorAttr->GetValueFromItemKey(StyleEntryKey);
			const FLinearColor Color(ColorValue.X, ColorValue.Y, ColorValue.Z, ColorValue.W);
			if (UMaterialInterface* Resolved = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("roof"), MaterialName, Color))
			{
				RoofMaterial = Resolved;
			}
		}

		UPCGDynamicMeshData* RoofData = FPCGContext::NewObject_AnyThread<UPCGDynamicMeshData>(Context);
		RoofData->Initialize(MoveTemp(RoofMesh), { RoofMaterial });

		FPCGTaggedData& RoofOut = Context->OutputData.TaggedData.Emplace_GetRef();
		RoofOut.Data = RoofData;
		RoofOut.Pin = RoofPinLabel;
		RoofOut.Tags = Input.Tags;
	}

	return true;
}
