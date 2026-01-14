#include "guis/GuiAiGraphics.h"
#include "components/ImageComponent.h"
#include "renderers/Renderer.h"
#include "ThemeData.h"
#include "LocaleES.h"
#include "views/ViewController.h"
#include "SystemData.h"
#include "Settings.h"
#include "resources/Font.h"
#include "Window.h"
#include "services/LlmStreamService.h"
#include "utils/FileSystemUtil.h"
#include "AudioManager.h"

#include <cstdio>
#include <chrono>
#include <thread>

// Phoneme ID to face image mapping (154 phonemes: IDs 0-153)
// Mouth shapes: E (wide), L (relaxed), M (closed), O (round/open)
// Based on IPA phonetic characteristics:
// - "a" sounds (a, ɑ, ɒ, æ, etc.) → E (wide)
// - Front vowels (e, i, ɛ) → E (wide)
// - Round vowels (o, u, ɔ) → O (round)
// - Bilabial stops (p, b, m) → M (closed)
// - Other consonants and mid vowels → L (relaxed)
static const char* PHONEME_FACE_MAP[154] = {
    // IDs 0-13: Special characters and punctuation
    ":/BMO_Face/L.png",   // 0: _ (pad) - hold last
    ":/BMO_Face/L.png",   // 1: ^ (start) - hold last
    ":/BMO_Face/L.png",   // 2: $ (end) - hold last
    ":/BMO_Face/L.png",   // 3: space - hold last
    ":/BMO_Face/M.png",   // 4: ! - end of sentence
    ":/BMO_Face/L.png",   // 5: ' - hold last
    ":/BMO_Face/L.png",   // 6: ( - hold last
    ":/BMO_Face/L.png",   // 7: ) - hold last
    ":/BMO_Face/L.png",   // 8: , - hold last
    ":/BMO_Face/L.png",   // 9: - - hold last
    ":/BMO_Face/M.png",   // 10: . - end of sentence
    ":/BMO_Face/L.png",   // 11: : - hold last
    ":/BMO_Face/L.png",   // 12: ; - hold last
    ":/BMO_Face/M.png",   // 13: ? - end of sentence
    
    // IDs 14-38: Basic Latin letters
    ":/BMO_Face/A.png",   // 14: a - open vowel (ah)
    ":/BMO_Face/M.png",   // 15: b - bilabial stop
    ":/BMO_Face/E.png",   // 16: c - "see" sound (ee)
    ":/BMO_Face/L.png",   // 17: d - alveolar stop
    ":/BMO_Face/E.png",   // 18: e - front vowel (eh)
    ":/BMO_Face/F.png",   // 19: f - labiodental fricative
    ":/BMO_Face/A.png",   // 20: h - open glottal
    ":/BMO_Face/E.png",   // 21: i - front vowel (ee)
    ":/BMO_Face/E.png",   // 22: j - palatal approximant (like y in yes)
    ":/BMO_Face/E.png",   // 23: k - velar stop (open mouth)
    ":/BMO_Face/L.png",   // 24: l - alveolar lateral
    ":/BMO_Face/M.png",   // 25: m - bilabial nasal
    ":/BMO_Face/L.png",   // 26: n - alveolar nasal
    ":/BMO_Face/O.png",   // 27: o - round back vowel (oh)
    ":/BMO_Face/M.png",   // 28: p - bilabial stop
    ":/BMO_Face/L.png",   // 29: q - velar stop
    ":/BMO_Face/L.png",   // 30: r - alveolar approximant
    ":/BMO_Face/E.png",   // 31: s - alveolar fricative (teeth showing)
    ":/BMO_Face/L.png",   // 32: t - alveolar stop
    ":/BMO_Face/O.png",   // 33: u - round back vowel (oo)
    ":/BMO_Face/F.png",   // 34: v - labiodental fricative
    ":/BMO_Face/M.png",   // 35: w - bilabial approximant
    ":/BMO_Face/E.png",   // 36: x - "ex" sound (ee)
    ":/BMO_Face/E.png",   // 37: y - front vowel
    ":/BMO_Face/E.png",   // 38: z - alveolar fricative (teeth showing)
    
    // IDs 39-49: Extended Latin
    ":/BMO_Face/A.png",   // 39: æ - open front vowel (ash)
    ":/BMO_Face/L.png",   // 40: ç - voiceless palatal fricative
    ":/BMO_Face/L.png",   // 41: ð - dental fricative (th in "this")
    ":/BMO_Face/O.png",   // 42: ø - front rounded vowel
    ":/BMO_Face/A.png",   // 43: ħ - pharyngeal fricative
    ":/BMO_Face/L.png",   // 44: ŋ - velar nasal (ng)
    ":/BMO_Face/O.png",   // 45: œ - front rounded vowel
    ":/BMO_Face/L.png",   // 46: ǀ - dental click
    ":/BMO_Face/L.png",   // 47: ǁ - lateral click
    ":/BMO_Face/L.png",   // 48: ǂ - palatal click
    ":/BMO_Face/L.png",   // 49: ǃ - alveolar click
    
    // IDs 50-129: IPA symbols
    ":/BMO_Face/A.png",   // 50: ɐ - near-open central vowel
    ":/BMO_Face/A.png",   // 51: ɑ - open back vowel (ah)
    ":/BMO_Face/O.png",   // 52: ɒ - open back rounded vowel
    ":/BMO_Face/M.png",   // 53: ɓ - bilabial implosive
    ":/BMO_Face/O.png",   // 54: ɔ - open-mid back rounded vowel (aw)
    ":/BMO_Face/L.png",   // 55: ɕ - alveolo-palatal fricative
    ":/BMO_Face/L.png",   // 56: ɖ - retroflex stop
    ":/BMO_Face/L.png",   // 57: ɗ - dental implosive
    ":/BMO_Face/E.png",   // 58: ɘ - close-mid central vowel
    ":/BMO_Face/E.png",   // 59: ə - schwa (mid central vowel)
    ":/BMO_Face/E.png",   // 60: ɚ - r-colored schwa
    ":/BMO_Face/E.png",   // 61: ɛ - open-mid front vowel (eh)
    ":/BMO_Face/E.png",   // 62: ɜ - open-mid central vowel
    ":/BMO_Face/O.png",   // 63: ɞ - open-mid central rounded vowel
    ":/BMO_Face/L.png",   // 64: ɟ - palatal stop
    ":/BMO_Face/L.png",   // 65: ɠ - velar implosive
    ":/BMO_Face/L.png",   // 66: ɡ - voiced velar stop
    ":/BMO_Face/L.png",   // 67: ɢ - uvular stop
    ":/BMO_Face/L.png",   // 68: ɣ - voiced velar fricative
    ":/BMO_Face/O.png",   // 69: ɤ - close-mid back unrounded vowel
    ":/BMO_Face/O.png",   // 70: ɥ - labial-palatal approximant
    ":/BMO_Face/A.png",   // 71: ɦ - voiced glottal fricative
    ":/BMO_Face/L.png",   // 72: ɧ - sj-sound
    ":/BMO_Face/E.png",   // 73: ɨ - close central unrounded vowel
    ":/BMO_Face/E.png",   // 74: ɪ - near-close front vowel (ih)
    ":/BMO_Face/L.png",   // 75: ɫ - velarized alveolar lateral (dark l)
    ":/BMO_Face/L.png",   // 76: ɬ - voiceless alveolar lateral fricative
    ":/BMO_Face/L.png",   // 77: ɭ - retroflex lateral
    ":/BMO_Face/L.png",   // 78: ɮ - voiced alveolar lateral fricative
    ":/BMO_Face/O.png",   // 79: ɯ - close back unrounded vowel
    ":/BMO_Face/L.png",   // 80: ɰ - velar approximant
    ":/BMO_Face/M.png",   // 81: ɱ - labiodental nasal
    ":/BMO_Face/L.png",   // 82: ɲ - palatal nasal
    ":/BMO_Face/L.png",   // 83: ɳ - retroflex nasal
    ":/BMO_Face/L.png",   // 84: ɴ - uvular nasal
    ":/BMO_Face/O.png",   // 85: ɵ - close-mid central rounded vowel
    ":/BMO_Face/A.png",   // 86: ɶ - open front rounded vowel
    ":/BMO_Face/M.png",   // 87: ɸ - voiceless bilabial fricative
    ":/BMO_Face/L.png",   // 88: ɹ - alveolar approximant (r)
    ":/BMO_Face/L.png",   // 89: ɺ - alveolar lateral flap
    ":/BMO_Face/L.png",   // 90: ɻ - retroflex approximant
    ":/BMO_Face/L.png",   // 91: ɽ - retroflex flap
    ":/BMO_Face/L.png",   // 92: ɾ - alveolar tap
    ":/BMO_Face/L.png",   // 93: ʀ - uvular trill
    ":/BMO_Face/L.png",   // 94: ʁ - voiced uvular fricative
    ":/BMO_Face/L.png",   // 95: ʂ - voiceless retroflex fricative
    ":/BMO_Face/L.png",   // 96: ʃ - voiceless postalveolar fricative (sh)
    ":/BMO_Face/L.png",   // 97: ʄ - palatal implosive
    ":/BMO_Face/L.png",   // 98: ʈ - voiceless retroflex stop
    ":/BMO_Face/O.png",   // 99: ʉ - close central rounded vowel
    ":/BMO_Face/O.png",   // 100: ʊ - near-close back rounded vowel (uh)
    ":/BMO_Face/F.png",   // 101: ʋ - labiodental approximant
    ":/BMO_Face/A.png",   // 102: ʌ - open-mid back unrounded vowel (uh)
    ":/BMO_Face/M.png",   // 103: ʍ - voiceless labial-velar fricative
    ":/BMO_Face/L.png",   // 104: ʎ - palatal lateral
    ":/BMO_Face/E.png",   // 105: ʏ - near-close front rounded vowel
    ":/BMO_Face/L.png",   // 106: ʐ - voiced retroflex fricative
    ":/BMO_Face/L.png",   // 107: ʑ - voiced alveolo-palatal fricative
    ":/BMO_Face/L.png",   // 108: ʒ - voiced postalveolar fricative (zh)
    ":/BMO_Face/L.png",   // 109: ʔ - glottal stop
    ":/BMO_Face/A.png",   // 110: ʕ - voiced pharyngeal fricative
    ":/BMO_Face/M.png",   // 111: ʘ - bilabial click
    ":/BMO_Face/M.png",   // 112: ʙ - bilabial trill
    ":/BMO_Face/L.png",   // 113: ʛ - uvular implosive
    ":/BMO_Face/A.png",   // 114: ʜ - voiceless epiglottal fricative
    ":/BMO_Face/L.png",   // 115: ʝ - voiced palatal fricative
    ":/BMO_Face/L.png",   // 116: ʟ - velar lateral
    ":/BMO_Face/A.png",   // 117: ʡ - epiglottal stop
    ":/BMO_Face/A.png",   // 118: ʢ - voiced epiglottal fricative
    ":/BMO_Face/L.png",   // 119: ʲ - palatalization
    ":/BMO_Face/L.png",   // 120: ˈ - primary stress (hold last)
    ":/BMO_Face/L.png",   // 121: ˌ - secondary stress (hold last)
    ":/BMO_Face/L.png",   // 122: ː - length marker (hold last)
    ":/BMO_Face/L.png",   // 123: ˑ - half-length (hold last)
    ":/BMO_Face/L.png",   // 124: ˞ - rhoticity (hold last)
    ":/BMO_Face/M.png",   // 125: β - voiced bilabial fricative
    ":/BMO_Face/L.png",   // 126: θ - voiceless dental fricative (th)
    ":/BMO_Face/L.png",   // 127: χ - voiceless uvular fricative
    ":/BMO_Face/E.png",   // 128: ᵻ - near-close central vowel
    ":/BMO_Face/F.png",   // 129: ⱱ - labiodental flap
    
    // IDs 130-139: Digits
    ":/BMO_Face/O.png",   // 130: 0 - oh
    ":/BMO_Face/O.png",   // 131: 1 - wun
    ":/BMO_Face/O.png",   // 132: 2 - too
    ":/BMO_Face/L.png",   // 133: 3 - three
    ":/BMO_Face/O.png",   // 134: 4 - four
    ":/BMO_Face/F.png",   // 135: 5 - five
    ":/BMO_Face/L.png",   // 136: 6 - six
    ":/BMO_Face/L.png",   // 137: 7 - seven
    ":/BMO_Face/A.png",   // 138: 8 - eight
    ":/BMO_Face/A.png",   // 139: 9 - nine
    
    // IDs 140-153: Diacritics and special symbols
    ":/BMO_Face/L.png",   // 140: ̧ (cedilla) - hold last
    ":/BMO_Face/L.png",   // 141: ̃ (nasalization) - hold last
    ":/BMO_Face/L.png",   // 142: ̪ (dental) - hold last
    ":/BMO_Face/L.png",   // 143: ̯ (non-syllabic) - hold last
    ":/BMO_Face/L.png",   // 144: ̩ (syllabic) - hold last
    ":/BMO_Face/L.png",   // 145: ʰ (aspiration) - hold last
    ":/BMO_Face/L.png",   // 146: ˤ (pharyngealization) - hold last
    ":/BMO_Face/E.png",   // 147: ε - open-mid front vowel
    ":/BMO_Face/L.png",   // 148: ↓ (downstep) - hold last
    ":/BMO_Face/L.png",   // 149: # (word boundary) - hold last
    ":/BMO_Face/L.png",   // 150: " - hold last
    ":/BMO_Face/L.png",   // 151: ↑ (upstep) - hold last
    ":/BMO_Face/L.png",   // 152: ̺ (apical) - hold last
    ":/BMO_Face/L.png"    // 153: ̻ (laminal) - hold last
};

