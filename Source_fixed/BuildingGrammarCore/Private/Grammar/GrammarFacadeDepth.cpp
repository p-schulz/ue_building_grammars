#include "Grammar/GrammarFacadeDepth.h"
#include "Grammar/GrammarEngineInternal.h"
#include "Grammar/GrammarPlacementHelpers.h"
#include "Geometry/GrammarGeometry2D.h"

namespace
{
	FGrammarPlacementRecord PanelPlacement(const FString& Role, const FString& VariantKey, const FVector2D& Start, const FVector2D& Tangent, const FVector2D& Normal, double CenterOffset, double Width, double Bottom, double Height, double Depth, const FLinearColor& Color)
	{
		FGrammarBoxPlacementParams Params;
		Params.Center = FGrammarGeometry2D::PointOnSegment(Start, Tangent, Normal, CenterOffset, Depth);
		Params.Tangent = Tangent;
		Params.Normal = Normal;
		Params.Width = Width;
		Params.Depth = Depth;
		Params.Height = Height;
		Params.Bottom = Bottom;
		return FGrammarPlacementHelpers::MakeBoxPlacement(Role, VariantKey, Params, Color);
	}

	TArray<FGrammarPlacementRecord> FacadePatternPlacements(const FVector2D& Start, const FVector2D& Tangent, const FVector2D& Normal, const TArray<double>& FloorHeights, double TotalHeight, const FFacadeStyleConfig& Style, const TMap<FString, FString>& Tags, double Length)
	{
		TArray<FGrammarPlacementRecord> Result;
		const TSet<FString> Tokens = GrammarEngineInternal::StyleTokens(Style, Tags);

		if (GrammarEngineInternal::HasAny(Tokens, { TEXT("plattenbau"), TEXT("prefab"), TEXT("industrial"), TEXT("warehouse"), TEXT("parking"), TEXT("office"), TEXT("curtain") }))
		{
			const int32 VerticalCount = FMath::Min(FMath::Max(static_cast<int32>(Length / 3.2), 1), 8);
			for (int32 Index = 1; Index <= VerticalCount; ++Index)
			{
				const double Offset = Length * Index / static_cast<double>(VerticalCount + 1);
				Result.Add(PanelPlacement(TEXT("panel_seam"), TEXT("Grammar Facade Panel Seams"), Start, Tangent, Normal, Offset, 0.045, 0.0, TotalHeight, 0.045, FLinearColor(0.2, 0.205, 0.2, 1.0)));
			}
			const TArray<double> FloorBottoms = FGrammarGeometry2D::FloorBottoms(FloorHeights);
			for (int32 FloorIndex = 1; FloorIndex < FloorBottoms.Num(); ++FloorIndex)
			{
				const double Bottom = FloorBottoms[FloorIndex];
				Result.Add(PanelPlacement(TEXT("panel_seam"), TEXT("Grammar Facade Panel Seams"), Start, Tangent, Normal, Length / 2.0, Length, Bottom - 0.025, 0.05, 0.045, FLinearColor(0.2, 0.205, 0.2, 1.0)));
			}
		}

		if (GrammarEngineInternal::HasAny(Tokens, { TEXT("passivhaus"), TEXT("contemporary"), TEXT("modern"), TEXT("bauhaus") }))
		{
			const TArray<double> FloorBottoms = FGrammarGeometry2D::FloorBottoms(FloorHeights);
			for (int32 FloorIndex = 1; FloorIndex < FloorBottoms.Num(); ++FloorIndex)
			{
				const double Bottom = FloorBottoms[FloorIndex];
				Result.Add(PanelPlacement(TEXT("insulation_band"), TEXT("Grammar Insulation Shadow Bands"), Start, Tangent, Normal, Length / 2.0, Length * 0.94, Bottom - 0.06, 0.12, 0.065, FLinearColor(0.78, 0.78, 0.72, 1.0)));
			}
		}

		if (GrammarEngineInternal::HasAny(Tokens, { TEXT("gruenderzeit"), TEXT("jugendstil"), TEXT("fachwerk"), TEXT("historic"), TEXT("altbau"), TEXT("kontorhaus"), TEXT("church"), TEXT("cathedral"), TEXT("sacral") }))
		{
			const TArray<double> FloorBottoms = FGrammarGeometry2D::FloorBottoms(FloorHeights);
			for (int32 FloorIndex = 1; FloorIndex < FloorBottoms.Num(); ++FloorIndex)
			{
				const double Bottom = FloorBottoms[FloorIndex];
				Result.Add(PanelPlacement(TEXT("facade_ornament"), TEXT("Grammar Facade Ornament Bands"), Start, Tangent, Normal, Length / 2.0, Length * 0.92, Bottom - 0.08, 0.16, 0.09, FLinearColor(0.78, 0.72, 0.62, 1.0)));
			}

			int32 PilasterCount = FMath::Min(FMath::Max(static_cast<int32>(Length / 4.0), 1), 5);
			if (GrammarEngineInternal::HasAny(Tokens, { TEXT("church"), TEXT("cathedral"), TEXT("sacral") }))
			{
				PilasterCount = FMath::Min(FMath::Max(static_cast<int32>(Length / 3.2), 2), 8);
			}
			for (int32 Index = 1; Index <= PilasterCount; ++Index)
			{
				const double Offset = Length * Index / static_cast<double>(PilasterCount + 1);
				Result.Add(PanelPlacement(TEXT("facade_ornament"), TEXT("Grammar Facade Ornament Pilasters"), Start, Tangent, Normal, Offset, 0.16, 0.0, TotalHeight, 0.08, FLinearColor(0.72, 0.66, 0.56, 1.0)));
			}
		}

		return Result;
	}
}

