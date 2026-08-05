#include "Grammar/GrammarWallWindow.h"
#include "Grammar/GrammarEngineInternal.h"
#include "Grammar/GrammarPlacementHelpers.h"
#include "Geometry/GrammarGeometry2D.h"
#include "Geometry/GrammarFace.h"

namespace
{
	void ComputeWindowVerticalExtent(const FWindowStyleConfig& Window, double FloorHeight, double& OutHeight, double& OutBottomOffset)
	{
		OutHeight = FMath::Min(Window.Height, FMath::Max(0.2, FloorHeight - Window.SillHeight - 0.25));
		OutBottomOffset = FMath::Min(Window.SillHeight, FMath::Max(0.15, FloorHeight - OutHeight - 0.15));
	}

	// Port of grammar.py's _window_panel_mesh, retargeted to emit a placement instead of a flat
	// quad's 4 vertices -- see GrammarPlacementHelpers.h.
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

namespace GrammarWallWindow
{
	TArray<double> WindowOffsets(double Length, double Width, double Spacing, double Margin)
	{
		const double Usable = Length - Margin * 2.0;
		if (Usable < Width)
		{
			return {};
		}
		const int32 Count = FMath::Max(1, static_cast<int32>(FMath::FloorToDouble((Usable + Spacing - Width) / Spacing)));
		const double TotalWidth = (Count - 1) * Spacing + Width;
		const double Start = (Length - TotalWidth) / 2.0 + Width / 2.0;

		TArray<double> Offsets;
		Offsets.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Offsets.Add(Start + Index * Spacing);
		}
		return Offsets;
	}

	FGrammarMeshSpec WallMesh(const FString& MeshSourceName, int32 SideIndex, const FVector2D& Start, const FVector2D& End, double Height, const FFacadeStyleConfig& Style, const FString& SourceName)
	{
		const TPair<int32, FLinearColor> ColorResult = GrammarEngineInternal::VariantWallColor(Style, SourceName, SideIndex);

		FGrammarMeshSpec Mesh;
		Mesh.Name = FString::Printf(TEXT("%s.facade_%d"), *MeshSourceName, SideIndex);
		Mesh.Role = TEXT("facade");
		Mesh.Material = GrammarEngineInternal::WallMaterialName(Style.WallMaterial, TEXT("variant"), ColorResult.Key);
		Mesh.Color = ColorResult.Value;
		Mesh.TexturePath = Style.WallTexturePath;
		Mesh.Vertices = {
			FVector(Start.X, Start.Y, 0.0),
			FVector(End.X, End.Y, 0.0),
			FVector(End.X, End.Y, Height),
			FVector(Start.X, Start.Y, Height)
		};
		Mesh.Faces = { FGrammarFace({ 0, 1, 2, 3 }) };
		Mesh.TextureScale = Style.WallTextureScale;
		return Mesh;
	}

	FGrammarMeshSpec WallRowMesh(const FString& SourceName, int32 SideIndex, int32 FloorIndex, const FVector2D& Start, const FVector2D& End, double FloorBottom, double FloorHeight, const FFacadeStyleConfig& Style)
	{
		const TPair<int32, FLinearColor> ColorResult = GrammarEngineInternal::RowWallColor(Style, FloorIndex);

		FGrammarMeshSpec Mesh;
		Mesh.Name = FString::Printf(TEXT("%s.facade_%d_row_%d"), *SourceName, SideIndex, FloorIndex);
		Mesh.Role = TEXT("facade");
		Mesh.Material = GrammarEngineInternal::WallMaterialName(Style.WallMaterial, TEXT("row"), ColorResult.Key);
		Mesh.Color = ColorResult.Value;
		Mesh.TexturePath = Style.WallTexturePath;
		Mesh.Vertices = {
			FVector(Start.X, Start.Y, FloorBottom),
			FVector(End.X, End.Y, FloorBottom),
			FVector(End.X, End.Y, FloorBottom + FloorHeight),
			FVector(Start.X, Start.Y, FloorBottom + FloorHeight)
		};
		Mesh.Faces = { FGrammarFace({ 0, 1, 2, 3 }) };
		Mesh.TextureScale = Style.WallTextureScale;
		return Mesh;
	}

	FGrammarPlacementRecord WindowPlacement(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Offset, double FloorBottom, double FloorHeight, const FFacadeStyleConfig& Style)
	{
		const FVector2D Tangent = FGrammarGeometry2D::Tangent(Start, End);
		const FWindowStyleConfig& Window = Style.Window;

		double Height, BottomOffset;
		ComputeWindowVerticalExtent(Window, FloorHeight, Height, BottomOffset);
		const double Bottom = FloorBottom + BottomOffset;

		FGrammarBoxPlacementParams Params;
		Params.Center = FGrammarGeometry2D::PointOnSegment(Start, Tangent, Normal, Offset, Window.Depth);
		Params.Tangent = Tangent;
		Params.Normal = Normal;
		Params.Width = Window.Width;
		Params.Depth = Window.Depth;
		Params.Height = Height;
		Params.Bottom = Bottom;
		return FGrammarPlacementHelpers::MakeBoxPlacement(TEXT("window"), Window.Material, Params, Window.Color);
	}

