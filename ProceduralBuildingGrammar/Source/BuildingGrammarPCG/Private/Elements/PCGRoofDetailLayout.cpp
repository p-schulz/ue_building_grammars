#include "Elements/PCGRoofDetailLayout.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Geometry/GrammarRoofFrame.h"
#include "GrammarKitResolver.h"
#include "PCGBuildingGrammarDefaults.h"
#include "Math/RotationMatrix.h"
#include "Algo/Reverse.h"
#include "UObject/SoftObjectPath.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const FName FootprintPinLabel = TEXT("Footprint");
	const FName StyleInfoPinLabel = TEXT("StyleInfo");
	const FName BuildingInfoPinLabel = TEXT("BuildingInfo");
	const FName PlacementsPinLabel = TEXT("Placements");

	// Same "rotate tangent -90 degrees" CCW-outward-normal formula UPCGExtrudeFootprintToWallsSettings
	// already uses -- see that node's header comment.
	FVector2D OutwardNormal2D(const FVector2D& Tangent)
	{
		return FVector2D(Tangent.Y, -Tangent.X);
	}

	// Footprint's own longest edge direction -- matches UPCGRoofFrameGeneratorSettings' own inline
	// ridge-direction computation exactly (self-contained, not a call into BuildingGrammarCore's
	// FGrammarGeometry2D::LongestAxisDirection -- see that node's header comment for why).
	FVector2D LongestAxisDirection(const TArray<FVector>& Base)
	{
		FVector2D Best(1.0, 0.0);
		double BestLengthSquared = -1.0;
		for (int32 Index = 0; Index < Base.Num(); ++Index)
		{
			const FVector2D A(Base[Index].X, Base[Index].Y);
			const FVector2D B(Base[(Index + 1) % Base.Num()].X, Base[(Index + 1) % Base.Num()].Y);
			const FVector2D Edge = B - A;
			const double LengthSquared = Edge.SizeSquared();
			if (LengthSquared > BestLengthSquared)
			{
				BestLengthSquared = LengthSquared;
				Best = Edge;
			}
		}
		if (!Best.IsNearlyZero())
		{
			Best.Normalize();
		}
		return Best;
	}

	FQuat RotationFromTangentNormal(const FVector2D& Tangent, const FVector2D& Normal)
	{
		return FRotationMatrix::MakeFromXY(FVector(Tangent.X, Tangent.Y, 0.0), FVector(Normal.X, Normal.Y, 0.0)).ToQuat();
	}

	// Reverse counterpart of PCGSelectFacadeStyle.cpp's RoofTypeToString -- spelling only needs to
	// match between the two, not any UI-facing text.
	EGrammarRoofType RoofTypeFromString(const FString& Value)
	{
		if (Value == TEXT("Gabled")) return EGrammarRoofType::Gabled;
		if (Value == TEXT("Hipped")) return EGrammarRoofType::Hipped;
		if (Value == TEXT("Pyramid")) return EGrammarRoofType::Pyramid;
		return EGrammarRoofType::Flat;
	}
}

TArray<FPCGPinProperties> UPCGRoofDetailLayoutSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(FootprintPinLabel, EPCGDataType::Spline);
	// Neither is a required pin -- this node works fine without them, falling back to its own
	// Settings.
	Pins.Emplace(StyleInfoPinLabel, EPCGDataType::Param);
	Pins.Emplace(BuildingInfoPinLabel, EPCGDataType::Param);
	return Pins;
}