static const char* mapPhonemeIdToFace(std::int64_t phoneme_id)
{
    static const char* lastFace = ":/BMO_Face/M.png"; // Start with closed mouth
    
    // Padding and punctuation (except end-of-sentence) should hold last mouth
    if (phoneme_id == 0 ||   // _ (pad)
        phoneme_id == 1 ||   // ^ (start)
        phoneme_id == 3 ||   // space
        phoneme_id == 5 ||   // '
        phoneme_id == 6 ||   // (
        phoneme_id == 7 ||   // )
        phoneme_id == 8 ||   // ,
        phoneme_id == 9 ||   // -
        phoneme_id == 11 ||  // :
        phoneme_id == 12 ||  // ;
        phoneme_id >= 120 && phoneme_id <= 124 || // stress/length markers
        phoneme_id >= 140 && phoneme_id <= 153)   // diacritics
    {
        // Hold last face (don't update lastFace)
        return lastFace;
    }
    
    // End of sentence → closed mouth (M sound)
    if (phoneme_id == 10 ||  // .
        phoneme_id == 2 ||   // $ (end)
        phoneme_id == 4 ||   // !
        phoneme_id == 13)    // ?
    {
        lastFace = ":/BMO_Face/M.png";
        return lastFace;
    }
    
    // Normal phonemes
    if (phoneme_id >= 0 && phoneme_id < 154) {
        lastFace = PHONEME_FACE_MAP[phoneme_id];
        return lastFace;
    }
    
    // Default fallback
    lastFace = ":/BMO_Face/E.png";
    return lastFace;
}

