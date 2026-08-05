#include "Grammar/GrammarDoor.h"
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
}

namespace GrammarDoor
{
	FGrammarPlacementRecord DoorPlacement(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Offset, double GroundFloorHeight, const FFacadeStyleConfig& Style)
	{
		const FVector2D Tangent = FGrammarGeometry2D::Tangent(Start, End);
		const FDoorStyleConfig& Door = Style.Door;
		const double Height = FMath::Min(Door.Height, FMath::Max(1.6, GroundFloorHeight - 0.25));

		FGrammarBoxPlacementParams Params;
		Params.Center = FGrammarGeometry2D::PointOnSegment(Start, Tangent, Normal, Offset, Door.Depth);
		Params.Tangent = Tangent;
		Params.Normal = Normal;
		Params.Width = Door.Width;
		Params.Depth = Door.Depth;
		Params.Height = Height;
		Params.Bottom = 0.0;
		return FGrammarPlacementHelpers::MakeBoxPlacement(TEXT("door"), Door.Material, Params, Door.Color);
	}

	TArray<FGrammarPlacementRecord> DoorDetailPlacements(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Offset, double GroundFloorHeight, const FFacadeStyleConfig& Style)
	{
		const FVector2D Tangent = FGrammarGeometry2D::Tangent(Start, End);
		const FDoorStyleConfig& Door = Style.Door;
		const double Height = FMath::Min(Door.Height, FMath::Max(1.6, GroundFloorHeight - 0.25));
		const double Depth = Door.Depth + FMath::Max(Door.FrameDepth, 0.0);
		const double FrameWidth = FMath::Max(Door.FrameWidth, 0.0);

		TArray<FGrammarPlacementRecord> Result;

		if (FrameWidth > 0.0)
		{
			for (const double Lateral : { -Door.Width / 2.0 - FrameWidth / 2.0, Door.Width / 2.0 + FrameWidth / 2.0 })
			{
				Result.Add(PanelPlacement(TEXT("door_frame"), Door.FrameMaterial, Start, Tangent, Normal, Offset + Lateral, FrameWidth, 0.0, Height + FrameWidth, Depth, Door.FrameColor));
			}
			Result.Add(PanelPlacement(TEXT("door_frame"), Door.FrameMaterial, Start, Tangent, Normal, Offset, Door.Width + FrameWidth * 2.0, Height, FrameWidth, Depth, Door.FrameColor));
		}

		if (Door.bHandleEnabled && Door.HandleRadius > 0.0)
		{
			FGrammarBoxPlacementParams HandleParams;
			HandleParams.Center = FGrammarGeometry2D::PointOnSegment(Start, Tangent, Normal, Offset + Door.Width * 0.28, Depth + Door.HandleRadius);
			HandleParams.Tangent = Tangent;
			HandleParams.Normal = Normal;
			HandleParams.Width = Door.HandleRadius * 1.6;
			HandleParams.Depth = Door.HandleRadius * 1.6;
			HandleParams.Height = Door.HandleRadius * 1.6;
			HandleParams.Bottom = FMath::Max(0.7, FMath::Min(Height * 0.55, Height - 0.25));
			Result.Add(FGrammarPlacementHelpers::MakeBoxPlacement(TEXT("door_handle"), Door.HandleMaterial, HandleParams, Door.HandleColor));
		}

		if (Door.bCanopyEnabled && Door.CanopyWidth > 0.0 && Door.CanopyDepth > 0.0 && Door.CanopyThickness > 0.0)
		{
			FGrammarBoxPlacementParams CanopyParams;
			CanopyParams.Center = FGrammarGeometry2D::PointOnSegment(Start, Tangent, Normal, Offset, Door.CanopyDepth / 2.0);
			CanopyParams.Tangent = Tangent;
			CanopyParams.Normal = Normal;
			CanopyParams.Width = Door.CanopyWidth;
			CanopyParams.Depth = Door.CanopyDepth;
			CanopyParams.Height = Door.CanopyThickness;
			CanopyParams.Bottom = FMath::Min(GroundFloorHeight - Door.CanopyThickness, Height + 0.18);
			Result.Add(FGrammarPlacementHelpers::MakeBoxPlacement(TEXT("door_canopy"), Door.CanopyMaterial, CanopyParams, Door.CanopyColor));
		}

		return Result;
	}
}
