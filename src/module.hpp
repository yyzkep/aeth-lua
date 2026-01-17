#pragma once
#include <string>
#include <cstdint>
#include <lua.hpp>

namespace aeth {

class Module {
public:
    std::string name;
    uintptr_t base;
    size_t size;

    Module(const std::string& module_name);
    uintptr_t find_signature(const std::string& sig);
    uintptr_t get_vfunc(uintptr_t inst, int index);
    uintptr_t get_interface(const char* interface_name);
};

void export_module(lua_State* L, const char* lua_name, const char* so_name);

} // namespace aeth