// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "Framework/Docking/TabManager.h"
#include "ISettingsModule.h"
#include "SVarnPythonBrowser.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "VarnPythonToolsSettings.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "VarnPythonBrowser"

namespace
{
	const FName PythonBrowserTabName(TEXT("VarnPythonBrowser"));
}

class FVarnPythonToolsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (IsRunningCommandlet())
		{
			return;
		}

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PythonBrowserTabName,
		                                                  FOnSpawnTab::CreateRaw(this, &FVarnPythonToolsModule::SpawnTab))
		                        .SetDisplayName(LOCTEXT("Title", "Python Browser"))
		                        .SetTooltipText(LOCTEXT("TabTip", "Browse and run Python scripts."))
		                        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Code")))
		                        .SetMenuType(ETabSpawnerMenuType::Hidden);

		ISettingsModule& Settings = FModuleManager::LoadModuleChecked<ISettingsModule>(TEXT("Settings"));
		Settings.RegisterSettings(TEXT("Editor"), TEXT("Plugins"), TEXT("VarnPythonBrowser"),
		                          LOCTEXT("SettingsTitle", "Rock Python Browser"),
		                          LOCTEXT("SettingsDescription", "Personal Python script folders for this project. Refresh the browser after editing."),
		                          GetMutableDefault<UVarnPythonToolsSettings>());

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
			this, &FVarnPythonToolsModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		if (IsRunningCommandlet())
		{
			return;
		}
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (ISettingsModule* Settings = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings")))
		{
			Settings->UnregisterSettings(TEXT("Editor"), TEXT("Plugins"), TEXT("VarnPythonBrowser"));
		}
		if (TSharedPtr<SDockTab> Tab = BrowserTab.Pin())
		{
			// Destroy our widgets before module code can be unloaded.
			Tab->SetContent(SNullWidget::NullWidget);
			Tab->RequestCloseTab();
		}
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PythonBrowserTabName);
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
		FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("VarnPythonBrowser"), LOCTEXT("Section", "Python"), FToolMenuInsert());
		Section.AddMenuEntry(PythonBrowserTabName,
		                     LOCTEXT("Title", "Python Browser"),
		                     LOCTEXT("MenuTip", "Browse and run Python scripts."),
		                     FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Code")),
		                     FUIAction(FExecuteAction::CreateLambda([]
		                     {
			                     FGlobalTabmanager::Get()->TryInvokeTab(PythonBrowserTabName);
		                     })));
	}

	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args)
	{
		TSharedRef<SDockTab> Tab = SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SVarnPythonBrowser)
			];
		BrowserTab = Tab;
		return Tab;
	}

	TWeakPtr<SDockTab> BrowserTab;
};

IMPLEMENT_MODULE(FVarnPythonToolsModule, VarnPythonTools)

#undef LOCTEXT_NAMESPACE
