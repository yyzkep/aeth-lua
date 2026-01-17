#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hook.hpp"
#include "module.hpp"
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace aeth {

static uintptr_t hex_to_ptr(const std::string &str) {
  try {
    return std::stoull(str, nullptr, 16);
  } catch (...) {
    return 0;
  }
}

std::string ptr_to_hex(uintptr_t ptr) {
  std::stringstream ss;
  ss << "0x" << std::hex << std::uppercase << ptr;
  return ss.str();
}

fs::path get_dll_path() {
  Dl_info info;
  if (dladdr((void *)get_dll_path, &info) != 0 && info.dli_fname != nullptr) {
    return fs::path(info.dli_fname).parent_path();
  }
  return fs::current_path();
}
static int l_tramp(lua_State *L) {
  HookManager::get().install(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
  return 0;
}

static int l_orig(lua_State *L) {
  HookManager::get().call_original(luaL_checkstring(L, 1), lua_touserdata(L, 2),
                                   lua_touserdata(L, 3));
  return 0;
}

static int l_mem_read_str(lua_State *L) {
  uintptr_t addr = hex_to_ptr(luaL_checkstring(L, 1));
  size_t len = (size_t)luaL_checkinteger(L, 2);

  if (addr < 0x1000)
    return 0;
  lua_pushlstring(L, reinterpret_cast<const char *>(addr), len);
  return 1;
}

static int l_mem_alloc(lua_State *L) {
  size_t size = (size_t)luaL_checkinteger(L, 1);
  void *ptr = std::malloc(size);
  if (!ptr)
    return 0;
  std::memset(ptr, 0, size);
  lua_pushstring(L, ptr_to_hex(reinterpret_cast<uintptr_t>(ptr)).c_str());
  return 1;
}

static int l_mem_free(lua_State *L) {
  uintptr_t addr = hex_to_ptr(luaL_checkstring(L, 1));
  if (addr)
    std::free(reinterpret_cast<void *>(addr));
  return 0;
}

static int l_mem_write_int(lua_State *L) {
  uintptr_t addr = hex_to_ptr(luaL_checkstring(L, 1));
  int value = (int)luaL_checkinteger(L, 2);
  if (addr >= 0x1000) {
    *reinterpret_cast<int *>(addr) = value;
  }
  return 0;
}

// mem.call("0xADDR", arg1, arg2, arg3...)
static int l_mem_call(lua_State *L) {
  uintptr_t func_addr = hex_to_ptr(luaL_checkstring(L, 1));
  if (!func_addr)
    return 0;

  int num_args = lua_gettop(L) - 1;
  std::vector<void *> arg_values(num_args);
  std::vector<ffi_type *> arg_types(num_args);

  std::vector<void *> ptr_storage(num_args);

  for (int i = 0; i < num_args; i++) {
    ptr_storage[i] = lua_touserdata(L, i + 2);
    arg_values[i] = &ptr_storage[i];
    arg_types[i] = &ffi_type_pointer;
  }

  ffi_cif cif;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, num_args, &ffi_type_void,
                   arg_types.data()) == FFI_OK) {
    ffi_call(&cif, FFI_FN(func_addr), nullptr, arg_values.data());
  }

  return 0;
}

static int l_orig_variadic(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  int top = lua_gettop(L);
  std::vector<void *> args;
  for (int i = 2; i <= top; i++) {
    args.push_back(lua_touserdata(L, i));
  }
  aeth::HookManager::get().call_original(name, args);
  return 0;
}

static int l_mem_call_bool(lua_State *L) {
  uintptr_t func_addr = hex_to_ptr(luaL_checkstring(L, 1));
  void *instance = reinterpret_cast<void *>(hex_to_ptr(luaL_checkstring(L, 2)));

  if (!func_addr || !instance) {
    lua_pushboolean(L, false);
    return 1;
  }

  typedef bool (*fn_t)(void *);
  lua_pushboolean(L, reinterpret_cast<fn_t>(func_addr)(instance));
  return 1;
}

