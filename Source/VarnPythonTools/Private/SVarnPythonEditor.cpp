// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SVarnPythonEditor.h"

#include "SVarnPythonDocument.h"
#include "VarnPythonScriptSources.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Templates/UnrealTemplate.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "VarnPythonEditor"

namespace
{
	// Opened scripts dock next to each other; this id only marks where the first one goes.
	const FName DocumentPlaceholderId(TEXT("VarnPythonDocument"));

	using EKind = VarnPythonTools::FScriptTreeNode::EKind;
}

void SVarnPythonEditor::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& OwnerTab)
{
	TabManager = FGlobalTabmanager::Get()->NewTabManager(OwnerTab);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("VarnPythonEditorLayout_v1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->Split
			(
				FTabManager::NewStack()
				->AddTab(DocumentPlaceholderId, ETabState::ClosedTab)
			)
		);

	ChildSlot
	[
		SNew(SSplitter)
		+ SSplitter::Slot().Value(0.25f).MinSize(160.f)
		[
			MakeExplorerPanel()
		]
		+ SSplitter::Slot().Value(0.75f)
		[
			TabManager->RestoreFrom(Layout, TSharedPtr<SWindow>()).ToSharedRef()
		]
	];

	RebuildTree();
}

SVarnPythonEditor::~SVarnPythonEditor()
{
	if (TabManager.IsValid())
	{
		TabManager->CloseAllAreas();
	}
}

TSharedRef<SWidget> SVarnPythonEditor::MakeExplorerPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
		.Padding(0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(6)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1)
				[
					SNew(SSearchBox)
					.HintText(LOCTEXT("Search", "Search scripts"))
					.OnTextChanged(this, &SVarnPythonEditor::OnSearchChanged)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("RefreshTip", "Rescan the script folders."))
					.OnClicked(this, &SVarnPythonEditor::OnRefreshClicked)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Refresh"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[
					VarnPythonTools::MakeOptionsMenuButton(
						FExecuteAction::CreateSP(this, &SVarnPythonEditor::ToggleFolderView),
						FIsActionChecked::CreateSPLambda(this, [this] { return bShowHierarchy; }))
				]
			]
			+ SVerticalBox::Slot().FillHeight(1)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(Tree, STreeView<FVarnPythonEditorNodePtr>)
					.TreeItemsSource(&RootNodes)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &SVarnPythonEditor::GenerateRow)
					.OnGetChildren(this, &SVarnPythonEditor::GetNodeChildren)
					.OnExpansionChanged(this, &SVarnPythonEditor::OnExpansionChanged)
					.OnMouseButtonDoubleClick(this, &SVarnPythonEditor::OnNodeDoubleClicked)
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(12)
				[
					SNew(STextBlock)
					.Visibility_Lambda([this] { return RootNodes.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					.Text(this, &SVarnPythonEditor::GetEmptyText)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Justification(ETextJustify::Center)
					.AutoWrapText(true)
				]
			]
		];
}

void SVarnPythonEditor::RebuildTree()
{
	TArray<VarnPythonTools::FScriptFile> Scripts = VarnPythonTools::FindScriptFiles(VarnPythonTools::GatherRoots());
	if (!SearchText.IsEmpty())
	{
		Scripts.RemoveAll(
			[this](const VarnPythonTools::FScriptFile& Script)
			{
				return !Script.Filename.Contains(SearchText) && !Script.RootLabel.Contains(SearchText);
			});
	}

	RootNodes = bShowHierarchy ? VarnPythonTools::BuildScriptTree(Scripts) : VarnPythonTools::BuildScriptList(Scripts);
	// While searching, open everything so every match is visible.
	Expansion.Restore(*Tree, RootNodes, !SearchText.IsEmpty());
	Tree->RequestTreeRefresh();
}

void SVarnPythonEditor::OnSearchChanged(const FText& Text)
{
	SearchText = Text.ToString().TrimStartAndEnd();
	RebuildTree();
}

FReply SVarnPythonEditor::OnRefreshClicked()
{
	RebuildTree();
	return FReply::Handled();
}

void SVarnPythonEditor::ToggleFolderView()
{
	bShowHierarchy = !bShowHierarchy;
	RebuildTree();
}

