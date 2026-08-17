#include "Config/GrammarConfigJson.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"

namespace
{
	// ---- Generic field readers, mirroring Python's dict.get(key, default) ----

	double GetNum(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, double Default)
	{
		double Value = Default;
		if (Obj.IsValid())
		{
			Obj->TryGetNumberField(Key, Value);
		}
		return Value;
	}

	int32 GetInt(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, int32 Default)
	{
		int32 Value = Default;
		if (Obj.IsValid())
		{
			Obj->TryGetNumberField(Key, Value);
		}
		return Value;
	}

	bool GetBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, bool Default)
	{
		bool Value = Default;
		if (Obj.IsValid())
		{
			Obj->TryGetBoolField(Key, Value);
		}
		return Value;
	}

	FString GetStr(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, const FString& Default)
	{
		FString Value = Default;
		if (Obj.IsValid())
		{
			Obj->TryGetStringField(Key, Value);
		}
		return Value;
	}

	// Optional[str] semantics: empty FString means "not set" (Python None), matching every
	// *_path field's meaning throughout the config structs.
	FString GetOptionalStr(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key)
	{
		return GetStr(Obj, Key, FString());
	}

	FLinearColor ColorFromChannels(const TArray<TSharedPtr<FJsonValue>>* Channels, const FLinearColor& Fallback)
	{
		if (!Channels)
		{
			return Fallback;
		}
		TArray<double> Values;
		for (const TSharedPtr<FJsonValue>& Value : *Channels)
		{
			double Number = 0.0;
			if (Value.IsValid() && Value->TryGetNumber(Number))
			{
				Values.Add(Number);
			}
		}
		if (Values.Num() == 3)
		{
			return FLinearColor(Values[0], Values[1], Values[2], 1.0);
		}
		if (Values.Num() == 4)
		{
			return FLinearColor(Values[0], Values[1], Values[2], Values[3]);
		}
		return Fallback;
	}

	FLinearColor GetColor(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, const FLinearColor& Default)
	{
		if (!Obj.IsValid())
		{
			return Default;
		}
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Obj->TryGetArrayField(Key, Array))
		{
			return ColorFromChannels(Array, Default);
		}
		return Default;
	}

	TArray<TSharedPtr<FJsonValue>> ColorToJsonArray(const FLinearColor& Color)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		Result.Add(MakeShared<FJsonValueNumber>(Color.R));
		Result.Add(MakeShared<FJsonValueNumber>(Color.G));
		Result.Add(MakeShared<FJsonValueNumber>(Color.B));
		Result.Add(MakeShared<FJsonValueNumber>(Color.A));
		return Result;
	}

	TArray<FLinearColor> GetColorList(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key)
	{
		TArray<FLinearColor> Result;
		if (!Obj.IsValid())
		{
			return Result;
		}
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Obj->TryGetArrayField(Key, Values) || !Values)
		{
			return Result;
		}
		for (const TSharedPtr<FJsonValue>& Item : *Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* Channels = nullptr;
			if (Item.IsValid() && Item->TryGetArray(Channels))
			{
				Result.Add(ColorFromChannels(Channels, FLinearColor(0.72, 0.68, 0.6, 1.0)));
			}
		}
		return Result;
	}

	TArray<TSharedPtr<FJsonValue>> ColorListToJsonArray(const TArray<FLinearColor>& Colors)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (const FLinearColor& Color : Colors)
		{
			Result.Add(MakeShared<FJsonValueArray>(ColorToJsonArray(Color)));
		}
		return Result;
	}

	// Mirrors config.py's _string_list: accepts either a JSON array of strings or a single
	// comma-separated string.
	TArray<FString> StringListFromValue(const TSharedPtr<FJsonValue>& Value)
	{
		TArray<FString> Result;
		if (!Value.IsValid())
		{
			return Result;
		}

		FString AsString;
		if (Value->TryGetString(AsString))
		{
			TArray<FString> Parts;
			AsString.ParseIntoArray(Parts, TEXT(","), true);
			for (FString Part : Parts)
			{
				Part.TrimStartAndEndInline();
				if (!Part.IsEmpty())
				{
					Result.Add(Part);
				}
			}
			return Result;
		}

		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Value->TryGetArray(Array) && Array)
		{
			for (const TSharedPtr<FJsonValue>& Item : *Array)
			{
				FString ItemString;
				if (Item.IsValid() && Item->TryGetString(ItemString))
				{
					ItemString.TrimStartAndEndInline();
					if (!ItemString.IsEmpty())
					{
						Result.Add(ItemString);
					}
				}
			}
		}
		return Result;
	}

	TArray<FString> GetStringList(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key)
	{
		if (!Obj.IsValid())
		{
			return {};
		}
		return StringListFromValue(Obj->TryGetField(Key));
	}

	TArray<TSharedPtr<FJsonValue>> StringListToJsonArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (const FString& Value : Values)
		{
			Result.Add(MakeShared<FJsonValueString>(Value));
		}
		return Result;
	}

	// ---- Enum <-> snake_case string, matching each config struct's Python field values ----

	EGrammarDoorPlacement DoorPlacementFromString(const FString& Value)
	{
		const FString Lower = Value.ToLower();
		if (Lower == TEXT("each_facade")) return EGrammarDoorPlacement::EachFacade;
		if (Lower == TEXT("none")) return EGrammarDoorPlacement::None;
		return EGrammarDoorPlacement::StreetFacing; // "first_facade" / "street_facing" / anything else
	}

	FString DoorPlacementToString(EGrammarDoorPlacement Value)
	{
		switch (Value)
		{
		case EGrammarDoorPlacement::EachFacade: return TEXT("each_facade");
		case EGrammarDoorPlacement::None: return TEXT("none");
		default: return TEXT("street_facing");
		}
	}

	EGrammarAntennaType AntennaTypeFromString(const FString& Value)
	{
		const FString Lower = Value.ToLower();
		if (Lower == TEXT("radio")) return EGrammarAntennaType::Radio;
		if (Lower == TEXT("satellite")) return EGrammarAntennaType::Satellite;
		if (Lower == TEXT("lightning_rod")) return EGrammarAntennaType::LightningRod;
		if (Lower == TEXT("cellular")) return EGrammarAntennaType::Cellular;
		if (Lower == TEXT("office_cluster")) return EGrammarAntennaType::OfficeCluster;
		if (Lower == TEXT("broadcast")) return EGrammarAntennaType::Broadcast;
		if (Lower == TEXT("lamp_post")) return EGrammarAntennaType::LampPost;
		return EGrammarAntennaType::Tv;
	}

	FString AntennaTypeToString(EGrammarAntennaType Value)
	{
		switch (Value)
		{
		case EGrammarAntennaType::Radio: return TEXT("radio");
		case EGrammarAntennaType::Satellite: return TEXT("satellite");
		case EGrammarAntennaType::LightningRod: return TEXT("lightning_rod");
		case EGrammarAntennaType::Cellular: return TEXT("cellular");
		case EGrammarAntennaType::OfficeCluster: return TEXT("office_cluster");
		case EGrammarAntennaType::Broadcast: return TEXT("broadcast");
		case EGrammarAntennaType::LampPost: return TEXT("lamp_post");
		default: return TEXT("tv");
		}
	}

	EGrammarRoofType RoofTypeFromString(const FString& Value)
	{
		const FString Lower = Value.ToLower();
		if (Lower == TEXT("gabled")) return EGrammarRoofType::Gabled;
		if (Lower == TEXT("hipped")) return EGrammarRoofType::Hipped;
		if (Lower == TEXT("pyramid") || Lower == TEXT("pyramidal")) return EGrammarRoofType::Pyramid;
		if (Lower == TEXT("gambrel")) return EGrammarRoofType::Gambrel;
		if (Lower == TEXT("mansard")) return EGrammarRoofType::Mansard;
		return EGrammarRoofType::Flat;
	}

	FString RoofTypeToString(EGrammarRoofType Value)
	{
		switch (Value)
		{
		case EGrammarRoofType::Gabled: return TEXT("gabled");
		case EGrammarRoofType::Hipped: return TEXT("hipped");
		case EGrammarRoofType::Pyramid: return TEXT("pyramid");
		case EGrammarRoofType::Gambrel: return TEXT("gambrel");
		case EGrammarRoofType::Mansard: return TEXT("mansard");
		default: return TEXT("flat");
		}
	}

	EGrammarRidgeAlignment RidgeAlignmentFromString(const FString& Value)
	{
		return Value.ToLower() == TEXT("longest_axis") ? EGrammarRidgeAlignment::LongestAxis : EGrammarRidgeAlignment::ClosestStreet;
	}

	FString RidgeAlignmentToString(EGrammarRidgeAlignment Value)
	{
		return Value == EGrammarRidgeAlignment::LongestAxis ? TEXT("longest_axis") : TEXT("closest_street");
	}

	EGrammarWallColorVariantMode WallColorVariantModeFromString(const FString& Value)
	{
		const FString Lower = Value.ToLower();
		if (Lower == TEXT("cycle")) return EGrammarWallColorVariantMode::Cycle;
		if (Lower == TEXT("building")) return EGrammarWallColorVariantMode::Building;
		if (Lower == TEXT("facade")) return EGrammarWallColorVariantMode::Facade;
		return EGrammarWallColorVariantMode::None;
	}

	FString WallColorVariantModeToString(EGrammarWallColorVariantMode Value)
	{
		switch (Value)
		{
		case EGrammarWallColorVariantMode::Cycle: return TEXT("cycle");
		case EGrammarWallColorVariantMode::Building: return TEXT("building");
		case EGrammarWallColorVariantMode::Facade: return TEXT("facade");
		default: return TEXT("none");
		}
	}

	EGrammarWallRowColorMode WallRowColorModeFromString(const FString& Value)
	{
		return Value.ToLower() == TEXT("ground_accent") ? EGrammarWallRowColorMode::GroundAccent : EGrammarWallRowColorMode::Cycle;
	}

	FString WallRowColorModeToString(EGrammarWallRowColorMode Value)
	{
		return Value == EGrammarWallRowColorMode::GroundAccent ? TEXT("ground_accent") : TEXT("cycle");
	}
}

