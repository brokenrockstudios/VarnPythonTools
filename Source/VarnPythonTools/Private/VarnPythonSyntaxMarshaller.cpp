// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "VarnPythonSyntaxMarshaller.h"

#include "VarnPythonSyntax.h"
#include "Framework/Text/IRun.h"
#include "Framework/Text/SlateTextLayout.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/TextLayout.h"

namespace
{
	using VarnPythonTools::EPythonToken;

	// VS Code Dark+ colors. Keyword, string, number and comment match the editor's SyntaxHighlight.SourceCode styles.
	FLinearColor GetTokenColor(const EPythonToken Kind)
	{
		switch (Kind)
		{
		case EPythonToken::Keyword: return FLinearColor(FColor(86, 156, 214));
		case EPythonToken::String: return FLinearColor(FColor(214, 157, 133));
		case EPythonToken::Comment: return FLinearColor(FColor(87, 166, 74));
		case EPythonToken::Number: return FLinearColor(FColor(181, 206, 168));
		case EPythonToken::Decorator: return FLinearColor(FColor(215, 186, 125));
		case EPythonToken::Function: return FLinearColor(FColor(220, 220, 170));
		case EPythonToken::Class: return FLinearColor(FColor(78, 201, 176));
		case EPythonToken::Member: return FLinearColor(FColor(156, 220, 254));
		default: return FLinearColor::White;
		}
	}
}

TSharedRef<FVarnPythonSyntaxMarshaller> FVarnPythonSyntaxMarshaller::Create()
{
	return MakeShareable(new FVarnPythonSyntaxMarshaller());
}

void FVarnPythonSyntaxMarshaller::SetText(const FString& SourceString, FTextLayout& TargetTextLayout)
{
	const FTextBlockStyle& DefaultStyle = static_cast<FSlateTextLayout&>(TargetTextLayout).GetDefaultTextStyle();

	constexpr int32 KindCount = static_cast<int32>(EPythonToken::Count);
	FTextBlockStyle KindStyles[KindCount];
	for (int32 Kind = 0; Kind < KindCount; ++Kind)
	{
		KindStyles[Kind] = FTextBlockStyle(DefaultStyle).SetColorAndOpacity(GetTokenColor(static_cast<EPythonToken>(Kind)));
	}

	TArray<FTextRange> LineRanges;
	FTextRange::CalculateLineRangesFromString(SourceString, LineRanges);
	const TArray<VarnPythonTools::FPythonSpan> Spans = VarnPythonTools::ScanPython(SourceString);

	TArray<FTextLayout::FNewLineData> LinesToAdd;
	LinesToAdd.Reserve(LineRanges.Num());

	int32 FirstSpan = 0;
	for (const FTextRange& LineRange : LineRanges)
	{
		const TSharedRef<FString> LineText = MakeShareable(new FString(SourceString.Mid(LineRange.BeginIndex, LineRange.Len())));
		TArray<TSharedRef<IRun>> Runs;

		// Ranges passed to AddRun are relative to the line, as the run's text is just this line.
		const auto AddRun = [&Runs, &LineText, &LineRange](const int32 Begin, const int32 End, const FTextBlockStyle& Style)
		{
			Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, Style, FTextRange(Begin - LineRange.BeginIndex, End - LineRange.BeginIndex)));
		};

		// A span that started on an earlier line (triple-quoted string) can still be running, so it is not consumed here.
		while (FirstSpan < Spans.Num() && Spans[FirstSpan].End <= LineRange.BeginIndex)
		{
			++FirstSpan;
		}

		int32 Cursor = LineRange.BeginIndex;
		for (int32 SpanIndex = FirstSpan; SpanIndex < Spans.Num() && Spans[SpanIndex].Begin < LineRange.EndIndex; ++SpanIndex)
		{
			const VarnPythonTools::FPythonSpan& Span = Spans[SpanIndex];
			const int32 Begin = FMath::Max(Span.Begin, LineRange.BeginIndex);
			const int32 End = FMath::Min(Span.End, LineRange.EndIndex);
			if (Begin > Cursor)
			{
				AddRun(Cursor, Begin, DefaultStyle);
			}
			if (End > Begin)
			{
				AddRun(Begin, End, KindStyles[static_cast<int32>(Span.Kind)]);
				Cursor = End;
			}
		}
		if (Cursor < LineRange.EndIndex)
		{
			AddRun(Cursor, LineRange.EndIndex, DefaultStyle);
		}
		if (Runs.IsEmpty())
		{
			// Like the plain layout, an empty line still gets a run so it keeps its height.
			Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, DefaultStyle));
		}

		LinesToAdd.Emplace(LineText, MoveTemp(Runs));
	}

	TargetTextLayout.AddLines(LinesToAdd);
}
