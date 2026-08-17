#include "Elements/PCGExtrudeBarrierWay.h"
#include "PCGContext.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGDynamicMeshData.h"
#include "Components/SplineComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Geometry/GrammarWallRecess.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const FName BarriersPinLabel = TEXT("Barriers");
	const FName BarrierPinLabel = TEXT("Barrier");

	FString ExtractTagValue(const TSet<FString>& Tags, const FString& Prefix)
	{
		for (const FString& Tag : Tags)
		{
			if (Tag.StartsWith(Prefix))
			{
				return Tag.Mid(Prefix.Len());
			}
		}
		return FString();
	}

	// Same triangle-append pattern as UPCGExtrudeFootprintToWallsSettings' own AppendTriangleWithComputedNormal
	// (BuildingGrammarPCG/Private/Elements/PCGExtrudeFootprintToWalls.cpp) -- winding and the stored
	// shading normal are independent controls, not assumed derivable from each other by hand math.
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
}

TArray<FPCGPinProperties> UPCGExtrudeBarrierWaySettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(BarriersPinLabel, EPCGDataType::Spline);
	return Pins;
}

TArray<FPCGPinProperties> UPCGExtrudeBarrierWaySettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(BarrierPinLabel, EPCGDataType::DynamicMesh);
	return Pins;
}

FPCGElementPtr UPCGExtrudeBarrierWaySettings::CreateElement() const
{
	return MakeShared<FPCGExtrudeBarrierWayElement>();
}

bool FPCGExtrudeBarrierWayElement::ExecuteInternal(FPCGContext* Context) const
{
	using namespace UE::Geometry;

	const UPCGExtrudeBarrierWaySettings* Settings = Context->GetInputSettings<UPCGExtrudeBarrierWaySettings>();
	check(Settings);

	UMaterialInterface* FallbackMaterial = Settings->Material.LoadSynchronous();
	const double TextureScale = FMath::Max(Settings->TextureScale, 1.0);

	for (const FPCGTaggedData& BarrierTaggedData : Context->InputData.GetInputsByPin(BarriersPinLabel))
	{
		const UPCGSplineData* BarrierSpline = Cast<UPCGSplineData>(BarrierTaggedData.Data.Get());
		if (!BarrierSpline)
		{
			continue;
		}

		const TArray<FSplinePoint> SplinePoints = BarrierSpline->GetSplinePoints();
		const int32 Count = SplinePoints.Num();
		if (Count < 2)
		{
			continue;
		}

		const FString HeightTagValue = ExtractTagValue(BarrierTaggedData.Tags, TEXT("Height:"));
		const double EffectiveHeight = HeightTagValue.IsEmpty() ? Settings->Height : FCString::Atod(*HeightTagValue);

		FDynamicMesh3 BarrierMesh;
		BarrierMesh.EnableAttributes();
		FDynamicMeshNormalOverlay* Normals = BarrierMesh.Attributes()->PrimaryNormals();
		FDynamicMeshUVOverlay* UVs = BarrierMesh.Attributes()->PrimaryUV();
		BarrierMesh.Attributes()->EnableMaterialID();
		FDynamicMeshMaterialAttribute* MaterialIDs = BarrierMesh.Attributes()->GetMaterialID();

		// Open polyline: Count-1 segments, no closing wrap (unlike
		// UPCGExtrudeFootprintToWallsSettings' closed-ring % Count).
		for (int32 Index = 0; Index + 1 < Count; ++Index)
		{
			const FVector& Start = SplinePoints[Index].Position;
			const FVector& End = SplinePoints[Index + 1].Position;
			const FVector Edge = End - Start;
			const double Length = Edge.Size();
			if (Length <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const FVector Tangent = Edge / Length;

			// No inherent "outward" for an open barrier line (unlike a closed footprint ring) --
			// rotate tangent -90 degrees for a consistent, arbitrary-but-fixed side; bFlipNormals/
			// bFlipWinding exist precisely to let a user correct this per-graph if it renders backwards.
			const FVector2D Normal2D(Tangent.Y, -Tangent.X);
			const FVector2D Start2D(Start.X, Start.Y);
			const FVector2D End2D(End.X, End.Y);

			// Empty Openings -- a fence/wall has no windows -- still gets the correct single flush
			// quad + winding + UV logic for free from the shared geometry helper.
			for (const FGrammarWallQuad& Quad : FGrammarWallRecess::BuildSegments(Start2D, End2D, Normal2D, Start.Z, Start.Z + EffectiveHeight, TArray<FGrammarWallOpening>()))
			{
				AppendWallQuadMesh(BarrierMesh, Normals, UVs, MaterialIDs, 0, Quad, TextureScale, Settings->bFlipWinding, Settings->bFlipNormals);
			}
		}

		UPCGDynamicMeshData* BarrierData = FPCGContext::NewObject_AnyThread<UPCGDynamicMeshData>(Context);
		BarrierData->Initialize(MoveTemp(BarrierMesh), TArray<UMaterialInterface*>{ FallbackMaterial });

		FPCGTaggedData& Out = Context->OutputData.TaggedData.Emplace_GetRef();
		Out.Data = BarrierData;
		Out.Pin = BarrierPinLabel;
		Out.Tags = BarrierTaggedData.Tags;
	}

	return true;
}
