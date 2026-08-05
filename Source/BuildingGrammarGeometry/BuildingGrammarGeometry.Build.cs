using UnrealBuildTool;

// Turns BuildingGrammarCore's FMeshSpec/FPlacementRecord output into real Unreal geometry:
// FDynamicMesh3 hero surfaces (walls/roofs), and a baked Nanite "kit" -- one shared unit-box
// UStaticMesh plus a per-(role,material) UMaterialInstanceDynamic -- for every instanced element
// (see GrammarKitResolver.h). The DynamicMesh runtime path has no Editor dependency; only the
// unit-box mesh + master-material construction (GrammarKitAssetBuilder) are WITH_EDITOR-guarded,
// since baking new UStaticMesh/UMaterial assets is only possible inside the editor -- a packaged
// game loads the assets the editor already baked and saved.
public class BuildingGrammarGeometry : ModuleRules
{
	public BuildingGrammarGeometry(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GeometryCore",
			"GeometryFramework",
			"BuildingGrammarCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"MeshDescription",
			"StaticMeshDescription"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new[]
			{
				"UnrealEd",
				"GeometryScriptingCore",
				"MaterialEditingLibrary",
				"AssetRegistry",
				"AssetTools"
			});
		}
	}
}
