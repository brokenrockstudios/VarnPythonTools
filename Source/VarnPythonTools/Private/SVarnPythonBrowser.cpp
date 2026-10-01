// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SVarnPythonBrowser.h"

#include "Editor.h"
#include "IPythonScriptPlugin.h"
#include "PythonScriptTypes.h"
#include "VarnPythonScriptSources.h"
#include "VarnPythonPaths.h"
#include "VarnPythonToolsSettings.h"
#include "HAL/FileManager.h"
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
		if (const FString* SavedArguments = Settings->ScriptArguments.Find(VarnPythonTools::PathKey(File.Filename)))
		{
			Script->Arguments = *SavedArguments;
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
	const FVarnPythonBrowserScriptPtr Script = FindScript(Node);
	if (!Script.IsValid())
	{
		return GenerateFolderRow(Node, Owner);
	}

	TSharedPtr<SEditableTextBox> ArgumentsBox;
	TSharedRef<ITableRow> Row = SNew(STableRow<FVarnPythonBrowserNodePtr>, Owner)
		.Padding(4)
		.ToolTipText(FText::FromString(Script->Filename))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					// Only the flat list sets a location; in the folder view the folder rows already say where a script lives.
					VarnPythonTools::MakeNameAndLocation(SNew(STextBlock).Text(FText::FromString(Script->DisplayName)), Node->Location)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
				[
					// Appears under the name while editing; Enter or clicking away saves, Escape cancels.
					SAssignNew(ArgumentsBox, SEditableTextBox)
					.Visibility_Lambda([this, Script] { return EditingScript == Script ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text_Lambda([this] { return ArgumentsEditBuffer; })
					.OnTextChanged_Lambda([this](const FText& Text) { ArgumentsEditBuffer = Text; })
					.OnTextCommitted(this, &SVarnPythonBrowser::OnArgumentsCommitted, Script)
					.HintText(LOCTEXT("ArgumentsHint", "Run arguments, e.g. --dry-run \"some value\""))
					.SelectAllTextWhenFocused(true)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Info"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.ToolTipText_Lambda([Script] { return VarnPythonTools::GetScriptInfoTooltip(Script); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SImage)
				.Visibility_Lambda([Script] { return Script->Arguments.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
				.Image(FAppStyle::GetBrush("Icons.Settings"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.ToolTipText_Lambda([Script]
				{
					return FText::Format(LOCTEXT("ArgumentsTip", "Run arguments:\n{0}\n\nRight-click the script to edit."), FText::FromString(Script->Arguments));
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ToolTipText(FText::Format(LOCTEXT("RunTip", "Run {0}\nRequires Python to be ready and PIE to be stopped."), FText::FromString(Script->Filename)))
				.IsEnabled(this, &SVarnPythonBrowser::CanRunScripts)
				.OnClicked(this, &SVarnPythonBrowser::RunScript, Script)
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
				]
			]
		];
	Script->ArgumentsEditor = ArgumentsBox;
	return Row;
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

FVarnPythonBrowserScriptPtr SVarnPythonBrowser::FindScript(const FVarnPythonBrowserNodePtr& Node) const
{
	if (Node->Kind != VarnPythonTools::FScriptTreeNode::EKind::File)
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
	const FVarnPythonBrowserScriptPtr Script = Selected.IsEmpty() ? FVarnPythonBrowserScriptPtr() : FindScript(Selected[0]);
	if (!Script.IsValid())
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("EditArguments", "Edit Run Arguments..."),
		LOCTEXT("EditArgumentsTip", "Edit the command-line arguments passed to this script (sys.argv) when it is run."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
		FUIAction(FExecuteAction::CreateSP(this, &SVarnPythonBrowser::BeginEditArguments, Script)));
	return MenuBuilder.MakeWidget();
}

void SVarnPythonBrowser::BeginEditArguments(FVarnPythonBrowserScriptPtr Script)
{
	EditingScript = Script;
	ArgumentsEditBuffer = FText::FromString(Script->Arguments);

	// The editor is collapsed until this frame's visibility pass, so focus it on the next tick.
	TWeakPtr<SWidget> Editor = Script->ArgumentsEditor;
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda(
		[Editor](double, float)
		{
			if (TSharedPtr<SWidget> Widget = Editor.Pin())
			{
				FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::SetDirectly);
			}
			return EActiveTimerReturnType::Stop;
		}));
}

void SVarnPythonBrowser::OnArgumentsCommitted(const FText& Text, ETextCommit::Type CommitType, FVarnPythonBrowserScriptPtr Script)
{
	// Collapsing the editor fires a second commit from the focus loss; ignore it.
	if (EditingScript != Script)
	{
		return;
	}
	EditingScript.Reset();
	if (CommitType == ETextCommit::OnCleared)
	{
		return; // Escape cancels.
	}

	Script->Arguments = Text.ToString().TrimStartAndEnd();
	Script->Arguments.ReplaceInline(TEXT("\r"), TEXT(" "));
	Script->Arguments.ReplaceInline(TEXT("\n"), TEXT(" "));
	UVarnPythonToolsSettings* Settings = GetMutableDefault<UVarnPythonToolsSettings>();
	const FString Key = VarnPythonTools::PathKey(Script->Filename);
	if (Script->Arguments.IsEmpty())
	{
		Settings->ScriptArguments.Remove(Key);
	}
	else
	{
		Settings->ScriptArguments.Add(Key, Script->Arguments);
	}
	Settings->SaveConfig();
}

FReply SVarnPythonBrowser::RunScript(FVarnPythonBrowserScriptPtr Script)
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
	UE_LOG(LogVarnPython, Display, TEXT("Running %s %s"), *Script->Filename, *Script->Arguments);
	FPythonCommandEx Command;
	// This is a filename parsed by Unreal, not interpolated Python source.
	// Quoting the full path preserves spaces; normalized paths use forward slashes.
	Command.Command = FString::Printf(TEXT("\"%s\""), *Script->Filename);
	if (!Script->Arguments.IsEmpty())
	{
		// ExecuteFile treats everything after the quoted filename as sys.argv[1:].
		Command.Command += TEXT(" ") + Script->Arguments;
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