namespace GrammarFacadeDepth
{
	TArray<FGrammarPlacementRecord> FacadeDepthPlacements(int32 SideIndex, const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, int32 StreetSideIndex, const TArray<double>& FloorHeights, double TotalHeight, const FFacadeStyleConfig& Style, const TMap<FString, FString>& Tags)
	{
		const double Length = FGrammarGeometry2D::Distance2D(Start, End);
		TArray<FGrammarPlacementRecord> Result;
		if (Length <= 0.1)
		{
			return Result;
		}
		const FVector2D Tangent = FGrammarGeometry2D::Tangent(Start, End);
		const double GroundHeight = FloorHeights.Num() > 0 ? FloorHeights[0] : TotalHeight;
		const bool bStreetFacing = SideIndex == StreetSideIndex;

		const bool bIsRetail = GrammarEngineInternal::IsRetailStyle(Style, Tags);
		const bool bIsIndustrial = GrammarEngineInternal::IsIndustrialStyle(Style, Tags);
		const bool bIsParking = GrammarEngineInternal::IsParkingStyle(Style, Tags);

		if (bStreetFacing && bIsRetail)
		{
			const double SignHeight = FMath::Min(0.62, FMath::Max(0.32, GroundHeight * 0.16));
			const double SignBottom = FMath::Min(FMath::Max(GroundHeight - SignHeight - 0.35, 2.4), FMath::Max(0.2, GroundHeight - SignHeight));
			Result.Add(PanelPlacement(TEXT("signboard"), TEXT("Grammar Retail Signboards"), Start, Tangent, Normal, Length / 2.0, Length * 0.82, SignBottom, SignHeight, 0.08, FLinearColor(0.88, 0.72, 0.24, 1.0)));

			const double AwningWidth = Length * 0.7;
			const double AwningDepth = 0.85;
			FGrammarBoxPlacementParams AwningParams;
			AwningParams.Center = FGrammarGeometry2D::PointOnSegment(Start, Tangent, Normal, Length / 2.0, AwningDepth / 2.0);
			AwningParams.Tangent = Tangent;
			AwningParams.Normal = Normal;
			AwningParams.Width = AwningWidth;
			AwningParams.Depth = AwningDepth;
			AwningParams.Height = 0.12;
			AwningParams.Bottom = FMath::Max(2.1, SignBottom - 0.22);
			Result.Add(FGrammarPlacementHelpers::MakeBoxPlacement(TEXT("awning"), TEXT("Grammar Fabric Awnings"), AwningParams, FLinearColor(0.56, 0.08, 0.07, 1.0)));
		}

		if (bStreetFacing && (bIsIndustrial || bIsParking))
		{
			const double DoorWidth = FMath::Min(Length * 0.55, bIsIndustrial ? 4.8 : 5.6);
			const double DoorHeight = FMath::Min(FMath::Max(GroundHeight - 0.25, 2.2), 4.2);
			Result.Add(PanelPlacement(TEXT("garage_door"), TEXT("Grammar Sectional Garage Doors"), Start, Tangent, Normal, Length / 2.0, DoorWidth, 0.0, DoorHeight, 0.09, FLinearColor(0.24, 0.25, 0.24, 1.0)));

			if (bIsIndustrial)
			{
				FGrammarBoxPlacementParams DockParams;
				DockParams.Center = FGrammarGeometry2D::PointOnSegment(Start, Tangent, Normal, Length / 2.0, 0.38);
				DockParams.Tangent = Tangent;
				DockParams.Normal = Normal;
				DockParams.Width = DoorWidth + 1.0;
				DockParams.Depth = 0.76;
				DockParams.Height = 0.45;
				DockParams.Bottom = 0.0;
				Result.Add(FGrammarPlacementHelpers::MakeBoxPlacement(TEXT("loading_dock"), TEXT("Grammar Concrete Loading Docks"), DockParams, FLinearColor(0.42, 0.42, 0.39, 1.0)));
			}
		}

		if (GrammarEngineInternal::ShouldAddStairCore(Style, Tags, bStreetFacing, Length, TotalHeight))
		{
			const double CoreWidth = FMath::Min(FMath::Max(Length * 0.18, 1.1), 2.4);
			const double CoreHeight = FMath::Max(TotalHeight - 0.4, 1.0);
			Result.Add(PanelPlacement(TEXT("stair_core"), TEXT("Grammar Stair Core Glass"), Start, Tangent, Normal, Length * 0.5, CoreWidth, 0.2, CoreHeight, 0.06, FLinearColor(0.09, 0.18, 0.22, 0.82)));
		}

		Result.Append(FacadePatternPlacements(Start, Tangent, Normal, FloorHeights, TotalHeight, Style, Tags, Length));
		return Result;
	}
}
