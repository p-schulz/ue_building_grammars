#pragma once

#include "CoreMinimal.h"
#include "Spec/MeshSpec.h"
#include "DynamicMesh/DynamicMesh3.h"

// Converts one hero-surface FGrammarMeshSpec (facade wall or roof plane -- everything else is a
// placement record, see FGrammarBuildingSpec's comment) into a triangle-mesh
// UE::Geometry::FDynamicMesh3: vertices, triangulated faces (FGrammarPolygonTriangulator, from
// BuildingGrammarCore), and flat per-triangle-corner normals + planar UVs (FGrammarMeshUVs).
// Suitable to feed a UDynamicMeshComponent directly for a pure-runtime-generated building, or as
// input to an editor-time UStaticMesh bake.
//
// This is the file in the whole port with the least amount of engine-reference verification --
// FDynamicMesh3's attribute-overlay API (EnableAttributes/PrimaryNormals/PrimaryUV/AppendElement)
// was written from real but imperfect recollection, without a compiler or engine headers
// available to check exact method names/signatures against. Check this file first if the
// BuildingGrammarGeometry module fails to compile.
class BUILDINGGRAMMARGEOMETRY_API FGrammarDynamicMeshBuilder
{
public:
	static void BuildDynamicMesh(const FGrammarMeshSpec& MeshSpec, UE::Geometry::FDynamicMesh3& OutMesh);
};
