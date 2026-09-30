// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "VarnPythonSyntax.h"

namespace VarnPythonTools
{
	namespace
	{
		bool IsDigit(const TCHAR C)
		{
			return C >= TEXT('0') && C <= TEXT('9');
		}

		bool IsHexDigit(const TCHAR C)
		{
			return IsDigit(C) || (C >= TEXT('a') && C <= TEXT('f')) || (C >= TEXT('A') && C <= TEXT('F'));
		}

		// Non-ASCII characters count as identifier characters, as Python allows.
		bool IsIdentifierStart(const TCHAR C)
		{
			return (C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('A') && C <= TEXT('Z')) || C == TEXT('_') || C > 127;
		}

		bool IsIdentifierChar(const TCHAR C)
		{
			return IsIdentifierStart(C) || IsDigit(C);
		}

		bool IsQuote(const TCHAR C)
		{
			return C == TEXT('"') || C == TEXT('\'');
		}

		bool IsKeyword(const FStringView Word)
		{
			static const FStringView Keywords[] = {
				TEXTVIEW("False"), TEXTVIEW("None"), TEXTVIEW("True"), TEXTVIEW("and"), TEXTVIEW("as"),
				TEXTVIEW("assert"), TEXTVIEW("async"), TEXTVIEW("await"), TEXTVIEW("break"), TEXTVIEW("class"),
				TEXTVIEW("continue"), TEXTVIEW("def"), TEXTVIEW("del"), TEXTVIEW("elif"), TEXTVIEW("else"),
				TEXTVIEW("except"), TEXTVIEW("finally"), TEXTVIEW("for"), TEXTVIEW("from"), TEXTVIEW("global"),
				TEXTVIEW("if"), TEXTVIEW("import"), TEXTVIEW("in"), TEXTVIEW("is"), TEXTVIEW("lambda"),
				TEXTVIEW("nonlocal"), TEXTVIEW("not"), TEXTVIEW("or"), TEXTVIEW("pass"), TEXTVIEW("raise"),
				TEXTVIEW("return"), TEXTVIEW("try"), TEXTVIEW("while"), TEXTVIEW("with"), TEXTVIEW("yield")
			};
			for (const FStringView Keyword : Keywords)
			{
				if (Word.Equals(Keyword, ESearchCase::CaseSensitive))
				{
					return true;
				}
			}
			return false;
		}

		// r, u, b, f and their two-letter combinations, in either case, when directly followed by a quote.
		bool IsStringPrefix(const FStringView Word)
		{
			if (Word.Len() < 1 || Word.Len() > 2)
			{
				return false;
			}
			for (int32 Index = 0; Index < Word.Len(); ++Index)
			{
				switch (Word[Index])
				{
				case TEXT('r'): case TEXT('R'): case TEXT('u'): case TEXT('U'):
				case TEXT('b'): case TEXT('B'): case TEXT('f'): case TEXT('F'):
					break;
				default:
					return false;
				}
			}
			return true;
		}

		TCHAR CharAt(const FString& Source, const int32 Index)
		{
			return Index >= 0 && Index < Source.Len() ? Source[Index] : TEXT('\0');
		}

		// Returns the index just past the string whose opening quote is at QuotePos. A single-quoted string
		// that is not closed ends at the line break; a triple-quoted one ends at the end of the text.
		int32 ScanString(const FString& Source, const int32 QuotePos)
		{
			const int32 Len = Source.Len();
			const TCHAR Quote = Source[QuotePos];
			const bool bTriple = CharAt(Source, QuotePos + 1) == Quote && CharAt(Source, QuotePos + 2) == Quote;

			int32 Index = QuotePos + (bTriple ? 3 : 1);
			while (Index < Len)
			{
				const TCHAR C = Source[Index];
				if (C == TEXT('\\'))
				{
					Index += 2; // Whatever follows is escaped, including the quote and a line break.
				}
				else if (bTriple)
				{
					if (C == Quote && CharAt(Source, Index + 1) == Quote && CharAt(Source, Index + 2) == Quote)
					{
						return Index + 3;
					}
					++Index;
				}
				else if (C == Quote)
				{
					return Index + 1;
				}
				else if (C == TEXT('\n'))
				{
					return Index;
				}
				else
				{
					++Index;
				}
			}
			return FMath::Min(Index, Len);
		}

		// Returns the index just past the number starting at Start: 0x1F, 1_000, 3.14, .5, 1e-3, 2j.
		int32 ScanNumber(const FString& Source, const int32 Start)
		{
			int32 Index = Start;
			const auto IsDigitOrSeparator = [&Source](const int32 At)
			{
				return IsDigit(CharAt(Source, At)) || CharAt(Source, At) == TEXT('_');
			};

			const TCHAR Radix = CharAt(Source, Index + 1);
			if (CharAt(Source, Index) == TEXT('0')
				&& (Radix == TEXT('x') || Radix == TEXT('X') || Radix == TEXT('b') || Radix == TEXT('B') || Radix == TEXT('o') || Radix == TEXT('O')))
			{
				Index += 2;
				while (IsHexDigit(CharAt(Source, Index)) || CharAt(Source, Index) == TEXT('_'))
				{
					++Index;
				}
				return Index;
			}

			while (IsDigitOrSeparator(Index))
			{
				++Index;
			}
			// A dot belongs to the number unless it starts an attribute (1.real) or is part of "..".
			if (CharAt(Source, Index) == TEXT('.') && !IsIdentifierStart(CharAt(Source, Index + 1)) && CharAt(Source, Index + 1) != TEXT('.'))
			{
				++Index;
				while (IsDigitOrSeparator(Index))
				{
					++Index;
				}
			}
			if (CharAt(Source, Index) == TEXT('e') || CharAt(Source, Index) == TEXT('E'))
			{
				int32 Exponent = Index + 1;
				if (CharAt(Source, Exponent) == TEXT('+') || CharAt(Source, Exponent) == TEXT('-'))
				{
					++Exponent;
				}
				if (IsDigit(CharAt(Source, Exponent)))
				{
					Index = Exponent;
					while (IsDigitOrSeparator(Index))
					{
						++Index;
					}
				}
			}
			if (CharAt(Source, Index) == TEXT('j') || CharAt(Source, Index) == TEXT('J'))
			{
				++Index;
			}
			return Index;
		}
	}

