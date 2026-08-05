#include "Grammar/BuildingGrammarEngine.h"
#include "Grammar/GrammarStyleSelection.h"
#include "Grammar/GrammarLevels.h"
#include "Grammar/GrammarEngineInternal.h"
#include "Grammar/GrammarWallWindow.h"
#include "Grammar/GrammarDoor.h"
#include "Grammar/GrammarLedgeBalcony.h"
#include "Grammar/GrammarFacadeDepth.h"
#include "Grammar/GrammarRoof.h"
#include "Grammar/GrammarRoofDetails.h"
#include "Geometry/GrammarGeometry2D.h"
#include "Geometry/GrammarStableHash.h"

namespace
{
	bool LedgeApplies(const FFacadeStyleConfig& Style, int32 FloorIndex)
	{
		return Style.Ledge.bEnabled && Style.Ledge.EveryNFloors > 0 && FloorIndex % Style.Ledge.EveryNFloors == 0;
	}

	bool BalconyApplies(const FFacadeStyleConfig& Style, int32 FloorIndex)
	{
		return Style.Balcony.bEnabled && FloorIndex > 0 && Style.Balcony.EveryNFloors > 0 && FloorIndex % Style.Balcony.EveryNFloors == 0;
	}

	// Selects the style with a roof override whose stable-hash index (keyed by SourceName) lands
	// on it -- unset if no style in Styles overrides the roof. Port of _roof_style_for_building.
	const FFacadeStyleConfig* RoofOverrideStyle(const FString& SourceName, const TArray<FFacadeStyleConfig>& Styles)
	{
		TArray<const FFacadeStyleConfig*> RoofStyles;
		for (const FFacadeStyleConfig& Style : Styles)
		{
			if (Style.bOverrideRoof)
			{
				RoofStyles.Add(&Style);
			}
		}
		if (RoofStyles.Num() == 0)
		{
			return nullptr;
		}
		return RoofStyles[FGrammarStableHash::StableIndex(SourceName, RoofStyles.Num())];
	}

	int32 WrapIndex(int32 Value, int32 Count)
	{
		return ((Value % Count) + Count) % Count;
	}
}

