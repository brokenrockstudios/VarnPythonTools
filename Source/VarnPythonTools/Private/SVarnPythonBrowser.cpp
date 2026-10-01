// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SVarnPythonBrowser.h"

#include "Editor.h"
#include "IPythonScriptPlugin.h"
#include "SVarnPythonEditor.h"
#include "PythonScriptTypes.h"
#include "VarnPythonScriptSources.h"
#include "VarnPythonPaths.h"
#include "VarnPythonToolsSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Templates/UnrealTemplate.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "VarnPythonBrowser"

DEFINE_LOG_CATEGORY_STATIC(LogVarnPython, Log, All);

namespace VarnPythonTools
{
	// Extracts the leading triple-quoted module docstring, or returns an empty string if there isn't one.
	FString ReadModuleDocstring(const FString& Filename)
	{
		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *Filename))
		{
			return FString();
		}

		int32 Pos = 0;
		const int32 Len = Source.Len();
		// Skip BOM, blank lines, and leading # comments (shebang, encoding, license headers).
		while (Pos < Len)
		{
			const TCHAR C = Source[Pos];
			if (C == 0xFEFF || FChar::IsWhitespace(C))
			{
				++Pos;
			}
			else if (C == TEXT('#'))
			{
				while (Pos < Len && Source[Pos] != TEXT('\n'))
				{
					++Pos;
				}
			}
			else
			{
				break;
			}
		}

		// Optional string prefix (r, u, R, U).
		if (Pos < Len && (FChar::ToLower(Source[Pos]) == TEXT('r') || FChar::ToLower(Source[Pos]) == TEXT('u')))
		{
			++Pos;
		}

		const FStringView Rest = FStringView(Source).RightChop(Pos);
		FStringView Quote;
		if (Rest.StartsWith(TEXT("\"\"\"")))
		{
			Quote = TEXTVIEW("\"\"\"");
		}
		else if (Rest.StartsWith(TEXT("'''")))
		{
			Quote = TEXTVIEW("'''");
		}
		else
		{
			return FString();
		}

		const FStringView Body = Rest.RightChop(3);
		const int32 End = Body.Find(Quote);
		FString Docstring = FString(End == INDEX_NONE ? Body : Body.Left(End));
		Docstring.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		Docstring.TrimStartAndEndInline();

		constexpr int32 MaxLength = 1500;
		if (Docstring.Len() > MaxLength)
		{
			Docstring = Docstring.Left(MaxLength).TrimEnd() + TEXT("...");
		}
		return Docstring;
	}

	FText GetScriptInfoTooltip(const FVarnPythonBrowserScriptPtr& Script)
	{
		if (!Script->bDescriptionLoaded)
		{
			Script->Description = ReadModuleDocstring(Script->Filename);
			Script->bDescriptionLoaded = true;
		}
		if (Script->Description.IsEmpty())
		{
			return FText::Format(
				LOCTEXT("NoDescription", "{0}\n\nNo description available (no triple-quoted docstring at the top of the script)."),
				FText::FromString(Script->DisplayName));
		}
		return FText::FromString(Script->Description);
	}

	// The Run button shared by script rows and argument rows.
	TSharedRef<SWidget> MakeRunButton(const FString& Command, TAttribute<bool> bEnabled, FOnClicked OnClicked)
	{
		return SNew(SButton)
			.ToolTipText(FText::Format(LOCTEXT("RunTip", "Run {0}\nRequires Python to be ready and PIE to be stopped."), FText::FromString(Command)))
			.IsEnabled(bEnabled)
			.OnClicked(OnClicked)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SImage).Image(FAppStyle::GetBrush("Icons.Play"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("Run", "Run"))
				]
			];
	}
}

