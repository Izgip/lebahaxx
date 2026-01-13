// tsunami_integration.hpp (updated)
#ifndef TSUNAMI_INTEGRATION_HPP
#define TSUNAMI_INTEGRATION_HPP

#include "tsunami_searcher.hpp"
#include "tsunami_vm.hpp"
#include <memory>

namespace tsunami {

class TsunamiSystem {
private:
    std::unique_ptr<CleanThreadManager> thread_manager;
    std::unique_ptr<BytecodeVM> vm;
    std::mutex vm_mutex;
    
    SearcherOffsets offsets;
    
public:
    TsunamiSystem(const SearcherOffsets& offs = SearcherOffsets()) : offsets(offs) {
        // Initialize thread manager (NOT the old searcher)
        thread_manager = std::make_unique<CleanThreadManager>(offsets);
        
        // Start searching for thread immediately
        thread_manager->start();
    }
    
    ~TsunamiSystem() {
        stop();
    }
    
    void start() {
        // Already starts in constructor
    }
    
    void stop() {
        if (thread_manager) {
            thread_manager->stop();
        }
        
        std::lock_guard<std::mutex> lock(vm_mutex);
        vm.reset();
    }
    
    bool isReady() const {
        std::lock_guard<std::mutex> lock(vm_mutex);
        return vm != nullptr;
    }
    
    // Wait for VM to be ready with timeout
    bool waitForReady(int timeout_seconds = 30) {
        auto start = std::chrono::steady_clock::now();
        
        while (std::chrono::steady_clock::now() - start < 
               std::chrono::seconds(timeout_seconds)) {
            
            if (isReady()) return true;
            
            // Check if thread is ready but VM not created yet
            if (thread_manager->isReady() && !isReady()) {
                createVMFromThread();
                if (isReady()) return true;
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        return false;
    }
    
    BytecodeVM* getVM() {
        std::lock_guard<std::mutex> lock(vm_mutex);
        return vm.get();
    }
    
    bool executeScript(const std::string& bytecode) {
        std::lock_guard<std::mutex> lock(vm_mutex);
        if (!vm) {
            // Try to create VM if thread is ready
            if (thread_manager->isReady()) {
                createVMFromThread();
            }
            if (!vm) {
                std::cerr << "[Tsunami] VM not ready\n";
                return false;
            }
        }
        return vm->execute(bytecode);
    }
    
    bool executeScriptSource(const std::string& source) {
        std::lock_guard<std::mutex> lock(vm_mutex);
        if (!vm) {
            if (thread_manager->isReady()) {
                createVMFromThread();
            }
            if (!vm) {
                std::cerr << "[Tsunami] VM not ready\n";
                return false;
            }
        }
        return vm->executeSource(source);
    }
    
    void registerFunction(const std::string& name, VMFunction func) {
        std::lock_guard<std::mutex> lock(vm_mutex);
        if (vm) {
            vm->registerGlobalFunction(name, func);
        }
    }
    
    lua_State* getLuaThread() {
        return thread_manager ? thread_manager->getThread() : nullptr;
    }
    
private:
    void createVMFromThread() {
        lua_State* thread = thread_manager->getThread();
        if (!thread) {
            std::cerr << "[Tsunami] No clean thread available\n";
            return;
        }
        
        std::cout << "[Tsunami] Creating VM with clean thread: 0x" 
                  << std::hex << reinterpret_cast<uint64_t>(thread) << std::dec << "\n";
        
        vm = std::make_unique<BytecodeVM>(thread);
        
        // Verify thread is clean
        if (offsets.lua_settop) {
            using LuaSetTopFn = void(*)(lua_State*, int);
            LuaSetTopFn settop = reinterpret_cast<LuaSetTopFn>(offsets.lua_settop);
            settop(thread, 0); // Ensure stack is clean
        }
        
        registerDefaultFunctions();
        
        std::cout << "[Tsunami] VM created with clean thread\n";
    }
    
    void registerDefaultFunctions() {
        if (!vm) return;
        
        vm->registerGlobalFunction("tsunami_thread_info", [this](const std::vector<VMValue>& args) -> VMValue {
            lua_State* thread = getLuaThread();
            std::string info = "Thread: 0x";
            
            std::ostringstream oss;
            oss << std::hex << reinterpret_cast<uint64_t>(thread);
            info += oss.str();
            
            return VMValue::String(info);
        });
        
        vm->registerGlobalFunction("tsunami_clean_stack", [this](const std::vector<VMValue>& args) -> VMValue {
            // This ensures stack stays clean
            lua_State* thread = getLuaThread();
            if (thread && offsets.lua_settop) {
                using LuaSetTopFn = void(*)(lua_State*, int);
                LuaSetTopFn settop = reinterpret_cast<LuaSetTopFn>(offsets.lua_settop);
                settop(thread, 0);
                return VMValue::String("Stack cleaned");
            }
            return VMValue::String("Cleanup failed");
        });
    }
};

// Global instance
namespace global {
    extern std::unique_ptr<TsunamiSystem> system;
    
    inline void initializeSystem(const SearcherOffsets& offsets = SearcherOffsets()) {
        if (!system) {
            system = std::make_unique<TsunamiSystem>(offsets);
        }
    }
    
    inline TsunamiSystem* getSystem() {
        return system.get();
    }
    
    inline bool executeScript(const std::string& bytecode) {
        return system ? system->executeScript(bytecode) : false;
    }
    
    inline lua_State* getThread() {
        return system ? system->getLuaThread() : nullptr;
    }
}

} // namespace tsunami

#endif // TSUNAMI_INTEGRATION_HPP
