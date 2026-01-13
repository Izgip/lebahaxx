#ifndef TSUNAMI_SEARCHER_HPP
#define TSUNAMI_SEARCHER_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <functional>
#include <mach-o/dyld.h>
namespace tsunami {

// ==================== SIMPLE OFFSETS ====================
struct SearcherOffsets {
    uintptr_t taskscheduler_address = 0x10184b28c + _dyld_get_image_vmaddr_slide(0);
    uintptr_t job_name_offset = 0x18;
    uintptr_t jobs_start_offset = 0x1F0;
    uintptr_t jobs_end_offset = 0x1F8;
    uintptr_t job_struct_size = 0x10;
    
    // Your function offsets
    uintptr_t rbx_getstate = 0x104033b24 + _dyld_get_image_vmaddr_slide(0); // getstate offset
    uintptr_t lua_newthread = 0x1033bd25c + _dyld_get_image_vmaddr_slide(0); // newthread offset
    uintptr_t lua_settop = 0x1033bd340 + _dyld_get_image_vmaddr_slide(0); // settop offset
    uintptr_t script_context_offset = 0x210;
};

// ==================== CLEAN THREAD MANAGER ====================
class LuaStateSearcher {
private:
    SearcherOffsets offsets;
    
    // Thread management
    std::atomic<bool> running{false};
    std::atomic<bool> has_thread{false};
    std::thread search_thread;
    
    // Function pointers
    using RBXGetStateFn = uint64_t(*)(uint64_t, void*, const int64_t*);
    RBXGetStateFn rbx_getstate = nullptr;
    
    using LuaNewThreadFn = lua_State*(*)(lua_State*);
    LuaNewThreadFn lua_newthread = nullptr;
    
    using LuaSetTopFn = void(*)(lua_State*, int);
    LuaSetTopFn lua_settop = nullptr;
    
    // Thread type for getstate
    int thread_type = 0;
    
    // Our clean thread (NOT the captured one)
    std::atomic<uint64_t> clean_lua_thread{0};
    std::atomic<uint64_t> original_captured_state{0};
    std::atomic<uint64_t> script_context{0};
    
    // Callback when clean thread is ready
    std::function<void(uint64_t)> on_thread_ready;
    std::mutex callback_mutex;
    
public:
    LuaStateSearcher() {
        initialize();
    }
    
    LuaStateSearcher(const SearcherOffsets& offs) : offsets(offs) {
        initialize();
    }
    
    ~LuaStateSearcher() {
        stop();
        cleanupThread();
    }
    
    // ==================== INITIALIZATION ====================
    void initialize() {
        rbx_getstate = reinterpret_cast<RBXGetStateFn>(offsets.rbx_getstate);
        lua_newthread = reinterpret_cast<LuaNewThreadFn>(offsets.lua_newthread);
        lua_settop = reinterpret_cast<LuaSetTopFn>(offsets.lua_settop);
        
        std::cout << "[Tsunami] Function pointers initialized:\n";
        std::cout << "  rbx_getstate: 0x" << std::hex << offsets.rbx_getstate << "\n";
        std::cout << "  lua_newthread: 0x" << offsets.lua_newthread << "\n";
        std::cout << "  lua_settop: 0x" << offsets.lua_settop << "\n" << std::dec;
    }
    
    void setOffsets(const SearcherOffsets& newOffsets) {
        offsets = newOffsets;
        initialize();
    }
    
    void setOnThreadReady(std::function<void(uint64_t)> callback) {
        std::lock_guard<std::mutex> lock(callback_mutex);
        on_thread_ready = callback;
    }
    
    // ==================== THREAD MANAGEMENT ====================
    void start() {
        if (running) return;
        
        running = true;
        has_thread = false;
        search_thread = std::thread(&LuaStateSearcher::searchLoop, this);
        
        std::cout << "[Tsunami] Lua state searcher started\n";
    }
    
    void stop() {
        running = false;
        if (search_thread.joinable()) {
            search_thread.join();
        }
        std::cout << "[Tsunami] Lua state searcher stopped\n";
    }
    
    bool isRunning() const { return running; }
    bool hasCleanThread() const { return has_thread; }
    uint64_t getCleanThread() const { return clean_lua_thread.load(); }
    uint64_t getOriginalState() const { return original_captured_state.load(); }
    
