#pragma once

#include "CoreMinimal.h"
#include "Geometry/GrammarFace.h"

// Ear-clipping triangulation for the arbitrary polygonal faces grammar generation produces (see
// GrammarFace.h). Most faces are already triangles or convex quads, but the flat-roof role emits
// a single face listing the whole footprint outline, which can be non-convex (L-shaped, U-shaped,
// or otherwise irregular buildings are common in OSM data) -- fan triangulation from vertex 0
// would produce wrong/inverted triangles for those, so this does real ear-clipping instead. Not
// derived from the sibling osm_building prototype's triangulate() (same well-known algorithm,
// reimplemented independently); does not need hole support since neither grammar.py nor this port
// threads footprint holes (courtyards) into wall/roof generation at all -- see docs/PLAN.md.
class BUILDINGGRAMMARCORE_API FGrammarPolygonTriangulator
{
public:
	// Triangulates one face of a mesh spec. Vertices is the mesh's full vertex array; Face.Indices
	// selects and orders this face's loop within it. Returns a flat list of triangle index
	// triples indexing directly into Vertices (i.e. already Face.Indices[i] values, not local
	// 0..N-1 face-relative indices) -- empty for a face with fewer than 3 vertices. Assumes Face
	// is planar (true for everything the grammar engine emits) and simple (non-self-intersecting).
	static TArray<int32> Triangulate(const TArray<FVector>& Vertices, const FGrammarFace& Face);
};