TSharedRef<ITableRow> SVarnPythonEditor::GenerateRow(FVarnPythonEditorNodePtr Node, const TSharedRef<STableViewBase>& Owner)
{
	const bool bIsRoot = Node->Kind == EKind::Root;
	const FTextBlockStyle& TextStyle = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>(
		bIsRoot ? FName("NormalText.Important") : FName("NormalText"));

	return SNew(STableRow<FVarnPythonEditorNodePtr>, Owner)
		.Padding(FMargin(0, 2))
		.ToolTipText(FText::FromString(Node->Path))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
			[
				SNew(SImage)
				.Image_Lambda([this, Node] { return GetNodeIcon(Node); })
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
			[
				// Only the flat list sets a location; without folders to read it from, it tells same-named scripts apart.
				VarnPythonTools::MakeNameAndLocation(
					SNew(STextBlock)
					.TextStyle(&TextStyle)
					.Text(FText::FromString(Node->Name))
					.HighlightText_Lambda([this] { return FText::FromString(SearchText); }),
					Node->Location)
			]
		];
}

void SVarnPythonEditor::GetNodeChildren(FVarnPythonEditorNodePtr Node, TArray<FVarnPythonEditorNodePtr>& OutChildren)
{
	OutChildren = Node->Children;
}

void SVarnPythonEditor::OnExpansionChanged(FVarnPythonEditorNodePtr Node, bool bExpanded)
{
	Expansion.OnExpansionChanged(Node, bExpanded);
}

void SVarnPythonEditor::OnNodeDoubleClicked(FVarnPythonEditorNodePtr Node)
{
	if (Node->Kind == EKind::File)
	{
		OpenDocument(Node->Path);
	}
	else
	{
		// Binding this delegate replaces the tree's default double-click-to-expand.
		Tree->SetItemExpansion(Node, !Tree->IsItemExpanded(Node));
	}
}

const FSlateBrush* SVarnPythonEditor::GetNodeIcon(const FVarnPythonEditorNodePtr& Node) const
{
	if (Node->Kind == EKind::File)
	{
		return FAppStyle::GetBrush("MainFrame.OpenSourceCodeEditor");
	}
	return FAppStyle::GetBrush(Tree->IsItemExpanded(Node) ? "Icons.FolderOpen" : "Icons.FolderClosed");
}

FText SVarnPythonEditor::GetEmptyText() const
{
	return SearchText.IsEmpty()
		? LOCTEXT("NoScripts", "No Python scripts found.\nAdd folders under Editor Preferences > Plugins > Rock Python Browser.")
		: LOCTEXT("NoMatches", "No scripts match the search.");
}

void SVarnPythonEditor::OpenDocument(const FString& Filename)
{
	const FString Key = VarnPythonTools::PathKey(Filename);
	if (const TWeakPtr<SDockTab>* Existing = OpenDocuments.Find(Key))
	{
		if (const TSharedPtr<SDockTab> OpenTab = Existing->Pin())
		{
			// Re-read the file so changes made outside the editor show up.
			OpenTab->SetContent(SNew(SVarnPythonDocument).Filename(Filename));
			TabManager->DrawAttention(OpenTab.ToSharedRef());
			return;
		}
	}

	const TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.TabRole(ETabRole::DocumentTab)
		.Label(FText::FromString(FPaths::GetCleanFilename(Filename)))
		.ToolTipText(FText::FromString(Filename))
		.OnTabClosed(SDockTab::FOnTabClosedCallback::CreateSP(this, &SVarnPythonEditor::OnDocumentClosed, Key))
		[
			SNew(SVarnPythonDocument).Filename(Filename)
		];
	Tab->SetTabIcon(FAppStyle::GetBrush("MainFrame.OpenSourceCodeEditor"));
	OpenDocuments.Add(Key, Tab);
	TabManager->InsertNewDocumentTab(DocumentPlaceholderId, FTabManager::ESearchPreference::PreferLiveTab, Tab);
}

void SVarnPythonEditor::OnDocumentClosed(TSharedRef<SDockTab> Tab, FString Key)
{
	OpenDocuments.Remove(Key);
}

#undef LOCTEXT_NAMESPACE
