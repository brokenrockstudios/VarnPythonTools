// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "VarnPythonScriptSources.h"

#include "VarnPythonPaths.h"
#include "VarnPythonToolsSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

namespace VarnPythonTools
{
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

		for (const FDirectoryPath& Directory : Settings->AdditionalScriptDirectories)
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

	FIgnoreList GatherIgnoreList()
	{
		const UVarnPythonToolsSettings* Settings = GetDefault<UVarnPythonToolsSettings>();
		FIgnoreList Ignored;
		for (const FDirectoryPath& Directory : Settings->IgnoredDirectories)
		{
			if (!Directory.Path.IsEmpty())
			{
				Ignored.Directories.Add(NormalizePath(Directory.Path));
			}
		}
		for (const FFilePath& File : Settings->IgnoredFiles)
		{
			if (!File.FilePath.IsEmpty())
			{
				Ignored.Files.Add(PathKey(NormalizePath(File.FilePath)));
			}
		}
		return Ignored;
	}

	bool IsIgnored(const FString& Filename, const FIgnoreList& Ignored)
	{
		if (Ignored.Files.Contains(PathKey(Filename)))
		{
			return true;
		}
		for (const FString& Directory : Ignored.Directories)
		{
			if (IsPathUnderDirectory(Filename, Directory))
			{
				return true;
			}
		}
		return false;
	}

	TArray<FScriptFile> FindScriptFiles(const TArray<FRoot>& Roots)
	{
		const FIgnoreList Ignored = GatherIgnoreList();
		TArray<FScriptFile> Scripts;
		TSet<FString> SeenFiles;

		for (const FRoot& Root : Roots)
		{
			if (!IFileManager::Get().DirectoryExists(*Root.Path))
			{
				continue;
			}

			TArray<FString> Files;
			IFileManager::Get().FindFilesRecursive(Files, *Root.Path, TEXT("*.py"), true, false);
			for (const FString& File : Files)
			{
				const FString Filename = NormalizePath(File);
				if (IsIgnored(Filename, Ignored) || SeenFiles.Contains(PathKey(Filename)))
				{
					continue;
				}
				SeenFiles.Add(PathKey(Filename));

				FScriptFile& Script = Scripts.AddDefaulted_GetRef();
				Script.Filename = Filename;
				Script.RootPath = Root.Path;
				Script.RootLabel = Root.Label;
				Script.RelativePath = Filename;
				FPaths::MakePathRelativeTo(Script.RelativePath, *(Root.Path + TEXT("/")));
			}
		}
		return Scripts;
	}
}