	TArray<FGrammarPlacementRecord> WindowDetailPlacements(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Offset, double FloorBottom, double FloorHeight, const FFacadeStyleConfig& Style)
	{
		const FVector2D Tangent = FGrammarGeometry2D::Tangent(Start, End);
		const FWindowStyleConfig& Window = Style.Window;
		const double FrameWidth = FMath::Max(Window.FrameWidth, 0.0);

		double Height, BottomOffset;
		ComputeWindowVerticalExtent(Window, FloorHeight, Height, BottomOffset);
		const double Bottom = FloorBottom + BottomOffset;
		const double Depth = Window.Depth + FMath::Max(Window.FrameDepth, 0.0);

		TArray<FGrammarPlacementRecord> Result;

		if (FrameWidth > 0.0)
		{
			const double FrameHeight = Height + FrameWidth * 2.0;
			const double FrameBottom = Bottom - FrameWidth;
			const double FrameSpan = Window.Width + FrameWidth * 2.0;

			for (const double Lateral : { -Window.Width / 2.0 - FrameWidth / 2.0, Window.Width / 2.0 + FrameWidth / 2.0 })
			{
				Result.Add(PanelPlacement(TEXT("window_frame"), Window.FrameMaterial, Start, Tangent, Normal, Offset + Lateral, FrameWidth, FrameBottom, FrameHeight, Depth, Window.FrameColor));
			}
			for (const double Z : { Bottom - FrameWidth / 2.0, Bottom + Height + FrameWidth / 2.0 })
			{
				Result.Add(PanelPlacement(TEXT("window_frame"), Window.FrameMaterial, Start, Tangent, Normal, Offset, FrameSpan, Z - FrameWidth / 2.0, FrameWidth, Depth, Window.FrameColor));
			}
		}

		const double MullionWidth = FMath::Max(FrameWidth * 0.65, 0.025);
		const int32 VerticalMullions = FMath::Max(Window.VerticalMullions, 0);
		for (int32 MullionIndex = 0; MullionIndex < VerticalMullions; ++MullionIndex)
		{
			const double Lateral = -Window.Width / 2.0 + Window.Width * (MullionIndex + 1) / static_cast<double>(VerticalMullions + 1);
			Result.Add(PanelPlacement(TEXT("window_mullion"), Window.FrameMaterial, Start, Tangent, Normal, Offset + Lateral, MullionWidth, Bottom, Height, Depth + 0.01, Window.FrameColor));
		}

		const int32 HorizontalMullions = FMath::Max(Window.HorizontalMullions, 0);
		for (int32 MullionIndex = 0; MullionIndex < HorizontalMullions; ++MullionIndex)
		{
			const double Z = Bottom + Height * (MullionIndex + 1) / static_cast<double>(HorizontalMullions + 1);
			Result.Add(PanelPlacement(TEXT("window_mullion"), Window.FrameMaterial, Start, Tangent, Normal, Offset, Window.Width, Z - MullionWidth / 2.0, MullionWidth, Depth + 0.01, Window.FrameColor));
		}

		if (Window.SillDepth > 0.0 && Window.SillThickness > 0.0)
		{
			FGrammarBoxPlacementParams SillParams;
			SillParams.Center = FGrammarGeometry2D::PointOnSegment(Start, Tangent, Normal, Offset, Window.SillDepth / 2.0);
			SillParams.Tangent = Tangent;
			SillParams.Normal = Normal;
			SillParams.Width = Window.Width + FrameWidth * 2.5;
			SillParams.Depth = Window.SillDepth;
			SillParams.Height = Window.SillThickness;
			SillParams.Bottom = FMath::Max(0.0, Bottom - Window.SillThickness);
			Result.Add(FGrammarPlacementHelpers::MakeBoxPlacement(TEXT("window_sill"), Window.SillMaterial, SillParams, Window.SillColor));
		}

		if (GrammarEngineInternal::StyleHasShutters(Style))
		{
			const double ShutterWidth = FMath::Max(FMath::Min(Window.Width * 0.28, 0.34), 0.16);
			const double ShutterHeight = Height + FrameWidth;
			const double ShutterBottom = FMath::Max(0.0, Bottom - FrameWidth * 0.5);
			static const FString ShutterMaterial = TEXT("Grammar Facade Shutters");
			const FLinearColor ShutterColor(0.14, 0.19, 0.14, 1.0);
			for (const double Lateral : { -Window.Width / 2.0 - ShutterWidth / 2.0 - FrameWidth, Window.Width / 2.0 + ShutterWidth / 2.0 + FrameWidth })
			{
				Result.Add(PanelPlacement(TEXT("shutter"), ShutterMaterial, Start, Tangent, Normal, Offset + Lateral, ShutterWidth, ShutterBottom, ShutterHeight, Depth + 0.025, ShutterColor));
			}
		}

		return Result;
	}
}
