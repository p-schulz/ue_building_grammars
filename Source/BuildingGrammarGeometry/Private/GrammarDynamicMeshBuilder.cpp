#include "GrammarDynamicMeshBuilder.h"
#include "Geometry/GrammarPolygonTriangulator.h"
#include "Geometry/GrammarMeshUVs.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"

using namespace UE::Geometry;

void FGrammarDynamicMeshBuilder::BuildDynamicMesh(const FGrammarMeshSpec& MeshSpec, FDynamicMesh3& OutMesh)
{
	OutMesh = FDynamicMesh3();
	OutMesh.EnableAttributes();
	FDynamicMeshNormalOverlay* Normals = OutMesh.Attributes()->PrimaryNormals();
	FDynamicMeshUVOverlay* UVs = OutMesh.Attributes()->PrimaryUV();

	TArray<int32> VertexIDs;
	VertexIDs.Reserve(MeshSpec.Vertices.Num());
	for (const FVector& Position : MeshSpec.Vertices)
	{
		VertexIDs.Add(OutMesh.AppendVertex(Position));
	}

	for (const FGrammarFace& Face : MeshSpec.Faces)
	{
		if (Face.Indices.Num() < 3)
		{
			continue;
		}

		// Faces are planar by construction (see GrammarFace.h) -- one flat normal per face,
		// shared by every triangle the face is split into.
		const FVector EdgeA = MeshSpec.Vertices[Face.Indices[1]] - MeshSpec.Vertices[Face.Indices[0]];
		const FVector EdgeB = MeshSpec.Vertices[Face.Indices[2]] - MeshSpec.Vertices[Face.Indices[0]];
		const FVector3f FaceNormal(FVector::CrossProduct(EdgeA, EdgeB).GetSafeNormal());

		const TArray<FVector2D> FaceUVs = FGrammarMeshUVs::ComputeFaceUVs(MeshSpec.Vertices, Face, MeshSpec.TextureScale);
		TMap<int32, FVector2D> UVByVertexIndex;
		for (int32 LoopIndex = 0; LoopIndex < Face.Indices.Num(); ++LoopIndex)
		{
			UVByVertexIndex.Add(Face.Indices[LoopIndex], FaceUVs[LoopIndex]);
		}

		const TArray<int32> Triangles = FGrammarPolygonTriangulator::Triangulate(MeshSpec.Vertices, Face);
		for (int32 TriIndex = 0; TriIndex + 2 < Triangles.Num(); TriIndex += 3)
		{
			const int32 A = Triangles[TriIndex];
			const int32 B = Triangles[TriIndex + 1];
			const int32 C = Triangles[TriIndex + 2];

			const int32 TriangleID = OutMesh.AppendTriangle(VertexIDs[A], VertexIDs[B], VertexIDs[C]);
			if (TriangleID < 0)
			{
				continue;
			}

			const int32 NA = Normals->AppendElement(FaceNormal);
			const int32 NB = Normals->AppendElement(FaceNormal);
			const int32 NC = Normals->AppendElement(FaceNormal);
			Normals->SetTriangle(TriangleID, FIndex3i(NA, NB, NC));

			const int32 UA = UVs->AppendElement(FVector2f(UVByVertexIndex[A]));
			const int32 UB = UVs->AppendElement(FVector2f(UVByVertexIndex[B]));
			const int32 UC = UVs->AppendElement(FVector2f(UVByVertexIndex[C]));
			UVs->SetTriangle(TriangleID, FIndex3i(UA, UB, UC));
		}
	}
}