void SVarnPythonBrowser::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.Padding(8)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1)
				[
					SNew(SSearchBox)
					.HintText(LOCTEXT("Search", "Search scripts"))
					.OnTextChanged(this, &SVarnPythonBrowser::OnSearchChanged)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("RefreshTip", "Rescan script folders. Hover over the script count to see search locations."))
					.IsEnabled_Lambda([this] { return !bRunning; })
					.OnClicked(this, &SVarnPythonBrowser::OnRefreshClicked)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Refresh"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[
					VarnPythonTools::MakeOptionsMenuButton(
						FExecuteAction::CreateSP(this, &SVarnPythonBrowser::ToggleFolderView),
						FIsActionChecked::CreateSPLambda(this, [this] { return bShowHierarchy; }))
				]
			]
			+ SVerticalBox::Slot().FillHeight(1)
			[
				SAssignNew(Tree, STreeView<FVarnPythonBrowserNodePtr>)
				.TreeItemsSource(&RootNodes)
				.SelectionMode(ESelectionMode::Single) // Single so a right-click selects the row the context menu applies to.
				.OnContextMenuOpening(this, &SVarnPythonBrowser::OnContextMenuOpening)
				.OnGenerateRow(this, &SVarnPythonBrowser::GenerateRow)
				.OnGetChildren(this, &SVarnPythonBrowser::GetNodeChildren)
				.OnExpansionChanged(this, &SVarnPythonBrowser::OnExpansionChanged)
				.OnMouseButtonDoubleClick(this, &SVarnPythonBrowser::OnNodeDoubleClicked)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
			[
				SNew(STextBlock)
				.Text(this, &SVarnPythonBrowser::GetSummary)
				.ToolTipText_Lambda([this] { return RootsTooltip; })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
			[
				SNew(STextBlock)
				.Text_Lambda([this] { return LastResult; })
				.AutoWrapText(true)
			]
		]
	];
	LastResult = LOCTEXT("Ready", "Run a script to see its output in the Output Log.");
	RefreshScripts();
}

void SVarnPythonBrowser::RefreshScripts()
{
	AllScripts.Reset();
	ScriptsByKey.Reset();
	FString RootDescription;

	const UVarnPythonToolsSettings* Settings = GetDefault<UVarnPythonToolsSettings>();
	const TArray<VarnPythonTools::FRoot> Roots = VarnPythonTools::GatherRoots();
	for (const VarnPythonTools::FRoot& Root : Roots)
	{
		RootDescription += FString::Printf(
			TEXT("%s: %s%s\n"), *Root.Label, *Root.Path,
			IFileManager::Get().DirectoryExists(*Root.Path) ? TEXT("") : TEXT(" (not created)"));
	}

	for (const VarnPythonTools::FScriptFile& File : VarnPythonTools::FindScriptFiles(Roots))
	{
		const FString BaseName = FPaths::GetCleanFilename(File.Filename);
		if (BaseName.Equals(TEXT("__init__.py"), ESearchCase::IgnoreCase) || BaseName.Equals(TEXT("init_unreal.py"), ESearchCase::IgnoreCase))
		{
			continue; // Package initializers and editor startup hooks aren't launcher entries.
		}

		FVarnPythonBrowserScriptPtr Script = MakeShared<FVarnPythonBrowserScript>();
		Script->Filename = File.Filename;
		Script->DisplayName = BaseName;
		Script->Source = File.RootLabel;
		Script->RootPath = File.RootPath;
		Script->RelativePath = File.RelativePath;
		if (const FVarnPythonArgumentSets* SavedArguments = Settings->ScriptArgumentSets.Find(VarnPythonTools::PathKey(File.Filename)))
		{
			Script->ArgumentSets = SavedArguments->Sets;
		}
		ScriptsByKey.Add(VarnPythonTools::PathKey(File.Filename), Script);
		AllScripts.Add(MoveTemp(Script));
	}
	RootsTooltip = FText::FromString(RootDescription);
	FilterScripts();
}

