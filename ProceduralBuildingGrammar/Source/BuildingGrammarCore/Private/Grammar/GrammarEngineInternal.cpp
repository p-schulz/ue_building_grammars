#include "Grammar/GrammarEngineInternal.h"
#include "Geometry/GrammarGeometry2D.h"
#include "Geometry/GrammarStableHash.h"
#include "Grammar/GrammarTagParsing.h"
#include "Misc/Parse.h"

namespace
{
	TOptional<double> ParseMetersEither(const TMap<FString, FString>& Tags, const TCHAR* PrimaryKey, const TCHAR* FallbackKey)
	{
		TOptional<double> Value = FGrammarTagParsing::ParseMeters(Tags.Find(PrimaryKey));
		if (!Value.IsSet())
		{
			Value = FGrammarTagParsing::ParseMeters(Tags.Find(FallbackKey));
		}
		return Value;
	}

	FString JoinStrings(const TArray<FString>& Parts, const TCHAR* Delimiter)
	{
		FString Result;
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			if (Index > 0)
			{
				Result += Delimiter;
			}
			Result += Parts[Index];
		}
		return Result;
	}
}

namespace GrammarEngineInternal
{
	TSet<FString> StyleTokens(const FFacadeStyleConfig& Style, const TMap<FString, FString>& Tags)
	{
		auto Get = [&Tags](const TCHAR* Key) -> FString
		{
			const FString* Value = Tags.Find(Key);
			return Value ? *Value : FString();
		};

		const TArray<FString> Parts = {
			Style.Name, Style.WallMaterial, Style.Window.Material, Style.Ledge.Material,
			Get(TEXT("building")), Get(TEXT("building:part")), Get(TEXT("building:use")),
			Get(TEXT("shop")), Get(TEXT("office")), Get(TEXT("industrial")), Get(TEXT("amenity")),
			Get(TEXT("religion")), Get(TEXT("denomination")), Get(TEXT("landuse")), Get(TEXT("parking")),
			Get(TEXT("start_date"))
		};
		FString Raw = JoinStrings(Parts, TEXT(" "));
		Raw = Raw.ToLower().Replace(TEXT("_"), TEXT(" ")).Replace(TEXT("-"), TEXT(" ")).Replace(TEXT(":"), TEXT(" "));

		TArray<FString> Tokens;
		Raw.ParseIntoArrayWS(Tokens);
		TSet<FString> Result;
		Result.Append(Tokens);
		return Result;
	}

	bool HasAny(const TSet<FString>& Tokens, const TSet<FString>& Values)
	{
		for (const FString& Value : Values)
		{
			if (Tokens.Contains(Value))
			{
				return true;
			}
		}
		return false;
	}

	bool IsRetailStyle(const TSet<FString>& Tokens, const TMap<FString, FString>& Tags)
	{
		const FString* Shop = Tags.Find(TEXT("shop"));
		if (Shop && !Shop->IsEmpty())
		{
			return true;
		}
		return HasAny(Tokens, { TEXT("retail"), TEXT("shopfront"), TEXT("shop"), TEXT("supermarket"), TEXT("commercial") });
	}

	bool IsIndustrialStyle(const TSet<FString>& Tokens, const TMap<FString, FString>& Tags)
	{
		const FString* Industrial = Tags.Find(TEXT("industrial"));
		if (Industrial && !Industrial->IsEmpty())
		{
			return true;
		}
		return HasAny(Tokens, { TEXT("industrial"), TEXT("warehouse"), TEXT("factory"), TEXT("logistics"), TEXT("manufacturing") });
	}

	bool IsParkingStyle(const TSet<FString>& Tokens)
	{
		return HasAny(Tokens, { TEXT("parking"), TEXT("garage"), TEXT("multistorey"), TEXT("car") });
	}

