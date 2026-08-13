#include "Elements/PCGExtrudeFootprintToWalls.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGDynamicMeshData.h"
#include "Metadata/PCGMetadata.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Algo/Reverse.h"
#include "GrammarKitResolver.h"
#include "PCGBuildingGrammarDefaults.h"
#include "Geometry/GrammarStableHash.h"
#include "Config/FacadeStyleConfig.h"

namespace
{
	const FName FootprintPinLabel = TEXT("Footprint");
	const FName StyleInfoPinLabel = TEXT("StyleInfo");
	const FName BuildingInfoPinLabel = TEXT("BuildingInfo");
	const FName WallsPinLabel = TEXT("Walls");
	const FName EdgesPinLabel = TEXT("Edges");

	// Winding (which vertex order is actually appended to the triangle -- controls visibility/
	// culling) and the stored shading normal are fully independent controls, each toggleable
	// in-editor (bFlipWinding/bFlipNormal), rather than assumed derivable from each other by hand
	// math -- this project has gotten that wrong more than once even when a specific derivation
	// looked correct on paper (see GrammarKitAssetBuilder.cpp's GUnitBoxFaces comment for the same
	// class of issue elsewhere). The normal is always computed from the fixed (A,B,C) order
	// regardless of bFlipWinding, so the two never interact.
	void AppendTriangleWithComputedNormal(UE::Geometry::FDynamicMesh3& Mesh, UE::Geometry::FDynamicMeshNormalOverlay* Normals, UE::Geometry::FDynamicMeshUVOverlay* UVs, UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIDs, int32 MaterialSlot,
		const FVector& A, const FVector& B, const FVector& C, const FVector2f& UvA, const FVector2f& UvB, const FVector2f& UvC, bool bFlipWinding, bool bFlipNormal)
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
		MaterialIDs->SetValue(TriID, MaterialSlot);

		const int32 NA = Normals->AppendElement(Normal);
		const int32 NB = Normals->AppendElement(Normal);
		const int32 NC = Normals->AppendElement(Normal);
		Normals->SetTriangle(TriID, bFlipWinding ? FIndex3i(NA, NC, NB) : FIndex3i(NA, NB, NC));

