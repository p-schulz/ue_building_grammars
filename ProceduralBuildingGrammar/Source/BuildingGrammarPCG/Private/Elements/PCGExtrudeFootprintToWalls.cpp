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
#include "Geometry/GrammarGeometry2D.h"
#include "Geometry/GrammarWallRecess.h"
#include "Config/FacadeStyleConfig.h"
#include "Config/DoorStyleConfig.h"
#include "Math/Box2D.h"

namespace
{
	const FName FootprintPinLabel = TEXT("Footprint");
	const FName StyleInfoPinLabel = TEXT("StyleInfo");
	const FName BuildingInfoPinLabel = TEXT("BuildingInfo");
	const FName WallsPinLabel = TEXT("Walls");
	const FName EdgesPinLabel = TEXT("Edges");

	// Same tiny port PCGFacadeWindowDoorLayout.cpp's own anonymous namespace already has -- kept as a
	// separate copy rather than shared, matching this module's own established precedent of small
	// per-node duplication (see e.g. WindowOffsets below).
	EGrammarDoorPlacement ParseDoorPlacement(const FString& Value)
	{
		if (Value == TEXT("EachFacade")) return EGrammarDoorPlacement::EachFacade;
		if (Value == TEXT("None")) return EGrammarDoorPlacement::None;
		return EGrammarDoorPlacement::StreetFacing;
	}

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

