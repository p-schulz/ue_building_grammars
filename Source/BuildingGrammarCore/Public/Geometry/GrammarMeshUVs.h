#pragma once

#include "CoreMinimal.h"
#include "Geometry/GrammarFace.h"

// Port of blender_adapter.py's _assign_uvs/_uv_axes: per-face axis-aligned planar UV projection
// (picks whichever of XY/YZ/XZ the face's normal -- from its first 3 vertices, not a full
// Newell's-method normal, matching the Python source exactly -- is most aligned with, then UVs are
// each vertex's offset from the face's first vertex, dotted onto those two axes and divided by a
// texture scale). Not true per-face tangent-space UVs -- box-aligned/axis-planar mapping only,
// same limitation the original has.
class BUILDINGGRAMMARCORE_API FGrammarMeshUVs
{
public:
	// Returns one UV per entry of Face.Indices, in the same order (i.e. result[i] is the UV for
	// vertex Face.Indices[i]) -- matches how a mesh format stores one UV per face-corner ("loop"),
	// not one UV per shared vertex.
	static TArray<FVector2D> ComputeFaceUVs(const TArray<FVector>& Vertices, const FGrammarFace& Face, double TextureScale);

private:
	static void PickUVAxes(const TArray<FVector>& FaceVertices, FVector& OutAxisA, FVector& OutAxisB);
};
