#pragma once
#include <Zydis/Zydis.h>
#include <cstdint>
#include <cstddef>

namespace aeth {
    size_t get_stolen_bytes(void* target, size_t min_len);
    void relocate_code(uint8_t* src, uint8_t* dst, size_t len);
    void write_rel_jmp(void* at, void* to);
    void write_abs_jmp(void* at, void* to);
}