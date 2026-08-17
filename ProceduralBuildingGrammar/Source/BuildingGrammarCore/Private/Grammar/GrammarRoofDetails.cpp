#include "Grammar/GrammarRoofDetails.h"
#include "Grammar/GrammarRoofDirection.h"
#include "Grammar/GrammarEngineInternal.h"
#include "Grammar/GrammarPlacementHelpers.h"
#include "Grammar/GrammarTagParsing.h"
#include "Geometry/GrammarGeometry2D.h"
#include "Geometry/GrammarRoofFrame.h"

namespace
{
	TArray<FVector2D> ToXY(const TArray<FVector>& Points)
	{
		TArray<FVector2D> Result;
		Result.Reserve(Points.Num());
		for (const FVector& Point : Points)
		{
			Result.Add(FVector2D(Point.X, Point.Y));
		}
		return Result;
	}

	FGrammarPlacementRecord BoxPlacement(const FString& Role, const FString& VariantKey, const FVector2D& Center, const FVector2D& Tangent, const FVector2D& Normal, double Width, double Depth, double Height, double Bottom, const FLinearColor& Color)
	{
		FGrammarBoxPlacementParams Params;
		Params.Center = Center;
		Params.Tangent = Tangent;
		Params.Normal = Normal;
		Params.Width = Width;
		Params.Depth = Depth;
		Params.Height = Height;
		Params.Bottom = Bottom;
		return FGrammarPlacementHelpers::MakeBoxPlacement(Role, VariantKey, Params, Color);
	}

	FString RoofTypeString(EGrammarRoofType Type)
	{
		switch (Type)
		{
		case EGrammarRoofType::Gabled: return TEXT("gabled");
		case EGrammarRoofType::Hipped: return TEXT("hipped");
		case EGrammarRoofType::Pyramid: return TEXT("pyramid");
		case EGrammarRoofType::Flat:
		default: return TEXT("flat");
		}
	}

	TArray<FGrammarPlacementRecord> RoofTilePlacements(const FGrammarRoofFrame& Frame, const FRoofStyleConfig& Roof, EGrammarRoofType RoofType)
	{
		TArray<FGrammarPlacementRecord> Result;
		const int32 Rows = FMath::Max(Roof.TileRows, 0);
		if (Rows <= 0 || RoofType == EGrammarRoofType::Flat)
		{
			return Result;
		}
		const double LongSpan = FMath::Max(Frame.MaxLong - Frame.MinLong, 0.0);
		const double SideSpan = FMath::Max(Frame.MaxSide - Frame.MinSide, 0.0);
		if (LongSpan <= 0.0 || SideSpan <= 0.0)
		{
			return Result;
		}
		const double Width = LongSpan * 0.92;
		const double RowDepth = FMath::Max(Roof.TileSpacing * 0.12, Roof.TileDepth);

		for (const double SideSign : { -1.0, 1.0 })
		{
			const double SideLimit = SideSign > 0.0 ? Frame.MaxSide : Frame.MinSide;
			for (int32 RowIndex = 0; RowIndex < Rows; ++RowIndex)
			{
				const double Factor = (RowIndex + 1) / static_cast<double>(Rows + 1);
				const double SideValue = SideLimit * (1.0 - Factor * 0.88);
				const double Z = FGrammarRoofFrameMath::RoofSurfaceZ(0.0, SideValue, Frame) + FMath::Max(Roof.TileDepth, 0.0);
				const FVector Center = FGrammarRoofFrameMath::PointFromRoofAxes(Frame, 0.0, SideValue, Z);
				Result.Add(BoxPlacement(TEXT("roof_tile"), Roof.TileMaterial, FVector2D(Center.X, Center.Y), Frame.Direction, Frame.Normal, Width, RowDepth, FMath::Max(Roof.TileDepth, 0.01), Z, Roof.TileColor));
			}
		}
		return Result;
	}