// ---- FWindowStyleConfig ----

FWindowStyleConfig FGrammarConfigJson::WindowFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	FWindowStyleConfig Window;
	Window.Width = GetNum(JsonObject, TEXT("width"), 1.25);
	Window.Height = GetNum(JsonObject, TEXT("height"), 1.55);
	Window.SillHeight = GetNum(JsonObject, TEXT("sill_height"), 0.85);
	Window.Spacing = GetNum(JsonObject, TEXT("spacing"), 2.7);
	Window.MinMargin = GetNum(JsonObject, TEXT("min_margin"), 0.8);
	Window.Depth = GetNum(JsonObject, TEXT("depth"), 0.04);
	Window.RecessDepth = GetNum(JsonObject, TEXT("recess_depth"), 0.10);
	Window.Material = GetStr(JsonObject, TEXT("material"), TEXT("Grammar Glass"));
	Window.Color = GetColor(JsonObject, TEXT("color"), FLinearColor(0.12, 0.22, 0.32, 1.0));
	Window.TexturePath = GetOptionalStr(JsonObject, TEXT("texture_path"));
	Window.TextureScale = GetNum(JsonObject, TEXT("texture_scale"), 1.0);
	Window.FrameWidth = GetNum(JsonObject, TEXT("frame_width"), 0.08);
	Window.FrameDepth = GetNum(JsonObject, TEXT("frame_depth"), 0.03);
	Window.FrameMaterial = GetStr(JsonObject, TEXT("frame_material"), TEXT("Grammar Window Frames"));
	Window.FrameColor = GetColor(JsonObject, TEXT("frame_color"), FLinearColor(0.86, 0.84, 0.78, 1.0));
	Window.VerticalMullions = GetInt(JsonObject, TEXT("vertical_mullions"), 1);
	Window.HorizontalMullions = GetInt(JsonObject, TEXT("horizontal_mullions"), 0);
	Window.SillDepth = GetNum(JsonObject, TEXT("sill_depth"), 0.16);
	Window.SillThickness = GetNum(JsonObject, TEXT("sill_thickness"), 0.06);
	Window.SillMaterial = GetStr(JsonObject, TEXT("sill_material"), TEXT("Grammar Window Sills"));
	Window.SillColor = GetColor(JsonObject, TEXT("sill_color"), FLinearColor(0.78, 0.74, 0.68, 1.0));
	return Window;
}

