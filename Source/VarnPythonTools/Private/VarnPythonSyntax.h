// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace VarnPythonTools
{
	enum class EPythonToken : uint8
	{
		Keyword,
		String,
		Comment,
		Number,
		Decorator,
		// A def name, or any name that is called: foo(...).
		Function,
		// A class name in a class statement.
		Class,
		// An attribute after a dot, plus self and cls.
		Member,
		Count
	};

	struct FPythonSpan
	{
		// Half-open range into the scanned string.
		int32 Begin = 0;
		int32 End = 0;
		EPythonToken Kind = EPythonToken::Keyword;
	};

	/**
	 * Finds the parts of Python source worth coloring, in order and without overlap; text between spans is plain.
	 * A span can cross line breaks (triple-quoted strings). This is a lexical pass: it never fails on invalid code,
	 * and it does not tell local variables from globals.
	 */
	TArray<FPythonSpan> ScanPython(const FString& Source);
}