	TArray<FGrammarPlacementRecord> RoofWindowPlacements(const FGrammarRoofFrame& Frame, const FRoofStyleConfig& Roof, EGrammarRoofType RoofType)
	{
		TArray<FGrammarPlacementRecord> Result;
		const int32 Count = FMath::Max(Roof.RoofWindowCount, 0);
		if (Count <= 0 || RoofType == EGrammarRoofType::Flat)
		{
			return Result;
		}
		const TArray<double> LongPositions = FGrammarRoofFrameMath::DetailPositions(Count, Frame.MinLong, Frame.MaxLong, 0.22);
		for (int32 Index = 0; Index < LongPositions.Num(); ++Index)
		{
			const double LongValue = LongPositions[Index];
			const double SideSign = (Index % 2 == 0) ? -1.0 : 1.0;
			const double SideLimit = SideSign < 0.0 ? Frame.MinSide : Frame.MaxSide;
			const double SideValue = SideLimit * 0.48;
			const double Z = FGrammarRoofFrameMath::RoofSurfaceZ(LongValue, SideValue, Frame) + 0.045;
			const FVector Center = FGrammarRoofFrameMath::PointFromRoofAxes(Frame, LongValue, SideValue, Z);
			Result.Add(BoxPlacement(TEXT("roof_window"), Roof.RoofWindowMaterial, FVector2D(Center.X, Center.Y), Frame.Direction, Frame.Normal, Roof.RoofWindowWidth, Roof.RoofWindowHeight, 0.035, Z, Roof.RoofWindowColor));
		}
		return Result;
	}

	TArray<FGrammarPlacementRecord> DormerPlacements(const FGrammarRoofFrame& Frame, const FRoofStyleConfig& Roof, EGrammarRoofType RoofType)
	{
		TArray<FGrammarPlacementRecord> Result;
		const int32 Count = FMath::Max(Roof.DormerCount, 0);
		if (Count <= 0 || (RoofType != EGrammarRoofType::Gabled && RoofType != EGrammarRoofType::Hipped))
		{
			return Result;
		}
		const TArray<double> LongPositions = FGrammarRoofFrameMath::DetailPositions(Count, Frame.MinLong, Frame.MaxLong, 0.25);
		for (int32 Index = 0; Index < LongPositions.Num(); ++Index)
		{
			const double LongValue = LongPositions[Index];
			const double SideSign = (Index % 2 == 0) ? -1.0 : 1.0;
			const double SideLimit = SideSign < 0.0 ? Frame.MinSide : Frame.MaxSide;
			const double SideValue = SideLimit * 0.55;
			const double DormerWidth = Roof.DormerWidth;
			const double DormerDepth = Roof.DormerDepth;
			const double DormerHeight = Roof.DormerHeight;
			const double Z = FGrammarRoofFrameMath::RoofSurfaceZ(LongValue, SideValue, Frame);
			const FVector Center = FGrammarRoofFrameMath::PointFromRoofAxes(Frame, LongValue, SideValue, Z + DormerHeight / 2.0);
			const FVector2D Outward = Frame.Normal * SideSign;

			Result.Add(BoxPlacement(TEXT("dormer"), Roof.DormerMaterial, FVector2D(Center.X, Center.Y), Frame.Direction, Outward, DormerWidth, DormerDepth, DormerHeight, Z, Roof.DormerColor));

			const FVector WindowCenter = FGrammarRoofFrameMath::PointFromRoofAxes(Frame, LongValue, SideValue + SideSign * DormerDepth * 0.52, Z + DormerHeight * 0.48);
			Result.Add(BoxPlacement(TEXT("roof_window"), Roof.RoofWindowMaterial, FVector2D(WindowCenter.X, WindowCenter.Y), Frame.Direction, Outward, DormerWidth * 0.45, 0.035, DormerHeight * 0.38, Z + DormerHeight * 0.3, Roof.RoofWindowColor));
		}
		return Result;
	}

	// True only for a feature that IS a standalone chimney/smokestack (man_made=chimney or
	// chimney_pipe) -- i.e. one where its own OSM "height" tag unambiguously means "how tall this
	// chimney is". Deliberately NOT true for an ordinary building that merely has decorative roof
	// chimneys (Roof.ChimneyCount>0 from its style): that building's own "height" tag means the
	// BUILDING's height, not the chimney's -- using it for ChimneyHeight there would be wrong, not
	// an improvement (see EffectiveChimneyHeight's own comment below for what this fixes).
	bool IsStandaloneChimneyFeature(const TMap<FString, FString>& Tags)
	{
		const FString* ManMade = Tags.Find(TEXT("man_made"));
		return ManMade && (*ManMade == TEXT("chimney") || *ManMade == TEXT("chimney_pipe"));
	}

