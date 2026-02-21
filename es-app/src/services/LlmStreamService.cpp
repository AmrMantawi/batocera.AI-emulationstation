#include "LlmStreamService.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <cerrno>
#include <chrono>
#include <thread>

LlmStreamService& LlmStreamService::get()
{
    static LlmStreamService instance;
    return instance;
}

LlmStreamService::~LlmStreamService()
{
    stop();
}

void LlmStreamService::start(const std::string& shmPath, UiPoster uiPoster)
{
    stop();
    mShmPath = shmPath.empty() ? std::string("/tts_phoneme_queue") : shmPath;
    mUiPoster = uiPoster;
    
    // Open shared memory
    std::string fullPath = "/dev/shm" + mShmPath;
    mShmFd = open(fullPath.c_str(), O_RDWR);
    if (mShmFd < 0) {
        std::cerr << "[LlmStreamService] Failed to open shared memory at " << fullPath 
                  << ": " << strerror(errno) << std::endl;
        std::cerr << "[LlmStreamService] Make sure TTS server is running" << std::endl;
        return;
    }
    
    // Get size of shared memory
    struct stat sb;
    if (fstat(mShmFd, &sb) == -1) {
        std::cerr << "[LlmStreamService] Failed to get shared memory size: " << strerror(errno) << std::endl;
        close(mShmFd);
        mShmFd = -1;
        return;
    }
    
    // Map shared memory
    mSharedMem = mmap(nullptr, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, mShmFd, 0);
    if (mSharedMem == MAP_FAILED) {
        std::cerr << "[LlmStreamService] Failed to map shared memory: " << strerror(errno) << std::endl;
        close(mShmFd);
        mShmFd = -1;
        mSharedMem = nullptr;
        return;
    }
    
    // Read initial read_index from shared memory
    std::uint32_t* read_index_ptr = reinterpret_cast<std::uint32_t*>(static_cast<char*>(mSharedMem) + 4);
    mConsumerReadIndex = *read_index_ptr;
    
    mRunning = true;
    mThread = std::thread(&LlmStreamService::phonemeReaderThread, this);
    
    std::cout << "[LlmStreamService] Connected to phoneme queue at " << fullPath << std::endl;
}

void LlmStreamService::stop()
{
    if (!mRunning.load())
        return;
    mRunning = false;
    if (mThread.joinable())
        mThread.join();
    
    if (mSharedMem && mSharedMem != MAP_FAILED) {
        munmap(mSharedMem, 16 + 1024 * 24); // header + 1024 phonemes * 24 bytes
        mSharedMem = nullptr;
    }
    
    if (mShmFd >= 0) {
        close(mShmFd);
        mShmFd = -1;
    }
}

std::uint64_t LlmStreamService::subscribe(Callback cb)
{
    std::lock_guard<std::mutex> lk(mSubsMutex);
    std::uint64_t id = mNextId++;
    mSubscribers.push_back(SubRec{ id, std::move(cb) });
    return id;
}

void LlmStreamService::unsubscribe(std::uint64_t id)
{
    std::lock_guard<std::mutex> lk(mSubsMutex);
    auto it = std::remove_if(mSubscribers.begin(), mSubscribers.end(), [id](const SubRec& r){ return r.id == id; });
    mSubscribers.erase(it, mSubscribers.end());
}

