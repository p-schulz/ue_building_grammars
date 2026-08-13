#include "Elements/PCGRoofFrameGenerator.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGDynamicMeshData.h"
#include "Geometry/GrammarRoofFrame.h"
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

		// Resolved once per building, before any mesh-building math -- this building's own StyleInfo
		// row (if present) supplies its actual roof Type, not just Material/Color, so two buildings
		// with different resolved styles (Flat vs Gabled) render with genuinely different roof shapes
		// instead of every building always matching whatever this node's own flat Settings say. See
		// UPCGSelectFacadeStyleSettings' own header comment for how RoofType is resolved
		// (bOverrideRoof-else-DefaultRoof, same as RoofMaterial/RoofColor).
		const int64 StyleEntryKey = StyleInfo ? StyleInfo->FindMetadataKey(FName(*ExtractSourceNameFromTags(Input.Tags))) : INDEX_NONE;
		EGrammarRoofType EffectiveRoofType = Settings->RoofType;
		if (StyleEntryKey != INDEX_NONE && RoofTypeAttr)
		{
			EffectiveRoofType = RoofTypeFromString(RoofTypeAttr->GetValueFromItemKey(StyleEntryKey));
		}

		// Phase A: Flat and Gabled only -- Hipped/Pyramid are a documented Phase B follow-up (see
		// this node's own header comment); fall back to Flat for anything else.
		const bool bGabled = (EffectiveRoofType == EGrammarRoofType::Gabled);

		// This building's own OSM-derived TotalHeight (see UPCGLoadOsmBuildingVolumesSettings'
		// header comment) if BuildingInfo is connected and has a usable row, else this node's own
		// flat EaveHeight -- see this class's header comment.
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
			}
		}

		const TArray<FVector> Base = FGrammarRoofFrameMath::RoofBaseVertices(Footprint2D, EffectiveEaveHeight, Settings->Overhang);

		FDynamicMesh3 RoofMesh;
		RoofMesh.EnableAttributes();
		FDynamicMeshNormalOverlay* Normals = RoofMesh.Attributes()->PrimaryNormals();
		FDynamicMeshUVOverlay* UVs = RoofMesh.Attributes()->PrimaryUV();

		const double TextureScale = FMath::Max(Settings->TextureScale, 1.0);

		if (!bGabled)
		{
			for (int32 Index = 1; Index + 1 < Base.Num(); ++Index)
			{
				AppendTriangleWithComputedNormal(RoofMesh, Normals, UVs, Base[0], Base[Index], Base[Index + 1], TextureScale, Settings->bFlipWinding, Settings->bFlipNormals);
			}
		}
		else
		{
			// Ridge direction: the footprint's own longest edge (self-contained, not a call into
			// BuildingGrammarCore's FGrammarGeometry2D::LongestAxisDirection -- see this node's own
			// header comment).
			FVector2D RidgeDirection(1.0, 0.0);
			double BestLengthSquared = -1.0;
			for (int32 Index = 0; Index < Footprint2D.Num(); ++Index)
			{
				const FVector2D Edge = Footprint2D[(Index + 1) % Footprint2D.Num()] - Footprint2D[Index];
				const double LengthSquared = Edge.SizeSquared();
				if (LengthSquared > BestLengthSquared)
				{
					BestLengthSquared = LengthSquared;
					RidgeDirection = Edge;
				}
			}
			if (!RidgeDirection.IsNearlyZero())
			{
				RidgeDirection.Normalize();
			}

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
