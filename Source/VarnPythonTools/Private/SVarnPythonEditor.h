// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class FTabManager;
class SDockTab;

struct FVarnPythonEditorNode
{
	enum class EKind : uint8
	{
		Root,
		Folder,
		File
	};

	EKind Kind = EKind::File;
	FString Name;
	// Absolute path of the root directory, folder or script.
	FString Path;
	TArray<TSharedPtr<FVarnPythonEditorNode>> Children;
};

using FVarnPythonEditorNodePtr = TSharedPtr<FVarnPythonEditorNode>;

/** Script explorer on the left, one dockable tab per opened script on the right. */
class SVarnPythonEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVarnPythonEditor)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& OwnerTab);
	virtual ~SVarnPythonEditor() override;

private:
	TSharedRef<SWidget> MakeExplorerPanel();
	void RebuildTree();
	void ApplyExpansion(const TArray<FVarnPythonEditorNodePtr>& Nodes);
	void OnSearchChanged(const FText& Text);
	FReply OnRefreshClicked();
	TSharedRef<ITableRow> GenerateRow(FVarnPythonEditorNodePtr Node, const TSharedRef<STableViewBase>& Owner);
	void GetNodeChildren(FVarnPythonEditorNodePtr Node, TArray<FVarnPythonEditorNodePtr>& OutChildren);
	void OnExpansionChanged(FVarnPythonEditorNodePtr Node, bool bExpanded);
	void OnNodeDoubleClicked(FVarnPythonEditorNodePtr Node);
	const FSlateBrush* GetNodeIcon(const FVarnPythonEditorNodePtr& Node) const;
	FText GetEmptyText() const;

	void OpenDocument(const FString& Filename);
	void OnDocumentClosed(TSharedRef<SDockTab> Tab, FString Key);

	TSharedPtr<FTabManager> TabManager;
	TSharedPtr<STreeView<FVarnPythonEditorNodePtr>> Tree;
	TArray<FVarnPythonEditorNodePtr> RootNodes;
	// Opened scripts by PathKey, so a second double-click focuses the existing tab.
	TMap<FString, TWeakPtr<SDockTab>> OpenDocuments;
	// Expanded folders by PathKey; survives rebuilds, which replace every node.
	TSet<FString> ExpandedPaths;
	// Roots already shown once, so only a root's first appearance defaults to expanded.
	TSet<FString> SeenRootPaths;
	FString SearchText;
	bool bApplyingExpansion = false;
};