	// A style's own ChimneyHeight is necessarily a single fixed guess (e.g. an "industrial
	// smokestack" style might set 45m), but OSM's man_made=chimney/chimney_pipe tag alone carries no
	// size information -- a real, modestly-sized residential chimney mapped as its own feature
	// matches the exact same style as a genuine 45m industrial smokestack. When the matched
	// feature's own explicit "height" tag is present, prefer it over the style's fixed guess -- same
	// "explicit OSM tag overrides a style/config default" precedent as FGrammarLevels::InferLevels/
	// FloorHeightSequence's own height-tag handling for building height.
	double EffectiveChimneyHeight(const FRoofStyleConfig& Roof, const TMap<FString, FString>& Tags)
	{
		if (Roof.ChimneyCount > 0 && IsStandaloneChimneyFeature(Tags))
		{
			if (const TOptional<double> ExplicitHeight = FGrammarTagParsing::ParseMeters(Tags.Find(TEXT("height"))))
			{
				return ExplicitHeight.GetValue();
			}
		}
		return Roof.ChimneyHeight;
	}

	TArray<FGrammarPlacementRecord> ChimneyPlacements(const FGrammarRoofFrame& Frame, const FRoofStyleConfig& Roof)
	{
		TArray<FGrammarPlacementRecord> Result;
		const int32 Count = FMath::Max(Roof.ChimneyCount, 0);
		if (Count <= 0)
		{
			return Result;
		}
		const TArray<double> LongPositions = FGrammarRoofFrameMath::DetailPositions(Count, Frame.MinLong, Frame.MaxLong, 0.28);
		for (int32 Index = 0; Index < LongPositions.Num(); ++Index)
		{
			const double LongValue = LongPositions[Index];
			const double SideValue = (Index % 2 != 0 ? Frame.MaxSide : Frame.MinSide) * 0.18;
			const double Z = FGrammarRoofFrameMath::RoofSurfaceZ(LongValue, SideValue, Frame);
			const FVector Center = FGrammarRoofFrameMath::PointFromRoofAxes(Frame, LongValue, SideValue, Z + Roof.ChimneyHeight / 2.0);
			Result.Add(BoxPlacement(TEXT("chimney"), Roof.ChimneyMaterial, FVector2D(Center.X, Center.Y), Frame.Direction, Frame.Normal, Roof.ChimneyWidth, Roof.ChimneyDepth, Roof.ChimneyHeight, Z, Roof.ChimneyColor));
		}
		return Result;
	}