GuiAiGraphics::GuiAiGraphics(Window* window) : GuiComponent(window)
{
    // Stop background music while in AI GUI and prevent auto-restart
    if (AudioManager::isInitialized()) {
        AudioManager::setVideoPlaying(true);
        AudioManager::getInstance()->stopMusic(true);
    }

    // Calculate screen dimensions
    float screenWidth = (float)Renderer::getScreenWidth();
    float screenHeight = (float)Renderer::getScreenHeight();
    
    // Set this component to full screen
    setSize(screenWidth, screenHeight);
    
    // Create a fullscreen image on top of the background - start with M (closed mouth)
    mBackgroundImage = std::make_shared<ImageComponent>(window);
    mBackgroundImage->setPosition(0.0f, 0.0f);
    mBackgroundImage->setResize(screenWidth, 0); // scale by width, maintain aspect ratio (0 = auto height)
    mBackgroundImage->setImage(":/BMO_Face/M.png");

    // Add the image as a child component
    addChild(mBackgroundImage.get());

    // Transcript text area - HIDDEN FOR NOW
    // float boxWidth = screenWidth * 0.20f;
    // float boxHeight = screenHeight * 0.5f;
    // float boxX = screenWidth * 0.78f;
    // float boxY = (screenHeight - boxHeight) / 2.0f;
    // 
    // mTranscript = std::make_shared<TextComponent>(
    //     window,
    //     "",
    //     Font::get(FONT_SIZE_MEDIUM),
    //     0x000000FF,
    //     ALIGN_LEFT,
    //     Vector3f(boxX, boxY, 0.0f),
    //     Vector2f(boxWidth, boxHeight),
    //     0xFFFFFFFF);
    //
    // mTranscript->setRenderBackground(true);
    // mTranscript->setBackgroundColor(0xFFFFFFAA);
    // mTranscript->setAutoScroll(TextComponent::AutoScrollType::VERTICAL);
    // mTranscript->setLineSpacing(1.5f);
    // mTranscript->setVerticalAlignment(ALIGN_TOP);
    // mTranscript->setSize(boxWidth, boxHeight);
    // addChild(mTranscript.get());
}