static int l_mem_call_void_str(lua_State *L) {
  uintptr_t func_addr = hex_to_ptr(luaL_checkstring(L, 1));
  void *instance = reinterpret_cast<void *>(hex_to_ptr(luaL_checkstring(L, 2)));
  const char *arg = luaL_checkstring(L, 3);

  if (func_addr && instance) {
    typedef void (*fn_t)(void *, const char *);
    reinterpret_cast<fn_t>(func_addr)(instance, arg);
  }
  return 0;
}

static int l_reload(lua_State *L) {
  fs::path base_path = get_dll_path();
  fs::path init_file = base_path / "scripts" / "init.lua";

  if (luaL_dofile(L, init_file.c_str()) != LUA_OK) {
    std::cerr << "[aeth] runtime error: " << lua_tostring(L, -1) << std::endl;
  }
  return 0;
}

static int l_mem_call_int(lua_State *L) {
  uintptr_t func_addr = hex_to_ptr(luaL_checkstring(L, 1));
  void *instance = reinterpret_cast<void *>(hex_to_ptr(luaL_checkstring(L, 2)));

  if (!func_addr || !instance) {
    lua_pushinteger(L, 0);
    return 1;
  }

  typedef int (*fn_t)(void *);
  lua_pushinteger(L, reinterpret_cast<fn_t>(func_addr)(instance));
  return 1;
}

void register_mem_lib(lua_State *L) {
  lua_newtable(L);
  lua_pushcfunction(L, l_mem_read_str);
  lua_setfield(L, -2, "read_str");
  lua_pushcfunction(L, l_mem_write_int);
  lua_setfield(L, -2, "write_int");
  lua_pushcfunction(L, l_mem_alloc);
  lua_setfield(L, -2, "alloc");
  lua_pushcfunction(L, l_mem_free);
  lua_setfield(L, -2, "free");
  lua_pushcfunction(L, l_mem_call);
  lua_setfield(L, -2, "call");
  lua_pushcfunction(L, l_mem_call_bool);
  lua_setfield(L, -2, "call_bool");
  lua_pushcfunction(L, l_mem_call_int);
  lua_setfield(L, -2, "call_int");
  lua_pushcfunction(L, l_mem_call_void_str);
  lua_setfield(L, -2, "call_void_str");
  lua_setglobal(L, "mem");
}

} // namespace aeth

__attribute__((constructor)) static void load() {
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);

  aeth::HookManager::get().set_lua(L);

  lua_newtable(L);
  lua_pushcfunction(L, aeth::l_tramp);
  lua_setfield(L, -2, "trampoline");
  lua_pushcfunction(L, aeth::l_orig);
  lua_setfield(L, -2, "call_original");
  lua_pushcfunction(L, aeth::l_reload);
  lua_setfield(L, -2, "reload");
  lua_setglobal(L, "hook");

  aeth::register_mem_lib(L);
  aeth::export_module(L, "client", "./tf/bin/linux64/client.so");
  aeth::export_module(L, "engine", "./bin/linux64/engine.so");
  aeth::export_module(L, "surface", "./bin/linux64/vguimatsurface.so");
  aeth::export_module(L, "sdl", "libSDL2-2.0.so.0");

  fs::path base_path = aeth::get_dll_path();
  fs::path scripts_path = base_path / "scripts";
  fs::path init_file = scripts_path / "init.lua";

  fs::path source_folder = "scripts";

  try {
    if (!fs::exists(scripts_path)) {
      fs::create_directories(scripts_path);
      fs::copy(source_folder, scripts_path,
               fs::copy_options::recursive |
                   fs::copy_options::overwrite_existing);
    }
  } catch (const fs::filesystem_error &e) {
    std::cerr << "[aeth] failed to create directory: " << e.what() << std::endl;
  }

  if (luaL_dofile(L, init_file.c_str()) != LUA_OK) {
    std::cerr << "[aeth] runtime error: " << lua_tostring(L, -1) << std::endl;
  }
  printf("[aeth] done with interpreter.");
}