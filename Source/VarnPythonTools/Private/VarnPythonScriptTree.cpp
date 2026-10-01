// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "VarnPythonScriptTree.h"

#include "ISettingsModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Templates/UnrealTemplate.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "VarnPythonScriptTree"

namespace VarnPythonTools
{
	namespace
	{
		void OpenSettings()
		{
			if (ISettingsModule* Settings = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings")))
			{
				Settings->ShowViewer(TEXT("Editor"), TEXT("Plugins"), TEXT("VarnPythonBrowser"));
			}
		}

		void SortNodes(TArray<FScriptTreeNodePtr>& Nodes)
		{
			Nodes.Sort(
				[](const FScriptTreeNodePtr& A, const FScriptTreeNodePtr& B)
				{
					const bool bAIsFile = A->Kind == FScriptTreeNode::EKind::File;
					const bool bBIsFile = B->Kind == FScriptTreeNode::EKind::File;
					if (bAIsFile != bBIsFile)
					{
						return bBIsFile;
					}
					return A->Name.Compare(B->Name, ESearchCase::IgnoreCase) < 0;
				});
			for (const FScriptTreeNodePtr& Node : Nodes)
			{
				SortNodes(Node->Children);
			}
		}
	}

	TArray<FScriptTreeNodePtr> BuildScriptTree(const TArray<FScriptFile>& Scripts)
	{
		// Roots that share a label (several personal folders, say) are told apart by their path.
		TMap<FString, TSet<FString>> RootPathsByLabel;
		for (const FScriptFile& Script : Scripts)
		{
			RootPathsByLabel.FindOrAdd(Script.RootLabel).Add(Script.RootPath);
		}

		TArray<FScriptTreeNodePtr> RootNodes;
		TMap<FString, FScriptTreeNodePtr> RootsByPath;
		for (const FScriptFile& Script : Scripts)
		{
			FScriptTreeNodePtr Parent = RootsByPath.FindRef(Script.RootPath);
			if (!Parent.IsValid())
			{
				Parent = MakeShared<FScriptTreeNode>();
				Parent->Kind = FScriptTreeNode::EKind::Root;
				Parent->Path = Script.RootPath;
				Parent->Name = RootPathsByLabel.FindChecked(Script.RootLabel).Num() > 1
					? FString::Printf(TEXT("%s (%s)"), *Script.RootLabel, *Script.RootPath)
					: Script.RootLabel;
				RootsByPath.Add(Script.RootPath, Parent);
				RootNodes.Add(Parent);
			}

			TArray<FString> Segments;
			Script.RelativePath.ParseIntoArray(Segments, TEXT("/"));
			FString NodePath = Script.RootPath;
			for (int32 Index = 0; Index < Segments.Num(); ++Index)
			{
				const FString& Segment = Segments[Index];
				const bool bIsFile = Index == Segments.Num() - 1;
				NodePath += TEXT("/") + Segment;

				const FScriptTreeNodePtr* Existing = Parent->Children.FindByPredicate(
					[&Segment](const FScriptTreeNodePtr& Child)
					{
						return Child->Name.Equals(Segment, ESearchCase::CaseSensitive);
					});
				if (Existing)
				{
					Parent = *Existing;
					continue;
				}

				FScriptTreeNodePtr Node = MakeShared<FScriptTreeNode>();
				Node->Kind = bIsFile ? FScriptTreeNode::EKind::File : FScriptTreeNode::EKind::Folder;
				Node->Name = Segment;
				Node->Path = bIsFile ? Script.Filename : NodePath;
				Parent->Children.Add(Node);
				Parent = Node;
			}
		}
		for (const FScriptTreeNodePtr& Root : RootNodes)
		{
			SortNodes(Root->Children);
		}
		return RootNodes;
	}

	TArray<FScriptTreeNodePtr> BuildScriptList(const TArray<FScriptFile>& Scripts)
	{
		TArray<FScriptTreeNodePtr> Nodes;
		Nodes.Reserve(Scripts.Num());
		for (const FScriptFile& Script : Scripts)
		{
			FScriptTreeNodePtr Node = MakeShared<FScriptTreeNode>();
			Node->Kind = FScriptTreeNode::EKind::File;
			Node->Name = FPaths::GetCleanFilename(Script.Filename);
			Node->Path = Script.Filename;
			const FString Folder = FPaths::GetPath(Script.RelativePath);
			Node->Location = Folder.IsEmpty() ? Script.RootLabel : Script.RootLabel + TEXT(" / ") + Folder;
			Nodes.Add(MoveTemp(Node));
		}
		Nodes.Sort(
			[](const FScriptTreeNodePtr& A, const FScriptTreeNodePtr& B)
			{
				const int32 NameOrder = A->Name.Compare(B->Name, ESearchCase::IgnoreCase);
				return NameOrder == 0 ? A->Path < B->Path : NameOrder < 0;
			});
		return Nodes;
	}