TSharedRef<FJsonObject> FGrammarConfigJson::WindowToJsonObject(const FWindowStyleConfig& Window)
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("width"), Window.Width);
	Obj->SetNumberField(TEXT("height"), Window.Height);
	Obj->SetNumberField(TEXT("sill_height"), Window.SillHeight);
	Obj->SetNumberField(TEXT("spacing"), Window.Spacing);
	Obj->SetNumberField(TEXT("min_margin"), Window.MinMargin);
	Obj->SetNumberField(TEXT("depth"), Window.Depth);
	Obj->SetNumberField(TEXT("recess_depth"), Window.RecessDepth);
	Obj->SetStringField(TEXT("material"), Window.Material);
	Obj->SetArrayField(TEXT("color"), ColorToJsonArray(Window.Color));
	if (Window.TexturePath.IsEmpty()) Obj->SetField(TEXT("texture_path"), MakeShared<FJsonValueNull>()); else Obj->SetStringField(TEXT("texture_path"), Window.TexturePath);
	Obj->SetNumberField(TEXT("texture_scale"), Window.TextureScale);
	Obj->SetNumberField(TEXT("frame_width"), Window.FrameWidth);
	Obj->SetNumberField(TEXT("frame_depth"), Window.FrameDepth);
	Obj->SetStringField(TEXT("frame_material"), Window.FrameMaterial);
	Obj->SetArrayField(TEXT("frame_color"), ColorToJsonArray(Window.FrameColor));
	Obj->SetNumberField(TEXT("vertical_mullions"), Window.VerticalMullions);
	Obj->SetNumberField(TEXT("horizontal_mullions"), Window.HorizontalMullions);
	Obj->SetNumberField(TEXT("sill_depth"), Window.SillDepth);
	Obj->SetNumberField(TEXT("sill_thickness"), Window.SillThickness);
	Obj->SetStringField(TEXT("sill_material"), Window.SillMaterial);
	Obj->SetArrayField(TEXT("sill_color"), ColorToJsonArray(Window.SillColor));
	return Obj;
}

// ---- FLedgeStyleConfig ----

FLedgeStyleConfig FGrammarConfigJson::LedgeFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	FLedgeStyleConfig Ledge;
	Ledge.bEnabled = GetBool(JsonObject, TEXT("enabled"), true);
	Ledge.Depth = GetNum(JsonObject, TEXT("depth"), 0.16);
	Ledge.Height = GetNum(JsonObject, TEXT("height"), 0.08);
	Ledge.EveryNFloors = GetInt(JsonObject, TEXT("every_n_floors"), 1);
	Ledge.Material = GetStr(JsonObject, TEXT("material"), TEXT("Grammar Ledges"));
	Ledge.Color = GetColor(JsonObject, TEXT("color"), FLinearColor(0.78, 0.74, 0.68, 1.0));
	Ledge.TexturePath = GetOptionalStr(JsonObject, TEXT("texture_path"));
	Ledge.TextureScale = GetNum(JsonObject, TEXT("texture_scale"), 1.0);
	return Ledge;
}

TSharedRef<FJsonObject> FGrammarConfigJson::LedgeToJsonObject(const FLedgeStyleConfig& Ledge)
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("enabled"), Ledge.bEnabled);
	Obj->SetNumberField(TEXT("depth"), Ledge.Depth);
	Obj->SetNumberField(TEXT("height"), Ledge.Height);
	Obj->SetNumberField(TEXT("every_n_floors"), Ledge.EveryNFloors);
	Obj->SetStringField(TEXT("material"), Ledge.Material);
	Obj->SetArrayField(TEXT("color"), ColorToJsonArray(Ledge.Color));
	if (Ledge.TexturePath.IsEmpty()) Obj->SetField(TEXT("texture_path"), MakeShared<FJsonValueNull>()); else Obj->SetStringField(TEXT("texture_path"), Ledge.TexturePath);
	Obj->SetNumberField(TEXT("texture_scale"), Ledge.TextureScale);
	return Obj;
}

// ---- FBalconyStyleConfig ----

FBalconyStyleConfig FGrammarConfigJson::BalconyFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	FBalconyStyleConfig Balcony;
	Balcony.bEnabled = GetBool(JsonObject, TEXT("enabled"), true);
	Balcony.Width = GetNum(JsonObject, TEXT("width"), 1.9);
	Balcony.Depth = GetNum(JsonObject, TEXT("depth"), 0.75);
	Balcony.SlabHeight = GetNum(JsonObject, TEXT("slab_height"), 0.12);
	Balcony.RailingHeight = GetNum(JsonObject, TEXT("railing_height"), 0.9);
	Balcony.EveryNFloors = GetInt(JsonObject, TEXT("every_n_floors"), 2);
	Balcony.Material = GetStr(JsonObject, TEXT("material"), TEXT("Grammar Balconies"));
	Balcony.Color = GetColor(JsonObject, TEXT("color"), FLinearColor(0.58, 0.58, 0.55, 1.0));
	Balcony.TexturePath = GetOptionalStr(JsonObject, TEXT("texture_path"));
	Balcony.TextureScale = GetNum(JsonObject, TEXT("texture_scale"), 1.0);
	Balcony.RailingMaterial = GetStr(JsonObject, TEXT("railing_material"), TEXT("Grammar Balcony Railings"));
	Balcony.RailingColor = GetColor(JsonObject, TEXT("railing_color"), FLinearColor(0.16, 0.16, 0.15, 1.0));
	Balcony.RailingBarCount = GetInt(JsonObject, TEXT("railing_bar_count"), 5);
	Balcony.RailingBarWidth = GetNum(JsonObject, TEXT("railing_bar_width"), 0.04);
	Balcony.RailingBarDepth = GetNum(JsonObject, TEXT("railing_bar_depth"), 0.04);
	return Balcony;
}

