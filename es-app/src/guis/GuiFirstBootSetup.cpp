#include "guis/GuiFirstBootSetup.h"

#include "guis/GuiSettings.h"
#include "guis/GuiWifi.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiLoading.h"
#include "guis/GuiAiGraphics.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "Window.h"
#include "LocaleES.h"
#include "Log.h"

GuiFirstBootSetup::GuiFirstBootSetup(Window* window)
    : GuiComponent(window)
{
    setSize(0, 0);
    showStepName();
}

// ---------------------------------------------------------------------------
// Step 1 – User name
// ---------------------------------------------------------------------------
void GuiFirstBootSetup::showStepName()
{
    // "NEXT" advances; "BACK" just closes this screen without advancing.
    auto* s = new GuiSettings(mWindow, _("WELCOME TO BATOCERA AI"), _("NEXT"),
        [this](GuiSettings* gui)
        {
            gui->save();            // runs addInputTextRow save funcs + saveSystemConf
            gui->setSave(false);    // prevent double-save on close
            showStepWifi();
            gui->close();
        });

    s->setSubTitle(_("Enter your name to personalize your experience."));
    s->addInputTextRow(_("Your Name"), "system.username", /*password=*/false, /*storeInSettings=*/false);

    mWindow->pushGui(s);
}

// ---------------------------------------------------------------------------
// Step 2 – WiFi setup
// ---------------------------------------------------------------------------
void GuiFirstBootSetup::showStepWifi()
{
    // "NEXT" advances; "BACK" just closes this screen without advancing.
    auto* s = new GuiSettings(mWindow, _("NETWORK SETUP"), _("NEXT"),
        [this](GuiSettings* gui)
        {
            gui->setSave(false);
            showStepModels();
            gui->close();
        });

    s->setSubTitle(_("Connect to WiFi to enable AI features and model downloads."));

    s->addEntry(_("SELECT WIFI NETWORK"), /*arrow=*/true, [this]()
    {
        std::string ssid = SystemConf::getInstance()->get("wifi.ssid");
        mWindow->pushGui(new GuiWifi(mWindow, _("SELECT WIFI NETWORK"), ssid,
            [](const std::string& selectedSsid)
            {
                SystemConf::getInstance()->set("wifi.ssid", selectedSsid);
                SystemConf::getInstance()->set("wifi.enabled", "1");
                SystemConf::getInstance()->saveSystemConf();
                ApiSystem::getInstance()->enableWifi(
                    selectedSsid,
                    SystemConf::getInstance()->get("wifi.key"));
            }));
    });

    mWindow->pushGui(s);
}

// ---------------------------------------------------------------------------
// Step 3 – AI model install
// ---------------------------------------------------------------------------
void GuiFirstBootSetup::showStepModels()
{
    Window* window = mWindow;

    window->pushGui(new GuiMsgBox(window,
        _("Would you like to download the AI model files now?\n\nThis requires an internet connection and approximately 5 GB of free disk space."),
        _("YES"), [this, window]()
        {
            window->pushGui(new GuiLoading<bool>(window,
                _("DOWNLOADING AI MODELS — THIS MAY TAKE A WHILE..."),
                [](IGuiLoadingHandler* handler) -> bool
                {
                    LOG(LogInfo) << "GuiFirstBootSetup: running local-llm-setup-models";
                    handler->setText(_("Downloading AI models, please wait..."));
                    int ret = system("/usr/bin/local-llm-setup-models");
                    if (ret != 0)
                        LOG(LogWarning) << "GuiFirstBootSetup: local-llm-setup-models exited with code " << ret;
                    return (ret == 0);
                },
                [this, window](bool success)
                {
                    if (!success)
                    {
                        window->pushGui(new GuiMsgBox(window,
                            _("Model download encountered an error.\nYou can retry later from the main menu."),
                            _("OK"), [this]() { finalize(); }));
                    }
                    else
                    {
                        finalize();
                    }
                }));
        },
        _("SKIP"), [this]() { finalize(); },
        ICON_QUESTION));
}

// ---------------------------------------------------------------------------
// Finalize – mark first-boot done and launch the main screen
// ---------------------------------------------------------------------------
void GuiFirstBootSetup::finalize()
{
    SystemConf::getInstance()->set("system.firstboot", "done");
    SystemConf::getInstance()->saveSystemConf();

    LOG(LogInfo) << "GuiFirstBootSetup: setup complete, launching GuiAiGraphics";

    mWindow->pushGui(new GuiAiGraphics(mWindow));
    delete this;
}
