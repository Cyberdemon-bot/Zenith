#pragma once

#include <iosfwd>
#include <string>
#include <string_view>

namespace Zenith {

    class ProtoBuilder;

    class Assembler
    {
    public:
        Assembler() = delete;

        static bool Assemble(ProtoBuilder& builder, std::string_view source,
                             std::string* outError = nullptr);
        static bool Assemble(ProtoBuilder& builder, std::istream& is,
                             std::string* outError = nullptr);
    };

}