    // ==================== CLEANUP ====================
    void cleanupThread() {
        uint64_t thread = clean_lua_thread.load();
        if (thread != 0) {
            // Clean up the thread using settop
            lua_State* L = reinterpret_cast<lua_State*>(thread);
            if (lua_settop) {
                lua_settop(L, 0); // Clear the stack
            }
            clean_lua_thread = 0;
            has_thread = false;
            std::cout << "[Tsunami] Cleaned up thread\n";
        }
    }
    
    // ==================== MANUAL SEARCH ====================
    bool findAndCreateThreadNow() {
        if (has_thread) return true;
        
        try {
            return performSearchAndCreateThread();
        } catch (const std::exception& e) {
            std::cerr << "[Tsunami] Search error: " << e.what() << "\n";
            return false;
        }
    }
    
private:
    // ==================== SEARCH LOOP ====================
    void searchLoop() {
        std::cout << "[Tsunami] Beginning clean thread search...\n";
        
        while (running && !has_thread) {
            try {
                if (performSearchAndCreateThread()) {
                    has_thread = true;
                    std::cout << "[Tsunami] ✓ Clean thread created: 0x" 
                              << std::hex << clean_lua_thread.load() << std::dec << "\n";
                    
                    // Notify callback
                    std::lock_guard<std::mutex> lock(callback_mutex);
                    if (on_thread_ready) {
                        on_thread_ready(clean_lua_thread.load());
                    }
                    
                    // Monitor thread health
                    monitorThread();
                } else {
                    std::cout << "[Tsunami] . Searching for clean thread...\n";
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            } catch (const std::exception& e) {
                std::cerr << "[Tsunami] Search loop error: " << e.what() << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    }
    
    // ==================== THREAD CREATION ====================
    bool performSearchAndCreateThread() {
        // 1. Find the captured thread
        uint64_t captured_state = findCapturedThread();
        if (captured_state == 0) {
            std::cout << "[Tsunami] No captured thread found\n";
            return false;
        }
        
        std::cout << "[Tsunami] Found captured thread: 0x" << std::hex << captured_state << std::dec << "\n";
        
        // 2. Create clean thread from captured thread
        return createCleanThread(captured_state);
    }
    
    uint64_t findCapturedThread() {
        if (!isValidPointer(offsets.taskscheduler_address)) {
            return 0;
        }
        
        uint64_t scheduler = *reinterpret_cast<uint64_t*>(offsets.taskscheduler_address);
        if (scheduler == 0) {
            return 0;
        }
        
        // Get job list
        uint64_t jobs_start = *reinterpret_cast<uint64_t*>(scheduler + offsets.jobs_start_offset);
        uint64_t jobs_end = *reinterpret_cast<uint64_t*>(scheduler + offsets.jobs_end_offset);
        
        if (jobs_start == 0 || jobs_end == 0 || jobs_start >= jobs_end) {
            return 0;
        }
        
        // Scan jobs
        for (uint64_t job_ptr = jobs_start; job_ptr < jobs_end; job_ptr += offsets.job_struct_size) {
            if (!isValidPointer(job_ptr)) continue;
            
            uint64_t job = *reinterpret_cast<uint64_t*>(job_ptr);
            if (job == 0) continue;
            
            // Check job name
            const char* job_name = *reinterpret_cast<const char**>(job + offsets.job_name_offset);
            if (!job_name || !isValidString(job_name)) continue;
            
            std::string name(job_name);
            
            if (name.find("WaitingHybridScriptsJob") != std::string::npos ||
                name.find("HybridScripts") != std::string::npos) {
                
                // Get script context
                uint64_t context = *reinterpret_cast<uint64_t*>(job + offsets.script_context_offset);
                
                if (context != 0 && rbx_getstate) {
                    const int64_t trigger = 0;
                    uint64_t captured_state = rbx_getstate(context, &thread_type, &trigger);
                    
                    if (captured_state != 0) {
                        original_captured_state = captured_state;
                        script_context = context;
                        return captured_state;
                    }
                }
            }
        }
        
        return 0;
    }
    
    bool createCleanThread(uint64_t captured_state) {
        if (!lua_newthread || !lua_settop) {
            std::cerr << "[Tsunami] Missing function pointers\n";
            return false;
        }
        
        lua_State* captured_L = reinterpret_cast<lua_State*>(captured_state);
        
        // Create new thread from captured state
        lua_State* new_thread = lua_newthread(captured_L);
        if (!new_thread) {
            std::cerr << "[Tsunami] Failed to create new thread\n";
            return false;
        }
        
        std::cout << "[Tsunami] Created clean thread: 0x" << std::hex 
                  << reinterpret_cast<uint64_t>(new_thread) << std::dec << "\n";
        
        // IMPORTANT: Clean up the captured thread's stack
        // This is where Macsploit/Abyss went wrong - they used the captured thread
        lua_settop(captured_L, 0); // Clear captured thread stack
        
        // Also clear new thread stack to be safe
        lua_settop(new_thread, 0);
        
        // Store the clean thread
        clean_lua_thread = reinterpret_cast<uint64_t>(new_thread);
        
        std::cout << "[Tsunami] ✓ Clean thread ready (captured thread cleared)\n";
        return true;
    }
    
    // ==================== THREAD MONITORING ====================
    void monitorThread() {
        while (running && has_thread) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            
            if (!verifyThread()) {
                std::cout << "[Tsunami] ! Clean thread lost, searching again...\n";
                cleanupThread();
                has_thread = false;
                break;
            }
        }
    }
    
    bool verifyThread() {
        uint64_t thread = clean_lua_thread.load();
        if (thread == 0) return false;
        
        return isValidPointer(thread);
    }
    
    // ==================== UTILITY FUNCTIONS ====================
    bool isValidPointer(uintptr_t ptr) {
        if (ptr == 0) return false;
        
        // Simple validation
        __try {
            volatile uint8_t test = *reinterpret_cast<uint8_t*>(ptr);
            (void)test;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }
    
    bool isValidString(const char* str) {
        if (!str) return false;
        
        __try {
            for (int i = 0; i < 256; i++) {
                if (str[i] == 0) return true;
                if (str[i] < 32 || str[i] > 126) return false;
            }
            return false;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }
};

// ==================== INTEGRATION WITH VM ====================
class CleanThreadManager {
private:
    std::unique_ptr<LuaStateSearcher> searcher;
    uint64_t vm_thread = 0;
    std::mutex thread_mutex;
    
public:
    CleanThreadManager(const SearcherOffsets& offsets = SearcherOffsets()) {
        searcher = std::make_unique<LuaStateSearcher>(offsets);
        
        // When thread is ready, store it
        searcher->setOnThreadReady([this](uint64_t thread) {
            std::lock_guard<std::mutex> lock(thread_mutex);
            vm_thread = thread;
            std::cout << "[Tsunami] VM thread stored: 0x" << std::hex << thread << std::dec << "\n";
        });
    }
    
    void start() {
        searcher->start();
    }
    
    void stop() {
        searcher->stop();
        cleanup();
    }
    
    bool isReady() {
        std::lock_guard<std::mutex> lock(thread_mutex);
        return vm_thread != 0;
    }
    
    lua_State* getThread() {
        std::lock_guard<std::mutex> lock(thread_mutex);
        return reinterpret_cast<lua_State*>(vm_thread);
    }
    
    void cleanup() {
        std::lock_guard<std::mutex> lock(thread_mutex);
        if (vm_thread != 0) {
            // Clean up using settop
            auto offsets = searcher->setOffsets; // You'd need access to settop function
            // This would be handled by the searcher's cleanup
            vm_thread = 0;
        }
    }
    
    LuaStateSearcher* getSearcher() { return searcher.get(); }
};

// ==================== GLOBAL MANAGER ====================
namespace global {
    extern std::unique_ptr<CleanThreadManager> thread_manager;
    
    inline void initializeManager(const SearcherOffsets& offsets = SearcherOffsets()) {
        if (!thread_manager) {
            thread_manager = std::make_unique<CleanThreadManager>(offsets);
        }
    }
    
    inline CleanThreadManager* getManager() {
        return thread_manager.get();
    }
    
    inline lua_State* getVMThread() {
        return thread_manager ? thread_manager->getThread() : nullptr;
    }
    
    inline bool isVMReady() {
        return thread_manager && thread_manager->isReady();
    }
}

} // namespace tsunami

#endif // TSUNAMI_SEARCHER_HPP
