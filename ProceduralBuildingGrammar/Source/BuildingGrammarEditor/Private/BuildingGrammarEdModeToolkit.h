#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"

class FBuildingGrammarEdModeToolkit : public FModeToolkit
{
public:
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost) override;
	virtual FName GetToolkitFName() const override { return FName(TEXT("BuildingGrammarEdMode")); }
	virtual FText GetBaseToolkitName() const override;
	virtual FEdMode* GetEditorMode() const override;
	virtual TSharedPtr<SWidget> GetInlineContent() const override { return ToolkitWidget; }

private:
	TSharedPtr<SWidget> ToolkitWidget;
};
