#include "relo.hpp"
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

namespace aeth {
    size_t get_stolen_bytes(void* target, size_t min_len) {
        ZydisDecoder decoder;
        ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        ZydisDecodedInstruction insn;
        ZydisDecodedOperand operands[10]; 
        size_t offset = 0;
        while (offset < min_len) {
            if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, (uint8_t*)target + offset, 15, &insn, operands))) break; 
            offset += insn.length;
        }
        return offset;
    }

    void relocate_code(uint8_t* src, uint8_t* dst, size_t len) {
        ZydisDecoder decoder;
        ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        ZydisDecodedInstruction insn;
        ZydisDecodedOperand operands[10];
        size_t offset = 0;
        while (offset < len) {
            ZydisDecoderDecodeFull(&decoder, src + offset, len - offset, &insn, operands);
            std::memcpy(dst + offset, src + offset, insn.length);
            for (int i = 0; i < insn.operand_count; i++) {
                if (operands[i].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[i].mem.base == ZYDIS_REGISTER_RIP) {
                    uint64_t absolute_target;
                    ZydisCalcAbsoluteAddress(&insn, &operands[i], (uint64_t)src + offset, &absolute_target);
                    int32_t new_disp = (int32_t)((intptr_t)absolute_target - ((intptr_t)dst + offset + insn.length));
                    std::memcpy(dst + offset + insn.raw.disp.offset, &new_disp, 4);
                }
            }
            offset += insn.length;
        }
    }

    void write_rel_jmp(void* at, void* to) {
        int32_t rel = (int32_t)((intptr_t)to - ((intptr_t)at + 5));
        uint8_t patch[5] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };
        std::memcpy(patch + 1, &rel, 4);
        size_t ps = sysconf(_SC_PAGESIZE);
        void* base = (void*)((uintptr_t)at & ~(ps - 1));
        mprotect(base, ps * 2, PROT_READ | PROT_WRITE | PROT_EXEC);
        std::memcpy(at, patch, 5);
        mprotect(base, ps * 2, PROT_READ | PROT_EXEC);
    }

    void write_abs_jmp(void* at, void* to) {
        uint8_t patch[14] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
        std::memcpy(patch + 6, &to, 8);
        std::memcpy(at, patch, 14);
    }
}