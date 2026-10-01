// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VarnPythonScriptSources.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Views/STreeView.h"

namespace VarnPythonTools
{
	struct FScriptTreeNode
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
		// Only set in the flat list: the root and folder the script lives in, e.g. "Project / Materials". Empty in the tree.
		FString Location;
		TArray<TSharedPtr<FScriptTreeNode>> Children;
	};

	using FScriptTreeNodePtr = TSharedPtr<FScriptTreeNode>;

	/** One node per root, then folders mirroring each script's relative path. Folders sort before scripts, each alphabetically. */
	TArray<FScriptTreeNodePtr> BuildScriptTree(const TArray<FScriptFile>& Scripts);

	/** Every script as a top-level File node, sorted by name. */
	TArray<FScriptTreeNodePtr> BuildScriptList(const TArray<FScriptFile>& Scripts);

	/**
	 * Single-line label for a script row: the name, then Location dimmed and right-aligned. The name keeps its full
	 * width, so in a narrow row the location is the part that gets cut off (with an ellipsis).
	 */
	TSharedRef<SWidget> MakeNameAndLocation(const TSharedRef<SWidget>& Name, const FString& Location);

	/**
	 * The 3-dot button beside Refresh. Its drop-down opens the plugin's settings and has a Folder View entry that shows a
	 * check mark while IsFolderView returns true: scripts are grouped under their folders when it is, a flat list when not.
	 */
	TSharedRef<SWidget> MakeOptionsMenuButton(FExecuteAction ToggleFolderView, FIsActionChecked IsFolderView);

	/** Remembers which folders are open, because rebuilding a tree replaces every node. */
	class FScriptTreeExpansion
	{
	public:
		/** Re-expands the folders the user had open. A root's first appearance is open; bExpandAll opens everything. */
		void Restore(STreeView<FScriptTreeNodePtr>& Tree, const TArray<FScriptTreeNodePtr>& Roots, bool bExpandAll);

		/** Bind to the tree's OnExpansionChanged. */
		void OnExpansionChanged(const FScriptTreeNodePtr& Node, bool bExpanded);

	private:
		void ApplyTo(STreeView<FScriptTreeNodePtr>& Tree, const TArray<FScriptTreeNodePtr>& Nodes, bool bExpandAll) const;

		// Expanded folders by PathKey.
		TSet<FString> ExpandedPaths;
		// Roots already shown once, so only a root's first appearance defaults to expanded.
		TSet<FString> SeenRootPaths;
		bool bRestoring = false;
	};
}
