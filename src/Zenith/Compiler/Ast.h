#pragma once

#include "Zenith/Compiler/Node.h"
#include "Zenith/Compiler/Token.h"
#include <cstdint>
#include <string_view>
#include <span>

namespace Zenith {

    struct AstView
    {
        std::string_view source;
        std::span<const Node> nodes;
        std::span<const Token> tokens;
        std::span<const uint32_t> extra;
        uint32_t root;
    };  

}