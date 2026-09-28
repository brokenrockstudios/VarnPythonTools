// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "VarnPythonToolsSettings.h"

void UVarnPythonToolsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	SaveConfig();
}
