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
	// Saved command-line argument presets (sys.argv[1:]), each shown as a run entry under the script.
	// Persisted in UVarnPythonToolsSettings.
	TArray<FString> ArgumentSets;
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
	TSharedRef<ITableRow> GenerateArgumentsRow(FVarnPythonBrowserNodePtr Node, const TSharedRef<STableViewBase>& Owner);
	// Adds an Arguments child under every script node in Nodes.
	void AttachArgumentNodes(const TArray<FVarnPythonBrowserNodePtr>& Nodes);
	void GetNodeChildren(FVarnPythonBrowserNodePtr Node, TArray<FVarnPythonBrowserNodePtr>& OutChildren);
	void OnExpansionChanged(FVarnPythonBrowserNodePtr Node, bool bExpanded);
	void OnNodeDoubleClicked(FVarnPythonBrowserNodePtr Node);
	// The script a file or arguments node stands for; null for roots and folders.
	FVarnPythonBrowserScriptPtr FindScript(const FVarnPythonBrowserNodePtr& Node) const;
	TSharedPtr<SWidget> OnContextMenuOpening();
	void AddArguments(FVarnPythonBrowserScriptPtr Script);
	void DuplicateArguments(FVarnPythonBrowserScriptPtr Script, int32 Index);
	void DeleteArguments(FVarnPythonBrowserScriptPtr Script, int32 Index);
	void BeginEditArguments(FVarnPythonBrowserScriptPtr Script, int32 Index);
	void OnArgumentsCommitted(const FText& Text, ETextCommit::Type CommitType, FVarnPythonBrowserScriptPtr Script, int32 Index);
	// Writes the script's argument presets to the settings.
	void SaveArguments(const FVarnPythonBrowserScriptPtr& Script);
	// Rebuilds the tree after the presets changed; the script's row stays open.
	void RefreshAfterArgumentsChanged(const FVarnPythonBrowserScriptPtr& Script);
	bool IsEditing(const FVarnPythonBrowserScriptPtr& Script, int32 Index) const;
	static FString ArgumentEditorKey(const FVarnPythonBrowserScriptPtr& Script, int32 Index);
	FReply RunScript(FVarnPythonBrowserScriptPtr Script, FString Arguments);
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
	// The argument preset being edited inline: EditingIndex in EditingScript's ArgumentSets.
	FVarnPythonBrowserScriptPtr EditingScript;
	int32 EditingIndex = INDEX_NONE;
	FText ArgumentsEditBuffer;
	// Inline editors of the generated argument rows, by ArgumentEditorKey. Rebuilt along with the rows.
	TMap<FString, TWeakPtr<SWidget>> ArgumentEditors;
	FString SearchText;
	FText RootsTooltip;
	FText LastResult;
	int32 NumShownScripts = 0;
	// Folder hierarchy when true, a flat list of every script when false.
	bool bShowHierarchy = false;
	bool bRunning = false;
};
