#include "module.hpp"
#include "mem.hpp"
#include <cstdint>
#include <vector>
#include <link.h> 
#include <sstream>
#include <iostream>
#include <dlfcn.h>

typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);

namespace aeth {

std::string ptr_to_hex(uintptr_t ptr) {
    std::stringstream ss;
    ss << "0x" << std::hex << std::uppercase << ptr;
    return ss.str();
}

struct Signature {
    std::vector<uint8_t> bytes;
    std::vector<bool> mask; 
};

static Signature parse_hex_sig(const std::string& sig) {
    Signature parsed;
    std::stringstream ss(sig);
    std::string word;
    while (ss >> word) {
        if (word == "?" || word == "??") {
            parsed.bytes.push_back(0);
            parsed.mask.push_back(false);
        } else {
            parsed.bytes.push_back((uint8_t)std::stoul(word, nullptr, 16));
            parsed.mask.push_back(true);
        }
    }
    return parsed;
}

uintptr_t Module::get_vfunc(uintptr_t inst, int index) {
    if (!inst) return 0;
    void* object_ptr = reinterpret_cast<void*>(inst);
    return reinterpret_cast<uintptr_t>(aeth::find_vfunc(object_ptr, index));
}

uintptr_t Module::get_interface(const char* interface_name) {
    void* handle = dlopen(name.c_str(), RTLD_NOLOAD | RTLD_NOW);
    if (!handle) {
        handle = dlopen(name.c_str(), RTLD_LAZY);
    }
    
    if (!handle) return 0;

    auto factory = (CreateInterfaceFn)dlsym(handle, "CreateInterface");
    if (!factory) return 0;

    void* inst = factory(interface_name, nullptr);
    return reinterpret_cast<uintptr_t>(inst);
}

uintptr_t Module::find_signature(const std::string& sig_str) {
    if (!base || !size) return 0;

    auto sig = parse_hex_sig(sig_str);
    uint8_t* scan_start = reinterpret_cast<uint8_t*>(base);
    
    for (size_t i = 0; i < size - sig.bytes.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < sig.bytes.size(); ++j) {
            if (sig.mask[j] && scan_start[i + j] != sig.bytes[j]) {
                found = false;
                break;
            }
        }
        if (found) return reinterpret_cast<uintptr_t>(&scan_start[i]);
    }
    std::cerr << "[aeth] signature not found in module '" << name << "'" << std::endl;
    return 0;
}

static int l_module_get_proc_address(lua_State* L) {
    auto* mod = *static_cast<Module**>(luaL_checkudata(L, 1, "aeth.module"));
    const char* sym_name = luaL_checkstring(L, 2);
    void* handle = dlopen(mod->name.c_str(), RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) handle = dlopen(mod->name.c_str(), RTLD_LAZY);
    void* addr = dlsym(handle, sym_name);
    lua_pushstring(L, ptr_to_hex(reinterpret_cast<uintptr_t>(addr)).c_str());
    return 1;
}


static int l_module_get_interface(lua_State* L) {
    auto* mod = *static_cast<Module**>(luaL_checkudata(L, 1, "aeth.module"));
    const char* interface_name = luaL_checkstring(L, 2);
    uintptr_t addr = mod->get_interface(interface_name);
    lua_pushstring(L, ptr_to_hex(addr).c_str());
    return 1;
}

static int l_module_get_vfunc(lua_State* L) {
    auto* mod = *static_cast<Module**>(luaL_checkudata(L, 1, "aeth.module"));
    std::string inst_str = luaL_checkstring(L, 2);
    int index = (int)luaL_checkinteger(L, 3);
    uintptr_t inst = std::stoull(inst_str, nullptr, 16);
    uintptr_t addr = mod->get_vfunc(inst, index);  
    lua_pushstring(L, ptr_to_hex(addr).c_str());
    return 1;
}

static int l_module_find_sig(lua_State* L) {
    auto* mod = *static_cast<Module**>(luaL_checkudata(L, 1, "aeth.module"));
    const char* sig = luaL_checkstring(L, 2);
    uintptr_t addr = mod->find_signature(sig);
    lua_pushstring(L, addr == 0 ? "0x0" : ptr_to_hex(addr).c_str());
    return 1; 
}

void export_module(lua_State* L, const char* lua_name, const char* so_name) {
   auto* mod = new Module(so_name);
    
    if (mod->base == 0) {
        std::cerr << "[aeth] " << so_name << "' not found in process memory map" << std::endl;
    }

    Module** udata = static_cast<Module**>(lua_newuserdata(L, sizeof(Module*)));
    *udata = mod;

    if ((*udata)->base == 0) {
        printf("[aeth] '%s' was not found in the process memory map.\n", so_name);
    }

    if (luaL_newmetatable(L, "aeth.module")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        
        lua_pushcfunction(L, l_module_find_sig);
        lua_setfield(L, -2, "find_sig");
        
        lua_pushcfunction(L, l_module_get_vfunc);
        lua_setfield(L, -2, "get_vfunc");

        lua_pushcfunction(L, l_module_get_interface);
        lua_setfield(L, -2, "get_interface");

        lua_pushcfunction(L, l_module_get_proc_address);
        lua_setfield(L, -2, "get_proc_address");
    }
    lua_setmetatable(L, -2);
    lua_setglobal(L, lua_name);
}

Module::Module(const std::string& module_name) : name(module_name), base(0), size(0) {
    dl_iterate_phdr([](struct dl_phdr_info* info, size_t, void* data) -> int {
        Module* m = reinterpret_cast<Module*>(data);
        std::string current_path = info->dlpi_name;

        if (current_path.empty()) return 0;

        bool match = false;
        if (m->name == current_path) {
            match = true;
        } 
        else if (m->name.size() > 2 && m->name.substr(0, 2) == "./") {
            std::string suffix = m->name.substr(1);
            if (current_path.size() >= suffix.size() &&
                current_path.compare(current_path.size() - suffix.size(), suffix.size(), suffix) == 0) {
                match = true;
            }
        }
        else if (m->name.find('/') == std::string::npos) {
            size_t pos = current_path.find_last_of('/');
            std::string filename = (pos == std::string::npos) ? current_path : current_path.substr(pos + 1);
            if (filename == m->name) {
                match = true;
            }
        }

        if (match) {
            m->name = current_path; 
            m->base = info->dlpi_addr;
            m->size = 0;
            for (int i = 0; i < info->dlpi_phnum; i++) {
                uintptr_t end_addr = info->dlpi_phdr[i].p_vaddr + info->dlpi_phdr[i].p_memsz;
                if (end_addr > m->size) m->size = end_addr;
            }
            return 1;
        }
        return 0;
    }, this);
}
} // namespace aeth