	// One or more quads per footprint edge (or, in "row" mode, per edge PER FLOOR -- see
	// UPCGExtrudeFootprintToWallsSettings' header comment): the flush wall plane, plus a recessed
	// pocket's worth of extra quads wherever a window/door opening falls (FGrammarWallRecess::
	// BuildSegments, BuildingGrammarCore/Geometry -- the one piece of this otherwise self-contained
	// node that does call into BuildingGrammarCore, since the recess decomposition math is shared
	// with the classic engine rather than reimplemented here too). Each quad appended into one shared
	// mesh with a per-quad MaterialSlot so different edges/floors can carry different resolved
	// colors. Mirrors FGrammarDynamicMeshBuilder's per-triangle-corner normal/UV overlay pattern (each
	// triangle gets its own fresh normal/UV elements -- the overlay model doesn't let corners be
	// shared across triangles). UVs are derived from each quad's own real-world edge lengths (not a
	// fixed 0..1 per quad), so a texture repeats at a consistent physical size instead of always
	// stretching exactly one tile across each quad regardless of its size.
	void AppendWallQuadMesh(UE::Geometry::FDynamicMesh3& Mesh, UE::Geometry::FDynamicMeshNormalOverlay* Normals, UE::Geometry::FDynamicMeshUVOverlay* UVs, UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIDs, int32 MaterialSlot,
		const FGrammarWallQuad& Quad, double TextureScale, bool bFlipWinding, bool bFlipNormal)
	{
		const FVector& V0 = Quad.Corners[0];
		const FVector& V1 = Quad.Corners[1];
		const FVector& V2 = Quad.Corners[2];
		const FVector& V3 = Quad.Corners[3];

		const float U = static_cast<float>((V1 - V0).Size() / TextureScale);
		const float V = static_cast<float>((V2 - V1).Size() / TextureScale);

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

	// One entry per Footprint input, gathered up front so wall-overlap suppression (see this node's
	// own header comment) can check any building's edges against every OTHER building's footprint,
	// not just its own.
	struct FFootprintVolumeInfo
	{
		FString SourceName;
		FString ParentSourceName;
		TArray<FVector2D> Ring2D; // CCW-normalized, same winding as the corresponding SplinePoints entry
		FBox2D Bounds2D = FBox2D(ForceInit);
		double Area = 0.0;
		// This volume's own vertical extent (Unreal centimeters) -- the same EffectiveHeight
		// resolution the main per-building loop already does (BuildingInfo's TotalHeight, else
		// Settings->Height), cached here per volume so suppression can compare heights across
		// volumes, not just footprints. MinHeightZ is the footprint spline's own Z (see
		// UPCGLoadOsmBuildingVolumesSettings' header comment on min_height baking) -- nonzero for a
		// building:part that starts above ground.
		double MinHeightZ = 0.0;
		double MaxHeightZ = 0.0;
		// Cached alongside the height extent above purely so the main loop below can reuse it
		// instead of re-resolving TotalHeight/Levels a second time for the same building.
		double EffectiveHeight = 0.0;
		int32 EffectiveLevels = 1;
	};

	// True if Other's footprint takes priority over Self's at a point they both cover -- i.e. Self's
	// wall should be suppressed there. Never true the other way around for a parent/child pair (a
	// building:part's own walls always win over its parent); for any other pair (siblings under the
	// same parent, or two unrelated overlapping/adjacent buildings), the larger footprint wins, with
	// SourceName as a last-resort deterministic tiebreak for an exact area tie.
	bool DoesOtherOutrankSelf(const FFootprintVolumeInfo& Self, const FFootprintVolumeInfo& Other)
	{
		if (!Self.ParentSourceName.IsEmpty() && Self.ParentSourceName == Other.SourceName)
		{
			return false;
		}
		if (!Other.ParentSourceName.IsEmpty() && Other.ParentSourceName == Self.SourceName)
		{
			return true;
		}
		if (!FMath::IsNearlyEqual(Self.Area, Other.Area))
		{
			return Other.Area > Self.Area;
		}
		return Other.SourceName > Self.SourceName;
	}

	// True if Other's own height range fully covers Self's -- i.e. suppressing Self's wall here
	// genuinely can't leave a gap, because whatever Other renders in its place reaches at least as
	// high and at least as low. Required IN ADDITION to DoesOtherOutrankSelf/the XY footprint check
	// before actually suppressing: a shorter building:part (or one with its own min_height, starting
	// above ground) only covers PART of its parent's own height there, so suppressing the parent's
	// full-height wall on the strength of a 2D-only overlap would leave a real hole below and/or
	// above the part -- exactly the "missing ground floor / gap under the roof" failure this check
	// exists to prevent. The tradeoff: a partial-height overlap (e.g. a shorter attached wing) is
	// left NOT suppressed at all, so both volumes' walls render there -- a visible double wall in
	// that band, which is a far less broken result than a hole.
	bool DoesOtherVerticallyContainSelf(const FFootprintVolumeInfo& Self, const FFootprintVolumeInfo& Other)
	{
		return Other.MinHeightZ <= Self.MinHeightZ + KINDA_SMALL_NUMBER && Other.MaxHeightZ >= Self.MaxHeightZ - KINDA_SMALL_NUMBER;
	}

	bool IsPointCoveredByAnotherVolume(const FVector2D& Point, int32 SelfIndex, const TArray<FFootprintVolumeInfo>& Volumes, const TArray<int32>& Candidates)
	{
		const FFootprintVolumeInfo& Self = Volumes[SelfIndex];
		for (const int32 OtherIndex : Candidates)
		{
			if (OtherIndex == SelfIndex)
			{
				continue;
			}
			const FFootprintVolumeInfo& Other = Volumes[OtherIndex];
			if (!Other.Bounds2D.IsInside(Point))
			{
				continue;
			}
			if (!DoesOtherOutrankSelf(Self, Other) || !DoesOtherVerticallyContainSelf(Self, Other))
			{
				continue;
			}
			if (FGrammarGeometry2D::PointInRing(Point, Other.Ring2D))
			{
				return true;
			}
		}
		return false;
	}

	// Samples Length/SampleSpacing points along the edge (Start2D, Start2D + Tangent2D*Length) and
	// returns the sub-ranges (as [DistanceAlongEdge, DistanceAlongEdge] pairs) NOT covered by any
	// other footprint -- see this node's own header comment for the full algorithm and its known
	// approximation (a covered/uncovered transition is only located to within one sample spacing,
	// snapped to the midpoint between the last uncovered and first covered sample).
	TArray<TPair<double, double>> ComputeUncoveredSubRanges(const FVector2D& Start2D, const FVector2D& Tangent2D, double Length, int32 SelfIndex, const TArray<FFootprintVolumeInfo>& Volumes, const TArray<int32>& Candidates, double SampleSpacing)
	{
		TArray<TPair<double, double>> Result;
		if (Candidates.IsEmpty())
		{
			Result.Add(TPair<double, double>(0.0, Length));
			return Result;
		}

		const int32 NumSamples = FMath::Clamp(FMath::CeilToInt32(Length / FMath::Max(SampleSpacing, 1.0)) + 1, 2, 256);
		TArray<bool> Covered;
		Covered.SetNumUninitialized(NumSamples);
		for (int32 Index = 0; Index < NumSamples; ++Index)
		{
			const double T = static_cast<double>(Index) / (NumSamples - 1);
			Covered[Index] = IsPointCoveredByAnotherVolume(Start2D + Tangent2D * (T * Length), SelfIndex, Volumes, Candidates);
		}

		auto DistAtMidpoint = [&](int32 A, int32 B) { return ((A + B) * 0.5 / (NumSamples - 1)) * Length; };

		int32 RunStart = -1;
		for (int32 Index = 0; Index < NumSamples; ++Index)
		{
			if (!Covered[Index])
			{
				if (RunStart < 0)
				{
					RunStart = Index;
				}
				continue;
			}
			if (RunStart >= 0)
			{
				const double StartDist = (RunStart == 0) ? 0.0 : DistAtMidpoint(RunStart - 1, RunStart);
				const double EndDist = DistAtMidpoint(Index - 1, Index);
				if (EndDist - StartDist > KINDA_SMALL_NUMBER)
				{
					Result.Add(TPair<double, double>(StartDist, EndDist));
				}
				RunStart = -1;
			}
		}
		if (RunStart >= 0)
		{
			const double StartDist = (RunStart == 0) ? 0.0 : DistAtMidpoint(RunStart - 1, RunStart);
			if (Length - StartDist > KINDA_SMALL_NUMBER)
			{
				Result.Add(TPair<double, double>(StartDist, Length));
			}
		}
		return Result;
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

	// Window/door opening data, read only to cut matching recesses into the wall (see this class's
	// header comment for why this is scoped to StyleInfo-connected buildings only -- unlike
	// WallMaterial/WallColor above, this node has no Settings-level fallback for any of these, so a
	// building with no StyleInfo row simply gets no recessing, same as it gets no wall-color variation
	// today). Same attribute names UPCGFacadeWindowDoorLayoutSettings already reads -- both nodes must
	// resolve the exact same window/door positions for the recess and the actual window/door meshes to
	// line up.
	const FPCGMetadataAttribute<double>* StyleWindowWidthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowWidth")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleWindowHeightAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowHeight")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleWindowSpacingAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowSpacing")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleWindowMarginAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowMargin")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleWindowSillHeightAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowSillHeight")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleWindowRecessDepthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("WindowRecessDepth")) : nullptr;
	const FPCGMetadataAttribute<bool>* StyleDoorEnabledAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<bool>(TEXT("DoorEnabled")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleDoorWidthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("DoorWidth")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleDoorHeightAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("DoorHeight")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleDoorRecessDepthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("DoorRecessDepth")) : nullptr;
	const FPCGMetadataAttribute<FString>* StyleDoorPlacementAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("DoorPlacement")) : nullptr;
	const FPCGMetadataAttribute<double>* StyleDoorFrameWidthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("DoorFrameWidth")) : nullptr;

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
	const FPCGMetadataAttribute<bool>* IsBuildingPartAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<bool>(TEXT("IsBuildingPart")) : nullptr;
	const FPCGMetadataAttribute<FString>* ParentSourceNameAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<FString>(TEXT("ParentSourceName")) : nullptr;
	const FPCGMetadataAttribute<FString>* TagsJsonAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<FString>(TEXT("TagsJson")) : nullptr;

	UMaterialInterface* FallbackMaterial = Settings->Material.LoadSynchronous();

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(FootprintPinLabel);

	// Gathered once, up front, so wall-overlap suppression (see this node's own header comment) can
	// check any building's edges against every OTHER building's footprint -- normalizing winding and
	// extracting the 2D ring here too, so the main loop below reuses this instead of redoing it.
	TArray<TArray<FSplinePoint>> NormalizedSplinePointsByInput;
	TArray<FFootprintVolumeInfo> Volumes;
	NormalizedSplinePointsByInput.Reserve(Inputs.Num());
	Volumes.Reserve(Inputs.Num());
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSplineData* SplineData = Cast<UPCGSplineData>(Input.Data.Get());
		TArray<FSplinePoint> SplinePoints = SplineData ? SplineData->GetSplinePoints() : TArray<FSplinePoint>();
		const int32 Count = SplinePoints.Num();

		double SignedArea = 0.0;
		if (Count >= 3)
		{
			// Normalize every building's ring to the same winding (CCW viewed from above) regardless
			// of its original OSM order, so "outward" means the same thing for every building this
			// node processes -- without this, buildings with different source winding would end up
			// with walls facing opposite ways even though the per-triangle normal/winding are now
			// always internally consistent (see AppendTriangleWithComputedNormal's comment). Same
			// technique as UPCGRoofFrameGeneratorSettings.
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const FVector& A = SplinePoints[Index].Position;
				const FVector& B = SplinePoints[(Index + 1) % Count].Position;
				SignedArea += (A.X * B.Y - B.X * A.Y);
			}
			if (SignedArea < 0.0)
			{
				Algo::Reverse(SplinePoints);
				SignedArea = -SignedArea;
			}
		}
		NormalizedSplinePointsByInput.Add(SplinePoints);

		FFootprintVolumeInfo VolumeInfo;
		VolumeInfo.SourceName = ExtractSourceNameFromTags(Input.Tags);
		VolumeInfo.Area = FMath::Abs(SignedArea) * 0.5;
		VolumeInfo.Ring2D.Reserve(Count);
		VolumeInfo.Bounds2D.Init();
		for (const FSplinePoint& Point : SplinePoints)
		{
			const FVector2D Point2D(Point.Position.X, Point.Position.Y);
			VolumeInfo.Ring2D.Add(Point2D);
			VolumeInfo.Bounds2D += Point2D;
		}
		VolumeInfo.MinHeightZ = (Count > 0) ? SplinePoints[0].Position.Z : 0.0;

		// Same TotalHeight/Levels resolution the main loop below used to do inline -- cached per
		// volume (not just for Self) so wall-overlap suppression can compare heights across
		// volumes -- see DoesOtherVerticallyContainSelf's own comment.
		VolumeInfo.EffectiveHeight = Settings->Height;
		VolumeInfo.EffectiveLevels = 1;
		if (InfoMetadata)
		{
			const int64 InfoEntryKey = BuildingInfo->FindMetadataKey(FName(*VolumeInfo.SourceName));
			if (InfoEntryKey != INDEX_NONE)
			{
				if (IsBuildingPartAttr && IsBuildingPartAttr->GetValueFromItemKey(InfoEntryKey))
				{
					VolumeInfo.ParentSourceName = ParentSourceNameAttr ? ParentSourceNameAttr->GetValueFromItemKey(InfoEntryKey) : FString();
				}
				if (TotalHeightAttr)
				{
					const double TotalHeight = TotalHeightAttr->GetValueFromItemKey(InfoEntryKey);
					if (TotalHeight > 0.0)
					{
						VolumeInfo.EffectiveHeight = TotalHeight;
					}
					if (LevelsAttr)
					{
						const int32 Levels = LevelsAttr->GetValueFromItemKey(InfoEntryKey);
						if (Levels > 0)
						{
							VolumeInfo.EffectiveLevels = Levels;
						}
					}
				}
			}
		}
		VolumeInfo.MaxHeightZ = VolumeInfo.MinHeightZ + VolumeInfo.EffectiveHeight;
		Volumes.Add(MoveTemp(VolumeInfo));
	}

	// Candidate lists (bounding-box-overlap only -- cheap, O(N^2) pairs) computed once so the
	// per-edge-sample point-in-polygon checks below only ever run against footprints that could
	// plausibly overlap, not every footprint in the whole input -- see this node's own header
	// comment on why this isn't a spatial grid (out of scope for this feature; fine for a single
	// generation run over a normal-sized OSM extract).
	TArray<TArray<int32>> CandidatesByVolume;
	if (Settings->bSuppressOverlappingWalls)
	{
		CandidatesByVolume.SetNum(Volumes.Num());
		for (int32 SelfIndex = 0; SelfIndex < Volumes.Num(); ++SelfIndex)
		{
			if (Volumes[SelfIndex].Ring2D.Num() < 3)
			{
				continue;
			}
			for (int32 OtherIndex = 0; OtherIndex < Volumes.Num(); ++OtherIndex)
			{
				if (OtherIndex == SelfIndex || Volumes[OtherIndex].Ring2D.Num() < 3)
				{
					continue;
				}
				if (Volumes[SelfIndex].Bounds2D.Intersect(Volumes[OtherIndex].Bounds2D))
				{
					CandidatesByVolume[SelfIndex].Add(OtherIndex);
				}
			}
		}
	}

	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
	{
		const FPCGTaggedData& Input = Inputs[InputIndex];
		const TArray<FSplinePoint>& SplinePoints = NormalizedSplinePointsByInput[InputIndex];
		const int32 Count = SplinePoints.Num();
		if (Count < 3)
		{
			continue;
		}

		// This building's own OSM-derived TotalHeight/Levels (see UPCGLoadOsmBuildingVolumesSettings'
		// header comment) if BuildingInfo is connected and has a usable row, else this node's own flat
		// Height (Levels default to 1, i.e. no row-splitting possible without a real Levels count) --
		// see this class's header comment. Resolved once already, in the pre-pass above (Volumes is
		// index-aligned with Inputs), and reused here rather than redone.
		const double EffectiveHeight = Volumes[InputIndex].EffectiveHeight;
		const int32 EffectiveLevels = Volumes[InputIndex].EffectiveLevels;

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

		// Window/door opening data, for cutting recesses into the wall below -- see this class's
		// header comment for why this only ever activates when StyleInfo has a row for this building
		// (Openings stays empty otherwise, and the wall renders exactly as it did before this feature
		// existed). Ported from UPCGFacadeWindowDoorLayoutSettings' own per-edge resolution
		// (WindowOffsets/DoorApplies/WindowOverlapsDoor, PCGFacadeWindowDoorLayout.cpp lines 540-598) --
		// this node has to independently reach the exact same offsets that node will separately compute
		// for the actual window/door meshes, or the recess and the openings it's meant to match would
		// drift apart.
		const int64 WindowStyleEntryKey = StyleInfo ? StyleInfo->FindMetadataKey(FName(*SourceName)) : INDEX_NONE;
		const bool bHasWindowStyleRow = (WindowStyleEntryKey != INDEX_NONE) && StyleWindowWidthAttr;
		double WindowWidth = 0.0, WindowHeight = 0.0, WindowSpacing = 0.0, WindowMargin = 0.0, WindowSillHeight = 0.0, WindowRecessDepth = 0.0;
		bool bDoorEnabled = false;
		double DoorWidth = 0.0, DoorHeight = 0.0, DoorRecessDepth = 0.0, DoorFrameWidth = 12.0;
		EGrammarDoorPlacement DoorPlacement = EGrammarDoorPlacement::None;
		if (bHasWindowStyleRow)
		{
			WindowWidth = StyleWindowWidthAttr->GetValueFromItemKey(WindowStyleEntryKey);
			WindowHeight = StyleWindowHeightAttr ? StyleWindowHeightAttr->GetValueFromItemKey(WindowStyleEntryKey) : 0.0;
			WindowSpacing = StyleWindowSpacingAttr ? StyleWindowSpacingAttr->GetValueFromItemKey(WindowStyleEntryKey) : 0.0;
			WindowMargin = StyleWindowMarginAttr ? StyleWindowMarginAttr->GetValueFromItemKey(WindowStyleEntryKey) : 0.0;
			WindowSillHeight = StyleWindowSillHeightAttr ? StyleWindowSillHeightAttr->GetValueFromItemKey(WindowStyleEntryKey) : 0.0;
			WindowRecessDepth = StyleWindowRecessDepthAttr ? StyleWindowRecessDepthAttr->GetValueFromItemKey(WindowStyleEntryKey) : 0.0;
			bDoorEnabled = StyleDoorEnabledAttr && StyleDoorEnabledAttr->GetValueFromItemKey(WindowStyleEntryKey);
			DoorWidth = StyleDoorWidthAttr ? StyleDoorWidthAttr->GetValueFromItemKey(WindowStyleEntryKey) : 0.0;
			DoorHeight = StyleDoorHeightAttr ? StyleDoorHeightAttr->GetValueFromItemKey(WindowStyleEntryKey) : 0.0;
			DoorRecessDepth = StyleDoorRecessDepthAttr ? StyleDoorRecessDepthAttr->GetValueFromItemKey(WindowStyleEntryKey) : 0.0;
			DoorFrameWidth = StyleDoorFrameWidthAttr ? StyleDoorFrameWidthAttr->GetValueFromItemKey(WindowStyleEntryKey) : 12.0;
			DoorPlacement = StyleDoorPlacementAttr ? ParseDoorPlacement(StyleDoorPlacementAttr->GetValueFromItemKey(WindowStyleEntryKey)) : EGrammarDoorPlacement::None;
		}

		// This building's raw OSM tags -- only consulted for grammar:disable_ground_entrance and
		// (if needed below) StreetFacing door detection's own tag tiers, same as
		// UPCGFacadeWindowDoorLayoutSettings' identical use.
		TMap<FString, FString> Tags;
		if (InfoMetadata && TagsJsonAttr)
		{
			const int64 InfoEntryKey = BuildingInfo->FindMetadataKey(FName(*SourceName));
			if (InfoEntryKey != INDEX_NONE)
			{
				Tags = DeserializeTagsFromJson(TagsJsonAttr->GetValueFromItemKey(InfoEntryKey));
			}
		}
		const bool bGroundEntranceDisabled = [&Tags]
		{
			const FString* Disable = Tags.Find(TEXT("grammar:disable_ground_entrance"));
			if (!Disable)
			{
				return false;
			}
			const FString Normalized = Disable->TrimStartAndEnd().ToLower();
			return Normalized == TEXT("1") || Normalized == TEXT("yes") || Normalized == TEXT("true");
		}();

		// Only resolved when actually needed (a StreetFacing door on this building) -- built from a
		// throwaway per-building EdgePoints/Length pair (one point per RAW edge, not split into
		// wall-overlap-suppression sub-ranges, matching UPCGFacadeWindowDoorLayoutSettings' own
		// resolution, which is likewise upstream of any such splitting).
		int32 StreetSideIndex = 0;
		const bool bDoorAppliesAtAll = bHasWindowStyleRow && bDoorEnabled && !bGroundEntranceDisabled && DoorPlacement != EGrammarDoorPlacement::None;
		if (bDoorAppliesAtAll && DoorPlacement == EGrammarDoorPlacement::StreetFacing)
		{
			UPCGPointData* TempEdgeData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
			UPCGMetadata* TempEdgeMetadata = TempEdgeData->MutableMetadata();
			FPCGMetadataAttribute<double>* TempLengthAttr = TempEdgeMetadata->CreateAttribute<double>(TEXT("Length"), 0.0, false, false);
			TArray<FPCGPoint>& TempPoints = TempEdgeData->GetMutablePoints();
			for (int32 EdgeIndex = 0; EdgeIndex < Count; ++EdgeIndex)
			{
				const FVector& EdgeStart = SplinePoints[EdgeIndex].Position;
				const FVector& EdgeEnd = SplinePoints[(EdgeIndex + 1) % Count].Position;
				const FVector EdgeVector = EdgeEnd - EdgeStart;
				const double EdgeLength = EdgeVector.Size();
				const FVector EdgeTangent = EdgeLength > KINDA_SMALL_NUMBER ? EdgeVector / EdgeLength : FVector::ForwardVector;
				const FVector EdgeOutwardNormal(EdgeTangent.Y, -EdgeTangent.X, 0.0);

				FPCGPoint TempPoint;
				TempPoint.Transform = FTransform(FRotationMatrix::MakeFromXY(EdgeTangent, EdgeOutwardNormal).ToQuat(), EdgeStart);
				TempPoint.MetadataEntry = TempEdgeMetadata->AddEntry();
				TempLengthAttr->SetValue(TempPoint.MetadataEntry, EdgeLength);
				TempPoints.Add(TempPoint);
			}
			// No Streets pin on this node (see this class's header comment) -- an empty segment list
			// here is the exact same fallback UPCGFacadeWindowDoorLayoutSettings itself uses whenever
			// its own optional Streets pin is left unconnected.
			StreetSideIndex = DetermineStreetFacingSideIndex(Tags, TempPoints, TempLengthAttr, TArray<FStreetSegment>(), 0.0);
		}

		// Centered window offsets along each edge -- these will be filtered per-edge below (an edge's
		// own Length varies), same centering formula UPCGFacadeWindowDoorLayoutSettings' own inline
		// port uses (itself ported from GrammarWallWindow::WindowOffsets, BuildingGrammarCore).
		auto ComputeWindowOffsets = [WindowWidth, WindowSpacing, WindowMargin](double EdgeLength) -> TArray<double>
		{
			TArray<double> Offsets;
			const double Usable = EdgeLength - WindowMargin * 2.0;
			if (WindowWidth <= 0.0 || Usable < WindowWidth)
			{
				return Offsets;
			}
			const int32 WindowCount = FMath::Max(1, static_cast<int32>(FMath::FloorToDouble((Usable + WindowSpacing - WindowWidth) / WindowSpacing)));
			const double TotalWidth = (WindowCount - 1) * WindowSpacing + WindowWidth;
			const double Start = (EdgeLength - TotalWidth) / 2.0 + WindowWidth / 2.0;
			Offsets.Reserve(WindowCount);
			for (int32 WindowIndex = 0; WindowIndex < WindowCount; ++WindowIndex)
			{
				Offsets.Add(Start + WindowIndex * WindowSpacing);
			}
			return Offsets;
		};

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

			// Extrusion base is Start.Z (== End.Z always -- every point of one building's footprint
			// shares the same MinHeight, baked in by UPCGLoadOsmBuildingVolumesSettings), not an
			// assumed 0.0 -- see that node's header comment for why this is the single source of
			// truth for this pipeline's min_height/min_level building-part offset.
			const double BaseZ = Start.Z;

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
			const FQuat EdgeRotation = FRotationMatrix::MakeFromXY(Tangent, OutwardNormal).ToQuat();
			const FVector2D Normal2D(OutwardNormal.X, OutwardNormal.Y);

			// Window/door opening rectangles for THIS edge, offset-along-edge (S) relative to this
			// edge's own Start (0..Length, matching ComputeWindowOffsets/DoorOffset below) -- rebased
			// per sub-range further down, since wall-overlap suppression can still split this edge.
			// Mirrors UPCGFacadeWindowDoorLayoutSettings' own per-edge DoorApplies/WindowOverlapsDoor
			// logic (PCGFacadeWindowDoorLayout.cpp lines 545-598) so the two nodes agree on where
			// openings are.
			TArray<FGrammarWallOpening> EdgeOpenings;
			if (bHasWindowStyleRow)
			{
				const bool bDoorOnThisEdge = bDoorAppliesAtAll
					&& (DoorPlacement == EGrammarDoorPlacement::EachFacade || Index == StreetSideIndex)
					&& Length > DoorWidth;
				const double DoorOffset = Length * 0.5;
				const double FloorHeight = EffectiveHeight / EffectiveLevels;

				for (const double WindowOffset : ComputeWindowOffsets(Length))
				{
					for (int32 FloorIndex = 0; FloorIndex < EffectiveLevels; ++FloorIndex)
					{
						if (FloorIndex == 0 && bDoorOnThisEdge)
						{
							const double Clearance = (WindowWidth + DoorWidth) * 0.5 + FMath::Max(DoorFrameWidth, 0.0);
							if (FMath::Abs(WindowOffset - DoorOffset) < Clearance)
							{
								continue;
							}
						}
						FGrammarWallOpening& Opening = EdgeOpenings.AddDefaulted_GetRef();
						Opening.SLeft = WindowOffset - WindowWidth / 2.0;
						Opening.SRight = WindowOffset + WindowWidth / 2.0;
						Opening.ZBottom = BaseZ + FloorIndex * FloorHeight + WindowSillHeight;
						Opening.ZTop = Opening.ZBottom + WindowHeight;
						Opening.RecessDepth = WindowRecessDepth;
					}
				}
				if (bDoorOnThisEdge)
				{
					FGrammarWallOpening& Opening = EdgeOpenings.AddDefaulted_GetRef();
					Opening.SLeft = DoorOffset - DoorWidth / 2.0;
					Opening.SRight = DoorOffset + DoorWidth / 2.0;
					Opening.ZBottom = BaseZ;
					Opening.ZTop = BaseZ + DoorHeight;
					Opening.RecessDepth = DoorRecessDepth;
				}
			}

			// Wall-overlap suppression (see this node's own header comment): split this edge into
			// just its sub-ranges NOT covered by another footprint. With suppression off, or no
			// candidate footprint could plausibly overlap this one at all, this is just the whole
			// edge unchanged (single [0, Length) range) -- the common case pays only the (cheap)
			// bounding-box check done once above, not any per-sample point-in-polygon work.
			TArray<TPair<double, double>> SubRanges;
			if (Settings->bSuppressOverlappingWalls && CandidatesByVolume.IsValidIndex(InputIndex) && !CandidatesByVolume[InputIndex].IsEmpty())
			{
				const FVector2D Start2D(Start.X, Start.Y);
				const FVector2D Tangent2D(Tangent.X, Tangent.Y);
				SubRanges = ComputeUncoveredSubRanges(Start2D, Tangent2D, Length, InputIndex, Volumes, CandidatesByVolume[InputIndex], Settings->WallSuppressionSampleSpacing);
			}
			else
			{
				SubRanges.Add(TPair<double, double>(0.0, Length));
			}

			for (const TPair<double, double>& SubRange : SubRanges)
			{
				const double SubStartDist = SubRange.Key;
				const double SubLength = SubRange.Value - SubRange.Key;
				if (SubLength <= KINDA_SMALL_NUMBER)
				{
					continue;
				}
				const FVector SubStart = Start + Tangent * SubStartDist;
				const FVector SubEnd = Start + Tangent * SubRange.Value;
				const FVector2D SubStart2D(SubStart.X, SubStart.Y);
				const FVector2D SubEnd2D(SubEnd.X, SubEnd.Y);

				// EdgeOpenings above are relative to the RAW edge's own Start -- rebase to this
				// sub-range's own Start (SubStartDist may be >0 when wall-overlap suppression split
				// the edge). FGrammarWallRecess::BuildSegments' own margin-fit check silently leaves
				// any opening that lands outside [0,SubLength] after this shift flush/uncut, so no
				// extra filtering is needed here.
				TArray<FGrammarWallOpening> SubRangeOpenings;
				SubRangeOpenings.Reserve(EdgeOpenings.Num());
				for (FGrammarWallOpening Opening : EdgeOpenings)
				{
					Opening.SLeft -= SubStartDist;
					Opening.SRight -= SubStartDist;
					SubRangeOpenings.Add(Opening);
				}

				// Port of BuildingGrammarEngine.cpp's own dispatch (WallRowColors.Num()>0 ? per-floor
				// rows : one whole-facade-height quad) -- see this node's header comment.
				// RowWallColor is FloorIndex-only (no SideIndex/stable-hash), VariantWallColor is
				// SideIndex-only (this edge's own loop Index, matching classic's per-side SideIndex
				// exactly, shared by every sub-range of this same edge) -- the two are mutually
				// exclusive per building, same as classic. Openings belonging to a DIFFERENT floor's Z
				// band than whichever [WallBottom,WallTop] a given BuildSegments call covers are simply
				// left out by its own margin-fit check -- see FGrammarWallRecess::BuildSegments' own
				// comment -- so the same SubRangeOpenings list is reused for every floor/whole-height
				// call below without pre-splitting it by floor.
				if (WallRowColors.Num() > 0)
				{
					const double FloorHeight = EffectiveHeight / EffectiveLevels;
					for (int32 FloorIndex = 0; FloorIndex < EffectiveLevels; ++FloorIndex)
					{
						const TPair<int32, FLinearColor> ColorResult = ResolveRowWallColor(WallRowColors, WallRowColorMode, BaseWallColor, FloorIndex);
						const FString MaterialName = WallMaterialName(BaseWallMaterial, TEXT("row"), ColorResult.Key);
						const int32 Slot = GetOrAddMaterialSlot(MaterialName, ColorResult.Value);
						for (const FGrammarWallQuad& Quad : FGrammarWallRecess::BuildSegments(SubStart2D, SubEnd2D, Normal2D, BaseZ + FloorIndex * FloorHeight, BaseZ + (FloorIndex + 1) * FloorHeight, SubRangeOpenings))
						{
							AppendWallQuadMesh(WallMesh, Normals, UVs, MaterialIDs, Slot, Quad, FMath::Max(Settings->TextureScale, 1.0), Settings->bFlipWinding, Settings->bFlipNormals);
						}
					}
				}
				else
				{
					const TPair<int32, FLinearColor> ColorResult = ResolveVariantWallColor(WallColorVariants, WallColorVariantMode, BaseWallColor, SourceName, Index);
					const FString MaterialName = WallMaterialName(BaseWallMaterial, TEXT("variant"), ColorResult.Key);
					const int32 Slot = GetOrAddMaterialSlot(MaterialName, ColorResult.Value);
					for (const FGrammarWallQuad& Quad : FGrammarWallRecess::BuildSegments(SubStart2D, SubEnd2D, Normal2D, BaseZ, BaseZ + EffectiveHeight, SubRangeOpenings))
					{
						AppendWallQuadMesh(WallMesh, Normals, UVs, MaterialIDs, Slot, Quad, FMath::Max(Settings->TextureScale, 1.0), Settings->bFlipWinding, Settings->bFlipNormals);
					}
				}

				FPCGPoint EdgePoint;
				EdgePoint.Transform = FTransform(EdgeRotation, SubStart);
				EdgePoint.Density = 1.0f;
				EdgePoint.Seed = Index;
				EdgePoint.MetadataEntry = EdgeMetadata->AddEntry();
				LengthAttr->SetValue(EdgePoint.MetadataEntry, SubLength);
				EdgeIndexAttr->SetValue(EdgePoint.MetadataEntry, Index);
				EdgePoints->GetMutablePoints().Add(EdgePoint);
			}
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
