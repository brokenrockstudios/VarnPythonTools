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
	// Module docstring, read lazily the first time the info tooltip is shown.
	FString Description;
	bool bDescriptionLoaded = false;
	// Command-line arguments passed to the script (sys.argv[1:]). Persisted in UVarnPythonToolsSettings.
	FString Arguments;
	// The inline arguments editor for this script's row, if one has been generated.
	TWeakPtr<SWidget> ArgumentsEditor;
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
	TSharedPtr<SWidget> OnContextMenuOpening();
	void BeginEditArguments(FVarnPythonBrowserScriptPtr Script);
	void OnArgumentsCommitted(const FText& Text, ETextCommit::Type CommitType, FVarnPythonBrowserScriptPtr Script);
	FReply RunScript(FVarnPythonBrowserScriptPtr Script);
	FReply OnRefreshClicked();
	FReply OnSettingsClicked();
	bool CanRunScripts() const;
	FText GetSummary() const;

	TArray<FVarnPythonBrowserScriptPtr> AllScripts;
	TArray<FVarnPythonBrowserScriptPtr> FilteredScripts;
	TSharedPtr<SListView<FVarnPythonBrowserScriptPtr>> ScriptList;
	FVarnPythonBrowserScriptPtr EditingScript;
	FText ArgumentsEditBuffer;
	FString SearchText;
	FText RootsTooltip;
	FText LastResult;
	bool bRunning = false;
};
