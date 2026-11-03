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

    std::string_view trim(std::string_view s)
    {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    std::string_view strip_comment(std::string_view s)
    {
        size_t pos = s.find(';');
        if (pos != std::string_view::npos) s = s.substr(0, pos);
        pos = s.find("//");
        if (pos != std::string_view::npos) s = s.substr(0, pos);
        return trim(s);
    }

    std::vector<std::string_view> split_tokens(std::string_view s)
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

    bool parse_register(std::string_view tok, uint8_t& out_reg)
    {
        if (tok.empty()) return false;
        if (tok[0] == 'R' || tok[0] == 'r') tok.remove_prefix(1);
        if (tok.empty()) return false;
        int val = 0;
        auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), val);
        if (ec != std::errc() || ptr != tok.data() + tok.size() || val < 0 || val > 255) return false;
        out_reg = static_cast<uint8_t>(val);
        return true;
    }

    bool parse_u16(std::string_view tok, uint16_t& out_val)
    {
        if (tok.empty()) return false;
        if (tok[0] == 'K' || tok[0] == 'k') tok.remove_prefix(1);
        if (tok.starts_with("Proto#")) tok.remove_prefix(6);
        if (tok.empty()) return false;
        int val = 0;
        auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), val);
        if (ec != std::errc() || ptr != tok.data() + tok.size() || val < 0 || val > 65535) return false;
        out_val = static_cast<uint16_t>(val);
        return true;
    }

    bool parse_i16(std::string_view tok, int16_t& out_val)
    {
        if (tok.empty()) return false;
        int val = 0;
        auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), val);
        if (ec != std::errc() || ptr != tok.data() + tok.size() || val < -32768 || val > 32767) return false;
        out_val = static_cast<int16_t>(val);
        return true;
    }

    std::optional<Opcode> parse_opcode(std::string_view name)
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

    Type parse_type(std::string_view name)
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
        size_t inst_idx;
        std::string label;
        size_t line_no;
    };

}

bool Assembler::assemble(ProtoBuilder& builder, std::istream& is, std::string* out_error)
{
    std::string source((std::istreambuf_iterator<char>(is)),
                        std::istreambuf_iterator<char>());
    return assemble(builder, source, out_error);
}

