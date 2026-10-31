#pragma once

#include "Zenith/VM/Proto.h"

#include <iostream>
#include <string>
#include <string_view>

namespace Zenith {

    class Assembler
    {
    public:
        Assembler() = delete;

        static bool assemble(ProtoBuilder& builder, std::string_view source,
                             std::string* out_error = nullptr);
        static bool assemble(ProtoBuilder& builder, std::istream& is,
                             std::string* out_error = nullptr);
    };

}