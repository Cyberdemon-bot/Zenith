#include "Zenith/VM/Assembler.h"

#include "Zenith/VM/Instruction.h"
#include "Zenith/VM/Opcode.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Zenith {
namespace {

    std::string_view Trim(std::string_view s)
    {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    std::string_view StripComment(std::string_view s)
    {
        size_t pos = s.find(';');
        if (pos != std::string_view::npos) s = s.substr(0, pos);
        pos = s.find("//");
        if (pos != std::string_view::npos) s = s.substr(0, pos);
        return Trim(s);
    }

    std::vector<std::string_view> SplitTokens(std::string_view s)
    {
        std::vector<std::string_view> tokens;
        size_t i = 0;
        while (i < s.size())
        {
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == ',')) ++i;
            if (i >= s.size()) break;
            size_t start = i;
            while (i < s.size() && s[i] != ' ' && s[i] != '\t' && s[i] != ',') ++i;
            tokens.push_back(s.substr(start, i - start));
        }
        return tokens;
    }

    bool ParseRegister(std::string_view tok, uint8_t& outReg)
    {
        if (tok.empty()) return false;
        if (tok[0] == 'R' || tok[0] == 'r') tok.remove_prefix(1);
        if (tok.empty()) return false;
        int val = 0;
        auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), val);
        if (ec != std::errc() || ptr != tok.data() + tok.size() || val < 0 || val > 255) return false;
        outReg = static_cast<uint8_t>(val);
        return true;
    }

    bool ParseU16(std::string_view tok, uint16_t& outVal)
    {
        if (tok.empty()) return false;
        if (tok[0] == 'K' || tok[0] == 'k') tok.remove_prefix(1);
        if (tok.starts_with("Proto#")) tok.remove_prefix(6);
        if (tok.empty()) return false;
        int val = 0;
        auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), val);
        if (ec != std::errc() || ptr != tok.data() + tok.size() || val < 0 || val > 65535) return false;
        outVal = static_cast<uint16_t>(val);
        return true;
    }

    bool ParseI16(std::string_view tok, int16_t& outVal)
    {
        if (tok.empty()) return false;
        int val = 0;
        auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), val);
        if (ec != std::errc() || ptr != tok.data() + tok.size() || val < -32768 || val > 32767) return false;
        outVal = static_cast<int16_t>(val);
        return true;
    }

    std::optional<Opcode> ParseOpcode(std::string_view name)
    {
        if (name == "LoadConst") return Opcode::LoadConst;
        if (name == "Add")       return Opcode::Add;
        if (name == "Sub")       return Opcode::Sub;
        if (name == "Mul")       return Opcode::Mul;
        if (name == "Div")       return Opcode::Div;
        if (name == "DivS")      return Opcode::DivS;
        if (name == "DivU")      return Opcode::DivU;
        if (name == "RemS")      return Opcode::RemS;
        if (name == "RemU")      return Opcode::RemU;
        if (name == "Neg")       return Opcode::Neg;
        if (name == "Mov")       return Opcode::Mov;
        if (name == "LoadImm")   return Opcode::LoadImm;
        if (name == "Eq")        return Opcode::Eq;
        if (name == "Lt")        return Opcode::Lt;
        if (name == "LNot")      return Opcode::LNot;
        if (name == "Jmp")       return Opcode::Jmp;
        if (name == "Jz")        return Opcode::Jz;
        if (name == "Jnz")       return Opcode::Jnz;
        if (name == "Call")      return Opcode::Call;
        if (name == "Return")    return Opcode::Return;
        if (name == "Halt")      return Opcode::Halt;
        return std::nullopt;
    }

    Type ParseType(std::string_view name)
    {
        if (name == "U32") return Type::U32;
        if (name == "I32") return Type::I32;
        if (name == "F32") return Type::F32;
        if (name == "U64") return Type::U64;
        if (name == "I64") return Type::I64;
        if (name == "F64") return Type::F64;
        if (name == "REF") return Type::REF;
        return Type::None;
    }

    struct PendingJump
    {
        size_t instIdx;
        std::string label;
        size_t lineNo;
    };

} // namespace

