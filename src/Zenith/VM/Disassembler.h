#pragma once

#include "Zenith/VM/Proto.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

namespace Zenith {

    class Disassembler
    {
    public:
        Disassembler() = delete;

        static void Dump(const Module& module, uint16_t protoId,
                         std::ostream& os = std::cout);
        static std::string Dump(const Module& module, uint16_t protoId);
        static void DumpInstruction(const Module& module, uint16_t protoId,
                                     size_t pc, std::ostream& os);
    };

}
