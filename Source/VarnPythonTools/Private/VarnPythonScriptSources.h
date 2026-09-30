// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace VarnPythonTools
{
	struct FRoot
	{
		FString Path;
		FString Label;
	};

	/** A .py file found under one of the search roots. */
	struct FScriptFile
	{
		FString Filename;
		FString RootPath;
		FString RootLabel;
		/** Path relative to its root, using forward slashes. */
		FString RelativePath;
	};

	/** Normalized ignore settings, ready for IsIgnored. */
	struct FIgnoreList
	{
		TArray<FString> Directories;
		TSet<FString> Files;
	};

	/** Key for comparing and de-duplicating paths. Windows paths are case-insensitive; other platforms preserve case. */
	FString PathKey(const FString& Path);

	/** Search roots from the project layout, enabled plugins and settings. Roots need not exist. */
	TArray<FRoot> GatherRoots();

	FIgnoreList GatherIgnoreList();

	bool IsIgnored(const FString& Filename, const FIgnoreList& Ignored);

	/**
	 * Recursively finds .py files under each existing root, skipping ignored ones. A file reachable from several
	 * roots is returned once, under the first root that contains it. Results follow the order of Roots.
	 */
	TArray<FScriptFile> FindScriptFiles(const TArray<FRoot>& Roots);
}