	TSharedRef<SWidget> MakeNameAndLocation(const TSharedRef<SWidget>& Name, const FString& Location)
	{
		return SNew(SHorizontalBox)
			// The name never shrinks, so a name wider than the row is cut at the row's edge instead of spilling over its neighbors.
			.Clipping(EWidgetClipping::ClipToBounds)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Name
			]
			// Starts at its text width, grows to push the text to the right edge, and shrinks first when the row is narrow.
			+ SHorizontalBox::Slot().FillContentWidth(1.f, 1.f).VAlign(VAlign_Center).Padding(8, 0, 4, 0)
			[
				SNew(STextBlock)
				.Visibility(Location.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
				.Text(FText::FromString(Location))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Justification(ETextJustify::Right)
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			];
	}

	TSharedRef<SWidget> MakeOptionsMenuButton(FExecuteAction ToggleFolderView, FIsActionChecked IsFolderView)
	{
		return SNew(SComboButton)
			.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
			.HasDownArrow(false)
			.ToolTipText(LOCTEXT("OptionsTip", "View options and settings."))
			.OnGetMenuContent_Lambda([ToggleFolderView, IsFolderView]()
			{
				FMenuBuilder MenuBuilder(true, nullptr);
				MenuBuilder.AddMenuEntry(
					LOCTEXT("Settings", "Settings"),
					LOCTEXT("SettingsTip", "Configure personal script folders in Editor Preferences. Refresh after changing them."),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings"),
					FUIAction(FExecuteAction::CreateStatic(&OpenSettings)));
				MenuBuilder.AddMenuEntry(
					LOCTEXT("FolderView", "Folder View"),
					LOCTEXT("FolderViewTip", "Group scripts under their folders instead of showing one flat list."),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderClosed"),
					FUIAction(ToggleFolderView, FCanExecuteAction(), IsFolderView),
					NAME_None,
					EUserInterfaceActionType::Check);
				return MenuBuilder.MakeWidget();
			})
			.ButtonContent()
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.EllipsisVerticalNarrow"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			];
	}

	void FScriptTreeExpansion::Restore(STreeView<FScriptTreeNodePtr>& Tree, const TArray<FScriptTreeNodePtr>& Roots, bool bExpandAll)
	{
		for (const FScriptTreeNodePtr& Root : Roots)
		{
			if (Root->Kind != FScriptTreeNode::EKind::Root)
			{
				continue;
			}
			// A root's first appearance is expanded; after that the user's choice sticks.
			const FString RootKey = PathKey(Root->Path);
			bool bAlreadySeen = false;
			SeenRootPaths.Add(RootKey, &bAlreadySeen);
			if (!bAlreadySeen)
			{
				ExpandedPaths.Add(RootKey);
			}
		}

		// Every node is new, so expansion is reapplied from ExpandedPaths; that must not rewrite ExpandedPaths.
		TGuardValue<bool> Guard(bRestoring, true);
		Tree.ClearExpandedItems();
		ApplyTo(Tree, Roots, bExpandAll);
	}

	void FScriptTreeExpansion::ApplyTo(STreeView<FScriptTreeNodePtr>& Tree, const TArray<FScriptTreeNodePtr>& Nodes, bool bExpandAll) const
	{
		for (const FScriptTreeNodePtr& Node : Nodes)
		{
			if (Node->Children.IsEmpty())
			{
				continue;
			}
			// Searching opens folders, not the argument presets under each matching script.
			const bool bOpenForSearch = bExpandAll && Node->Kind != FScriptTreeNode::EKind::File;
			if (bOpenForSearch || ExpandedPaths.Contains(PathKey(Node->Path)))
			{
				Tree.SetItemExpansion(Node, true);
			}
			ApplyTo(Tree, Node->Children, bExpandAll);
		}
	}

	void FScriptTreeExpansion::OnExpansionChanged(const FScriptTreeNodePtr& Node, bool bExpanded)
	{
		if (bRestoring)
		{
			return;
		}
		const FString Key = PathKey(Node->Path);
		if (bExpanded)
		{
			ExpandedPaths.Add(Key);
		}
		else
		{
			ExpandedPaths.Remove(Key);
		}
	}
}

#undef LOCTEXT_NAMESPACE
