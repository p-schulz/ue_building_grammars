#include "Elements/PCGFacadeWindowDoorLayout.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "GrammarKitResolver.h"
#include "PCGBuildingGrammarDefaults.h"
#include "UObject/SoftObjectPath.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"

namespace
{
	const FName EdgesPinLabel = TEXT("Edges");
	const FName StyleInfoPinLabel = TEXT("StyleInfo");
	const FName BuildingInfoPinLabel = TEXT("BuildingInfo");
	const FName PlacementsPinLabel = TEXT("Placements");

	// Per-building style values actually used for layout -- either read from StyleInfo's row for
	// this building (if connected and the row exists), or Settings' own fallback values. See this
	// node's header comment for why this correlation has to happen in C++ rather than through PCG's
	// generic per-node "Overrides" pin (which can't vary by item within one node execution).
	struct FResolvedStyle
	{
		FString StyleName;
		double WindowWidth = 0.0;
		double WindowHeight = 0.0;
		double WindowSpacing = 0.0;
		double WindowMargin = 0.0;
		double WindowSillHeight = 0.0;
		double WindowDepth = 0.0;
		double FrameWidth = 0.0;
		double FrameDepth = 0.0;
		int32 VerticalMullions = 0;
		int32 HorizontalMullions = 0;
		double SillDepth = 0.0;
		double SillThickness = 0.0;
		bool bDoorEnabled = false;
		double DoorWidth = 0.0;
		double DoorHeight = 0.0;
		double DoorDepth = 0.0;

		// Ledge/Balcony/Shutter -- see FLedgeStyleConfig/FBalconyStyleConfig's own field comments for
		// what each of these means; no Settings-level fallback exists for any of these (ledges/
		// balconies/shutters are opt-in per-style features with no equivalent flat-Settings concept
		// today), so they're all left at "disabled" defaults whenever StyleInfo is unconnected or has
		// no row for this building.
		bool bLedgeEnabled = false;
		double LedgeDepth = 0.0;
		double LedgeHeight = 0.0;
		int32 LedgeEveryNFloors = 0;
		bool bBalconyEnabled = false;
		double BalconyWidth = 0.0;
		double BalconyDepth = 0.0;
		double BalconySlabHeight = 0.0;
		double BalconyRailingHeight = 0.0;
		int32 BalconyEveryNFloors = 0;
		int32 BalconyRailingBarCount = 0;
		double BalconyRailingBarWidth = 0.0;
		double BalconyRailingBarDepth = 0.0;
		bool bShutterEnabled = false;

		// Resolved once per building (Phase A: one style per building, no per-side rotation -- see
		// UPCGSelectFacadeStyleSettings' own header comment), null if StyleInfo couldn't resolve a
		// material for that role (StyleInfo unconnected, no row, or ResolveMaterial itself failed --
		// see FGrammarKitResolver::ResolveMaterial's own null-return contract). A null Material here
		// means the corresponding placements are left with no MaterialOverride, so the Static Mesh
		// Spawner's own configured material is used instead, unchanged from before this pin existed.
		UMaterialInterface* WindowMaterial = nullptr;
		UMaterialInterface* FrameMaterial = nullptr;
		UMaterialInterface* SillMaterial = nullptr;
		UMaterialInterface* DoorMaterial = nullptr;
		UMaterialInterface* LedgeMaterial = nullptr;
		UMaterialInterface* BalconyMaterial = nullptr;
		UMaterialInterface* BalconyRailingMaterial = nullptr;
		UMaterialInterface* ShutterMaterial = nullptr;
	};

	template <typename T>
	T ResolveValue(const FPCGMetadataAttribute<T>* Attr, int64 EntryKey, T FallbackValue)
	{
		return (Attr && EntryKey != INDEX_NONE) ? Attr->GetValueFromItemKey(EntryKey) : FallbackValue;
	}

	FLinearColor ResolveColor(const FPCGMetadataAttribute<FVector4>* Attr, int64 EntryKey)
	{
		const FVector4 Value = ResolveValue(Attr, EntryKey, FVector4(1.0, 1.0, 1.0, 1.0));
		return FLinearColor(Value.X, Value.Y, Value.Z, Value.W);
	}
}

TArray<FPCGPinProperties> UPCGFacadeWindowDoorLayoutSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(EdgesPinLabel, EPCGDataType::Point);
	// Neither is a required pin -- this node works fine without them, falling back to its own
	// Settings.
	Pins.Emplace(StyleInfoPinLabel, EPCGDataType::Param);
	Pins.Emplace(BuildingInfoPinLabel, EPCGDataType::Param);
	return Pins;
}

