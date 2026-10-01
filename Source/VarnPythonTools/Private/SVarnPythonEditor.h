// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VarnPythonScriptTree.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class FTabManager;
class SDockTab;

using FVarnPythonEditorNodePtr = VarnPythonTools::FScriptTreeNodePtr;

/** Script explorer on the left, one dockable tab per opened script on the right. */
class SVarnPythonEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVarnPythonEditor)
		{
		}

	SLATE_END_ARGS()

	/** Id of the nomad tab that hosts the editor. */
	static const FName TabName;

	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& OwnerTab);
	virtual ~SVarnPythonEditor() override;

	/** Opens the editor tab if it isn't open, then opens the script in it. */
	static void OpenFile(const FString& Filename);

private:
	TSharedRef<SWidget> MakeExplorerPanel();
	void RebuildTree();
	void OnSearchChanged(const FText& Text);
	FReply OnRefreshClicked();
	void ToggleFolderView();
	TSharedRef<ITableRow> GenerateRow(FVarnPythonEditorNodePtr Node, const TSharedRef<STableViewBase>& Owner);
	void GetNodeChildren(FVarnPythonEditorNodePtr Node, TArray<FVarnPythonEditorNodePtr>& OutChildren);
	void OnExpansionChanged(FVarnPythonEditorNodePtr Node, bool bExpanded);
	void OnNodeDoubleClicked(FVarnPythonEditorNodePtr Node);
	const FSlateBrush* GetNodeIcon(const FVarnPythonEditorNodePtr& Node) const;
	FText GetEmptyText() const;

	void OpenDocument(const FString& Filename);
	void OnDocumentClosed(TSharedRef<SDockTab> Tab, FString Key);

	// The editor in the open editor tab, if any, for OpenFile.
	static TWeakPtr<SVarnPythonEditor> ActiveEditor;

	TSharedPtr<FTabManager> TabManager;
	TSharedPtr<STreeView<FVarnPythonEditorNodePtr>> Tree;
	TArray<FVarnPythonEditorNodePtr> RootNodes;
	// Opened scripts by PathKey, so a second double-click focuses the existing tab.
	TMap<FString, TWeakPtr<SDockTab>> OpenDocuments;
	VarnPythonTools::FScriptTreeExpansion Expansion;
	FString SearchText;
	// Folder hierarchy when true, a flat list of every script when false.
	bool bShowHierarchy = true;
};
