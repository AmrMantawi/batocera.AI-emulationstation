#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <semaphore.h>
#include <string>
#include <thread>
#include <vector>

// Shared memory queue header (layout must match TTS producer)
struct PhonemeQueueHeader {
  std::atomic<std::uint32_t> write_index{0};
  std::atomic<std::uint32_t> read_index{0};
  std::atomic<bool> shutdown_flag{false};
  sem_t sem;
  static constexpr size_t MAX_PHONEMES = 1024;
};

class LlmStreamService {
public:
  using UiPoster = std::function<void(const std::function<void()> &)>;

  // Phoneme data from shared memory
  struct PhonemeData {
    std::int64_t phoneme_id;
    float duration_seconds;
    std::uint64_t timestamp_us;
  };

  // Shared memory queue for phoneme data
  struct PhonemeSharedQueue {
    PhonemeQueueHeader header;
    PhonemeData phonemes[PhonemeQueueHeader::MAX_PHONEMES];
  };

  using Callback = std::function<void(const PhonemeData &)>;

  static LlmStreamService &get();

  void start(const std::string &shmPath, UiPoster uiPoster);
  void stop();

  // Subscribe to phoneme events; returns subscription id
  std::uint64_t subscribe(Callback cb);
  void unsubscribe(std::uint64_t id);

  // Send control commands to TTS server
  bool sendControlCommand(const std::string &command);

private:
  LlmStreamService() = default;
  ~LlmStreamService();

  void phonemeReaderThread();

  std::thread mThread;
  std::atomic<bool> mRunning{false};
  std::string mShmPath{"/tts_phoneme_queue"};
  std::string mControlSocketPath{"/tmp/tts_face_control.sock"};
  UiPoster mUiPoster;

  void *mSharedMem{nullptr};
  int mShmFd{-1};
  std::uint32_t mConsumerReadIndex{0};

  struct SubRec {
    std::uint64_t id;
    Callback cb;
  };
  std::mutex mSubsMutex;
  std::vector<SubRec> mSubscribers;
  std::atomic<std::uint64_t> mNextId{1};
};