void SVarnPythonBrowser::FilterScripts()
{
	EditingScript.Reset();
	EditingIndex = INDEX_NONE;
	ArgumentEditors.Reset();
	TArray<VarnPythonTools::FScriptFile> Shown;
	for (const FVarnPythonBrowserScriptPtr& Script : AllScripts)
	{
		if (SearchText.IsEmpty() || Script->Filename.Contains(SearchText) || Script->Source.Contains(SearchText))
		{
			Shown.Add({Script->Filename, Script->RootPath, Script->Source, Script->RelativePath});
		}
	}
	NumShownScripts = Shown.Num();

	RootNodes = bShowHierarchy ? VarnPythonTools::BuildScriptTree(Shown) : VarnPythonTools::BuildScriptList(Shown);
	AttachArgumentNodes(RootNodes);
	if (Tree.IsValid())
	{
		// While searching, open everything so every match is visible.
		Expansion.Restore(*Tree, RootNodes, !SearchText.IsEmpty());
		Tree->RequestTreeRefresh();
	}
}

void SVarnPythonBrowser::OnSearchChanged(const FText& Text)
{
	SearchText = Text.ToString().TrimStartAndEnd();
	FilterScripts();
}

TSharedRef<ITableRow> SVarnPythonBrowser::GenerateRow(FVarnPythonBrowserNodePtr Node, const TSharedRef<STableViewBase>& Owner)
{
	if (Node->Kind == VarnPythonTools::FScriptTreeNode::EKind::Arguments)
	{
		return GenerateArgumentsRow(Node, Owner);
	}
	const FVarnPythonBrowserScriptPtr Script = FindScript(Node);
	if (!Script.IsValid())
	{
		return GenerateFolderRow(Node, Owner);
	}

	return SNew(STableRow<FVarnPythonBrowserNodePtr>, Owner)
		.Padding(4)
		.ToolTipText(FText::FromString(Script->Filename))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				// Only the flat list sets a location; in the folder view the folder rows already say where a script lives.
				VarnPythonTools::MakeNameAndLocation(SNew(STextBlock).Text(FText::FromString(Script->DisplayName)), Node->Location)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Info"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.ToolTipText_Lambda([Script] { return VarnPythonTools::GetScriptInfoTooltip(Script); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(LOCTEXT("AddArgumentsTip", "Add a run entry with its own arguments under this script."))
				.OnClicked_Lambda([this, Script]
				{
					AddArguments(Script);
					return FReply::Handled();
				})
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Plus"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				VarnPythonTools::MakeRunButton(
					Script->Filename,
					TAttribute<bool>::CreateSP(this, &SVarnPythonBrowser::CanRunScripts),
					FOnClicked::CreateSP(this, &SVarnPythonBrowser::RunScript, Script, FString()))
			]
		];
}

