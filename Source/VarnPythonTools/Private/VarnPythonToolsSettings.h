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

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};
