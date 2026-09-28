// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "VarnPythonPaths.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVarnPythonPathsTest, "VarnPythonTools.Paths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVarnPythonPathsTest::RunTest(const FString& Parameters)
{
	using namespace VarnPythonTools;
	TestEqual(
		TEXT("Preserve UNC prefix and collapse interior duplicate slashes"),
		NormalizePath(TEXT("//server/share//Python/example.py")), FString(TEXT("//server/share/Python/example.py")));
	TestEqual(
		TEXT("Normalize backslash UNC paths"),
		NormalizePath(TEXT("\\\\server\\share\\Python\\example.py")), FString(TEXT("//server/share/Python/example.py")));
	TestEqual(
		TEXT("Collapse parent directories in UNC paths"),
		NormalizePath(TEXT("//server/share/Python/sub/../example.py")), FString(TEXT("//server/share/Python/example.py")));
	TestEqual(
		TEXT("Normalize local paths"),
		NormalizePath(TEXT("C:/Tools//Python/./example.py")), FString(TEXT("C:/Tools/Python/example.py")));
	TestTrue(
		TEXT("Relative paths resolve against the project"),
		FPaths::IsSamePath(
			NormalizePath(TEXT("Content/Python/example.py")),
			FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TEXT("Content/Python/example.py"))));

	TestTrue(TEXT("Spaces in paths are supported"), IsSupportedScriptPath(TEXT("C:/My Tools/example.py")));
	TestTrue(TEXT("UNC paths are supported"), IsSupportedScriptPath(TEXT("//server/share/example.py")));
	TestFalse(TEXT("An earlier .py in a directory is ambiguous"), IsSupportedScriptPath(TEXT("C:/tools.python/example.py")));
	TestFalse(TEXT("An earlier .py in a filename is ambiguous"), IsSupportedScriptPath(TEXT("C:/Tools/example.py.backup.py")));
	TestFalse(TEXT("Uppercase extension is not parsed as a file"), IsSupportedScriptPath(TEXT("C:/Tools/example.PY")));
	TestFalse(TEXT("Quotes are rejected"), IsSupportedScriptPath(TEXT("/tools/a\"b.py")));
	TestFalse(TEXT("Line breaks are rejected"), IsSupportedScriptPath(TEXT("/tools/a\nb.py")));
	TestFalse(TEXT("Missing extension is rejected"), IsSupportedScriptPath(TEXT("/tools/example")));
	TestFalse(TEXT("Empty path is rejected"), IsSupportedScriptPath(FString()));
	return true;
}
#endif
