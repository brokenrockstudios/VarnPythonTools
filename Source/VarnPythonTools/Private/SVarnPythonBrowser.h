// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VarnPythonScriptTree.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

struct FVarnPythonBrowserScript
{
	FString Filename;
	FString DisplayName;
	// Label of the root folder the script was found under.
	FString Source;
	FString RootPath;
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
using FVarnPythonBrowserNodePtr = VarnPythonTools::FScriptTreeNodePtr;

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
	TSharedRef<ITableRow> GenerateRow(FVarnPythonBrowserNodePtr Node, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<ITableRow> GenerateFolderRow(FVarnPythonBrowserNodePtr Node, const TSharedRef<STableViewBase>& Owner);
	void GetNodeChildren(FVarnPythonBrowserNodePtr Node, TArray<FVarnPythonBrowserNodePtr>& OutChildren);
	void OnExpansionChanged(FVarnPythonBrowserNodePtr Node, bool bExpanded);
	// The script a file node stands for; null for roots and folders.
	FVarnPythonBrowserScriptPtr FindScript(const FVarnPythonBrowserNodePtr& Node) const;
	TSharedPtr<SWidget> OnContextMenuOpening();
	void BeginEditArguments(FVarnPythonBrowserScriptPtr Script);
	void OnArgumentsCommitted(const FText& Text, ETextCommit::Type CommitType, FVarnPythonBrowserScriptPtr Script);
	FReply RunScript(FVarnPythonBrowserScriptPtr Script);
	FReply OnRefreshClicked();
	void ToggleFolderView();
	bool CanRunScripts() const;
	FText GetSummary() const;

	TArray<FVarnPythonBrowserScriptPtr> AllScripts;
	// AllScripts by PathKey of the filename, to get from a tree node back to its script.
	TMap<FString, FVarnPythonBrowserScriptPtr> ScriptsByKey;
	TArray<FVarnPythonBrowserNodePtr> RootNodes;
	TSharedPtr<STreeView<FVarnPythonBrowserNodePtr>> Tree;
	VarnPythonTools::FScriptTreeExpansion Expansion;
	FVarnPythonBrowserScriptPtr EditingScript;
	FText ArgumentsEditBuffer;
	FString SearchText;
	FText RootsTooltip;
	FText LastResult;
	int32 NumShownScripts = 0;
	// Folder hierarchy when true, a flat list of every script when false.
	bool bShowHierarchy = false;
	bool bRunning = false;
};