TSharedRef<ITableRow> SVarnPythonBrowser::GenerateArgumentsRow(FVarnPythonBrowserNodePtr Node, const TSharedRef<STableViewBase>& Owner)
{
	const FVarnPythonBrowserScriptPtr Script = FindScript(Node);
	const int32 Index = Node->ArgumentIndex;
	if (!Script.IsValid() || !Script->ArgumentSets.IsValidIndex(Index))
	{
		return SNew(STableRow<FVarnPythonBrowserNodePtr>, Owner)
			[
				SNullWidget::NullWidget
			];
	}
	const FString Arguments = Script->ArgumentSets[Index];
	const FString Command = FString::Printf(TEXT("%s %s"), *Script->Filename, *Arguments);

	TSharedPtr<SEditableTextBox> ArgumentsBox;
	TSharedRef<ITableRow> Row = SNew(STableRow<FVarnPythonBrowserNodePtr>, Owner)
		.Padding(4)
		.ToolTipText(FText::FromString(Command))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.ArrowRight"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(STextBlock)
					.Visibility_Lambda([this, Script, Index] { return IsEditing(Script, Index) ? EVisibility::Collapsed : EVisibility::Visible; })
					.Text(FText::FromString(Arguments))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SOverlay::Slot()
				[
					// Enter or clicking away saves, Escape cancels. Saving an empty value removes the entry.
					SAssignNew(ArgumentsBox, SEditableTextBox)
					.Visibility_Lambda([this, Script, Index] { return IsEditing(Script, Index) ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text_Lambda([this] { return ArgumentsEditBuffer; })
					.OnTextChanged_Lambda([this](const FText& Text) { ArgumentsEditBuffer = Text; })
					.OnTextCommitted(this, &SVarnPythonBrowser::OnArgumentsCommitted, Script, Index)
					.HintText(LOCTEXT("ArgumentsHint", "Run arguments, e.g. --dry-run \"some value\""))
					.SelectAllTextWhenFocused(true)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				VarnPythonTools::MakeRunButton(
					Command,
					TAttribute<bool>::CreateSP(this, &SVarnPythonBrowser::CanRunScripts),
					FOnClicked::CreateSP(this, &SVarnPythonBrowser::RunScript, Script, Arguments))
			]
		];
	ArgumentEditors.Add(ArgumentEditorKey(Script, Index), ArgumentsBox);
	return Row;
}

void SVarnPythonBrowser::AttachArgumentNodes(const TArray<FVarnPythonBrowserNodePtr>& Nodes)
{
	for (const FVarnPythonBrowserNodePtr& Node : Nodes)
	{
		if (Node->Kind != VarnPythonTools::FScriptTreeNode::EKind::File)
		{
			AttachArgumentNodes(Node->Children);
			continue;
		}
		const FVarnPythonBrowserScriptPtr Script = FindScript(Node);
		if (!Script.IsValid())
		{
			continue;
		}
		for (int32 Index = 0; Index < Script->ArgumentSets.Num(); ++Index)
		{
			FVarnPythonBrowserNodePtr Child = MakeShared<VarnPythonTools::FScriptTreeNode>();
			Child->Kind = VarnPythonTools::FScriptTreeNode::EKind::Arguments;
			Child->Name = Script->ArgumentSets[Index];
			Child->Path = Node->Path;
			Child->ArgumentIndex = Index;
			Node->Children.Add(MoveTemp(Child));
		}
	}
}

TSharedRef<ITableRow> SVarnPythonBrowser::GenerateFolderRow(FVarnPythonBrowserNodePtr Node, const TSharedRef<STableViewBase>& Owner)
{
	const bool bIsRoot = Node->Kind == VarnPythonTools::FScriptTreeNode::EKind::Root;
	const FTextBlockStyle& TextStyle = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>(
		bIsRoot ? FName("NormalText.Important") : FName("NormalText"));

	return SNew(STableRow<FVarnPythonBrowserNodePtr>, Owner)
		.Padding(4)
		.ToolTipText(FText::FromString(Node->Path))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
			[
				SNew(SImage)
				.Image_Lambda([this, Node] { return FAppStyle::GetBrush(Tree->IsItemExpanded(Node) ? "Icons.FolderOpen" : "Icons.FolderClosed"); })
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(&TextStyle)
				.Text(FText::FromString(Node->Name))
			]
		];
}

void SVarnPythonBrowser::GetNodeChildren(FVarnPythonBrowserNodePtr Node, TArray<FVarnPythonBrowserNodePtr>& OutChildren)
{
	OutChildren = Node->Children;
}

void SVarnPythonBrowser::OnExpansionChanged(FVarnPythonBrowserNodePtr Node, bool bExpanded)
{
	Expansion.OnExpansionChanged(Node, bExpanded);
}

void SVarnPythonBrowser::OnNodeDoubleClicked(FVarnPythonBrowserNodePtr Node)
{
	if (Node->Kind == VarnPythonTools::FScriptTreeNode::EKind::Arguments)
	{
		BeginEditArguments(FindScript(Node), Node->ArgumentIndex);
	}
	else
	{
		// Binding this delegate replaces the tree's default double-click-to-expand.
		Tree->SetItemExpansion(Node, !Tree->IsItemExpanded(Node));
	}
}