GuiAiGraphics::~GuiAiGraphics()
{
    // Stop animation thread
    mAnimationRunning = false;
    if (mAnimationThread.joinable())
        mAnimationThread.join();

    if (mWindow)
        mWindow->unregisterPostedFunctions(this);

    // Allow music to resume when leaving AI GUI
    if (AudioManager::isInitialized()) {
        AudioManager::setVideoPlaying(false);
        AudioManager::getInstance()->playRandomMusic();
    }
}

void GuiAiGraphics::onShow()
{
    GuiComponent::onShow();
    if (mSubId == 0)
    {
        // Send face_show command to TTS server to start generating phonemes
        LlmStreamService::get().sendControlCommand("face_show");
        
        // Start animation thread
        mAnimationRunning = true;
        mAnimationThread = std::thread([this]() {
            while (mAnimationRunning.load())
            {
                PhonemeQueueItem item;
                bool hasItem = false;
                
                // Get next phoneme from queue
                {
                    std::lock_guard<std::mutex> lock(mPhonemeQueueMutex);
                    if (!mPhonemeQueue.empty()) {
                        item = mPhonemeQueue.front();
                        mPhonemeQueue.erase(mPhonemeQueue.begin());
                        hasItem = true;
                    }
                }
                
                if (hasItem) {
                    // Update face on UI thread
                    mWindow->postToUiThread([this, item]() {
                        if (mBackgroundImage) {
                            mBackgroundImage->setImage(item.faceImage);
                            mLastFaceImage = item.faceImage;
                        }
                    }, this);
                    
                    // Wait for the phoneme duration
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(static_cast<int>(item.duration_seconds * 1000))
                    );
                } else {
                    // No phonemes in queue, sleep briefly
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        });
        
        // Subscribe to phoneme data from shared memory
        mSubId = LlmStreamService::get().subscribe(
            [this](const LlmStreamService::PhonemeData& data){
                // Text display hidden for now
                // char buffer[128];
                // snprintf(buffer, sizeof(buffer), "Phoneme ID: %lld, Duration: %.3fs", 
                //          (long long)data.phoneme_id, data.duration_seconds);
                // 
                // if (!mDisplayedTranscript.empty()) mDisplayedTranscript += "\n";
                // mDisplayedTranscript += buffer;
                //
                // if (mTranscript)
                //     mTranscript->setText(mDisplayedTranscript);
                
                // Queue phoneme for animation (with timing)
                const char* faceImage = mapPhonemeIdToFace(data.phoneme_id);
                {
                    std::lock_guard<std::mutex> lock(mPhonemeQueueMutex);
                    mPhonemeQueue.push_back({data.phoneme_id, data.duration_seconds, faceImage});
                }
            });
    }
}

void GuiAiGraphics::onHide()
{
    // Stop animation thread
    mAnimationRunning = false;
    if (mAnimationThread.joinable())
        mAnimationThread.join();
    
    // Clear phoneme queue
    {
        std::lock_guard<std::mutex> lock(mPhonemeQueueMutex);
        mPhonemeQueue.clear();
    }
    
    if (mSubId != 0)
    {
        LlmStreamService::get().unsubscribe(mSubId);
        mSubId = 0;
    }
    
    // Send face_hide command to TTS server to stop generating phonemes
    LlmStreamService::get().sendControlCommand("face_hide");
    
    GuiComponent::onHide();
}

bool GuiAiGraphics::input(InputConfig* config, Input input)
{
    // Handle UP button - show fully open image
    if (config->isMappedTo("up", input) && input.value != 0)
    {
        mBackgroundImage->setImage(":/BMO_Face/O.png");
        return true;
    }
    
    // Handle DOWN button - show half open image
    if (config->isMappedTo("down", input) && input.value != 0)
    {
        mBackgroundImage->setImage(":/BMO_Face/L.png");
        return true;
    }
    // Only respond to A/OK button press to exit
    if (config->isMappedTo(BUTTON_OK, input) && input.value != 0)
    {
        // Go to normal startup sequence first
        bool startOnGamelist = Settings::getInstance()->getBool("StartupOnGameList");
        auto requestedSystem = Settings::getInstance()->getString("StartupSystem");
        if (requestedSystem == "lastsystem")
            requestedSystem = Settings::getInstance()->getString("LastSystem");

        if("" != requestedSystem && "retropie" != requestedSystem)
        {
            auto system = SystemData::getSystem(requestedSystem);
            if (system != nullptr && !system->isGroupChildSystem())
            {
                if (startOnGamelist)
                    ViewController::get()->goToGameList(system, true);
                else
                    ViewController::get()->goToSystemView(system, true);
                // Close this screen after navigation
                delete this;
                return true;
            }
        }

        if (startOnGamelist)
            ViewController::get()->goToGameList(SystemData::getFirstVisibleSystem(), true);
        else
            ViewController::get()->goToSystemView(SystemData::getFirstVisibleSystem(), true);
        
        // Close this screen after navigation
        delete this;
        return true;
    }
    
    // Swallow all other inputs
    return true;
}

void GuiAiGraphics::update(int deltaTime)
{
    GuiComponent::update(deltaTime);
}

void GuiAiGraphics::render(const Transform4x4f& parentTrans)
{
    // Draw white background
    Transform4x4f trans = parentTrans * getTransform();
    Renderer::setMatrix(trans);
    Renderer::drawRect(0, 0, mSize.x(), mSize.y(), 0x74F5B6FF, 0x74F5B6FF);
    
    // Render children (the image)
    GuiComponent::renderChildren(trans);
}

std::vector<HelpPrompt> GuiAiGraphics::getHelpPrompts()
{
    std::vector<HelpPrompt> prompts;
    prompts.push_back(HelpPrompt("up/down", _("MANUAL FACE")));
    prompts.push_back(HelpPrompt(BUTTON_OK, _("CONTINUE")));
    return prompts;
}


