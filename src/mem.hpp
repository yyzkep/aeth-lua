#pragma once
#include <iostream>
namespace aeth {
template <typename T, typename... Args>
auto call_vfunc(void *instance, int index, Args... args) {
  using Fn = T (*)(void *, Args...);
  void **vtable = *static_cast<void ***>(instance);
  auto fn = reinterpret_cast<Fn>(vtable[index]);
  return fn(instance, args...);
}

inline void *find_vfunc(void *ptr, const unsigned short index) {
  if (!ptr) {
    std::cout << "[aeth] there wasnt even a pointer to begin with..."
              << std::endl;
    return nullptr;
  }
  void **vtable = *static_cast<void ***>(ptr);
  void *func_addr = vtable[index];
  return func_addr;
}
} // namespace aeth