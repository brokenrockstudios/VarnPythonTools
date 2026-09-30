// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Text/PlainTextLayoutMarshaller.h"

/**
 * Lays out Python source with a color per kind of token (see ScanPython). Every run keeps the text style of the
 * widget it is shown in and only changes the color, so line heights match a plain layout exactly.
 */
class FVarnPythonSyntaxMarshaller : public FPlainTextLayoutMarshaller
{
public:
	static TSharedRef<FVarnPythonSyntaxMarshaller> Create();

	// ITextLayoutMarshaller
	virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout) override;

protected:
	FVarnPythonSyntaxMarshaller() = default;
};