TSharedRef<FJsonObject> FGrammarConfigJson::BalconyToJsonObject(const FBalconyStyleConfig& Balcony)
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("enabled"), Balcony.bEnabled);
	Obj->SetNumberField(TEXT("width"), Balcony.Width);
	Obj->SetNumberField(TEXT("depth"), Balcony.Depth);
	Obj->SetNumberField(TEXT("slab_height"), Balcony.SlabHeight);
	Obj->SetNumberField(TEXT("railing_height"), Balcony.RailingHeight);
	Obj->SetNumberField(TEXT("every_n_floors"), Balcony.EveryNFloors);
	Obj->SetStringField(TEXT("material"), Balcony.Material);
	Obj->SetArrayField(TEXT("color"), ColorToJsonArray(Balcony.Color));
	if (Balcony.TexturePath.IsEmpty()) Obj->SetField(TEXT("texture_path"), MakeShared<FJsonValueNull>()); else Obj->SetStringField(TEXT("texture_path"), Balcony.TexturePath);
	Obj->SetNumberField(TEXT("texture_scale"), Balcony.TextureScale);
	Obj->SetStringField(TEXT("railing_material"), Balcony.RailingMaterial);
	Obj->SetArrayField(TEXT("railing_color"), ColorToJsonArray(Balcony.RailingColor));
	Obj->SetNumberField(TEXT("railing_bar_count"), Balcony.RailingBarCount);
	Obj->SetNumberField(TEXT("railing_bar_width"), Balcony.RailingBarWidth);
	Obj->SetNumberField(TEXT("railing_bar_depth"), Balcony.RailingBarDepth);
	return Obj;
}

// ---- FDoorStyleConfig ----

FDoorStyleConfig FGrammarConfigJson::DoorFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	FDoorStyleConfig Door;
	Door.bEnabled = GetBool(JsonObject, TEXT("enabled"), true);
	Door.Placement = DoorPlacementFromString(GetStr(JsonObject, TEXT("placement"), TEXT("first_facade")));
	Door.Width = GetNum(JsonObject, TEXT("width"), 1.25);
	Door.Height = GetNum(JsonObject, TEXT("height"), 2.25);
	Door.Depth = GetNum(JsonObject, TEXT("depth"), 0.08);
	Door.RecessDepth = GetNum(JsonObject, TEXT("recess_depth"), 0.05);
	Door.Material = GetStr(JsonObject, TEXT("material"), TEXT("Grammar Door"));
	Door.Color = GetColor(JsonObject, TEXT("color"), FLinearColor(0.16, 0.1, 0.06, 1.0));
	Door.TexturePath = GetOptionalStr(JsonObject, TEXT("texture_path"));
	Door.TextureScale = GetNum(JsonObject, TEXT("texture_scale"), 1.0);
	Door.FrameWidth = GetNum(JsonObject, TEXT("frame_width"), 0.12);
	Door.FrameDepth = GetNum(JsonObject, TEXT("frame_depth"), 0.04);
	Door.FrameMaterial = GetStr(JsonObject, TEXT("frame_material"), TEXT("Grammar Door Frames"));
	Door.FrameColor = GetColor(JsonObject, TEXT("frame_color"), FLinearColor(0.72, 0.68, 0.6, 1.0));
	Door.bHandleEnabled = GetBool(JsonObject, TEXT("handle_enabled"), true);
	Door.HandleRadius = GetNum(JsonObject, TEXT("handle_radius"), 0.05);
	Door.HandleMaterial = GetStr(JsonObject, TEXT("handle_material"), TEXT("Grammar Door Handles"));
	Door.HandleColor = GetColor(JsonObject, TEXT("handle_color"), FLinearColor(0.82, 0.66, 0.32, 1.0));
	Door.bCanopyEnabled = GetBool(JsonObject, TEXT("canopy_enabled"), false);
	Door.CanopyWidth = GetNum(JsonObject, TEXT("canopy_width"), 1.8);
	Door.CanopyDepth = GetNum(JsonObject, TEXT("canopy_depth"), 0.75);
	Door.CanopyThickness = GetNum(JsonObject, TEXT("canopy_thickness"), 0.08);
	Door.CanopyMaterial = GetStr(JsonObject, TEXT("canopy_material"), TEXT("Grammar Door Canopies"));
	Door.CanopyColor = GetColor(JsonObject, TEXT("canopy_color"), FLinearColor(0.36, 0.36, 0.34, 1.0));
	return Door;
}

TSharedRef<FJsonObject> FGrammarConfigJson::DoorToJsonObject(const FDoorStyleConfig& Door)
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("enabled"), Door.bEnabled);
	Obj->SetStringField(TEXT("placement"), DoorPlacementToString(Door.Placement));
	Obj->SetNumberField(TEXT("width"), Door.Width);
	Obj->SetNumberField(TEXT("height"), Door.Height);
	Obj->SetNumberField(TEXT("depth"), Door.Depth);
	Obj->SetNumberField(TEXT("recess_depth"), Door.RecessDepth);
	Obj->SetStringField(TEXT("material"), Door.Material);
	Obj->SetArrayField(TEXT("color"), ColorToJsonArray(Door.Color));
	if (Door.TexturePath.IsEmpty()) Obj->SetField(TEXT("texture_path"), MakeShared<FJsonValueNull>()); else Obj->SetStringField(TEXT("texture_path"), Door.TexturePath);
	Obj->SetNumberField(TEXT("texture_scale"), Door.TextureScale);
	Obj->SetNumberField(TEXT("frame_width"), Door.FrameWidth);
	Obj->SetNumberField(TEXT("frame_depth"), Door.FrameDepth);
	Obj->SetStringField(TEXT("frame_material"), Door.FrameMaterial);
	Obj->SetArrayField(TEXT("frame_color"), ColorToJsonArray(Door.FrameColor));
	Obj->SetBoolField(TEXT("handle_enabled"), Door.bHandleEnabled);
	Obj->SetNumberField(TEXT("handle_radius"), Door.HandleRadius);
	Obj->SetStringField(TEXT("handle_material"), Door.HandleMaterial);
	Obj->SetArrayField(TEXT("handle_color"), ColorToJsonArray(Door.HandleColor));
	Obj->SetBoolField(TEXT("canopy_enabled"), Door.bCanopyEnabled);
	Obj->SetNumberField(TEXT("canopy_width"), Door.CanopyWidth);
	Obj->SetNumberField(TEXT("canopy_depth"), Door.CanopyDepth);
	Obj->SetNumberField(TEXT("canopy_thickness"), Door.CanopyThickness);
	Obj->SetStringField(TEXT("canopy_material"), Door.CanopyMaterial);
	Obj->SetArrayField(TEXT("canopy_color"), ColorToJsonArray(Door.CanopyColor));
	return Obj;
}