	void AntennaInstancePlacements(TArray<FGrammarPlacementRecord>& OutResult, const FVector2D& Position, double RoofZ, const FAntennaStyleConfig& Antenna)
	{
		const FVector2D Tangent(1.0, 0.0);
		const FVector2D Normal(0.0, 1.0);
		const double MastWidth = FMath::Max(Antenna.MastRadius * 2.0, 0.025);

		OutResult.Add(BoxPlacement(TEXT("antenna"), Antenna.Material, Position, Tangent, Normal, Antenna.BaseWidth, Antenna.BaseDepth, Antenna.BaseHeight, RoofZ, Antenna.Color));
		OutResult.Add(BoxPlacement(TEXT("antenna"), Antenna.Material, Position, Tangent, Normal, MastWidth, MastWidth, Antenna.MastHeight, RoofZ + Antenna.BaseHeight, Antenna.Color));

		const double TopZ = RoofZ + Antenna.BaseHeight + Antenna.MastHeight;

		switch (Antenna.Type)
		{
		case EGrammarAntennaType::Cellular:
		case EGrammarAntennaType::OfficeCluster:
		{
			const int32 PanelCount = Antenna.Type == EGrammarAntennaType::OfficeCluster ? 4 : 3;
			for (int32 PanelIndex = 0; PanelIndex < PanelCount; ++PanelIndex)
			{
				const int32 Direction = PanelIndex % 4;
				const FVector2D PanelTangent = (Direction % 2 == 0) ? FVector2D(1.0, 0.0) : FVector2D(0.0, 1.0);
				FVector2D PanelNormal;
				switch (Direction)
				{
				case 0: PanelNormal = FVector2D(0.0, 1.0); break;
				case 1: PanelNormal = FVector2D(1.0, 0.0); break;
				case 2: PanelNormal = FVector2D(0.0, -1.0); break;
				default: PanelNormal = FVector2D(-1.0, 0.0); break;
				}
				const FVector2D PanelCenter = Position + PanelNormal * (Antenna.PanelDepth + 0.08);
				OutResult.Add(BoxPlacement(TEXT("antenna_panel"), Antenna.AccentMaterial, PanelCenter, PanelTangent, PanelNormal, Antenna.PanelWidth, Antenna.PanelDepth, Antenna.PanelHeight, TopZ - Antenna.PanelHeight * 0.85, Antenna.AccentColor));
			}
			break;
		}
		case EGrammarAntennaType::Satellite:
		{
			const FVector2D DishCenter = Position + FVector2D(Antenna.PanelDepth + 0.08, 0.0);
			OutResult.Add(BoxPlacement(TEXT("antenna_panel"), Antenna.AccentMaterial, DishCenter, Normal, Tangent, Antenna.PanelWidth, Antenna.PanelDepth, Antenna.PanelHeight, TopZ - Antenna.PanelHeight * 0.65, Antenna.AccentColor));
			const FVector2D ArmCenter = Position + FVector2D(Antenna.PanelDepth * 0.75, 0.0);
			OutResult.Add(BoxPlacement(TEXT("antenna_panel"), Antenna.Material, ArmCenter, Tangent, Normal, Antenna.PanelDepth * 2.0, MastWidth, MastWidth, TopZ - Antenna.PanelHeight * 0.2, Antenna.Color));
			break;
		}
		case EGrammarAntennaType::Broadcast:
		{
			const double ZFactors[3] = { 0.35, 0.62, 0.88 };
			for (int32 BarIndex = 0; BarIndex < 3; ++BarIndex)
			{
				const bool bEven = (BarIndex % 2 == 0);
				OutResult.Add(BoxPlacement(TEXT("antenna_panel"), Antenna.AccentMaterial, Position, bEven ? Tangent : Normal, bEven ? Normal : Tangent, Antenna.PanelWidth * 3.0, MastWidth, MastWidth, RoofZ + Antenna.BaseHeight + Antenna.MastHeight * ZFactors[BarIndex], Antenna.AccentColor));
			}
			break;
		}
		case EGrammarAntennaType::LightningRod:
		{
			OutResult.Add(BoxPlacement(TEXT("antenna_panel"), Antenna.AccentMaterial, Position, Tangent, Normal, MastWidth * 0.6, MastWidth * 0.6, FMath::Max(Antenna.MastHeight * 0.28, 0.25), TopZ, Antenna.AccentColor));
			break;
		}
		case EGrammarAntennaType::LampPost:
		{
			const FVector2D LampCenter = Position + FVector2D(Antenna.PanelDepth * 0.6, 0.0);
			const double LampHeight = FMath::Max(Antenna.PanelHeight, MastWidth * 2.0);
			OutResult.Add(BoxPlacement(TEXT("roof_lamp"), Antenna.AccentMaterial, LampCenter, Tangent, Normal, FMath::Max(Antenna.PanelWidth, MastWidth * 3.0), FMath::Max(Antenna.PanelDepth, MastWidth * 1.8), LampHeight, TopZ - LampHeight * 0.5, Antenna.AccentColor));
			break;
		}
		default:
		{
			const double BarWidth = Antenna.PanelWidth * (Antenna.Type == EGrammarAntennaType::Radio ? 2.2 : 1.6);
			const double ZFactors[2] = { 0.55, 0.78 };
			for (int32 BarIndex = 0; BarIndex < 2; ++BarIndex)
			{
				const bool bEven = (BarIndex % 2 == 0);
				OutResult.Add(BoxPlacement(TEXT("antenna_panel"), Antenna.AccentMaterial, Position, bEven ? Tangent : Normal, bEven ? Normal : Tangent, BarWidth, MastWidth, MastWidth, RoofZ + Antenna.BaseHeight + Antenna.MastHeight * ZFactors[BarIndex], Antenna.AccentColor));
			}
			break;
		}
		}
	}

