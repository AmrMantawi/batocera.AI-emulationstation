#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <vector>
#include <cstdint>

class LlmStreamService
{
public:
    using UiPoster = std::function<void(const std::function<void()>&)>;
    
    // Phoneme data from shared memory
    struct PhonemeData {
        std::int64_t phoneme_id;
        float duration_seconds;
        std::uint64_t timestamp_us;
    };
    
    using Callback = std::function<void(const PhonemeData&)>;

    static LlmStreamService& get();

    void start(const std::string& shmPath, UiPoster uiPoster);
    void stop();

    // Subscribe to phoneme events; returns subscription id
    std::uint64_t subscribe(Callback cb);
    void unsubscribe(std::uint64_t id);
    
    // Send control commands to TTS server
    bool sendControlCommand(const std::string& command);

private:
    LlmStreamService() = default;
    ~LlmStreamService();

    void phonemeReaderThread();

    std::thread mThread;
    std::atomic<bool> mRunning{false};
    std::string mShmPath{"/tts_phoneme_queue"};
    std::string mControlSocketPath{"/run/local-llm.sock"};
    UiPoster mUiPoster;
    
    void* mSharedMem{nullptr};
    int mShmFd{-1};
    std::uint32_t mConsumerReadIndex{0};

    struct SubRec { std::uint64_t id; Callback cb; };
    std::mutex mSubsMutex;
    std::vector<SubRec> mSubscribers;
    std::atomic<std::uint64_t> mNextId{1};
};


