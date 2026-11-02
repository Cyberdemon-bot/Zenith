#include "Zenith/VM/Disassembler.h"

#include "Zenith/VM/Instruction.h"
#include "Zenith/VM/Opcode.h"

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace Zenith {
namespace {

    std::string_view opcode_name(Opcode op)
    {
        switch (op)
        {
            case Opcode::LoadConst: return "LoadConst";
            case Opcode::LoadImm:   return "LoadImm";
            case Opcode::Mov:       return "Mov";
            case Opcode::Add:       return "Add";
            case Opcode::Sub:       return "Sub";
            case Opcode::Mul:       return "Mul";
            case Opcode::Div:       return "Div";
            case Opcode::DivS:      return "DivS";
            case Opcode::DivU:      return "DivU";
            case Opcode::RemS:      return "RemS";
            case Opcode::RemU:      return "RemU";
            case Opcode::Neg:       return "Neg";
            case Opcode::Eq:        return "Eq";
            case Opcode::Lt:        return "Lt";
            case Opcode::LNot:      return "LNot";
            case Opcode::Jmp:       return "Jmp";
            case Opcode::Jz:        return "Jz";
            case Opcode::Jnz:       return "Jnz";
            case Opcode::Call:      return "Call";
            case Opcode::Return:    return "Return";
            case Opcode::Halt:      return "Halt";
            default:                return "Unknown";
        }
    }

    std::string_view type_suffix(Type ty)
    {
        switch (ty)
        {
            case Type::U32:  return ".U32";
            case Type::I32:  return ".I32";
            case Type::F32:  return ".F32";
            case Type::U64:  return ".U64";
            case Type::I64:  return ".I64";
            case Type::F64:  return ".F64";
            case Type::REF:  return ".REF";
            case Type::None: return "";
            default:         return "";
        }
    }

} // namespace

void Disassembler::dump(const Module& module, uint16_t proto_id, std::ostream& os)
{
    const Proto& p     = module.proto(proto_id);
    auto         code  = module.code(proto_id);
    auto         consts = module.consts(proto_id);

    os << "; === Proto #" << proto_id << " Header ===\n";
    os << "; Params:   " << static_cast<int>(p.num_param) << "\n";
    os << "; Max Regs: " << static_cast<int>(p.max_regs)  << "\n";
    os << "; Children: offset=" << p.first_child
       << ", count=" << p.child_count << "\n\n";

    if (!consts.empty())
    {
        os << "; === Constants Pool (" << consts.size() << ") ===\n";
        for (size_t i = 0; i < consts.size(); ++i)
        {
            const Value& v = consts[i];
            os << "  K" << std::setw(3) << std::left << i
               << " = 0x" << std::hex << std::right << std::setw(16) << std::setfill('0') << v.u64
               << std::dec << std::setfill(' ')
               << "  (i32: " << std::setw(11) << v.i32
               << " | f32: " << std::setw(11) << v.f32
               << " | f64: " << v.f64 << ")\n";
        }
        os << "\n";
    }

    os << "; === Instruction Listing (" << code.size() << ") ===\n";
    for (size_t pc = 0; pc < code.size(); ++pc)
    {
        dump_instruction(module, proto_id, pc, os);
        os << "\n";
    }
}

std::string Disassembler::dump(const Module& module, uint16_t proto_id)
{
    std::ostringstream ss;
    dump(module, proto_id, ss);
    return ss.str();
}

void Disassembler::dump_instruction(const Module& module, uint16_t proto_id,
                                    size_t pc, std::ostream& os)
{
    auto code = module.code(proto_id);
    const Instruction& inst = code[pc];
    uint8_t slot = inst.slot();
    const SlotInfo& info = SLOT_INFO[slot];

    std::string mnemonic = std::string(opcode_name(info.op))
                         + std::string(type_suffix(info.ty));

    os << "  [" << std::right << std::setw(4) << std::setfill('0') << pc << "] "
       << std::setfill(' ') << std::setw(14) << std::left << mnemonic;

    switch (info.format)
    {
    case Format::iNone:
        break;

    case Format::iABC:
    {
        auto [a, b, c] = inst.extract<Format::iABC>();
        if (info.op == Opcode::Mov || info.op == Opcode::Neg || info.op == Opcode::LNot)
            os << "R" << static_cast<int>(a) << ", R" << static_cast<int>(b);
        else if (info.op == Opcode::Return)
            os << "R" << static_cast<int>(a) << ", " << static_cast<int>(b);
        else
            os << "R" << static_cast<int>(a) << ", R" << static_cast<int>(b)
               << ", R" << static_cast<int>(c);
        break;
    }

    case Format::iABx:
    {
        auto [a, bx] = inst.extract<Format::iABx>();
        if (info.op == Opcode::LoadConst)
            os << "R" << static_cast<int>(a) << ", K" << bx;
        else if (info.op == Opcode::Call)
            os << "R" << static_cast<int>(a) << ", Proto#" << bx;
        else
            os << "R" << static_cast<int>(a) << ", " << bx;
        break;
    }

    case Format::iAsBx:
    {
        auto [a, sbx] = inst.extract<Format::iAsBx>();
        if (info.op == Opcode::LoadImm)
        {
            os << "R" << static_cast<int>(a) << ", " << sbx;
        }
        else if (info.op == Opcode::Jmp)
        {
            int32_t target = static_cast<int32_t>(pc) + 1 + sbx;
            os << (sbx >= 0 ? "+" : "") << sbx
               << " -> [" << std::right << std::setw(4) << std::setfill('0') << target
               << std::setfill(' ') << "]";
        }
        else if (info.op == Opcode::Jz || info.op == Opcode::Jnz)
        {
            int32_t target = static_cast<int32_t>(pc) + 1 + sbx;
            os << "R" << static_cast<int>(a) << ", " << (sbx >= 0 ? "+" : "") << sbx
               << " -> [" << std::right << std::setw(4) << std::setfill('0') << target
               << std::setfill(' ') << "]";
        }
        else
        {
            os << "R" << static_cast<int>(a) << ", " << sbx;
        }
        break;
    }

    case Format::iAx:
    {
        auto [ax] = inst.extract<Format::iAx>();
        os << ax;
        break;
    }
    }
}

} // namespace Zenith