#pragma once

namespace Zenith {

    template<Opcode Op>
    FORCE_INLINE auto decode_args(Instruction inst)
    {
        constexpr OpInfo info = OPCODE_INFO[static_cast<size_t>(Op)];

        if constexpr (info.mode == OpMode::iABC)
        {
            constexpr bool use_b = info.arg_b != OpArgMask::OpArgN;
            constexpr bool use_c = info.arg_c != OpArgMask::OpArgN;

            if constexpr (use_b && use_c)
                return std::tuple{get_arg_A(inst), get_arg_B(inst), get_arg_C(inst)};
            else if constexpr (use_b && !use_c)
                return std::tuple{get_arg_A(inst), get_arg_B(inst)};
            else if constexpr (!use_b && use_c)
                return std::tuple{get_arg_A(inst), get_arg_C(inst)};
            else
                return std::tuple{get_arg_A(inst)};
        }
        else if constexpr (info.mode == OpMode::iABx)
            return std::tuple{get_arg_A(inst), get_arg_Bx(inst)};
        else
            return std::tuple{get_arg_A(inst), get_arg_sBx(inst)};
    }
}