bool FBuildingGrammarEngine::GenerateBuildingSpec(const TArray<FVector2D>& Footprint, const TMap<FString, FString>& Tags, const FBuildingGrammarConfig& Config, const FString& SourceName, FGrammarBuildingSpec& OutSpec, FString& OutError)
{
	if (FGrammarStyleSelection::BuildingValueIsExcluded(Tags, Config.ExcludedBuildingValues, Config.Styles))
	{
		const FString* BuildingValue = Tags.Find(TEXT("building"));
		OutError = FString::Printf(TEXT("building value \"%s\" is excluded"), BuildingValue ? **BuildingValue : TEXT(""));
		return false;
	}

	const TArray<FVector2D> Clean = FGrammarGeometry2D::OrientFootprintCCW(Footprint);
	if (Clean.Num() < 3)
	{
		OutError = TEXT("A building footprint needs at least three points");
		return false;
	}

	static const TArray<FFacadeStyleConfig> DefaultStyles = { FFacadeStyleConfig() };
	const TArray<FFacadeStyleConfig>& StylesSource = Config.Styles.Num() > 0 ? Config.Styles : DefaultStyles;

	TArray<FFacadeStyleConfig> Styles;
	for (const FFacadeStyleConfig* Selected : FGrammarStyleSelection::SelectableStylesForTags(Tags, StylesSource))
	{
		Styles.Add(GrammarEngineInternal::FacadeStyleFromTags(*Selected, Tags));
	}
	if (Styles.Num() == 0)
	{
		Styles.Add(FFacadeStyleConfig());
	}

	const FFacadeStyleConfig& PrimaryStyle = Styles[0];
	const FFacadeStyleConfig* RoofStyleOverride = RoofOverrideStyle(SourceName, Styles);

	const int32 Levels = FGrammarLevels::InferLevels(Tags, Config, &PrimaryStyle);
	const TArray<double> FloorHeights = FGrammarLevels::FloorHeightSequence(Levels, Tags, Config, &PrimaryStyle);
	double TotalHeight = 0.0;
	for (const double Height : FloorHeights)
	{
		TotalHeight += Height;
	}

	FRoofStyleConfig Roof;
	if (RoofStyleOverride && RoofStyleOverride->bOverrideRoof)
	{
		Roof = RoofStyleOverride->RoofOverride;
	}
	else if (PrimaryStyle.bOverrideRoof)
	{
		Roof = PrimaryStyle.RoofOverride;
	}
	else
	{
		Roof = Config.Roof;
	}
	Roof = GrammarEngineInternal::RoofFromTags(Roof, Tags);

	const int32 StreetSideIndex = GrammarEngineInternal::StreetFacingSideIndex(Clean, Tags);
	const TArray<double> FloorBottoms = FGrammarGeometry2D::FloorBottoms(FloorHeights);
	const bool bCleanIsCCW = FGrammarGeometry2D::PolygonIsCCW(Clean);

	TArray<FGrammarMeshSpec> HeroMeshes;
	TArray<FGrammarPlacementRecord> Placements;

	const TArray<FGrammarGeometry2D::FEdge> Segments = FGrammarGeometry2D::GetSegments(Clean);
	for (int32 SideIndex = 0; SideIndex < Segments.Num(); ++SideIndex)
	{
		const FVector2D& Start = Segments[SideIndex].Start;
		const FVector2D& End = Segments[SideIndex].End;
		const FFacadeStyleConfig& Style = Styles[WrapIndex(SideIndex - StreetSideIndex, Styles.Num())];
		const FVector2D Normal = FGrammarGeometry2D::OutwardNormal(Start, End, bCleanIsCCW);
		const double Length = FGrammarGeometry2D::Distance2D(Start, End);
		if (Length == 0.0)
		{
			continue;
		}

		const TArray<double> WindowPositions = GrammarWallWindow::WindowOffsets(Length, Style.Window.Width, Style.Window.Spacing, Style.Window.MinMargin);
		const double DoorOffset = Length / 2.0;

		if (Style.WallRowColors.Num() > 0)
		{
			for (int32 FloorIndex = 0; FloorIndex < FloorBottoms.Num(); ++FloorIndex)
			{
				HeroMeshes.Add(GrammarWallWindow::WallRowMesh(SourceName, SideIndex, FloorIndex, Start, End, FloorBottoms[FloorIndex], FloorHeights[FloorIndex], Style));
			}
		}
		else
		{
			HeroMeshes.Add(GrammarWallWindow::WallMesh(SourceName, SideIndex, Start, End, TotalHeight, Style, SourceName));
		}

		const bool bDoorApplies = GrammarEngineInternal::DoorApplies(Style, SideIndex, StreetSideIndex, Tags);
		if (bDoorApplies)
		{
			Placements.Add(GrammarDoor::DoorPlacement(Start, End, Normal, DoorOffset, FloorHeights[0], Style));
			Placements.Append(GrammarDoor::DoorDetailPlacements(Start, End, Normal, DoorOffset, FloorHeights[0], Style));
		}

		for (int32 FloorIndex = 0; FloorIndex < FloorBottoms.Num(); ++FloorIndex)
		{
			const double FloorBottom = FloorBottoms[FloorIndex];
			const double FloorHeight = FloorHeights[FloorIndex];

			for (int32 WindowIndex = 0; WindowIndex < WindowPositions.Num(); ++WindowIndex)
			{
				const double Offset = WindowPositions[WindowIndex];
				if (FloorIndex == 0 && bDoorApplies && GrammarEngineInternal::WindowOverlapsDoor(Offset, DoorOffset, Style))
				{
					continue;
				}
				Placements.Add(GrammarWallWindow::WindowPlacement(Start, End, Normal, Offset, FloorBottom, FloorHeight, Style));
				Placements.Append(GrammarWallWindow::WindowDetailPlacements(Start, End, Normal, Offset, FloorBottom, FloorHeight, Style));

				if (BalconyApplies(Style, FloorIndex))
				{
					Placements.Add(GrammarLedgeBalcony::BalconyPlacement(Start, End, Normal, Offset, FloorBottom, Style));
					Placements.Append(GrammarLedgeBalcony::BalconyDetailPlacements(Start, End, Normal, Offset, FloorBottom, Style));
				}
			}

			if (LedgeApplies(Style, FloorIndex))
			{
				Placements.Add(GrammarLedgeBalcony::LedgePlacement(Start, End, Normal, FloorBottom, Style));
			}
		}

		Placements.Append(GrammarFacadeDepth::FacadeDepthPlacements(SideIndex, Start, End, Normal, StreetSideIndex, FloorHeights, TotalHeight, Style, Tags));
	}

	HeroMeshes.Add(GrammarRoof::RoofMesh(SourceName, Clean, TotalHeight, Roof, Tags));
	Placements.Append(GrammarRoofDetails::RoofEdgePlacements(Clean, TotalHeight, Roof));
	Placements.Append(GrammarRoofDetails::GutterPlacements(Clean, TotalHeight, Roof));
	Placements.Append(GrammarRoofDetails::RoofDetailPlacements(Clean, TotalHeight, Roof, Tags));

	const FFacadeStyleConfig& DetailStyle = Styles[FGrammarStableHash::StableIndex(SourceName, Styles.Num())];
	Placements.Append(GrammarRoofDetails::RoofServicePlacements(Clean, TotalHeight, Roof, DetailStyle, Tags));
	Placements.Append(GrammarRoofDetails::AntennaPlacements(Clean, TotalHeight, Roof, DetailStyle));

	OutSpec = FGrammarBuildingSpec();
	OutSpec.SourceName = SourceName;
	OutSpec.Levels = Levels;
	OutSpec.Height = TotalHeight;
	OutSpec.FloorHeights = FloorHeights;
	OutSpec.HeroMeshes = MoveTemp(HeroMeshes);
	OutSpec.Placements = MoveTemp(Placements);
	return true;
}
