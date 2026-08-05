#include "Grammar/GrammarRoofDirection.h"
#include "Geometry/GrammarGeometry2D.h"

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

	FString GetTrimmedLower(const TMap<FString, FString>& Tags, const TCHAR* Key)
	{
		const FString* Value = Tags.Find(Key);
		return Value ? Value->TrimStartAndEnd().ToLower() : FString();
	}
}

namespace GrammarRoofDirection
{
	FVector2D DirectionFromCardinalOrAngle(const FString& Value)
	{
		const FString Normalized = Value.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT("-"));

		static const TMap<FString, FVector2D> Cardinal = {
			{ TEXT("n"), FVector2D(0.0, 1.0) }, { TEXT("s"), FVector2D(0.0, 1.0) },
			{ TEXT("north"), FVector2D(0.0, 1.0) }, { TEXT("south"), FVector2D(0.0, 1.0) },
			{ TEXT("e"), FVector2D(1.0, 0.0) }, { TEXT("w"), FVector2D(1.0, 0.0) },
			{ TEXT("east"), FVector2D(1.0, 0.0) }, { TEXT("west"), FVector2D(1.0, 0.0) },
			{ TEXT("n-s"), FVector2D(0.0, 1.0) }, { TEXT("north-south"), FVector2D(0.0, 1.0) }, { TEXT("s-n"), FVector2D(0.0, 1.0) },
			{ TEXT("e-w"), FVector2D(1.0, 0.0) }, { TEXT("east-west"), FVector2D(1.0, 0.0) }, { TEXT("w-e"), FVector2D(1.0, 0.0) },
			{ TEXT("ne-sw"), FGrammarGeometry2D::Normalize2D(FVector2D(1.0, 1.0)) },
			{ TEXT("sw-ne"), FGrammarGeometry2D::Normalize2D(FVector2D(1.0, 1.0)) },
			{ TEXT("nw-se"), FGrammarGeometry2D::Normalize2D(FVector2D(-1.0, 1.0)) },
			{ TEXT("se-nw"), FGrammarGeometry2D::Normalize2D(FVector2D(-1.0, 1.0)) },
		};
		if (const FVector2D* Found = Cardinal.Find(Normalized))
		{
			return *Found;
		}

		FString AngleString = Normalized;
		if (AngleString.EndsWith(TEXT("deg")))
		{
			AngleString.LeftChopInline(3);
		}
		AngleString = AngleString.Replace(TEXT("°"), TEXT(""));
		if (AngleString.IsEmpty() || !FCString::IsNumeric(*AngleString))
		{
			return FVector2D::ZeroVector;
		}
		const double AngleDegrees = FCString::Atod(*AngleString);
		const double AngleRadians = FMath::DegreesToRadians(AngleDegrees);
		return FGrammarGeometry2D::Normalize2D(FVector2D(FMath::Sin(AngleRadians), FMath::Cos(AngleRadians)));
	}

	FVector2D RoofOrientationDirection(const TArray<FVector>& Base, const TMap<FString, FString>& Tags)
	{
		FString Orientation = GetTrimmedLower(Tags, TEXT("roof:orientation"));
		if (Orientation.IsEmpty())
		{
			Orientation = GetTrimmedLower(Tags, TEXT("roof:direction"));
		}
		if (Orientation.IsEmpty())
		{
			return FVector2D::ZeroVector;
		}

		const FVector2D Longest = FGrammarGeometry2D::LongestAxisDirection(ToXY(Base));
		if (Orientation == TEXT("along") || Orientation == TEXT("parallel") || Orientation == TEXT("longitudinal"))
		{
			return Longest;
		}
		if (Orientation == TEXT("across") || Orientation == TEXT("perpendicular") || Orientation == TEXT("transverse"))
		{
			return FVector2D(-Longest.Y, Longest.X);
		}
		return DirectionFromCardinalOrAngle(Orientation);
	}

	FVector2D RidgeDirectionFromTags(const TMap<FString, FString>& Tags)
	{
		for (const TCHAR* Key : { TEXT("grammar:roof:ridge_direction"), TEXT("roof:ridge:direction") })
		{
			const FString* Value = Tags.Find(Key);
			if (!Value || Value->IsEmpty())
			{
				continue;
			}
			TArray<FString> Parts;
			Value->Replace(TEXT(";"), TEXT(",")).ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() < 2)
			{
				continue;
			}
			const FString XStr = Parts[0].TrimStartAndEnd();
			const FString YStr = Parts[1].TrimStartAndEnd();
			if (!FCString::IsNumeric(*XStr) || !FCString::IsNumeric(*YStr))
			{
				continue;
			}
			const FVector2D Direction = FGrammarGeometry2D::Normalize2D(FVector2D(FCString::Atod(*XStr), FCString::Atod(*YStr)));
			if (!Direction.IsNearlyZero())
			{
				return Direction;
			}
		}
		return FVector2D::ZeroVector;
	}

	FVector2D GabledRidgeDirection(const TArray<FVector>& Base, const FRoofStyleConfig& Roof, const TMap<FString, FString>& Tags)
	{
		const FVector2D Oriented = RoofOrientationDirection(Base, Tags);
		if (!Oriented.IsNearlyZero())
		{
			return Oriented;
		}
		if (Roof.RidgeAlignment == EGrammarRidgeAlignment::ClosestStreet)
		{
			const FVector2D Hinted = RidgeDirectionFromTags(Tags);
			if (!Hinted.IsNearlyZero())
			{
				return Hinted;
			}
		}
		return FGrammarGeometry2D::LongestAxisDirection(ToXY(Base));
	}
}