	bool StyleHasShutters(const TSet<FString>& StyleOnlyTokens)
	{
		return HasAny(StyleOnlyTokens, { TEXT("fachwerk"), TEXT("mediterranean"), TEXT("rowhouse"), TEXT("reihenhaus"), TEXT("siedlung"), TEXT("gruenderzeit"), TEXT("jugendstil") });
	}

	bool ShouldAddStairCore(const TSet<FString>& Tokens, bool bStreetFacing, double Length, double TotalHeight)
	{
		if (bStreetFacing || Length < 4.0 || TotalHeight < 7.0)
		{
			return false;
		}
		return HasAny(Tokens, { TEXT("office"), TEXT("parking"), TEXT("plattenbau"), TEXT("apartment"), TEXT("apartments"), TEXT("residential"), TEXT("modern") });
	}

	TPair<int32, FLinearColor> VariantWallColor(const FFacadeStyleConfig& Style, const FString& SourceName, int32 SideIndex)
	{
		const TArray<FLinearColor>& Colors = Style.WallColorVariants;
		if (Colors.Num() == 0 || Style.WallColorVariantMode == EGrammarWallColorVariantMode::None)
		{
			return TPair<int32, FLinearColor>(-1, Style.WallColor);
		}
		int32 Index = 0;
		if (Style.WallColorVariantMode == EGrammarWallColorVariantMode::Building)
		{
			Index = FGrammarStableHash::StableIndex(SourceName, Colors.Num());
		}
		else if (Style.WallColorVariantMode == EGrammarWallColorVariantMode::Facade)
		{
			Index = FGrammarStableHash::StableIndex(FString::Printf(TEXT("%s:%d"), *SourceName, SideIndex), Colors.Num());
		}
		else
		{
			Index = SideIndex % Colors.Num();
		}
		return TPair<int32, FLinearColor>(Index, Colors[Index]);
	}

	TPair<int32, FLinearColor> RowWallColor(const FFacadeStyleConfig& Style, int32 FloorIndex)
	{
		const TArray<FLinearColor>& Colors = Style.WallRowColors;
		if (Colors.Num() == 0)
		{
			return TPair<int32, FLinearColor>(-1, Style.WallColor);
		}
		if (Style.WallRowColorMode == EGrammarWallRowColorMode::GroundAccent)
		{
			if (FloorIndex == 0)
			{
				return TPair<int32, FLinearColor>(0, Colors[0]);
			}
			const int32 Index = FMath::Min(1 + (FloorIndex - 1) % FMath::Max(1, Colors.Num() - 1), Colors.Num() - 1);
			return TPair<int32, FLinearColor>(Index, Colors[Index]);
		}
		const int32 Index = FloorIndex % Colors.Num();
		return TPair<int32, FLinearColor>(Index, Colors[Index]);
	}

	FString WallMaterialName(const FString& Base, const FString& Kind, int32 ColorIndex)
	{
		if (ColorIndex < 0)
		{
			return Base;
		}
		return FString::Printf(TEXT("%s %s %d"), *Base, *Kind, ColorIndex + 1);
	}

