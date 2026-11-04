#pragma once

#include "Zenith/VM/Proto.h"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace Zenith {

    class Disassembler
    {
    public:
        Disassembler() = delete;

        static void Dump(const Module& module, uint16_t proto_id,
                         std::ostream& os);
        static std::string Dump(const Module& module, uint16_t proto_id);
        static void DumpInstruction(const Module& module, uint16_t proto_id,
                                     size_t pc, std::ostream& os);
    };

}
