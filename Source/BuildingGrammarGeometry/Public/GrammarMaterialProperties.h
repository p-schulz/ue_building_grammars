#pragma once

#include "CoreMinimal.h"

// Port of blender_adapter.py's _material_roughness/_material_metallic keyword-sniffing. Neither
// grammar.py nor this port's config structs carry explicit PBR roughness/metallic fields -- the
// Python add-on infers them from the material *name* at materialization time, and this keeps that
// same behavior rather than introducing a config schema change. Explicit roughness/metallic fields
// (or a UMaterialInterface reference) on each style struct would be a cleaner long-term
// replacement; noted as a follow-up, not attempted here to keep this change scoped to "kit baking
// now works" rather than also redesigning the config schema.
class BUILDINGGRAMMARGEOMETRY_API FGrammarMaterialProperties
{
public:
	static float RoughnessForMaterialName(const FString& MaterialName);
	static float MetallicForMaterialName(const FString& MaterialName);
};
