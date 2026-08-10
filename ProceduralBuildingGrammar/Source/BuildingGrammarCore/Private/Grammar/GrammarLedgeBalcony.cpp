#include "Grammar/GrammarLedgeBalcony.h"
#include "Grammar/GrammarPlacementHelpers.h"
#include "Geometry/GrammarGeometry2D.h"

namespace GrammarLedgeBalcony
{
	FGrammarPlacementRecord LedgePlacement(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double FloorBottom, const FFacadeStyleConfig& Style)
	{
		constexpr double NominalThickness = 0.03;
		const FLedgeStyleConfig& Ledge = Style.Ledge;
		const FVector2D Tangent = FGrammarGeometry2D::Tangent(Start, End);
		const double Length = FGrammarGeometry2D::Distance2D(Start, End);
		const double Z = FMath::Max(0.05, FloorBottom + Ledge.Height);

		FGrammarBoxPlacementParams Params;
		Params.Center = FGrammarGeometry2D::PointOnSegment(Start, Tangent, Normal, Length / 2.0, Ledge.Depth / 2.0);
		Params.Tangent = Tangent;
		Params.Normal = Normal;
		Params.Width = Length;
		Params.Depth = Ledge.Depth;
		Params.Height = NominalThickness;
		Params.Bottom = Z - NominalThickness / 2.0;
		return FGrammarPlacementHelpers::MakeBoxPlacement(TEXT("ledge"), Ledge.Material, Params, Ledge.Color);
	}

	FGrammarPlacementRecord BalconyPlacement(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Offset, double FloorBottom, const FFacadeStyleConfig& Style)
	{
		const FVector2D Tangent = FGrammarGeometry2D::Tangent(Start, End);
		const FBalconyStyleConfig& Balcony = Style.Balcony;
		const double Bottom = FloorBottom + 0.08;

		FGrammarBoxPlacementParams Params;
		Params.Center = FGrammarGeometry2D::PointOnSegment(Start, Tangent, Normal, Offset, Balcony.Depth / 2.0);
		Params.Tangent = Tangent;
		Params.Normal = Normal;
		Params.Width = Balcony.Width;
		Params.Depth = Balcony.Depth;
		Params.Height = Balcony.SlabHeight;
		Params.Bottom = Bottom;
		return FGrammarPlacementHelpers::MakeBoxPlacement(TEXT("balcony"), Balcony.Material, Params, Balcony.Color);
	}

	TArray<FGrammarPlacementRecord> BalconyDetailPlacements(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Offset, double FloorBottom, const FFacadeStyleConfig& Style)
	{
		const FVector2D Tangent = FGrammarGeometry2D::Tangent(Start, End);
		const FBalconyStyleConfig& Balcony = Style.Balcony;
		const double Bottom = FloorBottom + 0.08;
		const double RailBottom = Bottom + Balcony.SlabHeight;
		const double RailBarWidth = FMath::Max(Balcony.RailingBarWidth, 0.02);
		const double RailBarDepth = FMath::Max(Balcony.RailingBarDepth, 0.02);

		TArray<FGrammarPlacementRecord> Result;

		// Front rail.
		{
			FGrammarBoxPlacementParams Params;
			Params.Center = FGrammarGeometry2D::PointOnSegment(Start, Tangent, Normal, Offset, Balcony.Depth);
			Params.Tangent = Tangent;
			Params.Normal = Normal;
			Params.Width = Balcony.Width;
			Params.Depth = RailBarDepth;
			Params.Height = Balcony.RailingHeight;
			Params.Bottom = RailBottom;
			Result.Add(FGrammarPlacementHelpers::MakeBoxPlacement(TEXT("balcony_rail"), Balcony.RailingMaterial, Params, Balcony.RailingColor));
		}

		// Side rails -- Tangent/Normal swapped so the piece's own width axis points outward.
		const double SideDepth = FMath::Max(Balcony.Depth - RailBarDepth, 0.05);
		for (const double Lateral : { -Balcony.Width / 2.0, Balcony.Width / 2.0 })
		{
			FGrammarBoxPlacementParams Params;
			Params.Center = FGrammarGeometry2D::PointOnSegment(Start, Tangent, Normal, Offset + Lateral, Balcony.Depth / 2.0);
			Params.Tangent = Normal;
			Params.Normal = Tangent;
			Params.Width = SideDepth;
			Params.Depth = RailBarWidth;
			Params.Height = Balcony.RailingHeight;
			Params.Bottom = RailBottom;
			Result.Add(FGrammarPlacementHelpers::MakeBoxPlacement(TEXT("balcony_rail"), Balcony.RailingMaterial, Params, Balcony.RailingColor));
		}

		// Vertical bars.
		const int32 BarCount = FMath::Max(Balcony.RailingBarCount, 0);
		for (int32 BarIndex = 0; BarIndex < BarCount; ++BarIndex)
		{
			const double Lateral = -Balcony.Width / 2.0 + Balcony.Width * (BarIndex + 1) / static_cast<double>(BarCount + 1);
			FGrammarBoxPlacementParams Params;
			Params.Center = FGrammarGeometry2D::PointOnSegment(Start, Tangent, Normal, Offset + Lateral, Balcony.Depth);
			Params.Tangent = Tangent;
			Params.Normal = Normal;
			Params.Width = RailBarWidth;
			Params.Depth = RailBarDepth;
			Params.Height = Balcony.RailingHeight;
			Params.Bottom = RailBottom;
			Result.Add(FGrammarPlacementHelpers::MakeBoxPlacement(TEXT("balcony_bar"), Balcony.RailingMaterial, Params, Balcony.RailingColor));
		}

		return Result;
	}
}