TArray<FPCGPinProperties> UPCGRoofDetailLayoutSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(PlacementsPinLabel, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGRoofDetailLayoutSettings::CreateElement() const
{
	return MakeShared<FPCGRoofDetailLayoutElement>();
}

bool FPCGRoofDetailLayoutElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPCGRoofDetailLayoutSettings* Settings = Context->GetInputSettings<UPCGRoofDetailLayoutSettings>();
	check(Settings);

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
	const FPCGMetadataAttribute<FString>* EdgeMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("EdgeMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* EdgeColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("EdgeColor")) : nullptr;
	const FPCGMetadataAttribute<FString>* TileMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("TileMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* TileColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("TileColor")) : nullptr;
	const FPCGMetadataAttribute<FString>* RoofWindowMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("RoofWindowMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* RoofWindowColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("RoofWindowColor")) : nullptr;
	const FPCGMetadataAttribute<FString>* DormerMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("DormerMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* DormerColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("DormerColor")) : nullptr;
	const FPCGMetadataAttribute<FString>* ChimneyMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("ChimneyMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* ChimneyColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("ChimneyColor")) : nullptr;

	UMaterialInterface* FallbackEdgeMaterial = Settings->EdgeMaterial.LoadSynchronous();
	UMaterialInterface* FallbackTileMaterial = Settings->TileMaterial.LoadSynchronous();
	UMaterialInterface* FallbackRoofWindowMaterial = Settings->RoofWindowMaterial.LoadSynchronous();
	UMaterialInterface* FallbackDormerMaterial = Settings->DormerMaterial.LoadSynchronous();
	UMaterialInterface* FallbackChimneyMaterial = Settings->ChimneyMaterial.LoadSynchronous();

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
	const FPCGMetadataAttribute<bool>* HasBuildingPartsAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<bool>(TEXT("HasBuildingParts")) : nullptr;

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

		// See UPCGRoofFrameGeneratorSettings' identical skip and its own comment on
		// HasBuildingPartsAttr -- a parent with building:part children gets no roof (and so no roof
		// details -- tiles, dormers, chimneys, roof windows -- either) here; each part gets its own.
		if (HasBuildingPartsAttr)
		{
			const int64 InfoEntryKeyForSkipCheck = BuildingInfo ? BuildingInfo->FindMetadataKey(FName(*ExtractSourceNameFromTags(Input.Tags))) : INDEX_NONE;
			if (InfoEntryKeyForSkipCheck != INDEX_NONE && HasBuildingPartsAttr->GetValueFromItemKey(InfoEntryKeyForSkipCheck))
			{
				continue;
			}
		}

		// Resolved once per building, before any gating/frame math -- see
		// UPCGRoofFrameGeneratorSettings' identical resolution for why (this building's own StyleInfo
		// row, if present, supplies its actual roof Type, not just per-role Material/Color).
		const int64 StyleEntryKey = StyleInfo ? StyleInfo->FindMetadataKey(FName(*ExtractSourceNameFromTags(Input.Tags))) : INDEX_NONE;
		EGrammarRoofType EffectiveRoofType = Settings->RoofType;
		if (StyleEntryKey != INDEX_NONE && RoofTypeAttr)
		{
			EffectiveRoofType = RoofTypeFromString(RoofTypeAttr->GetValueFromItemKey(StyleEntryKey));
		}

		const bool bFlat = (EffectiveRoofType == EGrammarRoofType::Flat);
		const bool bDormersAllowed = (EffectiveRoofType == EGrammarRoofType::Gabled || EffectiveRoofType == EGrammarRoofType::Hipped);

		// This building's own OSM-derived TotalHeight (see UPCGLoadOsmBuildingVolumesSettings'
		// header comment) if BuildingInfo is connected and has a usable row, else this node's own
		// flat EaveHeight -- see this class's header comment. MinHeight is added on top regardless
		// (0 if unavailable) -- see UPCGRoofFrameGeneratorSettings' identical addition for why.
		double EffectiveEaveHeight = Settings->EaveHeight;
		if (BuildingInfo && TotalHeightAttr)
		{
			const int64 InfoEntryKey = BuildingInfo->FindMetadataKey(FName(*ExtractSourceNameFromTags(Input.Tags)));
			if (InfoEntryKey != INDEX_NONE)
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
		}

		const TArray<FVector> Base = FGrammarRoofFrameMath::RoofBaseVertices(Footprint2D, EffectiveEaveHeight, Settings->Overhang);
		const FVector2D Direction = LongestAxisDirection(Base);
		const double RidgeZ = bFlat ? EffectiveEaveHeight : (EffectiveEaveHeight + Settings->RidgeHeight);
		const FGrammarRoofFrame Frame = FGrammarRoofFrameMath::BuildFrame(Base, Direction, EffectiveEaveHeight, RidgeZ);
		const double LongSpan = Frame.MaxLong - Frame.MinLong;
		const double SideSpan = Frame.MaxSide - Frame.MinSide;

		// Resolve this building's own StyleName + per-role materials once -- see
		// UPCGRoofFrameGeneratorSettings' identical resolution for why (persistent per-style asset via
		// FGrammarKitResolver::ResolveMaterial, falling back to this node's own flat Settings Material
		// whenever StyleInfo is unconnected or has no row for this building).
		FString StyleName;
		UMaterialInterface* EdgeMaterialResolved = FallbackEdgeMaterial;
		UMaterialInterface* TileMaterialResolved = FallbackTileMaterial;
		UMaterialInterface* RoofWindowMaterialResolved = FallbackRoofWindowMaterial;
		UMaterialInterface* DormerMaterialResolved = FallbackDormerMaterial;
		UMaterialInterface* ChimneyMaterialResolved = FallbackChimneyMaterial;
		UMaterialInterface* GutterMaterialResolved = nullptr;

		if (StyleEntryKey != INDEX_NONE)
		{
			StyleName = StyleNameAttr ? StyleNameAttr->GetValueFromItemKey(StyleEntryKey) : FString();

			auto ResolveRole = [&](const FPCGMetadataAttribute<FString>* MaterialAttr, const FPCGMetadataAttribute<FVector4>* ColorAttr, const TCHAR* Role, UMaterialInterface* Fallback) -> UMaterialInterface*
			{
				if (!MaterialAttr || !ColorAttr)
				{
					return Fallback;
				}
				const FString MaterialName = MaterialAttr->GetValueFromItemKey(StyleEntryKey);
				const FVector4 ColorValue = ColorAttr->GetValueFromItemKey(StyleEntryKey);
				const FLinearColor Color(ColorValue.X, ColorValue.Y, ColorValue.Z, ColorValue.W);
				UMaterialInterface* Resolved = FGrammarKitResolver::ResolveMaterial(StyleName, Role, MaterialName, Color);
				return Resolved ? Resolved : Fallback;
			};

			EdgeMaterialResolved = ResolveRole(EdgeMaterialAttr, EdgeColorAttr, TEXT("roof_edge"), FallbackEdgeMaterial);
			TileMaterialResolved = ResolveRole(TileMaterialAttr, TileColorAttr, TEXT("roof_tile"), FallbackTileMaterial);
			RoofWindowMaterialResolved = ResolveRole(RoofWindowMaterialAttr, RoofWindowColorAttr, TEXT("roof_window"), FallbackRoofWindowMaterial);
			DormerMaterialResolved = ResolveRole(DormerMaterialAttr, DormerColorAttr, TEXT("dormer"), FallbackDormerMaterial);
			ChimneyMaterialResolved = ResolveRole(ChimneyMaterialAttr, ChimneyColorAttr, TEXT("chimney"), FallbackChimneyMaterial);
		}
		// Gutters are never style-driven in classic (hardcoded name/color in
		// GrammarRoofDetails.cpp's GutterPlacements) -- still resolved via ResolveMaterial using
		// whatever StyleName is available (possibly empty -> "Default" folder) so the asset lands in
		// the right per-style Content Browser location, matching classic's own reasoning.
		GutterMaterialResolved = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("roof_gutter"), TEXT("Grammar Roof Gutters"), FLinearColor(0.18, 0.18, 0.17, 1.0));

		UPCGPointData* PlacementData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		UPCGMetadata* PlacementMetadata = PlacementData->MutableMetadata();
		FPCGMetadataAttribute<FString>* RoleAttr = PlacementMetadata->CreateAttribute<FString>(TEXT("Role"), FString(), false, false);
		FPCGMetadataAttribute<double>* WidthAttr = PlacementMetadata->CreateAttribute<double>(TEXT("Width"), 0.0, false, false);
		FPCGMetadataAttribute<double>* HeightAttr = PlacementMetadata->CreateAttribute<double>(TEXT("Height"), 0.0, false, false);
		FPCGMetadataAttribute<FSoftObjectPath>* MaterialOverrideAttr = PlacementMetadata->CreateAttribute<FSoftObjectPath>(TEXT("MaterialOverride"), FSoftObjectPath(), false, false);

		// Same box CENTER + (Width,Depth,Height) Scale convention as every other layout node in this
		// module -- see UPCGFacadeWindowDoorLayoutSettings' header comment.
		auto MakePlacementPoint = [&](const FQuat& Rotation, const FVector& WorldCenter, double Width, double Height, double Depth, const TCHAR* Role, UMaterialInterface* Material)
		{
			FPCGPoint Point;
			Point.Transform = FTransform(Rotation, WorldCenter, FVector(Width, Depth, Height));
			Point.Density = 1.0f;
			Point.MetadataEntry = PlacementMetadata->AddEntry();
			RoleAttr->SetValue(Point.MetadataEntry, FString(Role));
			WidthAttr->SetValue(Point.MetadataEntry, Width);
			HeightAttr->SetValue(Point.MetadataEntry, Height);
			if (Material)
			{
				MaterialOverrideAttr->SetValue(Point.MetadataEntry, FSoftObjectPath(Material));
			}
			PlacementData->GetMutablePoints().Add(Point);
		};

		// ---- Roof edge/corner trim (RoofEdgePlacements) -- Flat roofs only ----
		if (bFlat && Settings->bEdgeEnabled && Settings->EdgeWidth > 0.0 && Settings->EdgeHeight > 0.0)
		{
			const double EdgeBottomZ = EffectiveEaveHeight - FMath::Max(Settings->SurfaceInset, 0.0);
			for (int32 Index = 0; Index < Base.Num(); ++Index)
			{
				const FVector2D A(Base[Index].X, Base[Index].Y);
				const FVector2D B(Base[(Index + 1) % Base.Num()].X, Base[(Index + 1) % Base.Num()].Y);
				const FVector2D Edge = B - A;
				const double Length = Edge.Size();
				if (Length <= 0.0)
				{
					continue;
				}
				const FVector2D Tangent = Edge / Length;
				const FVector2D Normal = OutwardNormal2D(Tangent);
				const FVector2D Mid = (A + B) * 0.5 + Normal * (Settings->EdgeWidth * 0.5);
				const FVector Center(Mid.X, Mid.Y, EdgeBottomZ + Settings->EdgeHeight * 0.5);
				MakePlacementPoint(RotationFromTangentNormal(Tangent, Normal), Center, Length, Settings->EdgeHeight, Settings->EdgeWidth, TEXT("roof_edge"), EdgeMaterialResolved);
			}

			// Corner caps: normal is the (normalized) average of the two adjacent edges' outward
			// normals (falls back to the next edge's normal if degenerate), tangent runs prev->next.
			const double CapSize = FMath::Max(Settings->CornerCapSize, Settings->EdgeWidth);
			for (int32 Index = 0; Index < Base.Num(); ++Index)
			{
				const FVector2D Prev = Footprint2D[(Index - 1 + Footprint2D.Num()) % Footprint2D.Num()];
				const FVector2D Point = Footprint2D[Index];
				const FVector2D Next = Footprint2D[(Index + 1) % Footprint2D.Num()];

				const FVector2D PrevEdge = Point - Prev;
				const FVector2D NextEdge = Next - Point;
				const FVector2D PrevNormal = PrevEdge.IsNearlyZero() ? FVector2D::ZeroVector : OutwardNormal2D(PrevEdge.GetSafeNormal());
				const FVector2D NextNormal = NextEdge.IsNearlyZero() ? FVector2D::ZeroVector : OutwardNormal2D(NextEdge.GetSafeNormal());
				FVector2D Normal = PrevNormal + NextNormal;
				Normal = Normal.IsNearlyZero() ? NextNormal : Normal.GetSafeNormal();
				FVector2D Tangent = (Next - Prev);
				Tangent = Tangent.IsNearlyZero() ? FVector2D(1.0, 0.0) : Tangent.GetSafeNormal();

				const FVector2D BasePoint(Base[Index].X, Base[Index].Y);
				const FVector2D Center2D = BasePoint + Normal * (Settings->EdgeWidth * 0.5);
				const FVector Center(Center2D.X, Center2D.Y, EdgeBottomZ + Settings->EdgeHeight * 0.5);
				MakePlacementPoint(RotationFromTangentNormal(Tangent, Normal), Center, CapSize, Settings->EdgeHeight, CapSize, TEXT("roof_edge"), EdgeMaterialResolved);
			}
		}

		// ---- Gutters (GutterPlacements) -- any roof type ----
		{
			const double GutterZ = bFlat
				? EffectiveEaveHeight + FMath::Max(Settings->EdgeHeight - Settings->SurfaceInset, 0.0) * 0.55
				: EffectiveEaveHeight + 3.0;
			for (int32 Index = 0; Index < Base.Num(); ++Index)
			{
				const FVector2D A(Base[Index].X, Base[Index].Y);
				const FVector2D B(Base[(Index + 1) % Base.Num()].X, Base[(Index + 1) % Base.Num()].Y);
				const FVector2D Edge = B - A;
				const double Length = Edge.Size();
				if (Length <= 20.0)
				{
					continue;
				}
				const FVector2D Tangent = Edge / Length;
				const FVector2D Normal = OutwardNormal2D(Tangent);
				const FVector2D Mid = (A + B) * 0.5 + Normal * 8.0;
				// Center Z = GutterZ exactly: classic's Bottom = Z-4cm with Height=8cm gives
				// CenterZ = Bottom + Height/2 = Z-4+4 = Z.
				const FVector Center(Mid.X, Mid.Y, GutterZ);
				MakePlacementPoint(RotationFromTangentNormal(Tangent, Normal), Center, Length, 8.0, 12.0, TEXT("roof_gutter"), GutterMaterialResolved);
			}
		}

		// ---- Roof tile bands (RoofTilePlacements) -- non-Flat, needs a real span ----
		if (Settings->TileRows > 0 && !bFlat && LongSpan > 0.0 && SideSpan > 0.0)
		{
			const double TileWidth = LongSpan * 0.92;
			const double RowDepth = FMath::Max(Settings->TileSpacing * 0.12, Settings->TileDepth);
			const FQuat Rotation = RotationFromTangentNormal(Frame.Direction, Frame.Normal);
			for (const double SideSign : { -1.0, 1.0 })
			{
				const double SideLimit = SideSign > 0.0 ? Frame.MaxSide : Frame.MinSide;
				for (int32 RowIndex = 0; RowIndex < Settings->TileRows; ++RowIndex)
				{
					const double Factor = (RowIndex + 1) / static_cast<double>(Settings->TileRows + 1);
					const double SideValue = SideLimit * (1.0 - Factor * 0.88);
					const double Z = FGrammarRoofFrameMath::RoofSurfaceZ(0.0, SideValue, Frame) + FMath::Max(Settings->TileDepth, 0.0);
					const FVector Center = FGrammarRoofFrameMath::PointFromRoofAxes(Frame, 0.0, SideValue, Z);
					MakePlacementPoint(Rotation, Center, TileWidth, FMath::Max(Settings->TileDepth, 1.0), RowDepth, TEXT("roof_tile"), TileMaterialResolved);
				}
			}
		}

		// ---- Standalone roof windows (RoofWindowPlacements) -- non-Flat ----
		if (Settings->RoofWindowCount > 0 && !bFlat)
		{
			const TArray<double> LongPositions = FGrammarRoofFrameMath::DetailPositions(Settings->RoofWindowCount, Frame.MinLong, Frame.MaxLong, 0.22);
			const FQuat Rotation = RotationFromTangentNormal(Frame.Direction, Frame.Normal);
			for (int32 Index = 0; Index < LongPositions.Num(); ++Index)
			{
				const double LongValue = LongPositions[Index];
				const double SideSign = (Index % 2 == 0) ? -1.0 : 1.0;
				const double SideLimit = SideSign < 0.0 ? Frame.MinSide : Frame.MaxSide;
				const double SideValue = SideLimit * 0.48;
				const double Z = FGrammarRoofFrameMath::RoofSurfaceZ(LongValue, SideValue, Frame) + 4.5;
				const FVector Center = FGrammarRoofFrameMath::PointFromRoofAxes(Frame, LongValue, SideValue, Z);
				MakePlacementPoint(Rotation, Center, Settings->RoofWindowWidth, 3.5, Settings->RoofWindowHeight, TEXT("roof_window"), RoofWindowMaterialResolved);
			}
		}

		// ---- Dormers (+ companion roof window) (DormerPlacements) -- Gabled/Hipped only ----
		if (Settings->DormerCount > 0 && bDormersAllowed)
		{
			const TArray<double> LongPositions = FGrammarRoofFrameMath::DetailPositions(Settings->DormerCount, Frame.MinLong, Frame.MaxLong, 0.25);
			for (int32 Index = 0; Index < LongPositions.Num(); ++Index)
			{
				const double LongValue = LongPositions[Index];
				const double SideSign = (Index % 2 == 0) ? -1.0 : 1.0;
				const double SideLimit = SideSign < 0.0 ? Frame.MinSide : Frame.MaxSide;
				const double SideValue = SideLimit * 0.55;
				const double Z = FGrammarRoofFrameMath::RoofSurfaceZ(LongValue, SideValue, Frame);
				const FVector2D Outward = Frame.Normal * SideSign;
				const FQuat Rotation = RotationFromTangentNormal(Frame.Direction, Outward);

				const FVector DormerCenter = FGrammarRoofFrameMath::PointFromRoofAxes(Frame, LongValue, SideValue, Z + Settings->DormerHeight * 0.5);
				MakePlacementPoint(Rotation, DormerCenter, Settings->DormerWidth, Settings->DormerHeight, Settings->DormerDepth, TEXT("dormer"), DormerMaterialResolved);

				const FVector WindowCenter = FGrammarRoofFrameMath::PointFromRoofAxes(Frame, LongValue, SideValue + SideSign * Settings->DormerDepth * 0.52, Z + Settings->DormerHeight * 0.48);
				MakePlacementPoint(Rotation, WindowCenter, Settings->DormerWidth * 0.45, Settings->DormerHeight * 0.38, 3.5, TEXT("roof_window"), RoofWindowMaterialResolved);
			}
		}

		// ---- Chimneys (ChimneyPlacements) -- any roof type ----
		if (Settings->ChimneyCount > 0)
		{
			const TArray<double> LongPositions = FGrammarRoofFrameMath::DetailPositions(Settings->ChimneyCount, Frame.MinLong, Frame.MaxLong, 0.28);
			const FQuat Rotation = RotationFromTangentNormal(Frame.Direction, Frame.Normal);
			for (int32 Index = 0; Index < LongPositions.Num(); ++Index)
			{
				const double LongValue = LongPositions[Index];
				const double SideValue = (Index % 2 != 0 ? Frame.MaxSide : Frame.MinSide) * 0.18;
				const double Z = FGrammarRoofFrameMath::RoofSurfaceZ(LongValue, SideValue, Frame);
				const FVector Center = FGrammarRoofFrameMath::PointFromRoofAxes(Frame, LongValue, SideValue, Z + Settings->ChimneyHeight * 0.5);
				MakePlacementPoint(Rotation, Center, Settings->ChimneyWidth, Settings->ChimneyHeight, Settings->ChimneyDepth, TEXT("chimney"), ChimneyMaterialResolved);
			}
		}

		FPCGTaggedData& PlacementsOut = Context->OutputData.TaggedData.Emplace_GetRef();
		PlacementsOut.Data = PlacementData;
		PlacementsOut.Pin = PlacementsPinLabel;
		PlacementsOut.Tags = Input.Tags;
	}

	return true;
}
