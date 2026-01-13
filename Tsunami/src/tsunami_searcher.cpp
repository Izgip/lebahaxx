#include "tsunami_searcher.hpp"
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <cstring>
#include <stdexcept>

namespace tsunami {
namespace global {
    std::unique_ptr<CleanThreadManager> thread_manager;
}
}

// ==================== macOS-SPECIFIC IMPLEMENTATION ====================

bool tsunami::LuaStateSearcher::isValidPointer(uintptr_t ptr) {
    if (ptr == 0 || ptr == (uintptr_t)-1) return false;
    
    // Use vm_region to check if memory is readable
    vm_address_t address = (vm_address_t)ptr;
    vm_size_t size = 1;
    vm_region_basic_info_data_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT;
    memory_object_name_t object;
    
    kern_return_t kr = vm_region(
        mach_task_self(),
        &address,
        &size,
        VM_REGION_BASIC_INFO,
        (vm_region_info_t)&info,
        &info_count,
        &object
    );
    
    if (kr != KERN_SUCCESS) return false;
    
    // Check if region is readable
    return (info.protection & VM_PROT_READ) != 0;
}

bool tsunami::LuaStateSearcher::isValidString(const char* str) {
    if (!str || !isValidPointer((uintptr_t)str)) return false;
    
    // Use sigaction for signal-safe string checking
    struct sigaction old_action, new_action;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    new_action.sa_handler = SIG_IGN;
    
    sigaction(SIGSEGV, &new_action, &old_action);
    sigaction(SIGBUS, &new_action, &old_action);
    
    bool valid = false;
    __try {
        // Check for reasonable string
        for (int i = 0; i < 256; i++) {
            char c = str[i];
            if (c == 0) {
                valid = (i > 0); // Empty string might be valid but not interesting
                break;
            }
            // Basic ASCII printable check
            if (c < 32 || c > 126) {
                valid = false;
                break;
            }
        }
    } __catch (...) {
        valid = false;
    }
    
    // Restore signal handlers
    sigaction(SIGSEGV, &old_action, nullptr);
    sigaction(SIGBUS, &old_action, nullptr);
    
    return valid;
}

// Alternative signal-safe memory checking using mincore
bool tsunami::LuaStateSearcher::isValidPointerMinCore(uintptr_t ptr) {
    if (ptr == 0) return false;
    
    // Check if page is resident
    unsigned char vec;
    int result = mincore((void*)(ptr & ~(getpagesize() - 1)), 1, &vec);
    
    return result == 0;
}

// ==================== ASLR SLIDE UTILITIES ====================

uintptr_t tsunami::get_aslr_slide() {
    static uintptr_t slide = UINTPTR_MAX;
    
    if (slide == UINTPTR_MAX) {
        // Try to find our dylib by name first
        const char* our_names[] = {"Tsunami", "tsunami", "libTsunami", "libtsunami", "dylib"};
        
        for (uint32_t i = 0; i < _dyld_image_count(); i++) {
            const char* name = _dyld_get_image_name(i);
            if (!name) continue;
            
            // Get just the filename
            const char* basename = strrchr(name, '/');
            if (!basename) basename = name;
            else basename++;
            
            for (auto& n : our_names) {
                if (strstr(basename, n) != nullptr) {
                    slide = _dyld_get_image_vmaddr_slide(i);
                    printf("[Tsunami] Found dylib '%s' slide: 0x%lx\n", basename, slide);
                    return slide;
                }
            }
        }
        
        // Fallback: use slide of image containing this function
        Dl_info info;
        if (dladdr((void*)&tsunami::get_aslr_slide, &info)) {
            for (uint32_t i = 0; i < _dyld_image_count(); i++) {
                if (_dyld_get_image_header(i) == info.dli_fbase) {
                    slide = _dyld_get_image_vmaddr_slide(i);
                    printf("[Tsunami] Using our dylib slide: 0x%lx\n", slide);
                    return slide;
                }
            }
        }
        
        // Last resort: main executable slide
        slide = _dyld_get_image_vmaddr_slide(0);
        printf("[Tsunami] Using main executable slide: 0x%lx\n", slide);
    }
    
    return slide;
}

// Update offsets with slide dynamically
tsunami::SearcherOffsets tsunami::get_offsets_with_slide() {
    uintptr_t slide = get_aslr_slide();
    
    SearcherOffsets offsets;
    offsets.taskscheduler_address = 0x10184b28c + slide;
    offsets.rbx_getstate = 0x104033b24 + slide;
    offsets.lua_newthread = 0x1033bd25c + slide;
    offsets.lua_settop = 0x1033bd340 + slide;
    offsets.script_context_offset = 0x210;
    
    // Static offsets (don't change with ASLR)
    offsets.job_name_offset = 0x18;
    offsets.jobs_start_offset = 0x1F0;
    offsets.jobs_end_offset = 0x1F8;
    offsets.job_struct_size = 0x10;
    
    printf("[Tsunami] Calculated offsets with slide 0x%lx:\n", slide);
    printf("  taskscheduler: 0x%lx\n", offsets.taskscheduler_address);
    printf("  rbx_getstate: 0x%lx\n", offsets.rbx_getstate);
    printf("  lua_newthread: 0x%lx\n", offsets.lua_newthread);
    printf("  lua_settop: 0x%lx\n", offsets.lua_settop);
    
    return offsets;
}

// ==================== C INTERFACE (for Rust integration) ====================

extern "C" {
    
    TSUNAMI_API void tsunami_start() {
        auto offsets = tsunami::get_offsets_with_slide();
        tsunami::global::initializeManager(offsets);
        
        auto manager = tsunami::global::getManager();
        if (manager) {
            manager->start();
        }
    }
    
    TSUNAMI_API void tsunami_stop() {
        auto manager = tsunami::global::getManager();
        if (manager) {
            manager->stop();
        }
    }
    
    TSUNAMI_API uint64_t tsunami_get_thread() {
        auto manager = tsunami::global::getManager();
        if (manager && manager->isReady()) {
            return reinterpret_cast<uint64_t>(manager->getThread());
        }
        return 0;
    }
    
    TSUNAMI_API bool tsunami_is_ready() {
        return tsunami::global::isVMReady();
    }
    
    TSUNAMI_API uintptr_t tsunami_get_slide() {
        return tsunami::get_aslr_slide();
    }
}

// ==================== TEST FUNCTIONS ====================

void tsunami::test_memory_access() {
    printf("[Tsunami] Testing memory access...\n");
    
    // Test our offsets are valid
    auto offsets = get_offsets_with_slide();
    
    if (isValidPointer(offsets.taskscheduler_address)) {
        printf("  taskscheduler @ 0x%lx: OK\n", offsets.taskscheduler_address);
    } else {
        printf("  taskscheduler @ 0x%lx: INVALID\n", offsets.taskscheduler_address);
    }
    
    // Test function pointers
    void* getstate_ptr = reinterpret_cast<void*>(offsets.rbx_getstate);
    Dl_info info;
    if (dladdr(getstate_ptr, &info)) {
        printf("  rbx_getstate @ 0x%lx: OK (in %s)\n", offsets.rbx_getstate, info.dli_fname);
    } else {
        printf("  rbx_getstate @ 0x%lx: NOT FOUND\n", offsets.rbx_getstate);
    }
}