	TArray<FVector2D> AntennaPositions(const FVector2D& Center, const FBox2D& Bounds, int32 Count)
	{
		if (Count <= 1)
		{
			return { Center };
		}
		const double InsetX = FMath::Max((Bounds.Max.X - Bounds.Min.X) * 0.22, 0.5);
		const double InsetY = FMath::Max((Bounds.Max.Y - Bounds.Min.Y) * 0.22, 0.5);
		TArray<FVector2D> Candidates = {
			Center,
			FVector2D(Bounds.Max.X - InsetX, Bounds.Max.Y - InsetY),
			FVector2D(Bounds.Min.X + InsetX, Bounds.Max.Y - InsetY),
			FVector2D(Bounds.Max.X - InsetX, Bounds.Min.Y + InsetY),
			FVector2D(Bounds.Min.X + InsetX, Bounds.Min.Y + InsetY),
		};
		Candidates.SetNum(FMath::Min(Count, Candidates.Num()));
		return Candidates;
	}
}

namespace GrammarRoofDetails
{
	TArray<FGrammarPlacementRecord> RoofDetailPlacements(const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof, const TMap<FString, FString>& Tags)
	{
		const TArray<FVector> Base = FGrammarRoofFrameMath::RoofBaseVertices(Footprint, Height, Roof.Overhang);

		FGrammarRoofFrame Frame;
		if (Roof.Type == EGrammarRoofType::Flat)
		{
			Frame = FGrammarRoofFrameMath::BuildFrame(Base, FGrammarGeometry2D::LongestAxisDirection(ToXY(Base)), Height, Height);
		}
		else
		{
			const FVector2D Direction = (Roof.Type == EGrammarRoofType::Gabled || Roof.Type == EGrammarRoofType::Hipped)
				? GrammarRoofDirection::RidgeDirection(Base, Roof, Tags)
				: FGrammarGeometry2D::LongestAxisDirection(ToXY(Base));
			Frame = FGrammarRoofFrameMath::BuildFrame(Base, Direction, Height, Height + Roof.Height);
		}

		TArray<FGrammarPlacementRecord> Result;
		Result.Append(RoofTilePlacements(Frame, Roof, Roof.Type));
		Result.Append(RoofWindowPlacements(Frame, Roof, Roof.Type));
		Result.Append(DormerPlacements(Frame, Roof, Roof.Type));

		// See EffectiveChimneyHeight's own comment -- only actually differs from Roof.ChimneyHeight
		// for a standalone man_made=chimney/chimney_pipe feature with its own explicit "height" tag.
		const double ChimneyHeight = EffectiveChimneyHeight(Roof, Tags);
		if (ChimneyHeight != Roof.ChimneyHeight)
		{
			FRoofStyleConfig AdjustedRoof = Roof;
			AdjustedRoof.ChimneyHeight = ChimneyHeight;
			Result.Append(ChimneyPlacements(Frame, AdjustedRoof));
		}
		else
		{
			Result.Append(ChimneyPlacements(Frame, Roof));
		}
		return Result;
	}

	TArray<FGrammarPlacementRecord> GutterPlacements(const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof)
	{
		TArray<FGrammarPlacementRecord> Result;
		if (Footprint.Num() == 0)
		{
			return Result;
		}
		const double Z = (Roof.Type != EGrammarRoofType::Flat) ? Height + 0.03 : Height + FMath::Max(Roof.EdgeHeight - Roof.SurfaceInset, 0.0) * 0.55;
		const TArray<FVector2D> Base = ToXY(FGrammarRoofFrameMath::RoofBaseVertices(Footprint, Z, Roof.Overhang));
		const bool bCCW = FGrammarGeometry2D::PolygonIsCCW(Base);

		const TArray<FGrammarGeometry2D::FEdge> Segments = FGrammarGeometry2D::GetSegments(Base);
		for (const FGrammarGeometry2D::FEdge& Edge : Segments)
		{
			const double Length = FGrammarGeometry2D::Distance2D(Edge.Start, Edge.End);
			if (Length <= 0.2)
			{
				continue;
			}
			const FVector2D Tangent = FGrammarGeometry2D::Tangent(Edge.Start, Edge.End);
			const FVector2D Normal = FGrammarGeometry2D::OutwardNormal(Edge.Start, Edge.End, bCCW);
			const FVector2D Center = (Edge.Start + Edge.End) / 2.0 + Normal * 0.08;
			Result.Add(BoxPlacement(TEXT("gutter"), TEXT("Grammar Roof Gutters"), Center, Tangent, Normal, Length, 0.12, 0.08, Z - 0.04, FLinearColor(0.18, 0.18, 0.17, 1.0)));
		}
		return Result;
	}