// ---- FAntennaStyleConfig ----

FAntennaStyleConfig FGrammarConfigJson::AntennaFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	FAntennaStyleConfig Antenna;
	Antenna.bEnabled = GetBool(JsonObject, TEXT("enabled"), true);
	Antenna.Type = AntennaTypeFromString(GetStr(JsonObject, TEXT("type"), TEXT("tv")));
	Antenna.Count = GetInt(JsonObject, TEXT("count"), 1);
	Antenna.MastHeight = GetNum(JsonObject, TEXT("mast_height"), 1.4);
	Antenna.MastRadius = GetNum(JsonObject, TEXT("mast_radius"), 0.035);
	Antenna.BaseWidth = GetNum(JsonObject, TEXT("base_width"), 0.45);
	Antenna.BaseDepth = GetNum(JsonObject, TEXT("base_depth"), 0.45);
	Antenna.BaseHeight = GetNum(JsonObject, TEXT("base_height"), 0.18);
	Antenna.PanelWidth = GetNum(JsonObject, TEXT("panel_width"), 0.35);
	Antenna.PanelHeight = GetNum(JsonObject, TEXT("panel_height"), 0.7);
	Antenna.PanelDepth = GetNum(JsonObject, TEXT("panel_depth"), 0.06);
	Antenna.Material = GetStr(JsonObject, TEXT("material"), TEXT("Grammar Antenna Metal"));
	Antenna.Color = GetColor(JsonObject, TEXT("color"), FLinearColor(0.34, 0.34, 0.33, 1.0));
	Antenna.AccentMaterial = GetStr(JsonObject, TEXT("accent_material"), TEXT("Grammar Antenna Panels"));
	Antenna.AccentColor = GetColor(JsonObject, TEXT("accent_color"), FLinearColor(0.78, 0.78, 0.72, 1.0));
	return Antenna;
}

TSharedRef<FJsonObject> FGrammarConfigJson::AntennaToJsonObject(const FAntennaStyleConfig& Antenna)
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("enabled"), Antenna.bEnabled);
	Obj->SetStringField(TEXT("type"), AntennaTypeToString(Antenna.Type));
	Obj->SetNumberField(TEXT("count"), Antenna.Count);
	Obj->SetNumberField(TEXT("mast_height"), Antenna.MastHeight);
	Obj->SetNumberField(TEXT("mast_radius"), Antenna.MastRadius);
	Obj->SetNumberField(TEXT("base_width"), Antenna.BaseWidth);
	Obj->SetNumberField(TEXT("base_depth"), Antenna.BaseDepth);
	Obj->SetNumberField(TEXT("base_height"), Antenna.BaseHeight);
	Obj->SetNumberField(TEXT("panel_width"), Antenna.PanelWidth);
	Obj->SetNumberField(TEXT("panel_height"), Antenna.PanelHeight);
	Obj->SetNumberField(TEXT("panel_depth"), Antenna.PanelDepth);
	Obj->SetStringField(TEXT("material"), Antenna.Material);
	Obj->SetArrayField(TEXT("color"), ColorToJsonArray(Antenna.Color));
	Obj->SetStringField(TEXT("accent_material"), Antenna.AccentMaterial);
	Obj->SetArrayField(TEXT("accent_color"), ColorToJsonArray(Antenna.AccentColor));
	return Obj;
}

// ---- FRoofStyleConfig ----

FRoofStyleConfig FGrammarConfigJson::RoofFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	FRoofStyleConfig Roof;
	Roof.Type = RoofTypeFromString(GetStr(JsonObject, TEXT("type"), TEXT("flat")));
	Roof.Height = GetNum(JsonObject, TEXT("height"), 1.6);
	Roof.Overhang = GetNum(JsonObject, TEXT("overhang"), 0.25);
	Roof.RidgeAlignment = RidgeAlignmentFromString(GetStr(JsonObject, TEXT("ridge_alignment"), TEXT("closest_street")));
	Roof.Material = GetStr(JsonObject, TEXT("material"), TEXT("Grammar Roof"));
	Roof.Color = GetColor(JsonObject, TEXT("color"), FLinearColor(0.34, 0.08, 0.06, 1.0));
	Roof.TexturePath = GetOptionalStr(JsonObject, TEXT("texture_path"));
	Roof.TextureScale = GetNum(JsonObject, TEXT("texture_scale"), 1.0);
	Roof.bEdgeEnabled = GetBool(JsonObject, TEXT("edge_enabled"), true);
	Roof.EdgeWidth = GetNum(JsonObject, TEXT("edge_width"), 0.28);
	Roof.EdgeHeight = GetNum(JsonObject, TEXT("edge_height"), 0.35);
	Roof.SurfaceInset = GetNum(JsonObject, TEXT("surface_inset"), 0.08);
	Roof.EdgeMaterial = GetStr(JsonObject, TEXT("edge_material"), TEXT("Grammar Roof Edge"));
	Roof.EdgeColor = GetColor(JsonObject, TEXT("edge_color"), FLinearColor(0.22, 0.22, 0.2, 1.0));
	Roof.CornerCapSize = GetNum(JsonObject, TEXT("corner_cap_size"), 0.42);
	Roof.TileRows = GetInt(JsonObject, TEXT("tile_rows"), 6);
	Roof.TileDepth = GetNum(JsonObject, TEXT("tile_depth"), 0.035);
	Roof.TileSpacing = GetNum(JsonObject, TEXT("tile_spacing"), 0.55);
	Roof.TileMaterial = GetStr(JsonObject, TEXT("tile_material"), TEXT("Grammar Roof Tile Bands"));
	Roof.TileColor = GetColor(JsonObject, TEXT("tile_color"), FLinearColor(0.28, 0.07, 0.045, 1.0));
	Roof.DormerCount = GetInt(JsonObject, TEXT("dormer_count"), 0);
	Roof.DormerWidth = GetNum(JsonObject, TEXT("dormer_width"), 1.35);
	Roof.DormerDepth = GetNum(JsonObject, TEXT("dormer_depth"), 0.9);
	Roof.DormerHeight = GetNum(JsonObject, TEXT("dormer_height"), 0.9);
	Roof.DormerMaterial = GetStr(JsonObject, TEXT("dormer_material"), TEXT("Grammar Dormer Cladding"));
	Roof.DormerColor = GetColor(JsonObject, TEXT("dormer_color"), FLinearColor(0.62, 0.58, 0.5, 1.0));
	Roof.RoofWindowCount = GetInt(JsonObject, TEXT("roof_window_count"), 0);
	Roof.RoofWindowWidth = GetNum(JsonObject, TEXT("roof_window_width"), 0.75);
	Roof.RoofWindowHeight = GetNum(JsonObject, TEXT("roof_window_height"), 1.05);
	Roof.RoofWindowMaterial = GetStr(JsonObject, TEXT("roof_window_material"), TEXT("Grammar Roof Window Glass"));
	Roof.RoofWindowColor = GetColor(JsonObject, TEXT("roof_window_color"), FLinearColor(0.08, 0.16, 0.2, 0.86));
	Roof.ChimneyCount = GetInt(JsonObject, TEXT("chimney_count"), 0);
	Roof.ChimneyWidth = GetNum(JsonObject, TEXT("chimney_width"), 0.45);
	Roof.ChimneyDepth = GetNum(JsonObject, TEXT("chimney_depth"), 0.38);
	Roof.ChimneyHeight = GetNum(JsonObject, TEXT("chimney_height"), 1.15);
	Roof.ChimneyMaterial = GetStr(JsonObject, TEXT("chimney_material"), TEXT("Grammar Brick Chimney"));
	Roof.ChimneyColor = GetColor(JsonObject, TEXT("chimney_color"), FLinearColor(0.42, 0.16, 0.1, 1.0));
	return Roof;
}

