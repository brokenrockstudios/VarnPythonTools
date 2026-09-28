// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SVarnPythonBrowser.h"

#include "Editor.h"
#include "IPythonScriptPlugin.h"
#include "ISettingsModule.h"
#include "PythonScriptTypes.h"
#include "VarnPythonPaths.h"
#include "VarnPythonToolsSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Templates/UnrealTemplate.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "VarnPythonBrowser"

DEFINE_LOG_CATEGORY_STATIC(LogVarnPython, Log, All);

namespace VarnPythonTools
{
	struct FRoot
	{
		FString Path;
		FString Label;
	};

	FString PathKey(const FString& Path)
	{
		// Windows paths are case-insensitive; preserve case on other platforms.
#if PLATFORM_WINDOWS
		return Path.ToLower();
#else
		return Path;
#endif
	}

	TArray<FRoot> GatherRoots()
	{
		const UVarnPythonToolsSettings* Settings = GetDefault<UVarnPythonToolsSettings>();
		const bool bIncludeEngine = Settings->bIncludeEngineScripts;
		const FString EngineDir = NormalizePath(FPaths::EngineDir());

		TArray<FRoot> Roots;
		TSet<FString> Seen;
		auto Add = [&Roots, &Seen](const FString& Path, const FString& Label)
		{
			if (Path.IsEmpty())
			{
				return;
			}
			FString Normalized = NormalizePath(Path);
			FPaths::NormalizeDirectoryName(Normalized);
			const FString Key = PathKey(Normalized);
			if (!Seen.Contains(Key))
			{
				Seen.Add(Key);
				Roots.Add({Normalized, Label});
			}
		};

		Add(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Python")), TEXT("Project"));
		Add(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Python")), TEXT("Local project"));
		Add(FPaths::Combine(FPlatformProcess::UserDir(), TEXT("Unreal Projects/Python")), TEXT("User (Unreal Projects)"));
		Add(FPaths::Combine(FPlatformProcess::UserDir(), TEXT("UnrealEngine/Python")), TEXT("User (UnrealEngine)"));

		for (const FDirectoryPath& Directory : GetDefault<UVarnPythonToolsSettings>()->AdditionalScriptDirectories)
		{
			Add(Directory.Path, TEXT("Personal"));
		}

		TArray<TSharedRef<IPlugin>> Plugins = IPluginManager::Get().GetEnabledPlugins();
		Plugins.Sort(
			[](const TSharedRef<IPlugin>& A, const TSharedRef<IPlugin>& B)
			{
				return A->GetName() < B->GetName();
			});
		for (const TSharedRef<IPlugin>& Plugin : Plugins)
		{
			const bool bIsThisPlugin = Plugin->GetName() == TEXT("VarnPythonTools");
			if (!bIncludeEngine && IsPathUnderDirectory(NormalizePath(Plugin->GetBaseDir()), EngineDir)
				&& !(bIsThisPlugin && Settings->bAlwaysIncludeVarnPythonScripts))
			{
				continue;
			}
			Add(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/Python")), Plugin->GetName());
		}
		if (bIncludeEngine)
		{
			Add(FPaths::Combine(FPaths::EngineContentDir(), TEXT("Python")), TEXT("Engine"));
		}
		return Roots;
	}

	bool IsIgnored(const FString& Filename, const TArray<FString>& IgnoredDirs, const TSet<FString>& IgnoredFiles)
	{
		if (IgnoredFiles.Contains(PathKey(Filename)))
		{
			return true;
		}
		for (const FString& Directory : IgnoredDirs)
		{
			if (IsPathUnderDirectory(Filename, Directory))
			{
				return true;
			}
		}
		return false;
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
				+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("Refresh", "Refresh"))
					.ToolTipText(LOCTEXT("RefreshTip", "Rescan script folders. Hover over the script count to see search locations."))
					.IsEnabled_Lambda([this] { return !bRunning; })
					.OnClicked(this, &SVarnPythonBrowser::OnRefreshClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Settings", "Settings"))
					.ToolTipText(LOCTEXT("SettingsTip", "Configure personal script folders in Editor Preferences. Refresh after changing them."))
					.OnClicked(this, &SVarnPythonBrowser::OnSettingsClicked)
				]
			]
			+ SVerticalBox::Slot().FillHeight(1)
			[
				SAssignNew(ScriptList, SListView<FVarnPythonBrowserScriptPtr>)
				.ListItemsSource(&FilteredScripts)
				.SelectionMode(ESelectionMode::None)
				.OnGenerateRow(this, &SVarnPythonBrowser::GenerateRow)
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
	TSet<FString> SeenFiles;
	FString RootDescription;

	const UVarnPythonToolsSettings* Settings = GetDefault<UVarnPythonToolsSettings>();
	TArray<FString> IgnoredDirs;
	for (const FDirectoryPath& Directory : Settings->IgnoredDirectories)
	{
		if (!Directory.Path.IsEmpty())
		{
			IgnoredDirs.Add(VarnPythonTools::NormalizePath(Directory.Path));
		}
	}
	TSet<FString> IgnoredFiles;
	for (const FFilePath& File : Settings->IgnoredFiles)
	{
		if (!File.FilePath.IsEmpty())
		{
			IgnoredFiles.Add(VarnPythonTools::PathKey(VarnPythonTools::NormalizePath(File.FilePath)));
		}
	}

	for (const VarnPythonTools::FRoot& Root : VarnPythonTools::GatherRoots())
	{
		const bool bExists = IFileManager::Get().DirectoryExists(*Root.Path);
		RootDescription += FString::Printf(
			TEXT("%s: %s%s\n"), *Root.Label, *Root.Path,
			bExists ? TEXT("") : TEXT(" (not created)"));
		if (!bExists)
		{
			continue;
		}

		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Root.Path, TEXT("*.py"), true, false);
		for (const FString& File : Files)
		{
			const FString Filename = VarnPythonTools::NormalizePath(File);
			const FString BaseName = FPaths::GetCleanFilename(Filename);
			if (BaseName.Equals(TEXT("__init__.py"), ESearchCase::IgnoreCase) || BaseName.Equals(TEXT("init_unreal.py"), ESearchCase::IgnoreCase))
			{
				continue; // Package initializers and editor startup hooks aren't launcher entries.
			}
			if (VarnPythonTools::IsIgnored(Filename, IgnoredDirs, IgnoredFiles))
			{
				continue;
			}

			const FString Key = VarnPythonTools::PathKey(Filename);
			if (SeenFiles.Contains(Key))
			{
				continue;
			}
			SeenFiles.Add(Key);

			FVarnPythonBrowserScriptPtr Script = MakeShared<FVarnPythonBrowserScript>();
			Script->Filename = Filename;
			Script->DisplayName = BaseName;
			Script->Source = Root.Label;
			Script->RelativePath = Filename;
			FPaths::MakePathRelativeTo(Script->RelativePath, *(Root.Path + TEXT("/")));
			AllScripts.Add(MoveTemp(Script));
		}
	}
	RootsTooltip = FText::FromString(RootDescription);
	AllScripts.Sort(
		[](const FVarnPythonBrowserScriptPtr& A, const FVarnPythonBrowserScriptPtr& B)
		{
			const int32 NameOrder = A->DisplayName.Compare(B->DisplayName, ESearchCase::IgnoreCase);
			return NameOrder == 0 ? A->Filename < B->Filename : NameOrder < 0;
		});
	FilterScripts();
}

void SVarnPythonBrowser::FilterScripts()
{
	FilteredScripts.Reset();
	for (const FVarnPythonBrowserScriptPtr& Script : AllScripts)
	{
		if (SearchText.IsEmpty() || Script->Filename.Contains(SearchText) || Script->Source.Contains(SearchText))
		{
			FilteredScripts.Add(Script);
		}
	}
	if (ScriptList.IsValid())
	{
		ScriptList->RequestListRefresh();
	}
}

void SVarnPythonBrowser::OnSearchChanged(const FText& Text)
{
	SearchText = Text.ToString().TrimStartAndEnd();
	FilterScripts();
}

TSharedRef<ITableRow> SVarnPythonBrowser::GenerateRow(FVarnPythonBrowserScriptPtr Script, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(STableRow<FVarnPythonBrowserScriptPtr>, Owner)
		.Padding(4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Script->DisplayName))
					.ToolTipText(FText::FromString(Script->Filename))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Script->Source + TEXT(" / ") + Script->RelativePath))
					.ToolTipText(FText::FromString(Script->Filename))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
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
}

bool SVarnPythonBrowser::CanRunScripts() const
{
	IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
	return !bRunning && Python && Python->IsPythonAvailable() && Python->IsPythonInitialized()
		&& GEditor && !GEditor->PlayWorld;
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
	UE_LOG(LogVarnPython, Display, TEXT("Running %s"), *Script->Filename);
	FPythonCommandEx Command;
	// This is a filename parsed by Unreal, not interpolated Python source.
	// Quoting the full path preserves spaces; normalized paths use forward slashes.
	Command.Command = FString::Printf(TEXT("\"%s\""), *Script->Filename);
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

FReply SVarnPythonBrowser::OnSettingsClicked()
{
	if (ISettingsModule* Settings = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings")))
	{
		Settings->ShowViewer(TEXT("Editor"), TEXT("Plugins"), TEXT("VarnPythonBrowser"));
	}
	return FReply::Handled();
}

FText SVarnPythonBrowser::GetSummary() const
{
	return FText::Format(
		LOCTEXT("Count", "{0} of {1} scripts"),
		FText::AsNumber(FilteredScripts.Num()), FText::AsNumber(AllScripts.Num()));
}

#undef LOCTEXT_NAMESPACE
