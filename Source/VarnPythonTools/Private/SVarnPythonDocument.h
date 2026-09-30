// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

/** Read-only view of one script: a line-number gutter beside monospaced, selectable text. */
class SVarnPythonDocument : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVarnPythonDocument)
		{
		}

		SLATE_ARGUMENT(FString, Filename)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// The text widgets keep a pointer to this style, so it must live as long as they do.
	FTextBlockStyle CodeStyle;
};
