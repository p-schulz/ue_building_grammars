using UnrealBuildTool;

// Custom PCG (Procedural Content Generation framework) nodes offering an alternative,
// PCG-graph-driven building generation pipeline alongside BuildingGrammarRuntime's own deterministic
// C++ engine (see docs/PLAN.md-equivalent design notes for this feature). Data-source nodes reuse
// BuildingGrammarCore/Runtime's existing OSM parsing and projection rather than re-implementing it;
// only the per-building layout (walls/windows/roofs) is meant to be expressed as native PCG graph
// nodes -- see individual Elements/*.h headers.
public class BuildingGrammarPCG : ModuleRules
{
	public BuildingGrammarPCG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PCG",
			"GeometryCore",
			"BuildingGrammarCore",
			"BuildingGrammarRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			// FGrammarKitResolver::ResolveMaterial, used by the wall/roof/facade-detail nodes to
			// resolve the same persistent per-style UMaterialInstanceConstant assets the classic
			// (non-PCG) engine uses, instead of each node's own single fixed Material property.
			// Already reachable transitively via BuildingGrammarRuntime's own public dependency on
			// it, but listed explicitly since these nodes' .cpp files include its header directly.
			"BuildingGrammarGeometry",
			// FGrammarStreetAlignment-style TagsJson attribute serialization on the data-source
			// nodes' companion attribute-set output (see PCGLoadOsmBuildingVolumes.cpp/
			// PCGGetStreetNetwork.cpp).
			"Json",
			// IPluginManager::FindPlugin, used by PCGBuildingGrammarDefaults.cpp to locate the
			// bundled german_building_grammar_config.json under this plugin's own Content directory.
			"Projects"
		});
	}
}