	TArray<FGrammarPlacementRecord> RoofEdgePlacements(const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof)
	{
		TArray<FGrammarPlacementRecord> Result;
		if (Roof.Type != EGrammarRoofType::Flat || !Roof.bEdgeEnabled || Roof.EdgeWidth <= 0.0 || Roof.EdgeHeight <= 0.0)
		{
			return Result;
		}

		const TArray<FVector2D> Base = ToXY(FGrammarRoofFrameMath::RoofBaseVertices(Footprint, Height, Roof.Overhang));
		const bool bCCW = FGrammarGeometry2D::PolygonIsCCW(Footprint);
		const double Bottom = Height - FMath::Max(Roof.SurfaceInset, 0.0);

		const TArray<FGrammarGeometry2D::FEdge> Segments = FGrammarGeometry2D::GetSegments(Base);
		for (const FGrammarGeometry2D::FEdge& Edge : Segments)
		{
			const double Length = FGrammarGeometry2D::Distance2D(Edge.Start, Edge.End);
			if (Length == 0.0)
			{
				continue;
			}
			const FVector2D Normal = FGrammarGeometry2D::OutwardNormal(Edge.Start, Edge.End, bCCW);
			const FVector2D Tangent = FGrammarGeometry2D::Tangent(Edge.Start, Edge.End);
			const FVector2D Center = (Edge.Start + Edge.End) / 2.0 + Normal * (Roof.EdgeWidth / 2.0);
			Result.Add(BoxPlacement(TEXT("roof_edge"), Roof.EdgeMaterial, Center, Tangent, Normal, Length, Roof.EdgeWidth, Roof.EdgeHeight, Bottom, Roof.EdgeColor));
		}

		const double CapSize = FMath::Max(Roof.CornerCapSize, Roof.EdgeWidth);
		const int32 Count = Base.Num();
		for (int32 CornerIndex = 0; CornerIndex < Count; ++CornerIndex)
		{
			const FVector2D& Point = Base[CornerIndex];
			const FVector2D& PrevPoint = Base[(CornerIndex - 1 + Count) % Count];
			const FVector2D& NextPoint = Base[(CornerIndex + 1) % Count];
			const FVector2D PrevNormal = FGrammarGeometry2D::OutwardNormal(PrevPoint, Point, bCCW);
			const FVector2D NextNormal = FGrammarGeometry2D::OutwardNormal(Point, NextPoint, bCCW);
			FVector2D Normal = FGrammarGeometry2D::Normalize2D(PrevNormal + NextNormal);
			if (Normal.IsNearlyZero())
			{
				Normal = NextNormal;
			}
			const FVector2D Tangent = FGrammarGeometry2D::Normalize2D(NextPoint - PrevPoint);
			const FVector2D Center = Point + Normal * (Roof.EdgeWidth / 2.0);
			Result.Add(BoxPlacement(TEXT("roof_edge"), Roof.EdgeMaterial, Center, Tangent, Normal, CapSize, CapSize, Roof.EdgeHeight, Bottom, Roof.EdgeColor));
		}

		return Result;
	}