		const int32 UA = UVs->AppendElement(UvA);
		const int32 UB = UVs->AppendElement(UvB);
		const int32 UC = UVs->AppendElement(UvC);
		UVs->SetTriangle(TriID, bFlipWinding ? FIndex3i(UA, UC, UB) : FIndex3i(UA, UB, UC));
	}

	// One quad per footprint edge (or, in "row" mode, one quad per edge PER FLOOR -- see
	// UPCGExtrudeFootprintToWallsSettings' header comment), spanning [Bottom, Top] in Z, appended into
	// one shared mesh with a per-quad MaterialSlot so different edges/floors can carry different
	// resolved colors. Mirrors FGrammarDynamicMeshBuilder's per-triangle-corner normal/UV overlay
	// pattern (each triangle gets its own fresh normal/UV elements -- the overlay model doesn't let
	// corners be shared across triangles), but is otherwise a fresh, self-contained implementation
	// (this pipeline doesn't call into BuildingGrammarCore/Geometry -- see this node's own header
	// comment). UVs are scaled by real-world Length/(Top-Bottom)/TextureScale (not a fixed 0..1 per
	// quad), so a texture repeats at a consistent physical size across walls of different lengths
	// instead of always stretching exactly one tile across each quad regardless of its size.
	void AppendWallQuad(UE::Geometry::FDynamicMesh3& Mesh, UE::Geometry::FDynamicMeshNormalOverlay* Normals, UE::Geometry::FDynamicMeshUVOverlay* UVs, UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIDs, int32 MaterialSlot,
		const FVector& Start, const FVector& End, double Bottom, double Top, double Length, double TextureScale, bool bFlipWinding, bool bFlipNormal)
	{
		const FVector V0(Start.X, Start.Y, Bottom);
		const FVector V1(End.X, End.Y, Bottom);
		const FVector V2(End.X, End.Y, Top);
		const FVector V3(Start.X, Start.Y, Top);

		const float U = static_cast<float>(Length / TextureScale);
		const float V = static_cast<float>((Top - Bottom) / TextureScale);

		AppendTriangleWithComputedNormal(Mesh, Normals, UVs, MaterialIDs, MaterialSlot, V0, V1, V2, FVector2f(0, 0), FVector2f(U, 0), FVector2f(U, V), bFlipWinding, bFlipNormal);
		AppendTriangleWithComputedNormal(Mesh, Normals, UVs, MaterialIDs, MaterialSlot, V0, V2, V3, FVector2f(0, 0), FVector2f(U, V), FVector2f(0, V), bFlipWinding, bFlipNormal);
	}

	// Port of GrammarEngineInternal::VariantWallColor (GrammarGrammarCore/Private/Grammar/
	// GrammarEngineInternal.cpp:112-133) -- picks a color out of Variants (per-building "Building"
	// mode via FGrammarStableHash::StableIndex, keyed by SourceName alone so every edge of one
	// building gets the same color; per-edge "Facade" mode via StableIndex keyed by "SourceName:
	// SideIndex"; "Cycle" mode via plain SideIndex modulo) or falls back to BaseColor (sentinel index
	// -1) if Variants is empty or Mode is None. Reuses FGrammarStableHash::StableIndex directly
	// (BuildingGrammarCore, already exported) rather than reimplementing its djb2-variant hash by
	// hand.
	TPair<int32, FLinearColor> ResolveVariantWallColor(const TArray<FLinearColor>& Variants, EGrammarWallColorVariantMode Mode, const FLinearColor& BaseColor, const FString& SourceName, int32 SideIndex)
	{
		if (Variants.Num() == 0 || Mode == EGrammarWallColorVariantMode::None)
		{
			return TPair<int32, FLinearColor>(-1, BaseColor);
		}
		int32 Index = 0;
		if (Mode == EGrammarWallColorVariantMode::Building)
		{
			Index = FGrammarStableHash::StableIndex(SourceName, Variants.Num());
		}
		else if (Mode == EGrammarWallColorVariantMode::Facade)
		{
			Index = FGrammarStableHash::StableIndex(FString::Printf(TEXT("%s:%d"), *SourceName, SideIndex), Variants.Num());
		}
		else
		{
			Index = SideIndex % Variants.Num();
		}
		return TPair<int32, FLinearColor>(Index, Variants[Index]);
	}

	// Port of GrammarEngineInternal::RowWallColor (GrammarEngineInternal.cpp:135-153) -- no
	// stable-hash involved, unlike the variant-color function above. "GroundAccent" mode always gives
	// floor 0 RowColors[0], then floors 1+ cycle through RowColors[1..Count-1]; "Cycle" mode is plain
	// FloorIndex modulo across the whole array.
	TPair<int32, FLinearColor> ResolveRowWallColor(const TArray<FLinearColor>& RowColors, EGrammarWallRowColorMode Mode, const FLinearColor& BaseColor, int32 FloorIndex)
	{
		if (RowColors.Num() == 0)
		{
			return TPair<int32, FLinearColor>(-1, BaseColor);
		}
		if (Mode == EGrammarWallRowColorMode::GroundAccent)
		{
			if (FloorIndex == 0)
			{
				return TPair<int32, FLinearColor>(0, RowColors[0]);
			}
			const int32 Index = FMath::Min(1 + (FloorIndex - 1) % FMath::Max(1, RowColors.Num() - 1), RowColors.Num() - 1);
			return TPair<int32, FLinearColor>(Index, RowColors[Index]);
		}
		const int32 Index = FloorIndex % RowColors.Num();
		return TPair<int32, FLinearColor>(Index, RowColors[Index]);
	}

	// Port of GrammarEngineInternal::WallMaterialName (GrammarEngineInternal.cpp:155-162) -- 1-based
	// color index in the material name, matching classic's own asset-naming convention exactly so
	// per-variant/per-row material assets land under recognizable names.
	FString WallMaterialName(const FString& Base, const TCHAR* Kind, int32 ColorIndex)
	{
		if (ColorIndex < 0)
		{
			return Base;
		}
		return FString::Printf(TEXT("%s %s %d"), *Base, Kind, ColorIndex + 1);
	}

	// PCG metadata attributes have no array type -- the inverse of PCGSelectFacadeStyle.cpp's
	// SerializeColorArray (see that function's own comment for the "r,g,b,a;r,g,b,a;..." format).
	TArray<FLinearColor> ParseColorArray(const FString& Serialized)
	{
		TArray<FLinearColor> Colors;
		if (Serialized.IsEmpty())
		{
			return Colors;
		}
		TArray<FString> Parts;
		Serialized.ParseIntoArray(Parts, TEXT(";"), true);
		for (const FString& Part : Parts)
		{
			TArray<FString> Components;
			Part.ParseIntoArray(Components, TEXT(","), true);
			if (Components.Num() == 4)
			{
				Colors.Add(FLinearColor(FCString::Atod(*Components[0]), FCString::Atod(*Components[1]), FCString::Atod(*Components[2]), FCString::Atod(*Components[3])));
			}
		}
		return Colors;
	}

	EGrammarWallColorVariantMode ParseWallColorVariantMode(const FString& Value)
	{
		if (Value == TEXT("Cycle")) return EGrammarWallColorVariantMode::Cycle;
		if (Value == TEXT("Building")) return EGrammarWallColorVariantMode::Building;
		if (Value == TEXT("Facade")) return EGrammarWallColorVariantMode::Facade;
		return EGrammarWallColorVariantMode::None;
	}

	EGrammarWallRowColorMode ParseWallRowColorMode(const FString& Value)
	{
		if (Value == TEXT("GroundAccent")) return EGrammarWallRowColorMode::GroundAccent;
		return EGrammarWallRowColorMode::Cycle;
	}
}

