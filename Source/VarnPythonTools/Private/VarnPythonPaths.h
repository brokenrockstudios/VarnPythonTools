// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"

namespace VarnPythonTools
{
	inline FString NormalizePath(const FString& Path)
	{
		FString Result = FPaths::IsRelative(Path)
			? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Path)
			: Path;
		FPaths::NormalizeFilename(Result);
		const bool bUNCPath = Result.StartsWith(TEXT("//"));
		FPaths::CollapseRelativeDirectories(Result);
		FPaths::RemoveDuplicateSlashes(Result);
		// Duplicate-slash removal also collapses the UNC server prefix.
		if (bUNCPath)
		{
			Result = TEXT("/") + Result;
		}
		return Result;
	}

	inline bool IsSupportedScriptPath(const FString& Filename)
	{
		// UE 5.8 parses the first case-sensitive ".py" as the filename end.
		// Reject ambiguous paths rather than letting ExecuteFile fall back to code.
		const int32 ExtensionIndex = Filename.Find(TEXT(".py"), ESearchCase::CaseSensitive);
		return ExtensionIndex != INDEX_NONE && ExtensionIndex == Filename.Len() - 3
			&& !Filename.Contains(TEXT("\"")) && !Filename.Contains(TEXT("\n"))
			&& !Filename.Contains(TEXT("\r"));
	}
}
