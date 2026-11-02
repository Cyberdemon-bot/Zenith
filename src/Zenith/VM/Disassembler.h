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

        static void dump(const Module& module, uint16_t proto_id,
                         std::ostream& os = std::cout);
        static std::string dump(const Module& module, uint16_t proto_id);
        static void dump_instruction(const Module& module, uint16_t proto_id,
                                     size_t pc, std::ostream& os);
    };

}
