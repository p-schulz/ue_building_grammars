using UnrealBuildTool;

// Actors, the per-cell HISM instance pool, and the world subsystem that drives generation either
// from an editor button press or from runtime proximity streaming.
public class BuildingGrammarRuntime : ModuleRules
{
	public BuildingGrammarRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"BuildingGrammarCore",
			"BuildingGrammarGeometry",
			"GeometryCore",
			"GeometryFramework"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// ABuildingInstancePoolActor::BakeToStaticMesh builds an FMeshDescription (via
			// FStaticMeshAttributes) to feed UStaticMesh::BuildFromMeshDescriptions -- same pair
			// BuildingGrammarGeometry.Build.cs already lists for GrammarKitAssetBuilder.cpp's
			// identical pattern.
			"MeshDescription",
			"StaticMeshDescription"
		});

		if (Target.bBuildEditor)
		{
			// FBuildingActorPersistence's disk-backed save-and-unload path (FEditorFileUtils) is
			// editor-only -- guarded by WITH_EDITOR in BuildingActorPersistence.cpp.
			PrivateDependencyModuleNames.Add("UnrealEd");
			// ABuildingInstancePoolActor::BakeToStaticMesh's FMeshDescriptionToDynamicMesh /
			// FDynamicMeshToMeshDescription conversion, also editor-only (WITH_EDITOR-guarded).
			PrivateDependencyModuleNames.Add("MeshConversion");
			// BakeToStaticMesh's FAssetRegistryModule::AssetCreated call (new baked mesh asset).
			PrivateDependencyModuleNames.Add("AssetRegistry");
		}
	}
}
