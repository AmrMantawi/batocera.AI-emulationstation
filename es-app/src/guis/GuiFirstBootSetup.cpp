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
    // This component is invisible – it only orchestrates the wizard screens.
    setSize(0, 0);
    showStepName();
}

// ---------------------------------------------------------------------------
// Step 1 – User name
// ---------------------------------------------------------------------------
void GuiFirstBootSetup::showStepName()
{
    auto* s = new GuiSettings(mWindow, _("WELCOME TO BATOCERA AI"));
    s->setSubTitle(_("Let's get you set up. Enter your name to get started."));

    s->addInputTextRow(_("Your Name"), "system.username", /*password=*/false, /*storeInSettings=*/false);

    s->addSaveFunc([s]()
    {
        // addInputTextRow already queued the write to SystemConf via its own
        // save function, so we just need the conf to persist.
        SystemConf::getInstance()->saveSystemConf();
    });

    s->onFinalize([this]()
    {
        showStepWifi();
    });

    mWindow->pushGui(s);
}

// ---------------------------------------------------------------------------
// Step 2 – WiFi setup
// ---------------------------------------------------------------------------
void GuiFirstBootSetup::showStepWifi()
{
    // "NEXT" button advances the wizard; "SKIP" also advances (WiFi is optional).
    auto advance = [this]() { showStepModels(); };

    auto* s = new GuiSettings(mWindow, _("NETWORK SETUP"), _("NEXT"),
        [advance](GuiSettings* gui)
        {
            gui->save();
            advance();
            // GuiSettings::close() would call onFinalize then delete, but we
            // called save() manually and will delete via close() below.
            gui->setSave(false);
            gui->close();
        });

    s->setSubTitle(_("Connect to WiFi to enable AI features and model downloads."));

    // Current SSID for display
    std::string currentSsid = SystemConf::getInstance()->get("wifi.ssid");
    if (currentSsid.empty())
        currentSsid = _("Not configured");

    s->addEntry(_("SELECT WIFI NETWORK"), /*arrow=*/true, [this, s]()
    {
        std::string ssid = SystemConf::getInstance()->get("wifi.ssid");
        mWindow->pushGui(new GuiWifi(mWindow, _("SELECT WIFI NETWORK"), ssid,
            [](const std::string& selectedSsid)
            {
                // GuiWifi shows a password prompt internally when needed;
                // saving is handled by openNetworkSettings pattern – here we
                // just store the chosen SSID and let the user confirm via the
                // existing wifi.key flow.  For simplicity we enable wifi with
                // whatever credentials are already saved.
                SystemConf::getInstance()->set("wifi.ssid", selectedSsid);
                SystemConf::getInstance()->set("wifi.enabled", "1");
                SystemConf::getInstance()->saveSystemConf();
                ApiSystem::getInstance()->enableWifi(
                    SystemConf::getInstance()->get("wifi.ssid"),
                    SystemConf::getInstance()->get("wifi.key"));
            }));
    });

    // SKIP button – advances without saving anything wifi-related
    s->setCloseButton("start"); // allow START to close
    s->onFinalize([advance]() { advance(); });

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

    // Remove this invisible orchestrator from the stack.
    delete this;
}