	FString SemanticMaterialName(const FString& Role, const FString& Value)
	{
		FString Normalized = Value.TrimStartAndEnd().ToLower().Replace(TEXT("_"), TEXT(" ")).Replace(TEXT("-"), TEXT(" "));

		static const TMap<FString, FString> Aliases = {
			{ TEXT("brick"), TEXT("Brick") }, { TEXT("bricks"), TEXT("Brick") },
			{ TEXT("plaster"), TEXT("Plaster") }, { TEXT("render"), TEXT("Render") }, { TEXT("stucco"), TEXT("Stucco") },
			{ TEXT("glass"), TEXT("Glass") }, { TEXT("metal"), TEXT("Metal") }, { TEXT("steel"), TEXT("Steel") },
			{ TEXT("concrete"), TEXT("Concrete") }, { TEXT("wood"), TEXT("Wood") }, { TEXT("timber"), TEXT("Timber") },
			{ TEXT("stone"), TEXT("Stone") }, { TEXT("sandstone"), TEXT("Sandstone") },
			{ TEXT("tile"), TEXT("Tile") }, { TEXT("tiles"), TEXT("Tile") }, { TEXT("roof tiles"), TEXT("Tile") },
			{ TEXT("slate"), TEXT("Slate") }, { TEXT("copper"), TEXT("Copper") }, { TEXT("zinc"), TEXT("Zinc") },
			{ TEXT("asphalt"), TEXT("Asphalt") }, { TEXT("membrane"), TEXT("Membrane") },
		};

		FString Label;
		if (const FString* Alias = Aliases.Find(Normalized))
		{
			Label = *Alias;
		}
		else
		{
			TArray<FString> Parts;
			Normalized.ParseIntoArrayWS(Parts);
			TArray<FString> Capitalized;
			for (const FString& Part : Parts)
			{
				Capitalized.Add(Part.Left(1).ToUpper() + Part.Mid(1));
			}
			Label = JoinStrings(Capitalized, TEXT(" "));
		}
		return Label.IsEmpty() ? FString::Printf(TEXT("OSM %s"), *Role) : FString::Printf(TEXT("OSM %s %s"), *Role, *Label);
	}

	TOptional<FLinearColor> ParseOsmColor(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return TOptional<FLinearColor>();
		}
		FString Color = Value.TrimStartAndEnd().ToLower().Replace(TEXT("grey"), TEXT("gray"));
		int32 SemicolonIndex;
		if (Color.FindChar(TEXT(';'), SemicolonIndex))
		{
			Color = Color.Left(SemicolonIndex).TrimStartAndEnd();
		}

		if (Color.StartsWith(TEXT("#")))
		{
			FString HexValue = Color.Mid(1);
			if (HexValue.Len() == 3)
			{
				FString Expanded;
				for (const TCHAR Ch : HexValue)
				{
					Expanded.AppendChar(Ch);
					Expanded.AppendChar(Ch);
				}
				HexValue = Expanded;
			}
			if (HexValue.Len() == 6)
			{
				bool bValidHex = true;
				for (const TCHAR Ch : HexValue)
				{
					if (!FChar::IsHexDigit(Ch))
					{
						bValidHex = false;
						break;
					}
				}
				if (bValidHex)
				{
					const int32 R = FParse::HexNumber(*HexValue.Mid(0, 2));
					const int32 G = FParse::HexNumber(*HexValue.Mid(2, 2));
					const int32 B = FParse::HexNumber(*HexValue.Mid(4, 2));
					return FLinearColor(R / 255.0f, G / 255.0f, B / 255.0f, 1.0f);
				}
			}
		}

