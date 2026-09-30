// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "VarnPythonSyntax.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
	FString EscapeLineBreaks(FString Text)
	{
		Text.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		Text.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		return Text;
	}

	// One "Kind[text]" entry per colored span, in order, so a case reads as what would be colored and how.
	FString DescribeSpans(const FString& Source)
	{
		static const TCHAR* const KindNames[] = {
			TEXT("Keyword"), TEXT("String"), TEXT("Comment"), TEXT("Number"),
			TEXT("Decorator"), TEXT("Function"), TEXT("Class"), TEXT("Member")};
		static_assert(UE_ARRAY_COUNT(KindNames) == static_cast<int32>(VarnPythonTools::EPythonToken::Count));

		FString Result;
		int32 PreviousEnd = 0;
		for (const VarnPythonTools::FPythonSpan& Span : VarnPythonTools::ScanPython(Source))
		{
			if (Span.Begin < PreviousEnd || Span.End <= Span.Begin || Span.End > Source.Len())
			{
				return TEXT("INVALID SPAN");
			}
			PreviousEnd = Span.End;

			Result += FString::Printf(
				TEXT("%s%s[%s]"), Result.IsEmpty() ? TEXT("") : TEXT(" "), KindNames[static_cast<int32>(Span.Kind)],
				*EscapeLineBreaks(Source.Mid(Span.Begin, Span.End - Span.Begin)));
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVarnPythonSyntaxTest, "VarnPythonTools.Syntax",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVarnPythonSyntaxTest::RunTest(const FString& Parameters)
{
	const auto Check = [this](const FString& Source, const FString& Expected)
	{
		TestEqual(FString::Printf(TEXT("Spans for: %s"), *EscapeLineBreaks(Source)), DescribeSpans(Source), Expected);
	};

	Check(TEXT("import unreal"),
		TEXT("Keyword[import]"));
	Check(TEXT("from . import x"),
		TEXT("Keyword[from] Keyword[import]"));
	Check(TEXT("import os.path as p"),
		TEXT("Keyword[import] Member[path] Keyword[as]"));
	Check(TEXT("x = 1  # note 'quoted'"),
		TEXT("Number[1] Comment[# note 'quoted']"));
	Check(TEXT("s = 'a#b'"),
		TEXT("String['a#b']"));
	Check(TEXT("x = \"a\\\"b\" + y"),
		TEXT("String[\"a\\\"b\"]"));
	Check(TEXT("e = ''"),
		TEXT("String['']"));
	Check(TEXT("t = '''a\n")
			TEXT("b''' # c"),
		TEXT("String['''a\nb'''] Comment[# c]"));
	Check(TEXT("s = 'abc\n")
			TEXT("t = 2"),
		TEXT("String['abc] Number[2]"));
	Check(TEXT("f\"{x}\" rb'y' R\"z\""),
		TEXT("String[f\"{x}\"] String[rb'y'] String[R\"z\"]"));
	Check(TEXT("bar'x'"),
		TEXT("String['x']"));
	Check(TEXT("0x1F 1_000 3.14 1e-3 .5 2j 10."),
		TEXT("Number[0x1F] Number[1_000] Number[3.14] Number[1e-3] Number[.5] Number[2j] Number[10.]"));
	Check(TEXT("x1 = y2"),
		TEXT(""));
	Check(TEXT("a.b.c(1)"),
		TEXT("Member[b] Function[c] Number[1]"));
	Check(TEXT("@unreal.uclass()\n")
			TEXT("class Foo(Base):\n")
			TEXT("    @staticmethod\n")
			TEXT("    def run(self, x):\n")
			TEXT("        return self.value"),
		TEXT("Decorator[@unreal.uclass] Keyword[class] Class[Foo] Decorator[@staticmethod] Keyword[def] Function[run] Member[self] Keyword[return] Member[self] Member[value]"));
	Check(TEXT("a @ b"),
		TEXT(""));
	Check(TEXT("class Class: pass"),
		TEXT("Keyword[class] Class[Class] Keyword[pass]"));
	Check(TEXT("None none TRUE"),
		TEXT("Keyword[None]"));
	Check(TEXT("def f(): pass\n")
			TEXT("print(\"x\")"),
		TEXT("Keyword[def] Function[f] Keyword[pass] Function[print] String[\"x\"]"));
	Check(TEXT("lambda x: x(1)"),
		TEXT("Keyword[lambda] Function[x] Number[1]"));
	Check(TEXT("if (a) and not(b):"),
		TEXT("Keyword[if] Keyword[and] Keyword[not]"));
	Check(TEXT("\"abc\".join(x)"),
		TEXT("String[\"abc\"] Function[join]"));
	Check(TEXT("x = ..."),
		TEXT(""));
	Check(TEXT("a[1].b"),
		TEXT("Number[1] Member[b]"));
	Check(TEXT("x = 5.real"),
		TEXT("Number[5] Member[real]"));
	Check(TEXT("x[0].y"),
		TEXT("Number[0] Member[y]"));
	Check(TEXT("async def go(): await x"),
		TEXT("Keyword[async] Keyword[def] Function[go] Keyword[await]"));
	Check(TEXT("x = 1 # c\r\n")
			TEXT("y = 2"),
		TEXT("Number[1] Comment[# c] Number[2]"));
	Check(TEXT("s = 'a\\"),
		TEXT("String['a\\]"));
	Check(TEXT(""),
		TEXT(""));
	Check(TEXT("print \"what's up\""),
		TEXT("String[\"what's up\"]"));
	Check(TEXT("caf\u00e9 = 1"),
		TEXT("Number[1]"));
	Check(TEXT("\n")
			TEXT("\n")
			TEXT("  @dec\n"),
		TEXT("Decorator[@dec]"));
	Check(TEXT("self"),
		TEXT("Member[self]"));
	Check(TEXT("obj.self"),
		TEXT("Member[self]"));
	Check(TEXT("x = '''unterminated\n")
			TEXT("still"),
		TEXT("String['''unterminated\nstill]"));
	Check(TEXT("\"\"\"doc\"\"\"\n")
			TEXT("x"),
		TEXT("String[\"\"\"doc\"\"\"]"));
	Check(TEXT("@unreal.uclass()\n")
			TEXT("class MyCustomTool(unreal.PyMenu):\n")
			TEXT("\n")
			TEXT("    MyString = unreal.uproperty(str)\n")
			TEXT("\n")
			TEXT("    @unreal.ufunction(override = True)\n")
			TEXT("    def run(self):\n")
			TEXT("        print \"what's up bear buddies \" + str(self.MyString)\n")
			TEXT("\n")
			TEXT("unreal.PyMenu.open_menu(\"MyCustomTool\", \"Win Title\", 400, 300)"),
		TEXT("Decorator[@unreal.uclass] Keyword[class] Class[MyCustomTool] Member[PyMenu] Function[uproperty] Decorator[@unreal.ufunction] Keyword[True] Keyword[def] Function[run] Member[self] String[\"what's up bear buddies \"] Function[str] Member[self] Member[MyString] Member[PyMenu] Function[open_menu] String[\"MyCustomTool\"] String[\"Win Title\"] Number[400] Number[300]"));
	return true;
}
#endif