void LlmStreamService::phonemeReaderThread()
{
    if (!mSharedMem) return;
    
    // Shared memory layout:
    // Offset 0: write_index (uint32_t)
    // Offset 4: read_index (uint32_t)
    // Offset 8: shutdown (bool)
    // Offset 16: phoneme data array (1024 * 24 bytes)
    // Each phoneme: int64_t phoneme_id, float duration, 4 bytes padding, uint64_t timestamp
    
    const int WRITE_INDEX_OFFSET = 0;
    const int READ_INDEX_OFFSET = 4;
    const int SHUTDOWN_OFFSET = 8;
    const int HEADER_SIZE = 16;
    const int PHONEME_DATA_SIZE = 24;
    const int MAX_PHONEMES = 1024;
    
    char* shm_base = static_cast<char*>(mSharedMem);
    
    std::cout << "[LlmStreamService] Phoneme reader thread started" << std::endl;
    
    while (mRunning.load())
    {
        try {
            // Read header atomics
            std::uint32_t write_index = *reinterpret_cast<std::uint32_t*>(shm_base + WRITE_INDEX_OFFSET);
            std::uint32_t read_index = *reinterpret_cast<std::uint32_t*>(shm_base + READ_INDEX_OFFSET);
            bool shutdown_flag = *reinterpret_cast<bool*>(shm_base + SHUTDOWN_OFFSET);
            
            if (shutdown_flag) {
                std::cout << "[LlmStreamService] Shutdown signal received" << std::endl;
                break;
            }
            
            // Process all new phonemes
            while (mConsumerReadIndex != write_index)
            {
                // Read phoneme data
                int offset = HEADER_SIZE + (mConsumerReadIndex * PHONEME_DATA_SIZE);
                char* phoneme_bytes = shm_base + offset;
                
                PhonemeData data;
                std::memcpy(&data.phoneme_id, phoneme_bytes + 0, 8);
                std::memcpy(&data.duration_seconds, phoneme_bytes + 8, 4);
                std::memcpy(&data.timestamp_us, phoneme_bytes + 16, 8);
                
                // Validate data
                if (data.duration_seconds <= 0 || data.duration_seconds > 10.0f) {
                    std::cerr << "[LlmStreamService] Skipping phoneme " << data.phoneme_id 
                              << " with invalid duration " << data.duration_seconds << "s" << std::endl;
                    mConsumerReadIndex = (mConsumerReadIndex + 1) % MAX_PHONEMES;
                    
                    // Update read_index in shared memory
                    *reinterpret_cast<std::uint32_t*>(shm_base + READ_INDEX_OFFSET) = mConsumerReadIndex;
                    continue;
                }
                
                // Notify subscribers on UI thread
                if (mUiPoster)
                {
                    std::vector<SubRec> subs;
                    {
                        std::lock_guard<std::mutex> lk(mSubsMutex);
                        subs = mSubscribers; // copy
                    }
                    
                    if (!subs.empty())
                    {
                        mUiPoster([subs, data]() {
                            for (auto& r : subs)
                            {
                                r.cb(data);
                            }
                        });
                    }
                }
                
                // Move to next phoneme
                mConsumerReadIndex = (mConsumerReadIndex + 1) % MAX_PHONEMES;
                
                // Update read_index in shared memory
                *reinterpret_cast<std::uint32_t*>(shm_base + READ_INDEX_OFFSET) = mConsumerReadIndex;
            }
            
            // // Small delay to avoid busy waiting
            // std::this_thread::sleep_for(std::chrono::milliseconds(1));
            
        } catch (const std::exception& e) {
            std::cerr << "[LlmStreamService] Error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    std::cout << "[LlmStreamService] Phoneme reader thread stopped" << std::endl;
}

bool LlmStreamService::sendControlCommand(const std::string& command)
{
    // Connect to TTS control socket
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        std::cerr << "[LlmStreamService] Failed to create control socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, mControlSocketPath.c_str(), sizeof(addr.sun_path) - 1);
    
    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[LlmStreamService] Failed to connect to control socket at " 
                  << mControlSocketPath << ": " << strerror(errno) << std::endl;
        close(sock_fd);
        return false;
    }
    
    // Send command
    std::string cmd = command + "\n";
    ssize_t sent = write(sock_fd, cmd.c_str(), cmd.length());
    close(sock_fd);
    
    if (sent < 0) {
        std::cerr << "[LlmStreamService] Failed to send command: " << strerror(errno) << std::endl;
        return false;
    }
    
    std::cout << "[LlmStreamService] Sent control command: " << command << std::endl;
    return true;
}