bool Assembler::Assemble(ProtoBuilder& builder, std::istream& is, std::string* outError)
{
    std::string source((std::istreambuf_iterator<char>(is)),
                        std::istreambuf_iterator<char>());
    return Assemble(builder, source, outError);
}

bool Assembler::Assemble(ProtoBuilder& builder, std::string_view source, std::string* outError)
{
    builder.Clear();

    std::unordered_map<std::string, uint32_t> labelTable;
    std::vector<PendingJump> pendingJumps;

    size_t  pos         = 0;
    size_t  lineNo      = 0;
    uint8_t highestReg  = 0;

    while (pos < source.size())
    {
        size_t nextLine = source.find('\n', pos);
        if (nextLine == std::string_view::npos) nextLine = source.size();
        std::string_view rawLine = source.substr(pos, nextLine - pos);
        pos = nextLine + 1;
        ++lineNo;

        std::string_view line = StripComment(rawLine);
        if (line.empty()) continue;

        // ── Label
        if (line.back() == ':')
        {
            std::string label(Trim(line.substr(0, line.size() - 1)));
            if (label.empty())
            {
                if (outError) *outError = "Line " + std::to_string(lineNo)
                                          + ": Empty label";
                return false;
            }
            if (labelTable.contains(label))
            {
                if (outError) *outError = "Line " + std::to_string(lineNo)
                                          + ": Duplicate label '" + label + "'";
                return false;
            }
            labelTable[label] = builder.GetCodeSize();
            continue;
        }

        auto tokens = SplitTokens(line);
        if (tokens.empty()) continue;
        std::string_view head = tokens[0];

        // ── Directives
        if (head == ".params")
        {
            if (tokens.size() < 2)
            {
                if (outError) *outError = "Line " + std::to_string(lineNo)
                                          + ": .params requires <count>";
                return false;
            }
            int p = 0;
            std::from_chars(tokens[1].data(), tokens[1].data() + tokens[1].size(), p);
            builder.SetNumParams(static_cast<uint8_t>(p));
            continue;
        }
        if (head == ".regs")
        {
            if (tokens.size() < 2)
            {
                if (outError) *outError = "Line " + std::to_string(lineNo)
                                          + ": .regs requires <count>";
                return false;
            }
            int r = 0;
            std::from_chars(tokens[1].data(), tokens[1].data() + tokens[1].size(), r);
            builder.SetMaxRegs(static_cast<uint8_t>(r));
            continue;
        }
        if (head == ".const")
        {
            if (tokens.size() < 3)
            {
                if (outError) *outError = "Line " + std::to_string(lineNo)
                                          + ": .const requires <type> <value>";
                return false;
            }
            Type ty = ParseType(tokens[1]);
            Value val{};
            std::string valStr(tokens[2]);

            try {
                if      (ty == Type::I32) val.i32 = std::stoi(valStr);
                else if (ty == Type::U32) val.u32 = static_cast<uint32_t>(std::stoul(valStr));
                else if (ty == Type::I64) val.i64 = std::stoll(valStr);
                else if (ty == Type::U64) val.u64 = std::stoull(valStr, nullptr,
                                                    valStr.starts_with("0x") ? 16 : 10);
                else if (ty == Type::F32) val.f32 = std::stof(valStr);
                else if (ty == Type::F64) val.f64 = std::stod(valStr);
                else                      val.u64 = std::stoull(valStr, nullptr, 16);
            } catch (const std::exception& e) {
                if (outError) *outError = "Line " + std::to_string(lineNo)
                                          + ": invalid .const value '" + valStr + "'";
                return false;
            }

            if (builder.GetConstSize() >= 65535)
            {
                if (outError) *outError = "Line " + std::to_string(lineNo)
                                          + ": Constant pool overflow (max 65535)";
                return false;
            }
            builder.PushConstant(val);
            continue;
        }

        // ── Opcode + optional .Type suffix
        std::string_view opStr = head;
        Type ty = Type::None;
        size_t dotPos = head.find('.');
        if (dotPos != std::string_view::npos)
        {
            opStr = head.substr(0, dotPos);
            ty = ParseType(head.substr(dotPos + 1));
        }

        auto opOpt = ParseOpcode(opStr);
        if (!opOpt)
        {
            if (outError) *outError = "Line " + std::to_string(lineNo)
                                      + ": Unknown opcode '" + std::string(opStr) + "'";
            return false;
        }

        Opcode op = *opOpt;
        uint32_t oi = static_cast<uint32_t>(op);
        uint32_t slot = (OPCODE_INFO[oi].variants == 1)
                      ? OPCODE_BASE[oi]
                      : OPCODE_DISPATCH[oi][static_cast<uint32_t>(ty)];

        if (slot == INVALID_HANDLER)
        {
            if (outError) *outError = "Line " + std::to_string(lineNo)
                                      + ": Type not supported for opcode '"
                                      + std::string(opStr) + "'";
            return false;
        }

        Format fmt = SLOT_INFO[slot].format;
        Instruction inst;

        switch (fmt)
        {
        case Format::iNone:
            inst.raw = slot << 24;
            break;

        case Format::iABC:
        {
            uint8_t a = 0, b = 0, c = 0;
            if (op == Opcode::Return)
            {
                if (tokens.size() > 1 && !ParseRegister(tokens[1], a))
                {
                    if (outError) *outError = "Line " + std::to_string(lineNo)
                                              + ": Invalid register '" + std::string(tokens[1]) + "'";
                    return false;
                }
                if (tokens.size() > 2)
                {
                    int flag = 0;
                    auto [ptr, ec] = std::from_chars(tokens[2].data(),
                                                    tokens[2].data() + tokens[2].size(), flag);
                    if (ec != std::errc() || ptr != tokens[2].data() + tokens[2].size() || flag < 0 || flag > 255)
                    {
                        if (outError) *outError = "Line " + std::to_string(lineNo)
                                                  + ": Invalid return count '" + std::string(tokens[2]) + "'";
                        return false;
                    }
                    b = static_cast<uint8_t>(flag);
                }
                highestReg = std::max(highestReg, a);
            }
            else
            {
                if (tokens.size() > 1 && !ParseRegister(tokens[1], a))
                {
                    if (outError) *outError = "Line " + std::to_string(lineNo)
                                              + ": Invalid register '" + std::string(tokens[1]) + "'";
                    return false;
                }
                if (tokens.size() > 2 && !ParseRegister(tokens[2], b))
                {
                    if (outError) *outError = "Line " + std::to_string(lineNo)
                                              + ": Invalid register '" + std::string(tokens[2]) + "'";
                    return false;
                }
                if (tokens.size() > 3 && !ParseRegister(tokens[3], c))
                {
                    if (outError) *outError = "Line " + std::to_string(lineNo)
                                              + ": Invalid register '" + std::string(tokens[3]) + "'";
                    return false;
                }
                highestReg = std::max({ highestReg, a, b, c });
            }
            inst.raw = (slot << 24)
                     | (uint32_t(a) << 16)
                     | (uint32_t(b) << 8)
                     |  uint32_t(c);
            break;
        }

        case Format::iABx:
        {
            uint8_t  a  = 0;
            uint16_t bx = 0;
            if (tokens.size() > 1 && !ParseRegister(tokens[1], a))
            {
                if (outError) *outError = "Line " + std::to_string(lineNo)
                                          + ": Invalid register '" + std::string(tokens[1]) + "'";
                return false;
            }
            if (tokens.size() > 2 && !ParseU16(tokens[2], bx))
            {
                if (outError) *outError = "Line " + std::to_string(lineNo)
                                          + ": Invalid constant or proto id '" + std::string(tokens[2]) + "'";
                return false;
            }
            highestReg = std::max(highestReg, a);
            inst.raw = (slot << 24) | (uint32_t(a) << 16) | uint32_t(bx);
            break;
        }

        case Format::iAsBx:
        {
            uint8_t a = 0;
            int16_t sbx = 0;
            size_t  labelTokIdx = 0;

            if (op == Opcode::Jmp)
            {
                if (tokens.size() < 2)
                {
                    if (outError) *outError = "Line " + std::to_string(lineNo)
                                              + ": Jmp requires a target";
                    return false;
                }
                if (tokens.size() == 2) labelTokIdx = 1;
                else
                {
                    if (!ParseRegister(tokens[1], a))
                    {
                        if (outError) *outError = "Line " + std::to_string(lineNo)
                                                  + ": Invalid register '" + std::string(tokens[1]) + "'";
                        return false;
                    }
                    labelTokIdx = 2;
                }
            }
            else if (op == Opcode::LoadImm)
            {
                if (tokens.size() < 3)
                {
                    if (outError) *outError = "Line " + std::to_string(lineNo)
                                              + ": LoadImm requires a register and an immediate value";
                    return false;
                }
                if (!ParseRegister(tokens[1], a))
                {
                    if (outError) *outError = "Line " + std::to_string(lineNo)
                                              + ": Invalid register '" + std::string(tokens[1]) + "'";
                    return false;
                }
                if (!ParseI16(tokens[2], sbx))
                {
                    if (outError) *outError = "Line " + std::to_string(lineNo)
                                              + ": Invalid immediate value '" + std::string(tokens[2]) + "'";
                    return false;
                }
            }
            else
            {
                if (tokens.size() < 3)
                {
                    if (outError) *outError = "Line " + std::to_string(lineNo)
                                              + ": Conditional jump requires condition register and target";
                    return false;
                }
                if (!ParseRegister(tokens[1], a))
                {
                    if (outError) *outError = "Line " + std::to_string(lineNo)
                                              + ": Invalid register '" + std::string(tokens[1]) + "'";
                    return false;
                }
                labelTokIdx = 2;
            }
            highestReg = std::max(highestReg, a);

            if (op != Opcode::LoadImm && tokens.size() > labelTokIdx)
            {
                std::string_view target = tokens[labelTokIdx];
                if (!ParseI16(target, sbx))
                {
                    pendingJumps.push_back({ builder.GetCodeSize(),
                                              std::string(target), lineNo });
                    sbx = 0;
                }
            }
            inst.raw = (slot << 24)
                     | (uint32_t(a) << 16)
                     | static_cast<uint32_t>(std::bit_cast<uint16_t>(sbx));
            break;
        }

        case Format::iAx:
        {
            uint32_t ax = 0;
            if (tokens.size() > 1)
            {
                int v = 0;
                auto [ptr, ec] = std::from_chars(tokens[1].data(),
                                                tokens[1].data() + tokens[1].size(), v);
                if (ec != std::errc() || ptr != tokens[1].data() + tokens[1].size() || v < 0)
                {
                    if (outError) *outError = "Line " + std::to_string(lineNo)
                                              + ": Invalid Ax operand '" + std::string(tokens[1]) + "'";
                    return false;
                }
                ax = static_cast<uint32_t>(v);
            }
            inst.raw = (slot << 24) | (ax & 0xFFFFFFu);
            break;
        }
        }

        builder.CommitInstruction(inst);
    }

    // ── Patch forward jumps
    for (const auto& jmp : pendingJumps)
    {
        auto it = labelTable.find(jmp.label);
        if (it == labelTable.end())
        {
            if (outError) *outError = "Line " + std::to_string(jmp.lineNo)
                                      + ": Undefined label '" + jmp.label + "'";
            return false;
        }
        int32_t diff = static_cast<int32_t>(it->second)
                     - static_cast<int32_t>(jmp.instIdx + 1);
        if (diff < -32768 || diff > 32767)
        {
            if (outError) *outError = "Line " + std::to_string(jmp.lineNo)
                                      + ": Jump offset out of 16-bit range (" + std::to_string(diff) + ")";
            return false;
        }
        builder.PatchJmp(static_cast<uint32_t>(jmp.instIdx), static_cast<int16_t>(diff));
    }

    if (builder.GetMaxRegs() == 0)
        builder.SetMaxRegs(highestReg + 1);

    return true;
}

} // namespace Zenith