TSharedRef<FJsonObject> FGrammarConfigJson::RoofToJsonObject(const FRoofStyleConfig& Roof)
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("type"), RoofTypeToString(Roof.Type));
	Obj->SetNumberField(TEXT("height"), Roof.Height);
	Obj->SetNumberField(TEXT("overhang"), Roof.Overhang);
	Obj->SetStringField(TEXT("ridge_alignment"), RidgeAlignmentToString(Roof.RidgeAlignment));
	Obj->SetStringField(TEXT("material"), Roof.Material);
	Obj->SetArrayField(TEXT("color"), ColorToJsonArray(Roof.Color));
	if (Roof.TexturePath.IsEmpty()) Obj->SetField(TEXT("texture_path"), MakeShared<FJsonValueNull>()); else Obj->SetStringField(TEXT("texture_path"), Roof.TexturePath);
	Obj->SetNumberField(TEXT("texture_scale"), Roof.TextureScale);
	Obj->SetBoolField(TEXT("edge_enabled"), Roof.bEdgeEnabled);
	Obj->SetNumberField(TEXT("edge_width"), Roof.EdgeWidth);
	Obj->SetNumberField(TEXT("edge_height"), Roof.EdgeHeight);
	Obj->SetNumberField(TEXT("surface_inset"), Roof.SurfaceInset);
	Obj->SetStringField(TEXT("edge_material"), Roof.EdgeMaterial);
	Obj->SetArrayField(TEXT("edge_color"), ColorToJsonArray(Roof.EdgeColor));
	Obj->SetNumberField(TEXT("corner_cap_size"), Roof.CornerCapSize);
	Obj->SetNumberField(TEXT("tile_rows"), Roof.TileRows);
	Obj->SetNumberField(TEXT("tile_depth"), Roof.TileDepth);
	Obj->SetNumberField(TEXT("tile_spacing"), Roof.TileSpacing);
	Obj->SetStringField(TEXT("tile_material"), Roof.TileMaterial);
	Obj->SetArrayField(TEXT("tile_color"), ColorToJsonArray(Roof.TileColor));
	Obj->SetNumberField(TEXT("dormer_count"), Roof.DormerCount);
	Obj->SetNumberField(TEXT("dormer_width"), Roof.DormerWidth);
	Obj->SetNumberField(TEXT("dormer_depth"), Roof.DormerDepth);
	Obj->SetNumberField(TEXT("dormer_height"), Roof.DormerHeight);
	Obj->SetStringField(TEXT("dormer_material"), Roof.DormerMaterial);
	Obj->SetArrayField(TEXT("dormer_color"), ColorToJsonArray(Roof.DormerColor));
	Obj->SetNumberField(TEXT("roof_window_count"), Roof.RoofWindowCount);
	Obj->SetNumberField(TEXT("roof_window_width"), Roof.RoofWindowWidth);
	Obj->SetNumberField(TEXT("roof_window_height"), Roof.RoofWindowHeight);
	Obj->SetStringField(TEXT("roof_window_material"), Roof.RoofWindowMaterial);
	Obj->SetArrayField(TEXT("roof_window_color"), ColorToJsonArray(Roof.RoofWindowColor));
	Obj->SetNumberField(TEXT("chimney_count"), Roof.ChimneyCount);
	Obj->SetNumberField(TEXT("chimney_width"), Roof.ChimneyWidth);
	Obj->SetNumberField(TEXT("chimney_depth"), Roof.ChimneyDepth);
	Obj->SetNumberField(TEXT("chimney_height"), Roof.ChimneyHeight);
	Obj->SetStringField(TEXT("chimney_material"), Roof.ChimneyMaterial);
	Obj->SetArrayField(TEXT("chimney_color"), ColorToJsonArray(Roof.ChimneyColor));
	return Obj;
}

// ---- FFacadeStyleConfig ----