FVarnPythonBrowserScriptPtr SVarnPythonBrowser::FindScript(const FVarnPythonBrowserNodePtr& Node) const
{
	if (Node->Kind != VarnPythonTools::FScriptTreeNode::EKind::File && Node->Kind != VarnPythonTools::FScriptTreeNode::EKind::Arguments)
	{
		return nullptr;
	}
	return ScriptsByKey.FindRef(VarnPythonTools::PathKey(Node->Path));
}

bool SVarnPythonBrowser::CanRunScripts() const
{
	IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
	return !bRunning && Python && Python->IsPythonAvailable() && Python->IsPythonInitialized()
		&& GEditor && !GEditor->PlayWorld;
}

TSharedPtr<SWidget> SVarnPythonBrowser::OnContextMenuOpening()
{
	const TArray<FVarnPythonBrowserNodePtr> Selected = Tree->GetSelectedItems();
	const FVarnPythonBrowserNodePtr Node = Selected.IsEmpty() ? nullptr : Selected[0];
	const FVarnPythonBrowserScriptPtr Script = Node.IsValid() ? FindScript(Node) : nullptr;
	if (!Script.IsValid())
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(true, nullptr);
	if (Node->Kind == VarnPythonTools::FScriptTreeNode::EKind::Arguments)
	{
		const int32 Index = Node->ArgumentIndex;
		MenuBuilder.AddMenuEntry(
			LOCTEXT("EditArguments", "Edit Arguments"),
			LOCTEXT("EditArgumentsTip", "Edit the command-line arguments passed to this script (sys.argv) when this entry is run."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
			FUIAction(FExecuteAction::CreateSP(this, &SVarnPythonBrowser::BeginEditArguments, Script, Index)));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DuplicateArguments", "Duplicate"),
			LOCTEXT("DuplicateArgumentsTip", "Add a copy of this entry below it."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Duplicate"),
			FUIAction(FExecuteAction::CreateSP(this, &SVarnPythonBrowser::DuplicateArguments, Script, Index)));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeleteArguments", "Delete"),
			LOCTEXT("DeleteArgumentsTip", "Remove this entry."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
			FUIAction(FExecuteAction::CreateSP(this, &SVarnPythonBrowser::DeleteArguments, Script, Index)));
		return MenuBuilder.MakeWidget();
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddArguments", "Add Arguments"),
		LOCTEXT("AddArgumentsMenuTip", "Add a run entry with its own command-line arguments (sys.argv) under this script."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
		FUIAction(FExecuteAction::CreateSP(this, &SVarnPythonBrowser::AddArguments, Script)));
	MenuBuilder.AddMenuSeparator();
	MenuBuilder.AddMenuEntry(
		LOCTEXT("EditFile", "Edit File"),
		LOCTEXT("EditFileTip", "Open this script in the Python Editor."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
		FUIAction(FExecuteAction::CreateLambda([Filename = Script->Filename] { SVarnPythonEditor::OpenFile(Filename); })));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ShowInExplorer", "Show in Explorer"),
		LOCTEXT("ShowInExplorerTip", "Show this script in Windows Explorer."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderOpen"),
		FUIAction(FExecuteAction::CreateLambda([Filename = Script->Filename]
		{
			FString NativePath = Filename;
			FPaths::MakePlatformFilename(NativePath);
			FPlatformProcess::ExploreFolder(*NativePath);
		})));
	return MenuBuilder.MakeWidget();
}

bool SVarnPythonBrowser::IsEditing(const FVarnPythonBrowserScriptPtr& Script, int32 Index) const
{
	return EditingScript == Script && EditingIndex == Index;
}

FString SVarnPythonBrowser::ArgumentEditorKey(const FVarnPythonBrowserScriptPtr& Script, int32 Index)
{
	return FString::Printf(TEXT("%s#%d"), *VarnPythonTools::PathKey(Script->Filename), Index);
}

void SVarnPythonBrowser::SaveArguments(const FVarnPythonBrowserScriptPtr& Script)
{
	UVarnPythonToolsSettings* Settings = GetMutableDefault<UVarnPythonToolsSettings>();
	const FString Key = VarnPythonTools::PathKey(Script->Filename);
	if (Script->ArgumentSets.IsEmpty())
	{
		Settings->ScriptArgumentSets.Remove(Key);
	}
	else
	{
		Settings->ScriptArgumentSets.FindOrAdd(Key).Sets = Script->ArgumentSets;
	}
	Settings->SaveConfig();
}

void SVarnPythonBrowser::RefreshAfterArgumentsChanged(const FVarnPythonBrowserScriptPtr& Script)
{
	FilterScripts();

	// Keep the script open so its entries stay visible.
	const FString ScriptKey = VarnPythonTools::PathKey(Script->Filename);
	TFunction<FVarnPythonBrowserNodePtr(const TArray<FVarnPythonBrowserNodePtr>&)> FindFileNode =
		[&](const TArray<FVarnPythonBrowserNodePtr>& Nodes) -> FVarnPythonBrowserNodePtr
		{
			for (const FVarnPythonBrowserNodePtr& Node : Nodes)
			{
				if (Node->Kind == VarnPythonTools::FScriptTreeNode::EKind::File)
				{
					if (VarnPythonTools::PathKey(Node->Path) == ScriptKey)
					{
						return Node;
					}
				}
				else if (FVarnPythonBrowserNodePtr Found = FindFileNode(Node->Children))
				{
					return Found;
				}
			}
			return nullptr;
		};
	if (const FVarnPythonBrowserNodePtr FileNode = FindFileNode(RootNodes))
	{
		if (!FileNode->Children.IsEmpty())
		{
			Tree->SetItemExpansion(FileNode, true);
		}
	}
}

void SVarnPythonBrowser::AddArguments(FVarnPythonBrowserScriptPtr Script)
{
	// Not saved until the user commits a value; an entry left empty is dropped.
	Script->ArgumentSets.Add(FString());
	RefreshAfterArgumentsChanged(Script);
	BeginEditArguments(Script, Script->ArgumentSets.Num() - 1);
}

void SVarnPythonBrowser::DuplicateArguments(FVarnPythonBrowserScriptPtr Script, int32 Index)
{
	if (!Script->ArgumentSets.IsValidIndex(Index))
	{
		return;
	}
	Script->ArgumentSets.Insert(Script->ArgumentSets[Index], Index + 1);
	SaveArguments(Script);
	RefreshAfterArgumentsChanged(Script);
	BeginEditArguments(Script, Index + 1);
}

void SVarnPythonBrowser::DeleteArguments(FVarnPythonBrowserScriptPtr Script, int32 Index)
{
	if (!Script->ArgumentSets.IsValidIndex(Index))
	{
		return;
	}
	Script->ArgumentSets.RemoveAt(Index);
	SaveArguments(Script);
	RefreshAfterArgumentsChanged(Script);
}

void SVarnPythonBrowser::BeginEditArguments(FVarnPythonBrowserScriptPtr Script, int32 Index)
{
	if (!Script->ArgumentSets.IsValidIndex(Index))
	{
		return;
	}
	EditingScript = Script;
	EditingIndex = Index;
	ArgumentsEditBuffer = FText::FromString(Script->ArgumentSets[Index]);

	// The editor is collapsed until this frame's visibility pass, and a row added just now may not exist yet,
	// so look for it each tick for a short while.
	const FString Key = ArgumentEditorKey(Script, Index);
	const TSharedRef<int32> Tries = MakeShared<int32>(0);
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda(
		[this, Key, Tries](double, float)
		{
			if (const TWeakPtr<SWidget>* Editor = ArgumentEditors.Find(Key))
			{
				if (TSharedPtr<SWidget> Widget = Editor->Pin())
				{
					FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::SetDirectly);
					return EActiveTimerReturnType::Stop;
				}
			}
			return ++*Tries < 30 ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
		}));
}

void SVarnPythonBrowser::OnArgumentsCommitted(const FText& Text, ETextCommit::Type CommitType, FVarnPythonBrowserScriptPtr Script, int32 Index)
{
	// Collapsing the editor fires a second commit from the focus loss; ignore it.
	if (!IsEditing(Script, Index))
	{
		return;
	}
	EditingScript.Reset();
	EditingIndex = INDEX_NONE;
	if (!Script->ArgumentSets.IsValidIndex(Index))
	{
		return;
	}

	// Escape keeps the old value.
	FString Arguments = CommitType == ETextCommit::OnCleared ? Script->ArgumentSets[Index] : Text.ToString();
	Arguments.ReplaceInline(TEXT("\r"), TEXT(" "));
	Arguments.ReplaceInline(TEXT("\n"), TEXT(" "));
	Arguments.TrimStartAndEndInline();
	if (Arguments.IsEmpty())
	{
		Script->ArgumentSets.RemoveAt(Index);
	}
	else
	{
		Script->ArgumentSets[Index] = Arguments;
	}
	SaveArguments(Script);
	RefreshAfterArgumentsChanged(Script);
}

FReply SVarnPythonBrowser::RunScript(FVarnPythonBrowserScriptPtr Script, FString Arguments)
{
	if (!Script.IsValid() || !CanRunScripts())
	{
		return FReply::Handled();
	}
	if (!IFileManager::Get().FileExists(*Script->Filename))
	{
		LastResult = LOCTEXT("Missing", "That script no longer exists. Refresh the list.");
		return FReply::Handled();
	}

	if (!VarnPythonTools::IsSupportedScriptPath(Script->Filename))
	{
		LastResult = LOCTEXT("UnsupportedPath", "Cannot run this path: use a lowercase .py extension, no earlier .py in the path, and no double quotes or line breaks. Rename or move the script, then Refresh.");
		UE_LOG(LogVarnPython, Warning, TEXT("Unsupported Python script path: %s"), *Script->Filename);
		return FReply::Handled();
	}

	TGuardValue<bool> RunningGuard(bRunning, true);
	UE_LOG(LogVarnPython, Display, TEXT("Running %s %s"), *Script->Filename, *Arguments);
	FPythonCommandEx Command;
	// This is a filename parsed by Unreal, not interpolated Python source.
	// Quoting the full path preserves spaces; normalized paths use forward slashes.
	Command.Command = FString::Printf(TEXT("\"%s\""), *Script->Filename);
	if (!Arguments.IsEmpty())
	{
		// ExecuteFile treats everything after the quoted filename as sys.argv[1:].
		Command.Command += TEXT(" ") + Arguments;
	}
	Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	Command.FileExecutionScope = EPythonFileExecutionScope::Private;
	const bool bSuccess = IPythonScriptPlugin::Get()->ExecPythonCommandEx(Command);
	LastResult = FText::Format(
		bSuccess
		? LOCTEXT("Succeeded", "Finished: {0}. See Output Log for output.")
		: LOCTEXT("Failed", "Failed: {0}. See Output Log for the Python error."),
		FText::FromString(Script->DisplayName));
	return FReply::Handled();
}

FReply SVarnPythonBrowser::OnRefreshClicked()
{
	RefreshScripts();
	return FReply::Handled();
}

void SVarnPythonBrowser::ToggleFolderView()
{
	bShowHierarchy = !bShowHierarchy;
	FilterScripts();
}

FText SVarnPythonBrowser::GetSummary() const
{
	return FText::Format(
		LOCTEXT("Count", "{0} of {1} scripts"),
		FText::AsNumber(NumShownScripts), FText::AsNumber(AllScripts.Num()));
}

#undef LOCTEXT_NAMESPACE