	TArray<FPythonSpan> ScanPython(const FString& Source)
	{
		TArray<FPythonSpan> Spans;
		const int32 Len = Source.Len();
		const auto Add = [&Spans](const int32 Begin, const int32 End, const EPythonToken Kind)
		{
			Spans.Add({Begin, End, Kind});
		};

		int32 Pos = 0;
		// Only whitespace since the last line break: a leading @ is a decorator, not matrix multiplication.
		bool bLineStart = true;
		// The previous token was a dot, so a name here is an attribute.
		bool bAfterDot = false;
		// The previous token was def or class, so a name here is being defined.
		bool bExpectName = false;
		EPythonToken ExpectedNameKind = EPythonToken::Function;

		while (Pos < Len)
		{
			const TCHAR C = Source[Pos];

			if (C == TEXT('\n'))
			{
				bLineStart = true;
				bAfterDot = false;
				bExpectName = false;
				++Pos;
				continue;
			}
			if (C == TEXT(' ') || C == TEXT('\t') || C == TEXT('\r') || C == TEXT('\f') || C == TEXT('\v'))
			{
				++Pos;
				continue;
			}

			if (C == TEXT('#'))
			{
				int32 End = Pos + 1;
				while (End < Len && Source[End] != TEXT('\n') && Source[End] != TEXT('\r'))
				{
					++End;
				}
				Add(Pos, End, EPythonToken::Comment);
				Pos = End;
				continue;
			}

			bool bIsLiteral = false;
			int32 End = Pos;
			if (IsQuote(C))
			{
				End = ScanString(Source, Pos);
				Add(Pos, End, EPythonToken::String);
				bIsLiteral = true;
			}
			else if (IsDigit(C) || (C == TEXT('.') && IsDigit(CharAt(Source, Pos + 1))
				&& !(IsIdentifierChar(CharAt(Source, Pos - 1)) || CharAt(Source, Pos - 1) == TEXT(')') || CharAt(Source, Pos - 1) == TEXT(']'))))
			{
				End = ScanNumber(Source, Pos);
				Add(Pos, End, EPythonToken::Number);
				bIsLiteral = true;
			}
			if (bIsLiteral)
			{
				bLineStart = bAfterDot = bExpectName = false;
				Pos = End;
				continue;
			}

			if (IsIdentifierStart(C))
			{
				End = Pos + 1;
				while (End < Len && IsIdentifierChar(Source[End]))
				{
					++End;
				}
				const FStringView Word(*Source + Pos, End - Pos);

				if (IsQuote(CharAt(Source, End)) && IsStringPrefix(Word))
				{
					End = ScanString(Source, End);
					Add(Pos, End, EPythonToken::String);
					bLineStart = bAfterDot = bExpectName = false;
					Pos = End;
					continue;
				}

				int32 Next = End;
				while (CharAt(Source, Next) == TEXT(' ') || CharAt(Source, Next) == TEXT('\t'))
				{
					++Next;
				}
				const bool bIsCalled = CharAt(Source, Next) == TEXT('(');

				const bool bWasExpectingName = bExpectName;
				const bool bWasAfterDot = bAfterDot;
				bLineStart = bAfterDot = bExpectName = false;

				// Keywords come first so that "from . import x" still colors import.
				if (IsKeyword(Word))
				{
					Add(Pos, End, EPythonToken::Keyword);
					if (Word.Equals(TEXTVIEW("def"), ESearchCase::CaseSensitive))
					{
						bExpectName = true;
						ExpectedNameKind = EPythonToken::Function;
					}
					else if (Word.Equals(TEXTVIEW("class"), ESearchCase::CaseSensitive))
					{
						bExpectName = true;
						ExpectedNameKind = EPythonToken::Class;
					}
				}
				else if (bWasExpectingName)
				{
					Add(Pos, End, ExpectedNameKind);
				}
				else if (bWasAfterDot)
				{
					Add(Pos, End, bIsCalled ? EPythonToken::Function : EPythonToken::Member);
				}
				else if (Word.Equals(TEXTVIEW("self"), ESearchCase::CaseSensitive) || Word.Equals(TEXTVIEW("cls"), ESearchCase::CaseSensitive))
				{
					Add(Pos, End, EPythonToken::Member);
				}
				else if (bIsCalled)
				{
					Add(Pos, End, EPythonToken::Function);
				}
				Pos = End;
				continue;
			}

			if (C == TEXT('@') && bLineStart)
			{
				End = Pos + 1;
				while (End < Len && (IsIdentifierChar(Source[End]) || Source[End] == TEXT('.')))
				{
					++End;
				}
				Add(Pos, End, EPythonToken::Decorator);
				bLineStart = bAfterDot = bExpectName = false;
				Pos = End;
				continue;
			}

			// A dot makes the next name an attribute; any other punctuation ends that expectation.
			bLineStart = false;
			bAfterDot = C == TEXT('.');
			bExpectName = false;
			++Pos;
		}
		return Spans;
	}
}
