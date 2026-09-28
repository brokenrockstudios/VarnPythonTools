// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

struct FVarnPythonBrowserScript
{
	FString Filename;
	FString DisplayName;
	FString Source;
	FString RelativePath;
};

using FVarnPythonBrowserScriptPtr = TSharedPtr<FVarnPythonBrowserScript>;

class SVarnPythonBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVarnPythonBrowser)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
private:
	void RefreshScripts();
	void FilterScripts();
	void OnSearchChanged(const FText& Text);
	TSharedRef<ITableRow> GenerateRow(FVarnPythonBrowserScriptPtr Script, const TSharedRef<STableViewBase>& Owner);
	FReply RunScript(FVarnPythonBrowserScriptPtr Script);
	FReply OnRefreshClicked();
	FReply OnSettingsClicked();
	bool CanRunScripts() const;
	FText GetSummary() const;

	TArray<FVarnPythonBrowserScriptPtr> AllScripts;
	TArray<FVarnPythonBrowserScriptPtr> FilteredScripts;
	TSharedPtr<SListView<FVarnPythonBrowserScriptPtr>> ScriptList;
	FString SearchText;
	FText RootsTooltip;
	FText LastResult;
	bool bRunning = false;
};
