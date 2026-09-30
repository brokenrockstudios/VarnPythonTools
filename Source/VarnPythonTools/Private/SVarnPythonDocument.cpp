// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SVarnPythonDocument.h"

#include "VarnPythonSyntaxMarshaller.h"
#include "Framework/Text/ITextLayoutMarshaller.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "VarnPythonDocument"

namespace
{
	// Guards the editor thread: laying out very large files as one text block is slow.
	constexpr int64 MaxDisplayBytes = 2 * 1024 * 1024;
	// Coloring splits every line into runs, which costs far more than one plain run per line, so big files stay plain.
	constexpr int32 MaxHighlightChars = 200 * 1024;
	constexpr int32 TabWidth = 4;
}

void SVarnPythonDocument::Construct(const FArguments& InArgs)
{
	CodeStyle = FTextBlockStyle(FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
		.SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 10));

	FString Source;
	FText Error;
	if (IFileManager::Get().FileSize(*InArgs._Filename) > MaxDisplayBytes)
	{
		Error = LOCTEXT("TooLarge", "This file is too large to display.");
	}
	else if (!FFileHelper::LoadFileToString(Source, *InArgs._Filename))
	{
		Error = LOCTEXT("Unreadable", "This file could not be read. It may have been moved or deleted.");
	}

	if (!Error.IsEmpty())
	{
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Error)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
		return;
	}

	// Slate draws tab characters as a single narrow glyph, so expand them; the view is read-only.
	Source.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
	Source.ReplaceInline(TEXT("\r"), TEXT("\n"));
	Source.ReplaceInline(TEXT("\t"), *FString::ChrN(TabWidth, TEXT(' ')));

	int32 LineCount = 1;
	for (const TCHAR Char : Source)
	{
		if (Char == TEXT('\n'))
		{
			++LineCount;
		}
	}
	FString LineNumbers;
	LineNumbers.Reserve(LineCount * 6);
	for (int32 Line = 1; Line <= LineCount; ++Line)
	{
		if (Line > 1)
		{
			LineNumbers += TEXT('\n');
		}
		LineNumbers.AppendInt(Line);
	}

	TSharedPtr<ITextLayoutMarshaller> Marshaller;
	if (Source.Len() <= MaxHighlightChars)
	{
		Marshaller = FVarnPythonSyntaxMarshaller::Create();
	}

	// Vertical scrolling is shared by the gutter and the text so the numbers can never drift from their lines.
	// The horizontal bar is pinned below the scroll area so it stays reachable in tall files.
	const TSharedRef<SScrollBar> HorizontalBar = SNew(SScrollBar).Orientation(Orient_Horizontal);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
		.Padding(0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot().Padding(0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(12, 0)
					[
						SNew(STextBlock)
						.TextStyle(&CodeStyle)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Justification(ETextJustify::Right)
						.Text(FText::FromString(LineNumbers))
					]
					+ SHorizontalBox::Slot().FillWidth(1)
					[
						SNew(SScrollBox)
						.Orientation(Orient_Horizontal)
						.ExternalScrollbar(HorizontalBar)
						// Let the wheel scroll the file vertically even when a long line makes it scrollable sideways.
						.ConsumeMouseWheel(EConsumeMouseWheel::Never)
						+ SScrollBox::Slot()
						[
							SNew(SMultiLineEditableText)
							.TextStyle(&CodeStyle)
							.IsReadOnly(true)
							.AutoWrapText(false)
							.AllowContextMenu(true)
							.Marshaller(Marshaller)
							.Text(FText::FromString(Source))
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				HorizontalBar
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