FFacadeStyleConfig FGrammarConfigJson::StyleFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	FFacadeStyleConfig Style;
	if (!JsonObject.IsValid())
	{
		return Style;
	}

	Style.Name = GetStr(JsonObject, TEXT("name"), TEXT("default"));
	Style.BuildingValues = GetStringList(JsonObject, TEXT("building_values"));

	const TSharedPtr<FJsonObject>* TagFiltersObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("tag_filters"), TagFiltersObject) && TagFiltersObject && TagFiltersObject->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*TagFiltersObject)->Values)
		{
			FGrammarStringList List;
			List.Values = StringListFromValue(Pair.Value);
			Style.TagFilters.Add(Pair.Key, List);
		}
	}

	int32 Levels = 0;
	Style.bHasDefaultLevels = JsonObject->TryGetNumberField(TEXT("default_levels"), Levels);
	if (Style.bHasDefaultLevels)
	{
		Style.DefaultLevels = Levels;
	}

	double FloorHeight = 0.0;
	Style.bHasDefaultFloorHeight = JsonObject->TryGetNumberField(TEXT("default_floor_height"), FloorHeight);
	if (Style.bHasDefaultFloorHeight)
	{
		Style.DefaultFloorHeight = FloorHeight;
	}

	const TSharedPtr<FJsonObject>* RoofObject = nullptr;
	Style.bOverrideRoof = JsonObject->TryGetObjectField(TEXT("roof"), RoofObject)
		&& RoofObject && RoofObject->IsValid() && (*RoofObject)->Values.Num() > 0;
	if (Style.bOverrideRoof)
	{
		Style.RoofOverride = RoofFromJsonObject(*RoofObject);
	}

	Style.WallMaterial = GetStr(JsonObject, TEXT("wall_material"), TEXT("Grammar Facade"));
	Style.WallColor = GetColor(JsonObject, TEXT("wall_color"), FLinearColor(0.72, 0.68, 0.6, 1.0));
	Style.WallTexturePath = GetOptionalStr(JsonObject, TEXT("wall_texture_path"));
	Style.WallTextureScale = GetNum(JsonObject, TEXT("wall_texture_scale"), 1.0);
	Style.WallColorVariants = GetColorList(JsonObject, TEXT("wall_color_variants"));
	Style.WallColorVariantMode = WallColorVariantModeFromString(GetStr(JsonObject, TEXT("wall_color_variant_mode"), TEXT("none")));
	Style.WallRowColors = GetColorList(JsonObject, TEXT("wall_row_colors"));
	Style.WallRowColorMode = WallRowColorModeFromString(GetStr(JsonObject, TEXT("wall_row_color_mode"), TEXT("cycle")));

	const TSharedPtr<FJsonObject>* WindowObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("window"), WindowObject) && WindowObject)
	{
		Style.Window = WindowFromJsonObject(*WindowObject);
	}
	const TSharedPtr<FJsonObject>* LedgeObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("ledge"), LedgeObject) && LedgeObject)
	{
		Style.Ledge = LedgeFromJsonObject(*LedgeObject);
	}
	const TSharedPtr<FJsonObject>* BalconyObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("balcony"), BalconyObject) && BalconyObject)
	{
		Style.Balcony = BalconyFromJsonObject(*BalconyObject);
	}
	const TSharedPtr<FJsonObject>* DoorObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("door"), DoorObject) && DoorObject)
	{
		Style.Door = DoorFromJsonObject(*DoorObject);
	}
	const TSharedPtr<FJsonObject>* AntennaObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("antenna"), AntennaObject) && AntennaObject)
	{
		Style.Antenna = AntennaFromJsonObject(*AntennaObject);
	}

	return Style;
}

TSharedRef<FJsonObject> FGrammarConfigJson::StyleToJsonObject(const FFacadeStyleConfig& Style)
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("name"), Style.Name);
	Obj->SetArrayField(TEXT("building_values"), StringListToJsonArray(Style.BuildingValues));

	TSharedRef<FJsonObject> TagFiltersObject = MakeShared<FJsonObject>();
	for (const TPair<FString, FGrammarStringList>& Pair : Style.TagFilters)
	{
		TagFiltersObject->SetArrayField(Pair.Key, StringListToJsonArray(Pair.Value.Values));
	}
	Obj->SetObjectField(TEXT("tag_filters"), TagFiltersObject);

	if (Style.bHasDefaultLevels) Obj->SetNumberField(TEXT("default_levels"), Style.DefaultLevels); else Obj->SetField(TEXT("default_levels"), MakeShared<FJsonValueNull>());
	if (Style.bHasDefaultFloorHeight) Obj->SetNumberField(TEXT("default_floor_height"), Style.DefaultFloorHeight); else Obj->SetField(TEXT("default_floor_height"), MakeShared<FJsonValueNull>());
	if (Style.bOverrideRoof) Obj->SetObjectField(TEXT("roof"), RoofToJsonObject(Style.RoofOverride)); else Obj->SetField(TEXT("roof"), MakeShared<FJsonValueNull>());

	Obj->SetStringField(TEXT("wall_material"), Style.WallMaterial);
	Obj->SetArrayField(TEXT("wall_color"), ColorToJsonArray(Style.WallColor));
	if (Style.WallTexturePath.IsEmpty()) Obj->SetField(TEXT("wall_texture_path"), MakeShared<FJsonValueNull>()); else Obj->SetStringField(TEXT("wall_texture_path"), Style.WallTexturePath);
	Obj->SetNumberField(TEXT("wall_texture_scale"), Style.WallTextureScale);
	Obj->SetArrayField(TEXT("wall_color_variants"), ColorListToJsonArray(Style.WallColorVariants));
	Obj->SetStringField(TEXT("wall_color_variant_mode"), WallColorVariantModeToString(Style.WallColorVariantMode));
	Obj->SetArrayField(TEXT("wall_row_colors"), ColorListToJsonArray(Style.WallRowColors));
	Obj->SetStringField(TEXT("wall_row_color_mode"), WallRowColorModeToString(Style.WallRowColorMode));

	Obj->SetObjectField(TEXT("window"), WindowToJsonObject(Style.Window));
	Obj->SetObjectField(TEXT("ledge"), LedgeToJsonObject(Style.Ledge));
	Obj->SetObjectField(TEXT("balcony"), BalconyToJsonObject(Style.Balcony));
	Obj->SetObjectField(TEXT("door"), DoorToJsonObject(Style.Door));
	Obj->SetObjectField(TEXT("antenna"), AntennaToJsonObject(Style.Antenna));

	return Obj;
}

// ---- FBuildingGrammarConfig (root) ----

