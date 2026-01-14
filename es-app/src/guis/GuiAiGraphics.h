#pragma once
#ifndef ES_APP_GUIS_GUI_AI_GRAPHICS_H
#define ES_APP_GUIS_GUI_AI_GRAPHICS_H

#include "GuiComponent.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include <memory>
#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

class GuiAiGraphics : public GuiComponent
{
public:
    struct VisemeStep {
        std::string faceImage;
        int durationMs; // how long to display this face
    };

    GuiAiGraphics(Window* window);
    ~GuiAiGraphics() override;
    
    bool input(InputConfig* config, Input input) override;
    std::vector<HelpPrompt> getHelpPrompts() override;
    void render(const Transform4x4f& parentTrans) override;
    void onShow() override;
    void onHide() override;
    void update(int deltaTime) override;

private:
    std::shared_ptr<ImageComponent> mBackgroundImage;
    std::shared_ptr<TextComponent>  mTranscript;

    std::uint64_t mSubId = 0;
    std::string mDisplayedTranscript;
    std::string mLastFaceImage{":/BMO_Face/M.png"}; // Start with M (closed mouth)
    
    // Phoneme animation queue
    struct PhonemeQueueItem {
        std::int64_t phoneme_id;
        float duration_seconds;
        std::string faceImage;
    };
    std::vector<PhonemeQueueItem> mPhonemeQueue;
    std::mutex mPhonemeQueueMutex;
    std::thread mAnimationThread;
    std::atomic<bool> mAnimationRunning{false};
};

#endif // ES_APP_GUIS_GUI_AI_GRAPHICS_H

