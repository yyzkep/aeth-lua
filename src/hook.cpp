#include "hook.hpp"
#include "relo.hpp"
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace aeth {

thread_local bool HookManager::locked = false;

Hook::Hook(std::string n, void* t) : name(std::move(n)), target(t), closure(nullptr) {
    tramp = mmap(nullptr, 128, PROT_READ | PROT_WRITE | PROT_EXEC, 
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (tramp == MAP_FAILED) {
        std::cerr << "[aeth] failed to allocate trampoline memory for " << name << std::endl;
    }
}

Hook::~Hook() {
    if (closure) {
        ffi_closure_free(closure);
    }
    if (tramp && tramp != MAP_FAILED) {
        munmap(tramp, 128);
    }
}

HookManager& HookManager::get() {
    static HookManager instance;
    return instance;
}

void HookManager::install(const std::string& name, const std::string& target_input) {
   void* target = nullptr;
    if (target_input.size() > 2 && target_input.substr(0, 2) == "0x") {
        try {
            target = reinterpret_cast<void*>(std::stoull(target_input, nullptr, 16));
        } catch (const std::exception& e) {
            std::cerr << "[aeth] invalid hex format '" << target_input << "'" << std::endl;
            return;
        }
    } else {
        target = dlsym(RTLD_DEFAULT, target_input.c_str());
    }

   if (!target) {
        std::cerr << "[aeth] symbol resolution failed for '" << target_input << "'" << std::endl;
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(mtx);
    for (auto const& [hook_name, hook_ptr] : hooks) {
        if (hook_ptr->target == target) {
            std::cerr << "[aeth] target hook has already been hooked." << target << std::endl;
            return;
        }
    }

    size_t stolen = get_stolen_bytes(target, 5);
    auto h = std::make_unique<Hook>(name, target);
    h->stolen = stolen;

   if (h->tramp == MAP_FAILED) {
        std::cerr << "[aeth] trampoline memory allocation failed" << std::endl;
        return;
    }

    size_t page_size = sysconf(_SC_PAGESIZE);
    uintptr_t base_addr = reinterpret_cast<uintptr_t>(target) & ~(page_size - 1);
    mprotect(reinterpret_cast<void*>(base_addr), page_size * 2, PROT_READ | PROT_WRITE | PROT_EXEC);

    relocate_code((uint8_t*)target, (uint8_t*)h->tramp, stolen);

    write_abs_jmp((uint8_t*)h->tramp + stolen, (uint8_t*)target + stolen);

    static ffi_type* args[2] = { &ffi_type_pointer, &ffi_type_pointer };
    if (ffi_prep_cif(&h->cif, FFI_DEFAULT_ABI, 2, &ffi_type_void, args) != FFI_OK) {
        std::cerr << "[aeth] ffi cif preparation failed for " << name << std::endl;
        return;
    }

    h->closure = (ffi_closure*)ffi_closure_alloc(sizeof(ffi_closure), &h->closure_entry);
    if (ffi_prep_closure_loc(h->closure, &h->cif, cb, h.get(), h->closure_entry) != FFI_OK) {
        std::cerr << "[aeth] ffi closure linkage failed for " << name << std::endl;
        return;
    }

    write_rel_jmp(target, h->closure_entry);
    
    if (stolen > 5) {
        size_t ps = sysconf(_SC_PAGESIZE);
        void* base = (void*)((uintptr_t)target & ~(ps - 1));
        if (mprotect(base, ps * 2, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
            std::memset((uint8_t*)target + 5, 0x90, stolen - 5);
            mprotect(base, ps * 2, PROT_READ | PROT_EXEC);
            printf("[aeth] applied %zu NOP(s) at %p\n", stolen - 5, (uint8_t*)target + 5);
        } else {
            perror("[aeth] mprotect failed during NOP alignment");
        }
    }
    
    hooks[name] = std::move(h);
    printf("[aeth] hooked '%s'\n", name.c_str());
}
void HookManager::cb(ffi_cif*, void*, void** args, void* user) {
    auto& mgr = get();

    if (!mgr.L || locked) {
        return;
    }
    
    locked = true;
    {
        std::lock_guard<std::recursive_mutex> lock(mgr.mtx);
        auto* h = static_cast<Hook*>(user);
        
        int top = lua_gettop(mgr.L);

        lua_getglobal(mgr.L, "hook");
        if (lua_istable(mgr.L, -1)) {
            lua_getfield(mgr.L, -1, "event_bus");
            if (lua_isfunction(mgr.L, -1)) {
                lua_pushstring(mgr.L, h->name.c_str());
                lua_pushlightuserdata(mgr.L, *(void**)args[0]);
                lua_pushlightuserdata(mgr.L, *(void**)args[1]);
                
                if (lua_pcall(mgr.L, 3, 0, 0) != LUA_OK) {
                    std::cerr << "[aeth] lua runtime error at [" << h->name << "]: " << lua_tostring(mgr.L, -1) << std::endl;
                    lua_pop(mgr.L, 1);
                }
            }
        }
        
        lua_settop(mgr.L, top);
    }
    locked = false;
}

} // namespace Aeth