FBuildingGrammarConfig FGrammarConfigJson::ConfigFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	FBuildingGrammarConfig Config;
	if (!JsonObject.IsValid())
	{
		return Config;
	}

	// root_collection / source_collection / include_selected_only / replace_existing: read-and-
	// discard -- see this class's header comment.

	Config.bEnableBuildingParts = GetBool(JsonObject, TEXT("enable_building_parts"), true);
	Config.bSkipParentFootprintsWithParts = GetBool(JsonObject, TEXT("skip_parent_footprints_with_parts"), true);
	Config.bInheritParentTagsForParts = GetBool(JsonObject, TEXT("inherit_parent_tags_for_parts"), true);
	Config.BuildingPartMatchTolerance = GetNum(JsonObject, TEXT("building_part_match_tolerance"), 0.25);
	Config.bUseMeshInstancing = GetBool(JsonObject, TEXT("use_mesh_instancing"), true);
	Config.bBatchGeneratedMeshes = GetBool(JsonObject, TEXT("batch_generated_meshes"), true);

	const TArray<FString> BatchRoles = GetStringList(JsonObject, TEXT("batch_roles"));
	Config.BatchRoles = BatchRoles.Num() > 0 ? BatchRoles : FBuildingGrammarConfig::DefaultBatchRoles();

	Config.DefaultLevels = GetInt(JsonObject, TEXT("default_levels"), 4);
	Config.DefaultFloorHeight = GetNum(JsonObject, TEXT("default_floor_height"), 3.1);

	Config.IrregularFloorHeights.Empty();
	const TSharedPtr<FJsonObject>* IrregularObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("irregular_floor_heights"), IrregularObject) && IrregularObject && IrregularObject->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*IrregularObject)->Values)
		{
			double Value = 0.0;
			if (Pair.Value.IsValid() && Pair.Value->TryGetNumber(Value))
			{
				Config.IrregularFloorHeights.Add(FCString::Atoi(*Pair.Key), Value);
			}
		}
	}

	const TArray<FString> ExcludedValues = GetStringList(JsonObject, TEXT("excluded_building_values"));
	Config.ExcludedBuildingValues = ExcludedValues.Num() > 0 ? ExcludedValues : TArray<FString>{ TEXT("shelter") };

	Config.Styles.Empty();
	const TArray<TSharedPtr<FJsonValue>>* StylesArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("styles"), StylesArray) && StylesArray)
	{
		for (const TSharedPtr<FJsonValue>& Item : *StylesArray)
		{
			const TSharedPtr<FJsonObject>* StyleObject = nullptr;
			if (Item.IsValid() && Item->TryGetObject(StyleObject) && StyleObject)
			{
				Config.Styles.Add(StyleFromJsonObject(*StyleObject));
			}
		}
	}
	if (Config.Styles.Num() == 0)
	{
		Config.Styles.Add(FFacadeStyleConfig());
	}

	const TSharedPtr<FJsonObject>* RoofObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("roof"), RoofObject) && RoofObject)
	{
		Config.Roof = RoofFromJsonObject(*RoofObject);
	}

	Config.RoofStreetAlignmentSearchRadius = GetNum(JsonObject, TEXT("roof_street_alignment_search_radius"), 80.0);

	return Config;
}

TSharedRef<FJsonObject> FGrammarConfigJson::ConfigToJsonObject(const FBuildingGrammarConfig& Config)
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetBoolField(TEXT("enable_building_parts"), Config.bEnableBuildingParts);
	Obj->SetBoolField(TEXT("skip_parent_footprints_with_parts"), Config.bSkipParentFootprintsWithParts);
	Obj->SetBoolField(TEXT("inherit_parent_tags_for_parts"), Config.bInheritParentTagsForParts);
	Obj->SetNumberField(TEXT("building_part_match_tolerance"), Config.BuildingPartMatchTolerance);
	Obj->SetBoolField(TEXT("use_mesh_instancing"), Config.bUseMeshInstancing);
	Obj->SetBoolField(TEXT("batch_generated_meshes"), Config.bBatchGeneratedMeshes);
	Obj->SetArrayField(TEXT("batch_roles"), StringListToJsonArray(Config.BatchRoles));
	Obj->SetNumberField(TEXT("default_levels"), Config.DefaultLevels);
	Obj->SetNumberField(TEXT("default_floor_height"), Config.DefaultFloorHeight);

	TSharedRef<FJsonObject> IrregularObject = MakeShared<FJsonObject>();
	for (const TPair<int32, double>& Pair : Config.IrregularFloorHeights)
	{
		IrregularObject->SetNumberField(FString::FromInt(Pair.Key), Pair.Value);
	}
	Obj->SetObjectField(TEXT("irregular_floor_heights"), IrregularObject);

	Obj->SetArrayField(TEXT("excluded_building_values"), StringListToJsonArray(Config.ExcludedBuildingValues));

	TArray<TSharedPtr<FJsonValue>> StylesArray;
	for (const FFacadeStyleConfig& Style : Config.Styles)
	{
		StylesArray.Add(MakeShared<FJsonValueObject>(StyleToJsonObject(Style)));
	}
	Obj->SetArrayField(TEXT("styles"), StylesArray);

	Obj->SetObjectField(TEXT("roof"), RoofToJsonObject(Config.Roof));
	Obj->SetNumberField(TEXT("roof_street_alignment_search_radius"), Config.RoofStreetAlignmentSearchRadius);

	return Obj;
}

// ---- Public entry points ----

bool FGrammarConfigJson::LoadConfigFromPythonJsonString(const FString& JsonString, FBuildingGrammarConfig& OutConfig, FString& OutError)
{
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OutError = TEXT("Failed to parse JSON");
		return false;
	}
	OutConfig = ConfigFromJsonObject(JsonObject);
	return true;
}

bool FGrammarConfigJson::LoadConfigFromPythonJsonFile(const FString& FilePath, FBuildingGrammarConfig& OutConfig, FString& OutError)
{
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *FilePath))
	{
		OutError = FString::Printf(TEXT("Failed to read file '%s'"), *FilePath);
		return false;
	}
	return LoadConfigFromPythonJsonString(FileContents, OutConfig, OutError);
}

FString FGrammarConfigJson::SaveConfigToPythonJsonString(const FBuildingGrammarConfig& Config)
{
	const TSharedRef<FJsonObject> JsonObject = ConfigToJsonObject(Config);
	FString Output;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject, Writer);
	return Output;
}

bool FGrammarConfigJson::SaveConfigToPythonJsonFile(const FString& FilePath, const FBuildingGrammarConfig& Config, FString& OutError)
{
	const FString JsonString = SaveConfigToPythonJsonString(Config);
	if (!FFileHelper::SaveStringToFile(JsonString, *FilePath))
	{
		OutError = FString::Printf(TEXT("Failed to write file '%s'"), *FilePath);
		return false;
	}
	return true;
}
