#include "Zenith/VM/Disassembler.h"

#include "Zenith/VM/Instruction.h"
#include "Zenith/VM/Opcode.h"
#include "Zenith/VM/Proto.h"

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace Zenith {
namespace {

    std::string_view OpcodeName(Opcode op)
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
            case Opcode::Cast:      return "Cast";
            case Opcode::Jmp:       return "Jmp";
            case Opcode::Jz:        return "Jz";
            case Opcode::Jnz:       return "Jnz";
            case Opcode::Call:      return "Call";
            case Opcode::Return:    return "Return";
            case Opcode::Halt:      return "Halt";
            default:                return "Unknown";
        }
    }

    std::string_view TypeSuffix(Type ty)
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

    std::string_view CastKindName(uint8_t raw)
    {
        switch (static_cast<CastKind>(raw))
        {
            case CastKind::TruncI8:  return "TruncI8";
            case CastKind::TruncI16: return "TruncI16";
            case CastKind::TruncU8:  return "TruncU8";
            case CastKind::TruncU16: return "TruncU16";
            case CastKind::I32ToI64: return "I32ToI64";
            case CastKind::I64ToI32: return "I64ToI32";
            case CastKind::I32ToF32: return "I32ToF32";
            case CastKind::I32ToF64: return "I32ToF64";
            case CastKind::U32ToF32: return "U32ToF32";
            case CastKind::U32ToF64: return "U32ToF64";
            case CastKind::I64ToF32: return "I64ToF32";
            case CastKind::I64ToF64: return "I64ToF64";
            case CastKind::U64ToF32: return "U64ToF32";
            case CastKind::U64ToF64: return "U64ToF64";
            case CastKind::F32ToI32: return "F32ToI32";
            case CastKind::F32ToU32: return "F32ToU32";
            case CastKind::F32ToI64: return "F32ToI64";
            case CastKind::F32ToU64: return "F32ToU64";
            case CastKind::F64ToI32: return "F64ToI32";
            case CastKind::F64ToU32: return "F64ToU32";
            case CastKind::F64ToI64: return "F64ToI64";
            case CastKind::F64ToU64: return "F64ToU64";
            case CastKind::F32ToF64: return "F32ToF64";
            case CastKind::F64ToF32: return "F64ToF32";
            case CastKind::ToBool:   return "ToBool";
            default:                 return "UnknownCast";
        }
    }

} 

void Disassembler::Dump(const Module& module, uint16_t proto_id, std::ostream& os)
{
    if (proto_id >= module.GetNumProtos())
    {
        os << "; Error: Proto #" << proto_id << " not found\n";
        return;
    }
    const Proto& p     = module.GetProto(proto_id);
    auto         code  = module.GetCode(proto_id);
    auto         consts = module.GetConsts(proto_id);

    os << "; === Proto #" << proto_id << " Header ===\n";
    os << "; Params:   " << static_cast<int>(p.NumParams) << "\n";
    os << "; Max Regs: " << static_cast<int>(p.MaxRegs)   << "\n";
    os << "; Children: offset=" << p.FirstChild
       << ", count=" << p.ChildCount << "\n\n";

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
        DumpInstruction(module, proto_id, pc, os);
        os << "\n";
    }
}

std::string Disassembler::Dump(const Module& module, uint16_t proto_id)
{
    std::ostringstream ss;
    Dump(module, proto_id, ss);
    return ss.str();
}

void Disassembler::DumpInstruction(const Module& module, uint16_t proto_id,
                                    size_t pc, std::ostream& os)
{
    if (proto_id >= module.GetNumProtos())
    {
        os << "; Error: Proto #" << proto_id << " not found";
        return;
    }
    auto code = module.GetCode(proto_id);
    if (pc >= code.size())
    {
        os << "; Error: PC out of range";
        return;
    }
    const Instruction& inst = code[pc];
    uint8_t slot = inst.Slot();
    if (slot >= DISPATCH_SIZE)
    {
        os << "  [" << std::right << std::setw(4) << std::setfill('0') << pc << "] "
           << "Invalid Slot (" << static_cast<int>(slot) << ")";
        return;
    }
    const SlotInfo& info = SLOT_INFO[slot];

    std::string mnemonic = std::string(OpcodeName(info.op))
                         + std::string(TypeSuffix(info.ty));

    os << "  [" << std::right << std::setw(4) << std::setfill('0') << pc << "] "
       << std::setfill(' ') << std::setw(14) << std::left << mnemonic;

    switch (info.format)
    {
    case Format::iNone:
        break;

    case Format::iABC:
    {
        auto [a, b, c] = inst.Extract<Format::iABC>();
        if (info.op == Opcode::Mov || info.op == Opcode::Neg || info.op == Opcode::LNot)
            os << "R" << static_cast<int>(a) << ", R" << static_cast<int>(b);
        else if (info.op == Opcode::Return)
            os << "R" << static_cast<int>(a) << ", " << static_cast<int>(b);
        else if (info.op == Opcode::Cast)
            os << "R" << static_cast<int>(a) << ", R" << static_cast<int>(b)
               << ", " << CastKindName(c);
        else
            os << "R" << static_cast<int>(a) << ", R" << static_cast<int>(b)
               << ", R" << static_cast<int>(c);
        break;
    }

    case Format::iABx:
    {
        auto [a, bx] = inst.Extract<Format::iABx>();
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
        auto [a, sbx] = inst.Extract<Format::iAsBx>();
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
        auto [ax] = inst.Extract<Format::iAx>();
        os << ax;
        break;
    }
    }
}

} 