bool Assembler::assemble(ProtoBuilder& builder, std::string_view source, std::string* out_error)
{
    builder.clear();

    std::unordered_map<std::string, uint32_t> label_table;
    std::vector<PendingJump> pending_jumps;

    size_t  pos         = 0;
    size_t  line_no     = 0;
    uint8_t highest_reg = 0;

    while (pos < source.size())
    {
        size_t next_line = source.find('\n', pos);
        if (next_line == std::string_view::npos) next_line = source.size();
        std::string_view raw_line = source.substr(pos, next_line - pos);
        pos = next_line + 1;
        ++line_no;

        std::string_view line = strip_comment(raw_line);
        if (line.empty()) continue;

        // ── Label
        if (line.back() == ':')
        {
            std::string label(trim(line.substr(0, line.size() - 1)));
            if (label.empty())
            {
                if (out_error) *out_error = "Line " + std::to_string(line_no)
                                          + ": Empty label";
                return false;
            }
            if (label_table.contains(label))
            {
                if (out_error) *out_error = "Line " + std::to_string(line_no)
                                          + ": Duplicate label '" + label + "'";
                return false;
            }
            label_table[label] = builder.code_size();
            continue;
        }

        auto tokens = split_tokens(line);
        if (tokens.empty()) continue;
        std::string_view head = tokens[0];

        // ── Directives
        if (head == ".params")
        {
            if (tokens.size() < 2)
            {
                if (out_error) *out_error = "Line " + std::to_string(line_no)
                                          + ": .params requires <count>";
                return false;
            }
            int p = 0;
            std::from_chars(tokens[1].data(), tokens[1].data() + tokens[1].size(), p);
            builder.set_num_params(static_cast<uint8_t>(p));
            continue;
        }
        if (head == ".regs")
        {
            if (tokens.size() < 2)
            {
                if (out_error) *out_error = "Line " + std::to_string(line_no)
                                          + ": .regs requires <count>";
                return false;
            }
            int r = 0;
            std::from_chars(tokens[1].data(), tokens[1].data() + tokens[1].size(), r);
            builder.set_max_regs(static_cast<uint8_t>(r));
            continue;
        }
        if (head == ".const")
        {
            if (tokens.size() < 3)
            {
                if (out_error) *out_error = "Line " + std::to_string(line_no)
                                          + ": .const requires <type> <value>";
                return false;
            }
            Type ty = parse_type(tokens[1]);
            Value val{};
            std::string val_str(tokens[2]);

            try {
                if      (ty == Type::I32) val.i32 = std::stoi(val_str);
                else if (ty == Type::U32) val.u32 = static_cast<uint32_t>(std::stoul(val_str));
                else if (ty == Type::I64) val.i64 = std::stoll(val_str);
                else if (ty == Type::U64) val.u64 = std::stoull(val_str, nullptr,
                                                    val_str.starts_with("0x") ? 16 : 10);
                else if (ty == Type::F32) val.f32 = std::stof(val_str);
                else if (ty == Type::F64) val.f64 = std::stod(val_str);
                else                      val.u64 = std::stoull(val_str, nullptr, 16);
            } catch (const std::exception& e) {
                if (out_error) *out_error = "Line " + std::to_string(line_no)
                                          + ": invalid .const value '" + val_str + "'";
                return false;
            }

            builder.push_constant(val);
            continue;
        }

        // ── Opcode + optional .Type suffix
        std::string_view op_str = head;
        Type ty = Type::None;
        size_t dot_pos = head.find('.');
        if (dot_pos != std::string_view::npos)
        {
            op_str = head.substr(0, dot_pos);
            ty = parse_type(head.substr(dot_pos + 1));
        }

        auto op_opt = parse_opcode(op_str);
        if (!op_opt)
        {
            if (out_error) *out_error = "Line " + std::to_string(line_no)
                                      + ": Unknown opcode '" + std::string(op_str) + "'";
            return false;
        }

        Opcode op = *op_opt;
        uint32_t oi = static_cast<uint32_t>(op);
        uint32_t slot = (OPCODE_INFO[oi].variants == 1)
                      ? OPCODE_BASE[oi]
                      : OPCODE_DISPATCH[oi][static_cast<uint32_t>(ty)];

        if (slot == INVALID_HANDLER)
        {
            if (out_error) *out_error = "Line " + std::to_string(line_no)
                                      + ": Type not supported for opcode '"
                                      + std::string(op_str) + "'";
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
                if (tokens.size() > 1 && !parse_register(tokens[1], a))
                {
                    if (out_error) *out_error = "Line " + std::to_string(line_no)
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
                        if (out_error) *out_error = "Line " + std::to_string(line_no)
                                                  + ": Invalid return count '" + std::string(tokens[2]) + "'";
                        return false;
                    }
                    b = static_cast<uint8_t>(flag);
                }
                highest_reg = std::max(highest_reg, a);
            }
            else
            {
                if (tokens.size() > 1 && !parse_register(tokens[1], a))
                {
                    if (out_error) *out_error = "Line " + std::to_string(line_no)
                                              + ": Invalid register '" + std::string(tokens[1]) + "'";
                    return false;
                }
                if (tokens.size() > 2 && !parse_register(tokens[2], b))
                {
                    if (out_error) *out_error = "Line " + std::to_string(line_no)
                                              + ": Invalid register '" + std::string(tokens[2]) + "'";
                    return false;
                }
                if (tokens.size() > 3 && !parse_register(tokens[3], c))
                {
                    if (out_error) *out_error = "Line " + std::to_string(line_no)
                                              + ": Invalid register '" + std::string(tokens[3]) + "'";
                    return false;
                }
                highest_reg = std::max({ highest_reg, a, b, c });
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
            if (tokens.size() > 1 && !parse_register(tokens[1], a))
            {
                if (out_error) *out_error = "Line " + std::to_string(line_no)
                                          + ": Invalid register '" + std::string(tokens[1]) + "'";
                return false;
            }
            if (tokens.size() > 2 && !parse_u16(tokens[2], bx))
            {
                if (out_error) *out_error = "Line " + std::to_string(line_no)
                                          + ": Invalid constant or proto id '" + std::string(tokens[2]) + "'";
                return false;
            }
            highest_reg = std::max(highest_reg, a);
            inst.raw = (slot << 24) | (uint32_t(a) << 16) | uint32_t(bx);
            break;
        }

        case Format::iAsBx:
        {
            uint8_t a = 0;
            int16_t sbx = 0;
            size_t  label_tok_idx = 0;

            if (op == Opcode::Jmp)
            {
                if (tokens.size() < 2)
                {
                    if (out_error) *out_error = "Line " + std::to_string(line_no)
                                              + ": Jmp requires a target";
                    return false;
                }
                if (tokens.size() == 2) label_tok_idx = 1;
                else
                {
                    if (!parse_register(tokens[1], a))
                    {
                        if (out_error) *out_error = "Line " + std::to_string(line_no)
                                                  + ": Invalid register '" + std::string(tokens[1]) + "'";
                        return false;
                    }
                    label_tok_idx = 2;
                }
            }
            else if (op == Opcode::LoadImm)
            {
                if (tokens.size() < 3)
                {
                    if (out_error) *out_error = "Line " + std::to_string(line_no)
                                              + ": LoadImm requires a register and an immediate value";
                    return false;
                }
                if (!parse_register(tokens[1], a))
                {
                    if (out_error) *out_error = "Line " + std::to_string(line_no)
                                              + ": Invalid register '" + std::string(tokens[1]) + "'";
                    return false;
                }
                if (!parse_i16(tokens[2], sbx))
                {
                    if (out_error) *out_error = "Line " + std::to_string(line_no)
                                              + ": Invalid immediate value '" + std::string(tokens[2]) + "'";
                    return false;
                }
            }
            else
            {
                if (tokens.size() < 3)
                {
                    if (out_error) *out_error = "Line " + std::to_string(line_no)
                                              + ": Conditional jump requires condition register and target";
                    return false;
                }
                if (!parse_register(tokens[1], a))
                {
                    if (out_error) *out_error = "Line " + std::to_string(line_no)
                                              + ": Invalid register '" + std::string(tokens[1]) + "'";
                    return false;
                }
                label_tok_idx = 2;
            }
            highest_reg = std::max(highest_reg, a);

            if (op != Opcode::LoadImm && tokens.size() > label_tok_idx)
            {
                std::string_view target = tokens[label_tok_idx];
                if (!parse_i16(target, sbx))
                {
                    pending_jumps.push_back({ builder.code_size(),
                                              std::string(target), line_no });
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
                    if (out_error) *out_error = "Line " + std::to_string(line_no)
                                              + ": Invalid Ax operand '" + std::string(tokens[1]) + "'";
                    return false;
                }
                ax = static_cast<uint32_t>(v);
            }
            inst.raw = (slot << 24) | (ax & 0xFFFFFFu);
            break;
        }
        }

        builder.commit_instruction(inst);
    }

    // ── Patch forward jumps
    for (const auto& jmp : pending_jumps)
    {
        auto it = label_table.find(jmp.label);
        if (it == label_table.end())
        {
            if (out_error) *out_error = "Line " + std::to_string(jmp.line_no)
                                      + ": Undefined label '" + jmp.label + "'";
            return false;
        }
        int32_t diff = static_cast<int32_t>(it->second)
                     - static_cast<int32_t>(jmp.inst_idx + 1);
        if (diff < -32768 || diff > 32767)
        {
            if (out_error) *out_error = "Line " + std::to_string(jmp.line_no)
                                      + ": Jump offset out of 16-bit range (" + std::to_string(diff) + ")";
            return false;
        }
        builder.patch_jmp(static_cast<uint32_t>(jmp.inst_idx), static_cast<int16_t>(diff));
    }

    if (builder.max_regs() == 0)
        builder.set_max_regs(highest_reg + 1);

    return true;
}

} 