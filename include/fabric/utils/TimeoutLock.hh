#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>

namespace fabric {
namespace Utils {

/**
 * @brief Utility for timeout-protected lock acquisition 
 * 
 * This class provides static methods for acquiring locks with timeout protection
 * to prevent deadlocks and ensure non-blocking behavior.
 * 
 * @tparam MutexType The type of mutex to lock (must support standard locking operations)
 */
template<typename MutexType>
class TimeoutLock {
public:
    /**
     * @brief Try to acquire a shared (read) lock with timeout
     * 
     * @param mutex The mutex to lock
     * @param timeout Maximum time to wait for the lock
     * @return An optional containing the lock if successful, or empty if timeout occurred
     */
    static std::optional<std::shared_lock<MutexType>> tryLockShared(
        MutexType& mutex, 
        std::chrono::milliseconds timeout = std::chrono::milliseconds(100)
    ) {
        // First try to acquire the lock without waiting
        std::shared_lock<MutexType> lock(mutex, std::try_to_lock);
        if (lock.owns_lock()) {
            return lock;
        }
        
        // If immediate acquisition failed, try with timeout
        auto start = std::chrono::steady_clock::now();
        while (true) {
            // Try to acquire the lock
            lock = std::shared_lock<MutexType>(mutex, std::try_to_lock);
            if (lock.owns_lock()) {
                return lock;
            }
            
            // Check if we've exceeded the timeout
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start) >= timeout) {
                // Return empty optional to indicate timeout
                return std::nullopt;
            }
            
            // Sleep briefly to avoid hammering the mutex
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    /**
     * @brief Try to acquire an exclusive (write) lock with timeout
     * 
     * @param mutex The mutex to lock
     * @param timeout Maximum time to wait for the lock
     * @return An optional containing the lock if successful, or empty if timeout occurred
     */
    static std::optional<std::unique_lock<MutexType>> tryLockUnique(
        MutexType& mutex, 
        std::chrono::milliseconds timeout = std::chrono::milliseconds(100)
    ) {
        // First try to acquire the lock without waiting
        std::unique_lock<MutexType> lock(mutex, std::try_to_lock);
        if (lock.owns_lock()) {
            return lock;
        }
        
        // If immediate acquisition failed, try with timeout
        auto start = std::chrono::steady_clock::now();
        while (true) {
            // Try to acquire the lock
            lock = std::unique_lock<MutexType>(mutex, std::try_to_lock);
            if (lock.owns_lock()) {
                return lock;
            }
            
            // Check if we've exceeded the timeout
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start) >= timeout) {
                // Return empty optional to indicate timeout
                return std::nullopt;
            }
            
            // Sleep briefly to avoid hammering the mutex
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    /**
     * @brief Try to upgrade a shared lock to an exclusive lock with timeout
     * 
     * Note: This method will release the shared lock and acquire a unique lock.
     * It is NOT an atomic operation and should be used with caution.
     * 
     * @param mutex The mutex to lock
     * @param sharedLock The existing shared lock to upgrade
     * @param timeout Maximum time to wait for the lock
     * @return An optional containing the upgraded lock if successful, or empty if timeout occurred
     */
    static std::optional<std::unique_lock<MutexType>> tryUpgradeLock(
        MutexType& mutex,
        std::shared_lock<MutexType>& sharedLock, 
        std::chrono::milliseconds timeout = std::chrono::milliseconds(100)
    ) {
        // Release the shared lock
        sharedLock.unlock();
        
        // Try to acquire an exclusive lock
        auto uniqueLock = tryLockUnique(mutex, timeout);
        
        // If we couldn't get the exclusive lock, try to reacquire the shared lock
        if (!uniqueLock) {
            sharedLock = std::shared_lock<MutexType>(mutex);
        }
        
        return uniqueLock;
    }
    
};

} // namespace Utils
} // namespace fabric