	TArray<FGrammarPlacementRecord> RoofServicePlacements(const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof, const FFacadeStyleConfig& Style, const TMap<FString, FString>& Tags)
	{
		TArray<FGrammarPlacementRecord> Result;
		if (Roof.Type != EGrammarRoofType::Flat)
		{
			return Result;
		}
		const TSet<FString> Tokens = GrammarEngineInternal::StyleTokens(Style, Tags);
		if (!GrammarEngineInternal::HasAny(Tokens, { TEXT("office"), TEXT("industrial"), TEXT("warehouse"), TEXT("retail"), TEXT("supermarket"), TEXT("modern"), TEXT("passivhaus"), TEXT("parking") }))
		{
			return Result;
		}

		const TArray<FVector2D> Base = ToXY(FGrammarRoofFrameMath::RoofBaseVertices(Footprint, Height, FMath::Max(Roof.Overhang, 0.0)));
		const FBox2D Bounds = FGrammarGeometry2D::Bounds(Base);
		const double Width = Bounds.Max.X - Bounds.Min.X;
		const double Depth = Bounds.Max.Y - Bounds.Min.Y;
		if (Width <= 1.0 || Depth <= 1.0)
		{
			return Result;
		}

		const double RoofZ = !Roof.bEdgeEnabled ? Height : Height + FMath::Max(Roof.EdgeHeight - Roof.SurfaceInset, 0.0);
		const FVector2D Axis = (Width >= Depth) ? FVector2D(1.0, 0.0) : FVector2D(0.0, 1.0);
		const FVector2D Normal = (Axis == FVector2D(1.0, 0.0)) ? FVector2D(0.0, 1.0) : FVector2D(1.0, 0.0);
		const FVector2D Center = (Bounds.Min + Bounds.Max) / 2.0;

		if (GrammarEngineInternal::HasAny(Tokens, { TEXT("office"), TEXT("industrial"), TEXT("warehouse"), TEXT("retail"), TEXT("supermarket"), TEXT("modern"), TEXT("passivhaus") }))
		{
			const int32 PanelCount = (FMath::Max(Width, Depth) > 12.0) ? 4 : 2;
			for (int32 Index = 0; Index < PanelCount; ++Index)
			{
				const double Lateral = (Index - (PanelCount - 1) / 2.0) * 1.45;
				const FVector2D PanelCenter = Center + Normal * Lateral;
				Result.Add(BoxPlacement(TEXT("pv_panel"), TEXT("Grammar Roof PV Panels"), PanelCenter, Axis, Normal, FMath::Min(FMath::Max(Width, Depth) * 0.32, 3.8), 0.82, 0.08, RoofZ + 0.05, FLinearColor(0.04, 0.07, 0.09, 1.0)));
			}
		}

		if (GrammarEngineInternal::HasAny(Tokens, { TEXT("office"), TEXT("industrial"), TEXT("warehouse"), TEXT("retail"), TEXT("supermarket") }))
		{
			const int32 HvacCount = (FMath::Max(Width, Depth) > 16.0) ? 3 : 1;
			for (int32 Index = 0; Index < HvacCount; ++Index)
			{
				const double Shift = (Index - (HvacCount - 1) / 2.0) * 1.7;
				const FVector2D UnitCenter = Center + Axis * Shift - FVector2D(Normal.X * Depth * 0.16, Normal.Y * Width * 0.16);
				Result.Add(BoxPlacement(TEXT("hvac_unit"), TEXT("Grammar Roof HVAC Units"), UnitCenter, Axis, Normal, 1.1, 0.82, 0.55, RoofZ + 0.05, FLinearColor(0.52, 0.54, 0.52, 1.0)));
			}
		}

		if (GrammarEngineInternal::HasAny(Tokens, { TEXT("office"), TEXT("industrial"), TEXT("warehouse"), TEXT("supermarket"), TEXT("parking") }))
		{
			const double PlantWidth = FMath::Min(FMath::Max(Width, Depth) * 0.42, 5.5);
			const FVector2D PlantCenter = Center + FVector2D(Normal.X * Depth * 0.22, Normal.Y * Width * 0.22);
			Result.Add(BoxPlacement(TEXT("roof_plant"), TEXT("Grammar Roof Plant Screens"), PlantCenter, Axis, Normal, PlantWidth, 0.18, 1.05, RoofZ + 0.05, FLinearColor(0.24, 0.25, 0.24, 1.0)));
		}

		return Result;
	}

	TArray<FGrammarPlacementRecord> AntennaPlacements(const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof, const FFacadeStyleConfig& Style)
	{
		TArray<FGrammarPlacementRecord> Result;
		const FAntennaStyleConfig& Antenna = Style.Antenna;
		if (!Antenna.bEnabled || Antenna.Count <= 0 || Antenna.MastHeight <= 0.0)
		{
			return Result;
		}

		const FBox2D Bounds = FGrammarGeometry2D::Bounds(Footprint);
		const FVector2D Center = FGrammarGeometry2D::Centroid2D(Footprint);
		double RoofZ = Height;
		if (Roof.Type == EGrammarRoofType::Flat && Roof.bEdgeEnabled)
		{
			RoofZ = Height + FMath::Max(Roof.EdgeHeight - Roof.SurfaceInset, 0.0);
		}

		const TArray<FVector2D> Positions = AntennaPositions(Center, Bounds, Antenna.Count);
		for (const FVector2D& Position : Positions)
		{
			AntennaInstancePlacements(Result, Position, RoofZ, Antenna);
		}
		return Result;
	}
}