		static const TMap<FString, FLinearColor> Named = {
			{ TEXT("black"), FLinearColor(0.02f, 0.02f, 0.02f, 1.0f) },
			{ TEXT("white"), FLinearColor(0.92f, 0.9f, 0.86f, 1.0f) },
			{ TEXT("gray"), FLinearColor(0.45f, 0.45f, 0.43f, 1.0f) },
			{ TEXT("silver"), FLinearColor(0.68f, 0.68f, 0.66f, 1.0f) },
			{ TEXT("red"), FLinearColor(0.58f, 0.12f, 0.08f, 1.0f) },
			{ TEXT("brown"), FLinearColor(0.38f, 0.22f, 0.12f, 1.0f) },
			{ TEXT("beige"), FLinearColor(0.72f, 0.66f, 0.54f, 1.0f) },
			{ TEXT("cream"), FLinearColor(0.84f, 0.78f, 0.62f, 1.0f) },
			{ TEXT("yellow"), FLinearColor(0.78f, 0.64f, 0.22f, 1.0f) },
			{ TEXT("orange"), FLinearColor(0.72f, 0.38f, 0.12f, 1.0f) },
			{ TEXT("green"), FLinearColor(0.24f, 0.42f, 0.25f, 1.0f) },
			{ TEXT("blue"), FLinearColor(0.12f, 0.24f, 0.46f, 1.0f) },
			{ TEXT("anthracite"), FLinearColor(0.12f, 0.13f, 0.13f, 1.0f) },
			{ TEXT("terracotta"), FLinearColor(0.55f, 0.2f, 0.11f, 1.0f) },
		};
		if (const FLinearColor* Found = Named.Find(Color))
		{
			return *Found;
		}
		return TOptional<FLinearColor>();
	}

	FRoofStyleConfig RoofFromTags(const FRoofStyleConfig& Roof, const TMap<FString, FString>& Tags)
	{
		FRoofStyleConfig Result = Roof;

		FString RoofShape;
		for (const TCHAR* Key : { TEXT("roof:shape"), TEXT("roof:type"), TEXT("grammar:roof:type") })
		{
			if (const FString* Value = Tags.Find(Key))
			{
				RoofShape = Value->TrimStartAndEnd().ToLower();
				if (!RoofShape.IsEmpty())
				{
					break;
				}
			}
		}
		static const TMap<FString, EGrammarRoofType> ShapeAliases = {
			{ TEXT("flat"), EGrammarRoofType::Flat },
			{ TEXT("gabled"), EGrammarRoofType::Gabled }, { TEXT("gable"), EGrammarRoofType::Gabled },
			{ TEXT("pitched"), EGrammarRoofType::Gabled }, { TEXT("skillion"), EGrammarRoofType::Gabled },
			{ TEXT("saltbox"), EGrammarRoofType::Gabled }, { TEXT("gambrel"), EGrammarRoofType::Gabled },
			{ TEXT("mansard"), EGrammarRoofType::Gabled },
			{ TEXT("hipped"), EGrammarRoofType::Hipped }, { TEXT("hip"), EGrammarRoofType::Hipped },
			{ TEXT("half-hipped"), EGrammarRoofType::Hipped }, { TEXT("half hipped"), EGrammarRoofType::Hipped },
			{ TEXT("side_hipped"), EGrammarRoofType::Hipped },
			{ TEXT("pyramid"), EGrammarRoofType::Pyramid }, { TEXT("pyramidal"), EGrammarRoofType::Pyramid },
		};
		if (const EGrammarRoofType* MappedType = ShapeAliases.Find(RoofShape))
		{
			Result.Type = *MappedType;
		}

		const TOptional<double> RoofHeight = ParseMetersEither(Tags, TEXT("roof:height"), TEXT("roof:levels"));
		if (RoofHeight.IsSet())
		{
			Result.Height = FMath::Max(RoofHeight.GetValue(), 0.0);
		}

		FString RoofMaterial;
		if (const FString* Value = Tags.Find(TEXT("roof:material")))
		{
			RoofMaterial = *Value;
		}
		else if (const FString* Alt = Tags.Find(TEXT("roof:material:name")))
		{
			RoofMaterial = *Alt;
		}
		if (!RoofMaterial.IsEmpty())
		{
			Result.Material = SemanticMaterialName(TEXT("Roof"), RoofMaterial);
		}

		FString RoofColorValue;
		if (const FString* Value = Tags.Find(TEXT("roof:colour")))
		{
			RoofColorValue = *Value;
		}
		else if (const FString* Alt = Tags.Find(TEXT("roof:color")))
		{
			RoofColorValue = *Alt;
		}
		if (const TOptional<FLinearColor> RoofColor = ParseOsmColor(RoofColorValue))
		{
			Result.Color = RoofColor.GetValue();
		}

		return Result;
	}

	FFacadeStyleConfig FacadeStyleFromTags(const FFacadeStyleConfig& Style, const TMap<FString, FString>& Tags)
	{
		FFacadeStyleConfig Result = Style;

		FString Material;
		for (const TCHAR* Key : { TEXT("facade:material"), TEXT("building:facade:material"), TEXT("building:material") })
		{
			if (const FString* Value = Tags.Find(Key))
			{
				if (!Value->IsEmpty())
				{
					Material = *Value;
					break;
				}
			}
		}
		if (!Material.IsEmpty())
		{
			Result.WallMaterial = SemanticMaterialName(TEXT("Facade"), Material);
		}

		FString ColorValue;
		for (const TCHAR* Key : { TEXT("facade:colour"), TEXT("facade:color"), TEXT("building:colour"), TEXT("building:color") })
		{
			if (const FString* Value = Tags.Find(Key))
			{
				if (!Value->IsEmpty())
				{
					ColorValue = *Value;
					break;
				}
			}
		}
		if (const TOptional<FLinearColor> Color = ParseOsmColor(ColorValue))
		{
			Result.WallColor = Color.GetValue();
			Result.WallColorVariants.Empty();
			Result.WallRowColors.Empty();
		}

		return Result;
	}

	int32 StreetFacingSideIndex(const TArray<FVector2D>& Footprint, const TMap<FString, FString>& Tags)
	{
		FString PointValue;
		if (const FString* P1 = Tags.Find(TEXT("grammar:street:point")))
		{
			PointValue = *P1;
		}
		else if (const FString* P2 = Tags.Find(TEXT("grammar:street_point")))
		{
			PointValue = *P2;
		}

		if (!PointValue.IsEmpty())
		{
			TArray<FString> Parts;
			PointValue.Replace(TEXT(";"), TEXT(",")).ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() >= 2)
			{
				const FString XStr = Parts[0].TrimStartAndEnd();
				const FString YStr = Parts[1].TrimStartAndEnd();
				if (FCString::IsNumeric(*XStr) && FCString::IsNumeric(*YStr) && Footprint.Num() > 0)
				{
					const FVector2D Point(FCString::Atod(*XStr), FCString::Atod(*YStr));
					const TArray<FGrammarGeometry2D::FEdge> Segments = FGrammarGeometry2D::GetSegments(Footprint);
					int32 BestIndex = 0;
					double BestDistSq = TNumericLimits<double>::Max();
					for (int32 Index = 0; Index < Segments.Num(); ++Index)
					{
						const double DistSq = FGrammarGeometry2D::PointToSegmentDistanceSquared(Point, Segments[Index].Start, Segments[Index].End);
						if (DistSq < BestDistSq)
						{
							BestDistSq = DistSq;
							BestIndex = Index;
						}
					}
					return BestIndex;
				}
			}
		}

		if (const FString* SideValue = Tags.Find(TEXT("grammar:street_facing_side")))
		{
			const FString Trimmed = SideValue->TrimStartAndEnd();
			if (FCString::IsNumeric(*Trimmed) && Footprint.Num() > 0)
			{
				const int32 Value = FCString::Atoi(*Trimmed);
				const int32 Count = Footprint.Num();
				return ((Value % Count) + Count) % Count;
			}
			return 0;
		}
		return 0;
	}

	bool DoorApplies(const FFacadeStyleConfig& Style, int32 SideIndex, int32 StreetSideIndex, const TMap<FString, FString>& Tags)
	{
		if (const FString* Disable = Tags.Find(TEXT("grammar:disable_ground_entrance")))
		{
			const FString Normalized = Disable->TrimStartAndEnd().ToLower();
			if (Normalized == TEXT("1") || Normalized == TEXT("yes") || Normalized == TEXT("true"))
			{
				return false;
			}
		}
		if (!Style.Door.bEnabled || Style.Door.Placement == EGrammarDoorPlacement::None)
		{
			return false;
		}
		return Style.Door.Placement == EGrammarDoorPlacement::EachFacade || SideIndex == StreetSideIndex;
	}

	bool WindowOverlapsDoor(double Offset, double DoorOffset, const FFacadeStyleConfig& Style)
	{
		const double Clearance = (Style.Window.Width + Style.Door.Width) / 2.0 + FMath::Max(Style.Door.FrameWidth, 0.0);
		return FMath::Abs(Offset - DoorOffset) < Clearance;
	}
}