TArray<FPCGPinProperties> UPCGFacadeWindowDoorLayoutSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(PlacementsPinLabel, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGFacadeWindowDoorLayoutSettings::CreateElement() const
{
	return MakeShared<FPCGFacadeWindowDoorLayoutElement>();
}

bool FPCGFacadeWindowDoorLayoutElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPCGFacadeWindowDoorLayoutSettings* Settings = Context->GetInputSettings<UPCGFacadeWindowDoorLayoutSettings>();
	check(Settings);

	// At most one StyleInfo connection is expected (UPCGSelectFacadeStyleSettings emits a single
	// attribute set with one row per building) -- if more than one happens to be connected, only the
	// first is consulted.
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
	const FPCGMetadataAttribute<double>* StyleWindowWidthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowWidth")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleWindowHeightAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowHeight")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleWindowSpacingAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowSpacing")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleWindowMarginAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowMargin")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleWindowSillHeightAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowSillHeight")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleWindowDepthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowDepth")) : nullptr;
	const FPCGMetadataAttribute<FString>* StyleWindowMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("WindowMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* StyleWindowColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("WindowColor")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleWindowFrameWidthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowFrameWidth")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleWindowFrameDepthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowFrameDepth")) : nullptr;
	const FPCGMetadataAttribute<FString>* StyleWindowFrameMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("WindowFrameMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* StyleWindowFrameColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("WindowFrameColor")) : nullptr;
	const FPCGMetadataAttribute<int32>* StyleWindowVerticalMullionsAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<int32>(TEXT("WindowVerticalMullions")) : nullptr;
	const FPCGMetadataAttribute<int32>* StyleWindowHorizontalMullionsAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<int32>(TEXT("WindowHorizontalMullions")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleWindowSillDepthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowSillDepth")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleWindowSillThicknessAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowSillThickness")) : nullptr;
	const FPCGMetadataAttribute<FString>* StyleWindowSillMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("WindowSillMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* StyleWindowSillColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("WindowSillColor")) : nullptr;
	const FPCGMetadataAttribute<bool>* StyleDoorEnabledAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<bool>(TEXT("DoorEnabled")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleDoorWidthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("DoorWidth")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleDoorHeightAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("DoorHeight")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleDoorDepthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("DoorDepth")) : nullptr;
	const FPCGMetadataAttribute<FString>* StyleDoorMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("DoorMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* StyleDoorColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("DoorColor")) : nullptr;

	const FPCGMetadataAttribute<bool>* StyleLedgeEnabledAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<bool>(TEXT("LedgeEnabled")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleLedgeDepthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("LedgeDepth")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleLedgeHeightAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("LedgeHeight")) : nullptr;
	const FPCGMetadataAttribute<int32>* StyleLedgeEveryNFloorsAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<int32>(TEXT("LedgeEveryNFloors")) : nullptr;
	const FPCGMetadataAttribute<FString>* StyleLedgeMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("LedgeMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* StyleLedgeColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("LedgeColor")) : nullptr;

	const FPCGMetadataAttribute<bool>* StyleBalconyEnabledAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<bool>(TEXT("BalconyEnabled")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleBalconyWidthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("BalconyWidth")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleBalconyDepthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("BalconyDepth")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleBalconySlabHeightAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("BalconySlabHeight")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleBalconyRailingHeightAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("BalconyRailingHeight")) : nullptr;
	const FPCGMetadataAttribute<int32>* StyleBalconyEveryNFloorsAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<int32>(TEXT("BalconyEveryNFloors")) : nullptr;
	const FPCGMetadataAttribute<FString>* StyleBalconyMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("BalconyMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* StyleBalconyColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("BalconyColor")) : nullptr;
	const FPCGMetadataAttribute<FString>* StyleBalconyRailingMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("BalconyRailingMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* StyleBalconyRailingColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("BalconyRailingColor")) : nullptr;
	const FPCGMetadataAttribute<int32>* StyleBalconyRailingBarCountAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<int32>(TEXT("BalconyRailingBarCount")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleBalconyRailingBarWidthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("BalconyRailingBarWidth")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleBalconyRailingBarDepthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("BalconyRailingBarDepth")) : nullptr;

	const FPCGMetadataAttribute<bool>* StyleShutterEnabledAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<bool>(TEXT("ShutterEnabled")) : nullptr;

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
	const FPCGMetadataAttribute<int32>* LevelsAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<int32>(TEXT("Levels")) : nullptr;
	const FPCGMetadataAttribute<double>* TotalHeightAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<double>(TEXT("TotalHeight")) : nullptr;

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(EdgesPinLabel);
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGPointData* EdgeData = Cast<UPCGPointData>(Input.Data.Get());
		if (!EdgeData)
		{
			continue;
		}

		const UPCGMetadata* EdgeMetadata = EdgeData->ConstMetadata();
		const FPCGMetadataAttribute<double>* LengthAttr = EdgeMetadata ? EdgeMetadata->GetConstTypedAttribute<double>(TEXT("Length")) : nullptr;
		const TArray<FPCGPoint>& EdgePoints = EdgeData->GetPoints();
		if (EdgePoints.Num() == 0 || !LengthAttr)
		{
			continue;
		}

		// This building's own OSM-derived Levels/TotalHeight (see
		// UPCGLoadOsmBuildingVolumesSettings' header comment) if BuildingInfo is connected and has a
		// usable row, else this node's own flat FloorCount/FloorHeight -- see this class's header
		// comment. FloorHeight is derived as TotalHeight/Levels (uniform), matching this node's own
		// pre-existing uniform-floor-height assumption.
		int32 EffectiveFloorCount = Settings->FloorCount;
		double EffectiveFloorHeight = Settings->FloorHeight;
		if (BuildingInfo && LevelsAttr && TotalHeightAttr)
		{
			const int64 InfoEntryKey = BuildingInfo->FindMetadataKey(FName(*ExtractSourceNameFromTags(Input.Tags)));
			if (InfoEntryKey != INDEX_NONE)
			{
				const int32 Levels = LevelsAttr->GetValueFromItemKey(InfoEntryKey);
				const double TotalHeight = TotalHeightAttr->GetValueFromItemKey(InfoEntryKey);
				if (Levels > 0 && TotalHeight > 0.0)
				{
					EffectiveFloorCount = Levels;
					EffectiveFloorHeight = TotalHeight / Levels;
				}
			}
		}

		// Resolve this building's effective style once -- StyleInfo's row (matched by the
		// "SourceName:..." tag this data already carries, propagated from
		// UPCGLoadOsmBuildingVolumesSettings) if present, else this node's own Settings.
		const int64 StyleEntryKey = StyleInfo ? StyleInfo->FindMetadataKey(FName(*ExtractSourceNameFromTags(Input.Tags))) : INDEX_NONE;
		const bool bHasStyleRow = (StyleEntryKey != INDEX_NONE);

		FResolvedStyle Style;
		Style.StyleName = ResolveValue(StyleNameAttr, StyleEntryKey, FString());
		Style.WindowWidth = ResolveValue(StyleWindowWidthAttr, StyleEntryKey, Settings->WindowWidth);
		Style.WindowHeight = ResolveValue(StyleWindowHeightAttr, StyleEntryKey, Settings->WindowHeight);
		Style.WindowSpacing = ResolveValue(StyleWindowSpacingAttr, StyleEntryKey, Settings->WindowSpacing);
		Style.WindowMargin = ResolveValue(StyleWindowMarginAttr, StyleEntryKey, Settings->WindowMargin);
		Style.WindowSillHeight = ResolveValue(StyleWindowSillHeightAttr, StyleEntryKey, Settings->WindowSillHeight);
		Style.WindowDepth = ResolveValue(StyleWindowDepthAttr, StyleEntryKey, Settings->WindowDepth);
		Style.FrameWidth = ResolveValue(StyleWindowFrameWidthAttr, StyleEntryKey, Settings->WindowFrameWidth);
		Style.FrameDepth = ResolveValue(StyleWindowFrameDepthAttr, StyleEntryKey, Settings->WindowFrameDepth);
		Style.VerticalMullions = ResolveValue(StyleWindowVerticalMullionsAttr, StyleEntryKey, Settings->WindowVerticalMullions);
		Style.HorizontalMullions = ResolveValue(StyleWindowHorizontalMullionsAttr, StyleEntryKey, Settings->WindowHorizontalMullions);
		Style.SillDepth = ResolveValue(StyleWindowSillDepthAttr, StyleEntryKey, Settings->WindowSillDepth);
		Style.SillThickness = ResolveValue(StyleWindowSillThicknessAttr, StyleEntryKey, Settings->WindowSillThickness);
		Style.bDoorEnabled = ResolveValue(StyleDoorEnabledAttr, StyleEntryKey, Settings->bAddDoorOnLongestEdge);
		Style.DoorWidth = ResolveValue(StyleDoorWidthAttr, StyleEntryKey, Settings->DoorWidth);
		Style.DoorHeight = ResolveValue(StyleDoorHeightAttr, StyleEntryKey, Settings->DoorHeight);
		Style.DoorDepth = ResolveValue(StyleDoorDepthAttr, StyleEntryKey, Settings->DoorDepth);

		Style.bLedgeEnabled = ResolveValue(StyleLedgeEnabledAttr, StyleEntryKey, false);
		Style.LedgeDepth = ResolveValue(StyleLedgeDepthAttr, StyleEntryKey, 0.0);
		Style.LedgeHeight = ResolveValue(StyleLedgeHeightAttr, StyleEntryKey, 0.0);
		Style.LedgeEveryNFloors = ResolveValue(StyleLedgeEveryNFloorsAttr, StyleEntryKey, 0);

		Style.bBalconyEnabled = ResolveValue(StyleBalconyEnabledAttr, StyleEntryKey, false);
		Style.BalconyWidth = ResolveValue(StyleBalconyWidthAttr, StyleEntryKey, 0.0);
		Style.BalconyDepth = ResolveValue(StyleBalconyDepthAttr, StyleEntryKey, 0.0);
		Style.BalconySlabHeight = ResolveValue(StyleBalconySlabHeightAttr, StyleEntryKey, 0.0);
		Style.BalconyRailingHeight = ResolveValue(StyleBalconyRailingHeightAttr, StyleEntryKey, 0.0);
		Style.BalconyEveryNFloors = ResolveValue(StyleBalconyEveryNFloorsAttr, StyleEntryKey, 0);
		Style.BalconyRailingBarCount = ResolveValue(StyleBalconyRailingBarCountAttr, StyleEntryKey, 0);
		Style.BalconyRailingBarWidth = ResolveValue(StyleBalconyRailingBarWidthAttr, StyleEntryKey, 0.0);
		Style.BalconyRailingBarDepth = ResolveValue(StyleBalconyRailingBarDepthAttr, StyleEntryKey, 0.0);

		Style.bShutterEnabled = ResolveValue(StyleShutterEnabledAttr, StyleEntryKey, false);

		// Resolve each role's material once per building (not per placement) -- see FResolvedStyle's
		// own comment for why a null Material here is a normal, handled case, not an error.
		if (bHasStyleRow)
		{
			Style.WindowMaterial = FGrammarKitResolver::ResolveMaterial(Style.StyleName, TEXT("window"), ResolveValue(StyleWindowMaterialAttr, StyleEntryKey, FString()), ResolveColor(StyleWindowColorAttr, StyleEntryKey));
			Style.FrameMaterial = FGrammarKitResolver::ResolveMaterial(Style.StyleName, TEXT("window_frame"), ResolveValue(StyleWindowFrameMaterialAttr, StyleEntryKey, FString()), ResolveColor(StyleWindowFrameColorAttr, StyleEntryKey));
			Style.SillMaterial = FGrammarKitResolver::ResolveMaterial(Style.StyleName, TEXT("window_sill"), ResolveValue(StyleWindowSillMaterialAttr, StyleEntryKey, FString()), ResolveColor(StyleWindowSillColorAttr, StyleEntryKey));
			Style.DoorMaterial = FGrammarKitResolver::ResolveMaterial(Style.StyleName, TEXT("door"), ResolveValue(StyleDoorMaterialAttr, StyleEntryKey, FString()), ResolveColor(StyleDoorColorAttr, StyleEntryKey));
			if (Style.bLedgeEnabled)
			{
				Style.LedgeMaterial = FGrammarKitResolver::ResolveMaterial(Style.StyleName, TEXT("ledge"), ResolveValue(StyleLedgeMaterialAttr, StyleEntryKey, FString()), ResolveColor(StyleLedgeColorAttr, StyleEntryKey));
			}
			if (Style.bBalconyEnabled)
			{
				Style.BalconyMaterial = FGrammarKitResolver::ResolveMaterial(Style.StyleName, TEXT("balcony"), ResolveValue(StyleBalconyMaterialAttr, StyleEntryKey, FString()), ResolveColor(StyleBalconyColorAttr, StyleEntryKey));
				Style.BalconyRailingMaterial = FGrammarKitResolver::ResolveMaterial(Style.StyleName, TEXT("balcony_rail"), ResolveValue(StyleBalconyRailingMaterialAttr, StyleEntryKey, FString()), ResolveColor(StyleBalconyRailingColorAttr, StyleEntryKey));
			}
			if (Style.bShutterEnabled)
			{
				Style.ShutterMaterial = FGrammarKitResolver::ResolveMaterial(Style.StyleName, TEXT("shutter"), TEXT("Grammar Facade Shutters"), FLinearColor(0.14, 0.19, 0.14, 1.0));
			}
		}

		UPCGPointData* PlacementData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		UPCGMetadata* PlacementMetadata = PlacementData->MutableMetadata();
		FPCGMetadataAttribute<FString>* RoleAttr = PlacementMetadata->CreateAttribute<FString>(TEXT("Role"), FString(), false, false);
		FPCGMetadataAttribute<double>* WidthAttr = PlacementMetadata->CreateAttribute<double>(TEXT("Width"), 0.0, false, false);
		FPCGMetadataAttribute<double>* HeightAttr = PlacementMetadata->CreateAttribute<double>(TEXT("Height"), 0.0, false, false);
		FPCGMetadataAttribute<FSoftObjectPath>* MaterialOverrideAttr = PlacementMetadata->CreateAttribute<FSoftObjectPath>(TEXT("MaterialOverride"), FSoftObjectPath(), false, false);

		// Box CENTER + (Width, Depth, Height) Scale, oriented the same way as the source edge -- see
		// this node's own header comment for why this matches the existing shared kit box mesh's
		// convention (a centered unit box, scaled per-axis). POSITIVE local Y: as of
		// UPCGExtrudeFootprintToWallsSettings' own MakeFromXY(Tangent, OutwardNormal) fix, the Edges
		// pin's Transform local Y is the genuinely outward normal (matching
		// FGrammarPlacementHelpers::MakeBoxPlacement's own convention exactly -- see that node's
		// header comment), not the inward direction MakeFromXZ used to produce. +Depth/2 pushes the
		// box outward from the wall face, same sign FGrammarGeometry2D::PointOnSegment's own
		// DepthOutward parameter uses.
		//
		// The vertical (height) position is deliberately NOT folded into the same
		// EdgeTransform.TransformPosition call as the horizontal (X/Y) offset -- MakeBoxPlacement
		// itself never does that either (it sets world Z directly: Params.Bottom + Height/2, entirely
		// independent of the box's own rotation). That distinction used to be invisible here because
		// the Edges pin's old MakeFromXZ(Tangent, Up) rotation happened to have local Z map to world
		// Z exactly (a coincidental identity for that one axis), so transforming a local Z offset
		// through the rotation matched just adding it directly. Now that the rotation matches
		// MakeFromXY(Tangent, OutwardNormal) (see above), local Z maps to world -Z instead (verified:
		// for a CCW ring, Tangent x OutwardNormal = -Up) -- transforming a positive local Z height
		// through that rotation pushed every window/door DOWN by its own height instead of up,
		// putting them underneath the building. Splitting the transform avoids this: only the
		// horizontal (Z=0) offset goes through EdgeTransform's rotation (safe regardless of which way
		// local Z happens to be rotated, since a 0 Z-component contributes nothing either way), and
		// the vertical position is added afterward as a plain world-space Z, exactly like
		// MakeBoxPlacement does.
		// Shared point-creation tail for every AddPlacement* variant below -- Rotation/WorldCenter are
		// already fully resolved (world space) by the caller.
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

		auto AddPlacement = [&](const FTransform& EdgeTransform, double OffsetAlongEdge, double Width, double Height, double Depth, const TCHAR* Role, double FloorBottom, double BottomOffset, UMaterialInterface* Material)
		{
			FVector WorldCenter = EdgeTransform.TransformPosition(FVector(OffsetAlongEdge, Depth * 0.5, 0.0));
			WorldCenter.Z += FloorBottom + BottomOffset + Height * 0.5;
			MakePlacementPoint(EdgeTransform.GetRotation(), WorldCenter, Width, Height, Depth, Role, Material);
		};

		// Port of GrammarLedgeBalcony.cpp's BalconyDetailPlacements -- front rail + 2 side rails +
		// N vertical bars sitting on top of a balcony slab already placed at (EdgeTransform, Offset,
		// FloorBottom). All Z/depth constants here are the exact same ones classic uses (converted
		// meters -> cm): slab Bottom = FloorBottom + 8cm (see the balcony-slab AddPlacement call
		// below), RailBottom = that + SlabHeight, bar/rail thickness floored at 2cm.
		auto AddBalconyDetailPlacements = [&](const FTransform& EdgeTransform, double Offset, double FloorBottom)
		{
			const double SlabBottom = FloorBottom + 8.0;
			const double RailBottom = SlabBottom + Style.BalconySlabHeight;
			const double RailCenterZOffset = RailBottom + Style.BalconyRailingHeight * 0.5;
			const double RailBarWidth = FMath::Max(Style.BalconyRailingBarWidth, 2.0);
			const double RailBarDepth = FMath::Max(Style.BalconyRailingBarDepth, 2.0);

			// Front rail: pushed out by the FULL slab depth (not half, unlike AddPlacement's built-in
			// Depth*0.5 convention -- the rail's own thickness (RailBarDepth) is independent of how far
			// out it sits), own depth = RailBarDepth, spans the balcony's whole Width.
			{
				FVector Center = EdgeTransform.TransformPosition(FVector(Offset, Style.BalconyDepth, 0.0));
				Center.Z += RailCenterZOffset;
				MakePlacementPoint(EdgeTransform.GetRotation(), Center, Style.BalconyWidth, Style.BalconyRailingHeight, RailBarDepth, TEXT("balcony_rail"), Style.BalconyRailingMaterial);
			}

			// Side rails (x2, at the balcony's left/right edges): local X/Y (Tangent/Normal) SWAPPED --
			// ports GrammarLedgeBalcony.cpp's own "Params.Tangent = Normal; Params.Normal = Tangent"
			// verbatim by rebuilding the rotation from the swapped WORLD axes via the same
			// FRotationMatrix::MakeFromXY call MakeBoxPlacement itself uses (not a hand-derived guess --
			// see UPCGExtrudeFootprintToWallsSettings' own header comment for why this project doesn't
			// trust hand-derived axis math). Own "Width" (the box's local-X extent) runs along the
			// balcony's depth instead of its width, hence SideRailLength here.
			const double SideRailLength = FMath::Max(Style.BalconyDepth - RailBarDepth, 5.0);
			const FQuat SwappedRotation = FRotationMatrix::MakeFromXY(EdgeTransform.GetRotation().GetAxisY(), EdgeTransform.GetRotation().GetAxisX()).ToQuat();
			for (const double Lateral : { -Style.BalconyWidth * 0.5, Style.BalconyWidth * 0.5 })
			{
				FVector Center = EdgeTransform.TransformPosition(FVector(Offset + Lateral, Style.BalconyDepth * 0.5, 0.0));
				Center.Z += RailCenterZOffset;
				MakePlacementPoint(SwappedRotation, Center, SideRailLength, Style.BalconyRailingHeight, RailBarWidth, TEXT("balcony_rail"), Style.BalconyRailingMaterial);
			}

			// Vertical bars: evenly spaced strictly inside the balcony width (excluding both ends),
			// at the same outer (front) edge as the front rail.
			const int32 BarCount = FMath::Max(Style.BalconyRailingBarCount, 0);
			for (int32 BarIndex = 0; BarIndex < BarCount; ++BarIndex)
			{
				const double Lateral = -Style.BalconyWidth * 0.5 + Style.BalconyWidth * (BarIndex + 1) / static_cast<double>(BarCount + 1);
				FVector Center = EdgeTransform.TransformPosition(FVector(Offset + Lateral, Style.BalconyDepth, 0.0));
				Center.Z += RailCenterZOffset;
				MakePlacementPoint(EdgeTransform.GetRotation(), Center, RailBarWidth, Style.BalconyRailingHeight, RailBarDepth, TEXT("balcony_bar"), Style.BalconyRailingMaterial);
			}
		};

		// Frame (4 panels: 2 vertical sides + top/bottom)/mullion (vertical+horizontal divider bars)/
		// sill assembly around one window opening -- port of GrammarWallWindow.cpp's
		// WindowDetailPlacements, adapted to this node's local-edge-space AddPlacement convention
		// (Bottom passed as FloorBottom=0/BottomOffset=<absolute Z> rather than the classic engine's
		// separate 2D-point + world-Z parameterization). Depth/width math matches exactly; only the
		// coordinate plumbing differs.
		auto AddWindowDetailPlacements = [&](const FTransform& EdgeTransform, double Offset, double WindowBottomZ)
		{
			if (Style.FrameWidth > 0.0)
			{
				const double FrameHeight = Style.WindowHeight + Style.FrameWidth * 2.0;
				const double FrameBottom = WindowBottomZ - Style.FrameWidth;
				const double FrameSpan = Style.WindowWidth + Style.FrameWidth * 2.0;
				const double FrameDepthTotal = Style.WindowDepth + Style.FrameDepth;

				for (const double Lateral : { -Style.WindowWidth / 2.0 - Style.FrameWidth / 2.0, Style.WindowWidth / 2.0 + Style.FrameWidth / 2.0 })
				{
					AddPlacement(EdgeTransform, Offset + Lateral, Style.FrameWidth, FrameHeight, FrameDepthTotal, TEXT("window_frame"), 0.0, FrameBottom, Style.FrameMaterial);
				}
				for (const double Z : { WindowBottomZ - Style.FrameWidth / 2.0, WindowBottomZ + Style.WindowHeight + Style.FrameWidth / 2.0 })
				{
					AddPlacement(EdgeTransform, Offset, FrameSpan, Style.FrameWidth, FrameDepthTotal, TEXT("window_frame"), 0.0, Z - Style.FrameWidth / 2.0, Style.FrameMaterial);
				}
			}

			const double MullionWidth = FMath::Max(Style.FrameWidth * 0.65, 2.5);
			const double MullionDepth = Style.WindowDepth + Style.FrameDepth + 1.0;
			for (int32 MullionIndex = 0; MullionIndex < Style.VerticalMullions; ++MullionIndex)
			{
				const double Lateral = -Style.WindowWidth / 2.0 + Style.WindowWidth * (MullionIndex + 1) / static_cast<double>(Style.VerticalMullions + 1);
				AddPlacement(EdgeTransform, Offset + Lateral, MullionWidth, Style.WindowHeight, MullionDepth, TEXT("window_mullion"), 0.0, WindowBottomZ, Style.FrameMaterial);
			}
			for (int32 MullionIndex = 0; MullionIndex < Style.HorizontalMullions; ++MullionIndex)
			{
				const double Z = WindowBottomZ + Style.WindowHeight * (MullionIndex + 1) / static_cast<double>(Style.HorizontalMullions + 1);
				AddPlacement(EdgeTransform, Offset, Style.WindowWidth, MullionWidth, MullionDepth, TEXT("window_mullion"), 0.0, Z - MullionWidth / 2.0, Style.FrameMaterial);
			}

			if (Style.SillDepth > 0.0 && Style.SillThickness > 0.0)
			{
				const double SillWidth = Style.WindowWidth + Style.FrameWidth * 2.5;
				const double SillBottom = FMath::Max(0.0, WindowBottomZ - Style.SillThickness);
				AddPlacement(EdgeTransform, Offset, SillWidth, Style.SillThickness, Style.SillDepth, TEXT("window_sill"), 0.0, SillBottom, Style.SillMaterial);
			}

			// Port of GrammarWallWindow.cpp's WindowDetailPlacements shutter block -- gated by
			// Style.bShutterEnabled (StyleNameHasShutterKeyword, PCGSelectFacadeStyle.cpp), not a
			// dedicated config struct; material/color are the same hardcoded constants classic uses
			// (resolved once per building already, see Style.ShutterMaterial's own resolution above).
			if (Style.bShutterEnabled)
			{
				const double ShutterWidth = FMath::Clamp(Style.WindowWidth * 0.28, 16.0, 34.0);
				const double ShutterHeight = Style.WindowHeight + Style.FrameWidth;
				const double ShutterBottom = FMath::Max(0.0, WindowBottomZ - Style.FrameWidth * 0.5);
				const double ShutterDepth = Style.WindowDepth + 2.5;
				for (const double Lateral : { -Style.WindowWidth / 2.0 - ShutterWidth / 2.0 - Style.FrameWidth, Style.WindowWidth / 2.0 + ShutterWidth / 2.0 + Style.FrameWidth })
				{
					AddPlacement(EdgeTransform, Offset + Lateral, ShutterWidth, ShutterHeight, ShutterDepth, TEXT("shutter"), 0.0, ShutterBottom, Style.ShutterMaterial);
				}
			}
		};

		double LongestLength = 0.0;
		FTransform LongestEdgeTransform = FTransform::Identity;

		for (const FPCGPoint& EdgePoint : EdgePoints)
		{
			const double Length = LengthAttr->GetValueFromItemKey(EdgePoint.MetadataEntry);
			if (Length > LongestLength)
			{
				LongestLength = Length;
				LongestEdgeTransform = EdgePoint.Transform;
			}

			// Centered window offsets along this edge, porting GrammarWallWindow::WindowOffsets'
			// centering math (BuildingGrammarCore/Private/Grammar/GrammarWallWindow.cpp) -- this
			// pipeline reimplements the layout rule itself rather than calling into it, per the
			// module's own header comment, but the centering formula is the same well-tested math.
			// bWindowsFit gates ONLY the window sub-loop below, not the whole per-floor loop -- ledges
			// span the whole edge regardless of whether any window fits on it (matches
			// BuildingGrammarEngine.cpp's own call order: LedgePlacement is called once per floor,
			// unconditional on window count).
			const double Usable = Length - Style.WindowMargin * 2.0;
			const bool bWindowsFit = Usable >= Style.WindowWidth;
			int32 WindowCount = 0;
			double Start = 0.0;
			if (bWindowsFit)
			{
				WindowCount = FMath::Max(1, static_cast<int32>(FMath::FloorToDouble((Usable + Style.WindowSpacing - Style.WindowWidth) / Style.WindowSpacing)));
				const double TotalWidth = (WindowCount - 1) * Style.WindowSpacing + Style.WindowWidth;
				Start = (Length - TotalWidth) / 2.0 + Style.WindowWidth / 2.0;
			}

			for (int32 FloorIndex = 0; FloorIndex < EffectiveFloorCount; ++FloorIndex)
			{
				const double FloorBottom = FloorIndex * EffectiveFloorHeight;
				if (bWindowsFit)
				{
					for (int32 WindowIndex = 0; WindowIndex < WindowCount; ++WindowIndex)
					{
						const double Offset = Start + WindowIndex * Style.WindowSpacing;
						AddPlacement(EdgePoint.Transform, Offset, Style.WindowWidth, Style.WindowHeight, Style.WindowDepth, TEXT("window"), FloorBottom, Style.WindowSillHeight, Style.WindowMaterial);
						AddWindowDetailPlacements(EdgePoint.Transform, Offset, FloorBottom + Style.WindowSillHeight);

						// Port of BalconyApplies: ground floor (index 0) is always excluded regardless
						// of EveryNFloors, and a balcony is placed at THIS window's own offset (not
						// once per floor like a ledge) -- see GrammarLedgeBalcony.cpp's own comment.
						if (Style.bBalconyEnabled && FloorIndex > 0 && Style.BalconyEveryNFloors > 0 && (FloorIndex % Style.BalconyEveryNFloors == 0))
						{
							// Bottom = FloorBottom + 8cm (0.08m) is classic's own constant, not derived
							// from any style field -- see GrammarLedgeBalcony.cpp's BalconyPlacement.
							AddPlacement(EdgePoint.Transform, Offset, Style.BalconyWidth, Style.BalconySlabHeight, Style.BalconyDepth, TEXT("balcony"), 0.0, FloorBottom + 8.0, Style.BalconyMaterial);
							AddBalconyDetailPlacements(EdgePoint.Transform, Offset, FloorBottom);
						}
					}
				}

				// Port of LedgeApplies -- every floor (including ground) whose index is a multiple of
				// EveryNFloors gets one ledge spanning the whole edge. Z/thickness constants match
				// GrammarLedgeBalcony.cpp's LedgePlacement exactly (converted meters -> cm): 5cm
				// (0.05m) minimum height, 3cm (0.03m) nominal flat-quad thickness.
				if (Style.bLedgeEnabled && Style.LedgeEveryNFloors > 0 && (FloorIndex % Style.LedgeEveryNFloors == 0))
				{
					const double LedgeCenterZ = FMath::Max(5.0, FloorBottom + Style.LedgeHeight);
					constexpr double LedgeNominalThickness = 3.0;
					AddPlacement(EdgePoint.Transform, Length * 0.5, Length, LedgeNominalThickness, Style.LedgeDepth, TEXT("ledge"), 0.0, LedgeCenterZ - LedgeNominalThickness * 0.5, Style.LedgeMaterial);
				}
			}
		}

		if (Style.bDoorEnabled && LongestLength > Style.DoorWidth)
		{
			AddPlacement(LongestEdgeTransform, LongestLength * 0.5, Style.DoorWidth, Style.DoorHeight, Style.DoorDepth, TEXT("door"), 0.0, 0.0, Style.DoorMaterial);
		}

		FPCGTaggedData& PlacementsOut = Context->OutputData.TaggedData.Emplace_GetRef();
		PlacementsOut.Data = PlacementData;
		PlacementsOut.Pin = PlacementsPinLabel;
		PlacementsOut.Tags = Input.Tags;
	}

	return true;
}
