#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <ffi.h>
#include <lua.hpp>

namespace aeth {
    struct Hook {
        std::string name;
        void *target, *tramp;
        size_t stolen;
        ffi_cif cif;
        ffi_closure *closure;
        void *closure_entry;

        Hook(std::string n, void* t);
        ~Hook();
    };

    class HookManager {
        lua_State* L = nullptr;
        std::recursive_mutex mtx;
        std::unordered_map<std::string, std::unique_ptr<Hook>> hooks;
        static thread_local bool locked;

    public:
        static HookManager& get();
        void set_lua(lua_State* l) { L = l; }
        void install(const std::string& name, const std::string& sym);
        template <typename... Args>
        void call_original(const std::string& name, Args... args) {
            std::lock_guard<std::recursive_mutex> lock(mtx);
            auto it = hooks.find(name);
            if (it != hooks.end()) {
                void* ffi_args[] = { (&args)... };
                ffi_call(&it->second->cif, FFI_FN(it->second->tramp), nullptr, ffi_args);
            }
        }
        static void cb(ffi_cif* cif, void* ret, void** args, void* user);
    };
}