// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "VarnPythonToolsSettings.generated.h"

// No DefaultConfig: these preferences belong in Saved/Config, not Config/Default*.ini.
UCLASS(Config = EditorPerProjectUserSettings)
class UVarnPythonToolsSettings : public UObject
{
	GENERATED_BODY()
public:
	/** Personal search roots. Relative paths are resolved against the project directory.
	 *  These affect discovery only; they do not change Python's global import paths.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Search Paths", meta = (RelativePath))
	TArray<FDirectoryPath> AdditionalScriptDirectories;

	/** When off, scripts under the engine directory (Engine/Content/Python and engine plugins) are skipped. */
	UPROPERTY(EditAnywhere, Config, Category = "Filtering")
	bool bIncludeEngineScripts = true;

	/** Keep this plugin's own Content/Python (e.g. hello_world.py) listed even when it is installed under the engine
	 *  directory and Include Engine Scripts is off.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Filtering", meta = (EditCondition = "!bIncludeEngineScripts"))
	bool bAlwaysIncludeVarnPythonScripts = true;

	/** Scripts under these folders (recursively) are hidden from the browser. Relative paths resolve against the project directory. */
	UPROPERTY(EditAnywhere, Config, Category = "Filtering", meta = (RelativePath))
	TArray<FDirectoryPath> IgnoredDirectories;

	/** These individual scripts are hidden from the browser. Relative paths resolve against the project directory. */
	UPROPERTY(EditAnywhere, Config, Category = "Filtering", meta = (RelativePath, FilePathFilter = "py"))
	TArray<FFilePath> IgnoredFiles;

	/** Per-script run arguments, keyed by normalized script path. Edited from the browser's right-click menu, not shown in settings. */
	UPROPERTY(Config)
	TMap<FString, FString> ScriptArguments;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};