TArray<FPCGPinProperties> UPCGExtrudeFootprintToWallsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(FootprintPinLabel, EPCGDataType::Spline);
	// Neither is a required pin -- this node works fine without them, falling back to its own
	// Material/Height.
	Pins.Emplace(StyleInfoPinLabel, EPCGDataType::Param);
	Pins.Emplace(BuildingInfoPinLabel, EPCGDataType::Param);
	return Pins;
}

TArray<FPCGPinProperties> UPCGExtrudeFootprintToWallsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(WallsPinLabel, EPCGDataType::DynamicMesh);
	Pins.Emplace(EdgesPinLabel, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGExtrudeFootprintToWallsSettings::CreateElement() const
{
	return MakeShared<FPCGExtrudeFootprintToWallsElement>();
}

bool FPCGExtrudeFootprintToWallsElement::ExecuteInternal(FPCGContext* Context) const
{
	using namespace UE::Geometry;

	const UPCGExtrudeFootprintToWallsSettings* Settings = Context->GetInputSettings<UPCGExtrudeFootprintToWallsSettings>();
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
	const FPCGMetadataAttribute<FString>* WallMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("WallMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* WallColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("WallColor")) : nullptr;
	const FPCGMetadataAttribute<FString>* WallColorVariantsAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("WallColorVariants")) : nullptr;
	const FPCGMetadataAttribute<FString>* WallColorVariantModeAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("WallColorVariantMode")) : nullptr;
	const FPCGMetadataAttribute<FString>* WallRowColorsAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("WallRowColors")) : nullptr;
	const FPCGMetadataAttribute<FString>* WallRowColorModeAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("WallRowColorMode")) : nullptr;

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
	const FPCGMetadataAttribute<int32>* LevelsAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<int32>(TEXT("Levels")) : nullptr;

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

		// Normalize every building's ring to the same winding (CCW viewed from above) regardless of
		// its original OSM order, so "outward" means the same thing for every building this node
		// processes -- without this, buildings with different source winding would end up with walls
		// facing opposite ways even though the per-triangle normal/winding are now always internally
		// consistent (see AppendTriangleWithComputedNormal's comment). Same technique as
		// UPCGRoofFrameGeneratorSettings.
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

		// This building's own OSM-derived TotalHeight/Levels (see UPCGLoadOsmBuildingVolumesSettings'
		// header comment) if BuildingInfo is connected and has a usable row, else this node's own flat
		// Height (Levels default to 1, i.e. no row-splitting possible without a real Levels count) --
		// see this class's header comment.
		double EffectiveHeight = Settings->Height;
		int32 EffectiveLevels = 1;
		if (BuildingInfo && TotalHeightAttr)
		{
			const int64 InfoEntryKey = BuildingInfo->FindMetadataKey(FName(*ExtractSourceNameFromTags(Input.Tags)));
			if (InfoEntryKey != INDEX_NONE)
			{
				const double TotalHeight = TotalHeightAttr->GetValueFromItemKey(InfoEntryKey);
				if (TotalHeight > 0.0)
				{
					EffectiveHeight = TotalHeight;
				}
				if (LevelsAttr)
				{
					const int32 Levels = LevelsAttr->GetValueFromItemKey(InfoEntryKey);
					if (Levels > 0)
					{
						EffectiveLevels = Levels;
					}
				}
			}
		}

		// Resolve this building's own style-driven wall-coloring inputs once -- see this node's
		// header comment for why full VariantWallColor/RowWallColor resolution happens here (per
		// edge/floor, below) rather than in UPCGSelectFacadeStyleSettings, which has no per-edge
		// SideIndex.
		const FString SourceName = ExtractSourceNameFromTags(Input.Tags);
		FString StyleName;
		FString BaseWallMaterial;
		FLinearColor BaseWallColor = FLinearColor::White;
		TArray<FLinearColor> WallColorVariants;
		EGrammarWallColorVariantMode WallColorVariantMode = EGrammarWallColorVariantMode::None;
		TArray<FLinearColor> WallRowColors;
		EGrammarWallRowColorMode WallRowColorMode = EGrammarWallRowColorMode::Cycle;
		bool bHasStyleRow = false;
		if (StyleInfo && WallMaterialAttr && WallColorAttr)
		{
			const int64 StyleEntryKey = StyleInfo->FindMetadataKey(FName(*SourceName));
			if (StyleEntryKey != INDEX_NONE)
			{
				bHasStyleRow = true;
				StyleName = StyleNameAttr ? StyleNameAttr->GetValueFromItemKey(StyleEntryKey) : FString();
				BaseWallMaterial = WallMaterialAttr->GetValueFromItemKey(StyleEntryKey);
				const FVector4 ColorValue = WallColorAttr->GetValueFromItemKey(StyleEntryKey);
				BaseWallColor = FLinearColor(ColorValue.X, ColorValue.Y, ColorValue.Z, ColorValue.W);
				if (WallColorVariantsAttr)
				{
					WallColorVariants = ParseColorArray(WallColorVariantsAttr->GetValueFromItemKey(StyleEntryKey));
				}
				if (WallColorVariantModeAttr)
				{
					WallColorVariantMode = ParseWallColorVariantMode(WallColorVariantModeAttr->GetValueFromItemKey(StyleEntryKey));
				}
				if (WallRowColorsAttr)
				{
					WallRowColors = ParseColorArray(WallRowColorsAttr->GetValueFromItemKey(StyleEntryKey));
				}
				if (WallRowColorModeAttr)
				{
					WallRowColorMode = ParseWallRowColorMode(WallRowColorModeAttr->GetValueFromItemKey(StyleEntryKey));
				}
			}
		}

		FDynamicMesh3 WallMesh;
		WallMesh.EnableAttributes();
		FDynamicMeshNormalOverlay* Normals = WallMesh.Attributes()->PrimaryNormals();
		FDynamicMeshUVOverlay* UVs = WallMesh.Attributes()->PrimaryUV();
		// MaterialID needs to be enabled and set PER TRIANGLE as quads are appended below (not a
		// single blanket "every triangle = slot 0" pass afterward) -- different edges/floors can now
		// resolve to different material slots. Without this at all, the mesh renders with the engine's
		// default material regardless of what's passed to Initialize.
		WallMesh.Attributes()->EnableMaterialID();
		FDynamicMeshMaterialAttribute* MaterialIDs = WallMesh.Attributes()->GetMaterialID();

		UPCGPointData* EdgePoints = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		UPCGMetadata* EdgeMetadata = EdgePoints->MutableMetadata();
		FPCGMetadataAttribute<double>* LengthAttr = EdgeMetadata->CreateAttribute<double>(TEXT("Length"), 0.0, false, false);
		FPCGMetadataAttribute<int32>* EdgeIndexAttr = EdgeMetadata->CreateAttribute<int32>(TEXT("EdgeIndex"), 0, false, false);

		// Material slots, resolved lazily as distinct (MaterialName, Color) combinations are actually
		// encountered -- usually just 1 (no variants/rows configured), or a small handful for Cycle/
		// Facade-mode variants or row-banded styles. Keyed by MaterialName alone: WallMaterialName's
		// naming already embeds the color-array index, so identical MaterialName always implies
		// identical Color for a given building (deterministic array lookup), matching classic's own
		// per-(StyleName,Role,MaterialName) asset identity.
		TArray<UMaterialInterface*> MaterialSlots;
		TMap<FString, int32> SlotByMaterialName;
		auto GetOrAddMaterialSlot = [&](const FString& MaterialName, const FLinearColor& Color) -> int32
		{
			if (const int32* Existing = SlotByMaterialName.Find(MaterialName))
			{
				return *Existing;
			}
			UMaterialInterface* Material = FallbackMaterial;
			if (bHasStyleRow)
			{
				if (UMaterialInterface* Resolved = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("facade"), MaterialName, Color))
				{
					Material = Resolved;
				}
			}
			const int32 Slot = MaterialSlots.Add(Material);
			SlotByMaterialName.Add(MaterialName, Slot);
			return Slot;
		};

		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector& Start = SplinePoints[Index].Position;
			const FVector& End = SplinePoints[(Index + 1) % Count].Position;
			const FVector Edge = End - Start;
			const double Length = Edge.Size();
			if (Length <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const FVector Tangent = Edge / Length;

			// Port of BuildingGrammarEngine.cpp's own dispatch (WallRowColors.Num()>0 ? per-floor rows
			// : one whole-facade-height quad) -- see this node's header comment. RowWallColor is
			// FloorIndex-only (no SideIndex/stable-hash), VariantWallColor is SideIndex-only (this
			// edge's own loop Index, matching classic's per-side SideIndex exactly) -- the two are
			// mutually exclusive per building, same as classic.
			if (WallRowColors.Num() > 0)
			{
				const double FloorHeight = EffectiveHeight / EffectiveLevels;
				for (int32 FloorIndex = 0; FloorIndex < EffectiveLevels; ++FloorIndex)
				{
					const TPair<int32, FLinearColor> ColorResult = ResolveRowWallColor(WallRowColors, WallRowColorMode, BaseWallColor, FloorIndex);
					const FString MaterialName = WallMaterialName(BaseWallMaterial, TEXT("row"), ColorResult.Key);
					const int32 Slot = GetOrAddMaterialSlot(MaterialName, ColorResult.Value);
					AppendWallQuad(WallMesh, Normals, UVs, MaterialIDs, Slot, Start, End, FloorIndex * FloorHeight, (FloorIndex + 1) * FloorHeight, Length, FMath::Max(Settings->TextureScale, 1.0), Settings->bFlipWinding, Settings->bFlipNormals);
				}
			}
			else
			{
				const TPair<int32, FLinearColor> ColorResult = ResolveVariantWallColor(WallColorVariants, WallColorVariantMode, BaseWallColor, SourceName, Index);
				const FString MaterialName = WallMaterialName(BaseWallMaterial, TEXT("variant"), ColorResult.Key);
				const int32 Slot = GetOrAddMaterialSlot(MaterialName, ColorResult.Value);
				AppendWallQuad(WallMesh, Normals, UVs, MaterialIDs, Slot, Start, End, 0.0, EffectiveHeight, Length, FMath::Max(Settings->TextureScale, 1.0), Settings->bFlipWinding, Settings->bFlipNormals);
			}

			// X=Tangent, Y=outward normal, Z=computed -- deliberately matches
			// FGrammarPlacementHelpers::MakeBoxPlacement's own MakeFromXY(Tangent, Normal) exactly
			// (BuildingGrammarCore/Private/Grammar/GrammarPlacementHelpers.cpp), not the previously
			// used MakeFromXZ(Tangent, Up). The two are NOT equivalent: for a CCW-wound footprint,
			// MakeFromXY's computed Z ends up pointing world-DOWN (Z = Tangent x OutwardNormal, which
			// works out to -Up for a CCW ring), whereas MakeFromXZ's explicit Z=Up is the opposite
			// sign -- a 180-degree difference about the tangent axis. Since every window/door/frame/
			// mullion/sill placement instances the exact same shared kit box mesh classic's HISM path
			// also instances (see BuildingGrammarGeometry's kit mesh comment), that Z-sign mismatch is
			// exactly what was rendering PCG-placed windows upside-down relative to the classic engine
			// (and to the -Y-is-outward derivation used by UPCGFacadeWindowDoorLayoutSettings, itself
			// downstream of this Transform) even though it looked like a Y-only (depth) issue at
			// first. OutwardNormal here is the same "rotate tangent -90 degrees" formula this
			// pipeline already uses to normalize footprints to CCW (SignedArea above): for a CCW ring,
			// interior is left-of-travel, so outward is (Tangent.Y, -Tangent.X).
			const FVector OutwardNormal(Tangent.Y, -Tangent.X, 0.0);
			FPCGPoint EdgePoint;
			EdgePoint.Transform = FTransform(FRotationMatrix::MakeFromXY(Tangent, OutwardNormal).ToQuat(), Start);
			EdgePoint.Density = 1.0f;
			EdgePoint.Seed = Index;
			EdgePoint.MetadataEntry = EdgeMetadata->AddEntry();
			LengthAttr->SetValue(EdgePoint.MetadataEntry, Length);
			EdgeIndexAttr->SetValue(EdgePoint.MetadataEntry, Index);
			EdgePoints->GetMutablePoints().Add(EdgePoint);
		}

		// No quad was ever appended (e.g. every edge was degenerate) -- Initialize still needs a
		// non-empty materials array.
		if (MaterialSlots.Num() == 0)
		{
			MaterialSlots.Add(FallbackMaterial);
		}

		UPCGDynamicMeshData* WallData = FPCGContext::NewObject_AnyThread<UPCGDynamicMeshData>(Context);
		WallData->Initialize(MoveTemp(WallMesh), MaterialSlots);

		FPCGTaggedData& WallOut = Context->OutputData.TaggedData.Emplace_GetRef();
		WallOut.Data = WallData;
		WallOut.Pin = WallsPinLabel;
		WallOut.Tags = Input.Tags;

		FPCGTaggedData& EdgesOut = Context->OutputData.TaggedData.Emplace_GetRef();
		EdgesOut.Data = EdgePoints;
		EdgesOut.Pin = EdgesPinLabel;
		EdgesOut.Tags = Input.Tags;
	}

	return true;
}
