#include "oxpch.h"
#include "Oryx/Shaders/Compiler/MslShaderCompiler.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Shaders/Cache/ShaderHash.h"
#include "Oryx/Shaders/Compiler/ShaderInclude.h"

namespace oryx
{

namespace
{

constexpr uint32_t MAX_INCLUDE_DEPTH = 16;
constexpr uint32_t MAX_MACRO_DEPTH = 8;

struct SourceLine
{
    std::string file;
    uint32_t number = 0;
    std::string text;
};

struct Token
{
    enum class Kind : uint8_t
    {
        Ident,
        Number,
        Punct,
        End
    };

    Kind kind = Kind::End;
    std::string text;
    std::string file;
    uint32_t line = 0;
};

[[noreturn]] void fail(const std::string& file, uint32_t line, const std::string& message)
{
    throw Error(file + ":" + std::to_string(line) + ": " + message);
}

[[noreturn]] void fail(const Token& token, const std::string& message)
{
    fail(token.file, token.line, message);
}

std::string trim(const std::string& text)
{
    const size_t begin = text.find_first_not_of(" \t\r");
    if (begin == std::string::npos)
    {
        return {};
    }
    return text.substr(begin, text.find_last_not_of(" \t\r") - begin + 1);
}

std::vector<std::string> split_lines(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line))
    {
        lines.push_back(line);
    }
    return lines;
}

bool starts_with(const std::string& text, const char* prefix)
{
    return text.rfind(prefix, 0) == 0;
}

// Quoted includes are inlined once each; `<...>` includes and everything else stay for the Metal compiler.
void expand_includes(const std::string& file, const std::string& text, std::vector<SourceLine>& out, std::vector<std::string>& stack, std::set<std::string>& included, const ShaderCompilerInput& input)
{
    uint32_t number = 0;
    for (const std::string& raw : split_lines(text))
    {
        ++number;
        const std::string line = trim(raw);
        if (!starts_with(line, "#include"))
        {
            out.push_back({ file, number, raw });
            continue;
        }
        const size_t open = line.find('"');
        if (open == std::string::npos)
        {
            out.push_back({ file, number, raw });
            continue;
        }
        const size_t close = line.find('"', open + 1);
        if (close == std::string::npos)
        {
            fail(file, number, "malformed #include");
        }
        const std::string name = line.substr(open + 1, close - open - 1);
        if (std::find(stack.begin(), stack.end(), name) != stack.end())
        {
            fail(file, number, "include cycle through '" + name + "'");
        }
        if (stack.size() >= MAX_INCLUDE_DEPTH)
        {
            fail(file, number, "includes nested too deeply");
        }
        const std::map<std::string, std::string>::const_iterator resolved = input.includes.find(name);
        const std::string* source = resolved != input.includes.end() ? &resolved->second : find_shader_include(name);
        if (source == nullptr)
        {
            fail(file, number, "include '" + name + "' not found");
        }
        if (!included.insert(name).second)
        {
            continue;
        }
        stack.push_back(name);
        expand_includes(name, *source, out, stack, included, input);
        stack.pop_back();
    }
}

using MacroMap = std::map<std::string, std::string>;

struct Conditional
{
    bool parent_active = true;
    bool active = true;
    bool taken = false;
};

// Evaluates #define/#undef/#ifdef/#ifndef/#else/#endif; returns the lines that are live, without directives.
std::vector<SourceLine> evaluate_conditionals(const std::vector<SourceLine>& lines, MacroMap& macros)
{
    std::vector<SourceLine> live;
    std::vector<Conditional> stack;
    for (const SourceLine& line : lines)
    {
        const std::string text = trim(line.text);
        const bool active = stack.empty() || stack.back().active;
        if (text.empty() || text[0] != '#')
        {
            if (active)
            {
                live.push_back(line);
            }
            continue;
        }

        std::istringstream stream(text.substr(1));
        std::string directive;
        stream >> directive;
        std::string name;
        stream >> name;
        if (directive == "ifdef" || directive == "ifndef")
        {
            const bool defined = macros.count(name) != 0;
            const bool condition = directive == "ifdef" ? defined : !defined;
            stack.push_back({ active, active && condition, active && condition });
        }
        else if (directive == "else")
        {
            if (stack.empty())
            {
                fail(line.file, line.number, "#else without #if");
            }
            Conditional& top = stack.back();
            top.active = top.parent_active && !top.taken;
            top.taken = true;
        }
        else if (directive == "endif")
        {
            if (stack.empty())
            {
                fail(line.file, line.number, "#endif without #if");
            }
            stack.pop_back();
        }
        else if (directive == "define" && active)
        {
            if (name.empty())
            {
                fail(line.file, line.number, "#define needs a name");
            }
            if (name.find('(') != std::string::npos)
            {
                fail(line.file, line.number, "function-like macros are not supported");
            }
            std::string value;
            std::getline(stream, value);
            macros[name] = trim(value);
        }
        else if (directive == "undef" && active)
        {
            macros.erase(name);
        }
        else if (directive == "if" || directive == "elif")
        {
            fail(line.file, line.number, "#" + directive + " is not supported; use #ifdef/#ifndef");
        }
    }
    if (!stack.empty())
    {
        fail(lines.empty() ? std::string("shader") : lines.back().file, lines.empty() ? 0 : lines.back().number, "unterminated #if");
    }
    return live;
}

bool is_ident_start(char c)
{
    return std::isalpha(static_cast<uint8_t>(c)) || c == '_';
}

bool is_ident_char(char c)
{
    return std::isalnum(static_cast<uint8_t>(c)) || c == '_';
}

void tokenize_text(const std::string& file, uint32_t line, const std::string& text, const MacroMap& macros, bool& in_block_comment, uint32_t depth, std::vector<Token>& tokens)
{
    size_t i = 0;
    while (i < text.size())
    {
        const char c = text[i];
        if (in_block_comment)
        {
            if (c == '*' && i + 1 < text.size() && text[i + 1] == '/')
            {
                in_block_comment = false;
                i += 2;
            }
            else
            {
                ++i;
            }
            continue;
        }
        if (std::isspace(static_cast<uint8_t>(c)))
        {
            ++i;
        }
        else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/')
        {
            return;
        }
        else if (c == '/' && i + 1 < text.size() && text[i + 1] == '*')
        {
            in_block_comment = true;
            i += 2;
        }
        else if (is_ident_start(c))
        {
            size_t end = i;
            while (end < text.size() && is_ident_char(text[end]))
            {
                ++end;
            }
            const std::string word = text.substr(i, end - i);
            const MacroMap::const_iterator macro = macros.find(word);
            if (macro != macros.end())
            {
                if (depth >= MAX_MACRO_DEPTH)
                {
                    fail(file, line, "macro '" + word + "' expands too deeply");
                }
                bool unused = false;
                tokenize_text(file, line, macro->second, macros, unused, depth + 1, tokens);
            }
            else
            {
                tokens.push_back({ Token::Kind::Ident, word, file, line });
            }
            i = end;
        }
        else if (std::isdigit(static_cast<uint8_t>(c)) || (c == '.' && i + 1 < text.size() && std::isdigit(static_cast<uint8_t>(text[i + 1]))))
        {
            size_t end = i;
            while (end < text.size() && (is_ident_char(text[end]) || text[end] == '.'))
            {
                ++end;
            }
            tokens.push_back({ Token::Kind::Number, text.substr(i, end - i), file, line });
            i = end;
        }
        else
        {
            const std::string two = text.substr(i, 2);
            if (two == "[[" || two == "]]" || two == "::")
            {
                tokens.push_back({ Token::Kind::Punct, two, file, line });
                i += 2;
            }
            else
            {
                tokens.push_back({ Token::Kind::Punct, std::string(1, c), file, line });
                ++i;
            }
        }
    }
}

std::vector<Token> tokenize(const std::vector<SourceLine>& lines, const MacroMap& macros)
{
    std::vector<Token> tokens;
    bool in_block_comment = false;
    for (const SourceLine& line : lines)
    {
        if (!starts_with(trim(line.text), "#"))
        {
            tokenize_text(line.file, line.number, line.text, macros, in_block_comment, 0, tokens);
        }
    }
    const std::string file = lines.empty() ? std::string("shader") : lines.back().file;
    tokens.push_back({ Token::Kind::End, "", file, lines.empty() ? 0 : lines.back().number });
    return tokens;
}

enum class AttributeKind : uint8_t
{
    Attribute,
    Position,
    Colour,
    Buffer,
    Texture,
    Sampler,
    StageIn,
    User,
    Builtin
};

struct Attribute
{
    AttributeKind kind = AttributeKind::Builtin;
    uint32_t value = 0;
};

struct StructMember
{
    std::string name;
    ShaderDataType type;
    uint32_t offset = 0;
    uint32_t size = 0;
    std::vector<Attribute> attributes;
    Token token;
};

struct StructInfo
{
    std::string name;
    std::vector<StructMember> members;
    uint32_t size = 0;
};

struct TypeRef
{
    std::string name;
    std::vector<std::vector<Token>> args;
};

struct FunctionInfo
{
    std::string name;
    std::string qualifier;
    Token token;
    std::vector<Token> return_type;
    std::vector<std::vector<Token>> params;
};

uint32_t align_up(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

uint32_t parse_uint(const Token& token)
{
    std::string digits = token.text;
    if (!digits.empty() && (digits.back() == 'u' || digits.back() == 'U'))
    {
        digits.pop_back();
    }
    if (token.kind != Token::Kind::Number || digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos || digits.size() > 9)
    {
        fail(token, "expected an integer literal, got '" + token.text + "'");
    }
    return static_cast<uint32_t>(std::stoul(digits));
}

bool is_builtin_attribute(const std::string& name)
{
    static const char* const names[] = {
        "vertex_id", "instance_id", "base_vertex", "base_instance", "front_facing", "sample_id", "primitive_id", "point_size",
        "clip_distance", "depth", "sample_mask", "thread_position_in_grid", "thread_position_in_threadgroup",
        "threadgroup_position_in_grid", "thread_index_in_threadgroup", "threads_per_threadgroup",
        "threads_per_grid", "max_total_threads_per_threadgroup"
    };
    for (const char* candidate : names)
    {
        if (name == candidate)
        {
            return true;
        }
    }
    return false;
}

// Parses `[[a]] [[b(n)]]` starting at tokens[index]; stops at the first token that is not `[[`.
std::vector<Attribute> parse_attributes(const std::vector<Token>& tokens, size_t& index)
{
    std::vector<Attribute> attributes;
    while (index < tokens.size() && tokens[index].text == "[[" && tokens[index].kind == Token::Kind::Punct)
    {
        ++index;
        while (index < tokens.size() && tokens[index].text != "]]")
        {
            const Token& name = tokens[index];
            if (name.kind != Token::Kind::Ident)
            {
                fail(name, "malformed attribute");
            }
            ++index;
            Attribute attribute;
            bool has_argument = false;
            if (index < tokens.size() && tokens[index].text == "(")
            {
                has_argument = true;
                ++index;
                if (index < tokens.size() && tokens[index].kind == Token::Kind::Number)
                {
                    attribute.value = parse_uint(tokens[index]);
                }
                while (index < tokens.size() && tokens[index].text != ")")
                {
                    ++index;
                }
                ++index;
            }
            if (name.text == "attribute" && has_argument)
            {
                attribute.kind = AttributeKind::Attribute;
            }
            else if (name.text == "position")
            {
                attribute.kind = AttributeKind::Position;
            }
            else if (name.text == "color" && has_argument)
            {
                attribute.kind = AttributeKind::Colour;
            }
            else if (name.text == "buffer" && has_argument)
            {
                attribute.kind = AttributeKind::Buffer;
            }
            else if (name.text == "texture" && has_argument)
            {
                attribute.kind = AttributeKind::Texture;
            }
            else if (name.text == "sampler" && has_argument)
            {
                attribute.kind = AttributeKind::Sampler;
            }
            else if (name.text == "stage_in")
            {
                attribute.kind = AttributeKind::StageIn;
            }
            else if (name.text == "user")
            {
                attribute.kind = AttributeKind::User;
            }
            else if (is_builtin_attribute(name.text))
            {
                attribute.kind = AttributeKind::Builtin;
            }
            else
            {
                fail(name, "unsupported attribute '" + name.text + "'");
            }
            attributes.push_back(attribute);
            if (index < tokens.size() && tokens[index].text == ",")
            {
                ++index;
            }
        }
        if (index >= tokens.size())
        {
            fail(tokens.back(), "unterminated attribute");
        }
        ++index;
    }
    return attributes;
}

const Attribute* find_attribute(const std::vector<Attribute>& attributes, AttributeKind kind)
{
    for (const Attribute& attribute : attributes)
    {
        if (attribute.kind == kind)
        {
            return &attribute;
        }
    }
    return nullptr;
}

bool is_builtin_output(const std::vector<Attribute>& attributes)
{
    for (const Attribute& attribute : attributes)
    {
        if (attribute.kind == AttributeKind::Position || attribute.kind == AttributeKind::Builtin)
        {
            return true;
        }
    }
    return false;
}

// Reads `name` or `metal::name` plus an optional `<arg, arg>` list from tokens[index...].
TypeRef parse_type_ref(const std::vector<Token>& tokens, size_t& index)
{
    TypeRef type;
    if (index + 1 < tokens.size() && tokens[index].text == "metal" && tokens[index + 1].text == "::")
    {
        index += 2;
    }
    if (index >= tokens.size() || tokens[index].kind != Token::Kind::Ident)
    {
        fail(tokens.empty() ? Token{} : tokens[std::min(index, tokens.size() - 1)], "expected a type name");
    }
    type.name = tokens[index++].text;
    if (index < tokens.size() && tokens[index].text == "<")
    {
        ++index;
        uint32_t depth = 0;
        std::vector<Token> arg;
        while (index < tokens.size())
        {
            const Token& token = tokens[index];
            if (token.text == "<")
            {
                ++depth;
            }
            else if (token.text == ">")
            {
                if (depth == 0)
                {
                    break;
                }
                --depth;
            }
            else if (token.text == "," && depth == 0)
            {
                type.args.push_back(arg);
                arg.clear();
                ++index;
                continue;
            }
            arg.push_back(token);
            ++index;
        }
        if (index >= tokens.size())
        {
            fail(tokens.back(), "unterminated template argument list");
        }
        type.args.push_back(arg);
        ++index;
    }
    return type;
}

struct TextureKind
{
    const char* name;
    ShaderTextureDimension dimension;
    bool depth;
};

const TextureKind* find_texture_kind(const std::string& name)
{
    static const TextureKind kinds[] = {
        { "texture2d", ShaderTextureDimension::Tex2D, false },
        { "texture2d_array", ShaderTextureDimension::Tex2DArray, false },
        { "texturecube", ShaderTextureDimension::Cube, false },
        { "texture3d", ShaderTextureDimension::Tex3D, false },
        { "texture2d_ms", ShaderTextureDimension::Tex2DMultisample, false },
        { "depth2d", ShaderTextureDimension::Tex2D, true }
    };
    for (const TextureKind& kind : kinds)
    {
        if (name == kind.name)
        {
            return &kind;
        }
    }
    return nullptr;
}

struct BindingRange
{
    uint32_t begin = 0;
    uint32_t end = 0;
    std::string name;
};

enum class BindingSpace : uint8_t
{
    Buffer,
    Texture,
    Sampler
};

class Parser
{
public:
    Parser(const std::vector<Token>& tokens, const ShaderCompilerInput& input)
        : m_tokens(tokens)
        , m_input(input)
    {
    }

    ShaderReflection run()
    {
        parse_translation_unit();
        return reflect_entry();
    }

private:
    const Token& peek(size_t ahead = 0) const { return m_tokens[std::min(m_index + ahead, m_tokens.size() - 1)]; }
    bool at_end() const { return peek().kind == Token::Kind::End; }
    bool at(const char* text) const { return peek().kind != Token::Kind::End && peek().text == text; }

    const Token& expect(const char* text)
    {
        if (!at(text))
        {
            fail(peek(), std::string("expected '") + text + "', got '" + peek().text + "'");
        }
        return m_tokens[m_index++];
    }

    void skip_to_semicolon()
    {
        while (!at_end() && !at(";"))
        {
            ++m_index;
        }
        if (!at_end())
        {
            ++m_index;
        }
    }

    void skip_block()
    {
        expect("{");
        uint32_t depth = 1;
        while (depth > 0)
        {
            if (at_end())
            {
                fail(peek(), "unterminated '{'");
            }
            depth += at("{") ? 1 : 0;
            depth -= at("}") ? 1 : 0;
            ++m_index;
        }
    }

    void parse_translation_unit()
    {
        while (!at_end())
        {
            if (at(";"))
            {
                ++m_index;
            }
            else if (at("using") || at("constant") || at("constexpr") || at("typedef"))
            {
                skip_to_semicolon();
            }
            else if (at("struct"))
            {
                parse_struct();
            }
            else if (at("[["))
            {
                parse_attributes(m_tokens, m_index);
            }
            else
            {
                parse_function();
            }
        }
    }

    void parse_struct()
    {
        ++m_index;
        if (peek().kind != Token::Kind::Ident)
        {
            fail(peek(), "expected a struct name");
        }
        StructInfo info;
        info.name = m_tokens[m_index++].text;
        if (m_structs.count(info.name) != 0)
        {
            fail(m_tokens[m_index - 1], "struct '" + info.name + "' is defined twice");
        }
        expect("{");
        uint32_t offset = 0;
        uint32_t alignment = 1;
        while (!at("}"))
        {
            if (at_end())
            {
                fail(peek(), "unterminated struct '" + info.name + "'");
            }
            StructMember member;
            member.token = peek();
            std::vector<Token> type_tokens;
            while (!at_end() && !at(";") && !at("[["))
            {
                type_tokens.push_back(m_tokens[m_index++]);
            }
            if (type_tokens.size() != 2 || type_tokens[0].kind != Token::Kind::Ident || type_tokens[1].kind != Token::Kind::Ident)
            {
                fail(member.token, "unsupported member declaration in struct '" + info.name + "'");
            }
            if (!parse_shader_type(type_tokens[0].text, member.type))
            {
                fail(type_tokens[0], "unsupported member type '" + type_tokens[0].text + "'");
            }
            member.name = type_tokens[1].text;
            member.attributes = parse_attributes(m_tokens, m_index);
            expect(";");
            const uint32_t member_alignment = shader_type_alignment(member.type);
            member.size = shader_type_size(member.type);
            member.offset = align_up(offset, member_alignment);
            offset = member.offset + (member.type.columns == 1 && member.type.rows == 3 ? member_alignment : member.size); // MSL sizeof(float3) is 16
            alignment = std::max(alignment, member_alignment);
            info.members.push_back(member);
        }
        ++m_index;
        expect(";");
        info.size = align_up(offset, alignment);
        const std::string name = info.name;
        m_structs.emplace(name, std::move(info));
    }

    void parse_function()
    {
        FunctionInfo function;
        function.token = peek();
        std::vector<Token> head;
        while (!at_end() && !at("(") && !at(";") && !at("{"))
        {
            head.push_back(m_tokens[m_index++]);
        }
        if (!at("(") || head.size() < 2 || head.back().kind != Token::Kind::Ident)
        {
            fail(function.token, "unsupported top-level construct near '" + function.token.text + "'");
        }
        function.name = head.back().text;
        head.pop_back();
        const bool has_qualifier = head[0].text == "vertex" || head[0].text == "fragment" || head[0].text == "kernel";
        if (has_qualifier)
        {
            function.qualifier = head[0].text;
            head.erase(head.begin());
        }
        function.return_type = head;

        ++m_index;
        std::vector<Token> param;
        uint32_t depth = 0;
        while (true)
        {
            if (at_end())
            {
                fail(function.token, "unterminated parameter list of '" + function.name + "'");
            }
            if (at(")") && depth == 0)
            {
                break;
            }
            if (at("(") || at("<"))
            {
                ++depth;
            }
            else if (at(")") || at(">"))
            {
                --depth;
            }
            if (at(",") && depth == 0)
            {
                function.params.push_back(param);
                param.clear();
            }
            else
            {
                param.push_back(peek());
            }
            ++m_index;
        }
        ++m_index;
        if (!param.empty())
        {
            function.params.push_back(param);
        }
        if (at("{"))
        {
            skip_block();
        }
        else
        {
            expect(";");
        }
        m_functions.push_back(std::move(function));
    }

    const StructInfo* find_struct(const std::string& name) const
    {
        const std::map<std::string, StructInfo>::const_iterator it = m_structs.find(name);
        return it == m_structs.end() ? nullptr : &it->second;
    }

    ShaderReflection reflect_entry()
    {
        const FunctionInfo* entry = nullptr;
        for (const FunctionInfo& function : m_functions)
        {
            if (function.name == m_input.entry_point)
            {
                entry = &function;
                break;
            }
        }
        const std::string file = m_input.source.name;
        if (entry == nullptr)
        {
            fail(file, 1, "entry point '" + m_input.entry_point + "' not found");
        }
        const char* expected = nullptr;
        switch (m_input.stage)
        {
        case ShaderStage::Vertex: expected = "vertex"; break;
        case ShaderStage::Pixel: expected = "fragment"; break;
        case ShaderStage::Compute: expected = "kernel"; break;
        case ShaderStage::TessControl:
        case ShaderStage::TessEval: fail(entry->token, std::string("the MSL compiler does not support the ") + shader_stage_name(m_input.stage) + " stage");
        }
        if (entry->qualifier != expected)
        {
            fail(entry->token, "entry point '" + entry->name + "' must be declared '" + expected + "'");
        }

        ShaderReflection reflection;
        reflection.entry_point = entry->name;
        reflection.stage = m_input.stage;
        std::set<std::string> names;
        std::vector<BindingRange> ranges[3];
        for (const std::vector<Token>& param : entry->params)
        {
            reflect_parameter(*entry, param, reflection, names, ranges);
        }
        reflect_outputs(*entry, reflection);
        validate_locations(*entry, reflection);
        return reflection;
    }

    void reserve_slot(const Token& where, BindingSpace space, uint32_t slot, uint32_t count, const std::string& name, std::vector<BindingRange> (&ranges)[3])
    {
        std::vector<BindingRange>& used = ranges[static_cast<size_t>(space)];
        for (const BindingRange& range : used)
        {
            if (slot < range.end && range.begin < slot + count)
            {
                fail(where, "slot " + std::to_string(slot) + " of '" + name + "' is already used by '" + range.name + "'");
            }
        }
        used.push_back({ slot, slot + count, name });
    }

    void reflect_parameter(const FunctionInfo& entry, const std::vector<Token>& param, ShaderReflection& reflection, std::set<std::string>& names, std::vector<BindingRange> (&ranges)[3])
    {
        size_t attribute_start = 0;
        while (attribute_start < param.size() && param[attribute_start].text != "[[")
        {
            ++attribute_start;
        }
        const std::vector<Token> declarator(param.begin(), param.begin() + static_cast<std::ptrdiff_t>(attribute_start));
        size_t cursor = attribute_start;
        const std::vector<Attribute> attributes = parse_attributes(param, cursor);
        if (declarator.size() < 2 || declarator.back().kind != Token::Kind::Ident)
        {
            fail(param.empty() ? entry.token : param[0], "malformed parameter in '" + entry.name + "'");
        }
        const Token name_token = declarator.back();
        std::vector<Token> type_tokens(declarator.begin(), declarator.end() - 1);
        bool is_reference = false;
        bool is_pointer = false;
        if (!type_tokens.empty() && type_tokens.back().text == "&")
        {
            is_reference = true;
            type_tokens.pop_back();
        }
        else if (!type_tokens.empty() && type_tokens.back().text == "*")
        {
            is_pointer = true;
            type_tokens.pop_back();
        }
        std::string address_space;
        std::vector<Token> type_only;
        for (const Token& token : type_tokens)
        {
            if (token.text == "constant" || token.text == "device" || token.text == "thread" || token.text == "threadgroup")
            {
                address_space = token.text;
            }
            else if (token.text != "const")
            {
                type_only.push_back(token);
            }
        }
        if (type_only.empty())
        {
            fail(name_token, "parameter '" + name_token.text + "' has no type");
        }
        size_t type_index = 0;
        const TypeRef type = parse_type_ref(type_only, type_index);

        if (find_attribute(attributes, AttributeKind::StageIn) != nullptr)
        {
            reflect_stage_in(name_token, type, reflection);
            return;
        }

        const Attribute* buffer = find_attribute(attributes, AttributeKind::Buffer);
        const Attribute* texture = find_attribute(attributes, AttributeKind::Texture);
        const Attribute* sampler = find_attribute(attributes, AttributeKind::Sampler);
        if (buffer == nullptr && texture == nullptr && sampler == nullptr)
        {
            if (find_attribute(attributes, AttributeKind::Builtin) != nullptr)
            {
                return;
            }
            fail(name_token, "parameter '" + name_token.text + "' has no binding attribute");
        }
        if (!names.insert(name_token.text).second)
        {
            fail(name_token, "parameter '" + name_token.text + "' is declared twice");
        }

        ShaderBinding binding;
        binding.name = name_token.text;
        if (buffer != nullptr)
        {
            binding.slot = buffer->value;
            if (!is_reference && !is_pointer)
            {
                fail(name_token, "buffer parameter '" + binding.name + "' must be a reference or pointer");
            }
            ShaderDataType scalar_type;
            const StructInfo* info = find_struct(type.name);
            if (info != nullptr)
            {
                binding.size = is_reference ? info->size : 0;
                for (const StructMember& member : info->members)
                {
                    binding.members.push_back({ member.name, member.type, member.offset, member.size });
                }
            }
            else if (parse_shader_type(type.name, scalar_type))
            {
                binding.size = is_reference ? shader_type_size(scalar_type) : 0;
            }
            else
            {
                fail(name_token, "unknown buffer element type '" + type.name + "'");
            }
            // A `constant T&` is push-style constant data; `constant T*` is a bound uniform range.
            if (address_space == "constant")
            {
                binding.kind = is_reference ? ShaderBindingKind::Constants : ShaderBindingKind::UniformBuffer;
            }
            else if (address_space == "device")
            {
                binding.kind = ShaderBindingKind::StorageBuffer;
            }
            else
            {
                fail(name_token, "buffer parameter '" + binding.name + "' must be in the 'constant' or 'device' address space");
            }
            reserve_slot(name_token, BindingSpace::Buffer, binding.slot, 1, binding.name, ranges);
        }
        else
        {
            const TypeRef* element = &type;
            TypeRef inner;
            if (type.name == "array")
            {
                if (type.args.size() != 2)
                {
                    fail(name_token, "array needs an element type and a count");
                }
                size_t inner_index = 0;
                inner = parse_type_ref(type.args[0], inner_index);
                element = &inner;
                if (type.args[1].size() != 1)
                {
                    fail(name_token, "array count must be an integer literal");
                }
                binding.array_count = parse_uint(type.args[1][0]);
                if (binding.array_count == 0)
                {
                    fail(name_token, "array count of '" + binding.name + "' must be greater than zero");
                }
            }
            if (texture != nullptr)
            {
                reflect_texture(name_token, *element, binding);
                binding.slot = texture->value;
                reserve_slot(name_token, BindingSpace::Texture, binding.slot, binding.array_count, binding.name, ranges);
            }
            else
            {
                if (element->name != "sampler")
                {
                    fail(name_token, "'" + binding.name + "' has a [[sampler]] attribute but is not a sampler");
                }
                binding.kind = ShaderBindingKind::Sampler;
                binding.slot = sampler->value;
                reserve_slot(name_token, BindingSpace::Sampler, binding.slot, binding.array_count, binding.name, ranges);
            }
        }
        reflection.parameters.push_back(std::move(binding));
    }

    void reflect_texture(const Token& where, const TypeRef& type, ShaderBinding& binding)
    {
        const TextureKind* kind = find_texture_kind(type.name);
        if (kind == nullptr)
        {
            fail(where, "'" + binding.name + "' has a [[texture]] attribute but type '" + type.name + "' is not a texture");
        }
        binding.texture_dimension = kind->dimension;
        std::string element = "float";
        if (!type.args.empty() && !type.args[0].empty())
        {
            element = type.args[0][0].text;
        }
        if (kind->depth)
        {
            binding.texture_data_type = ShaderTextureData::Depth;
        }
        else if (element == "float" || element == "half")
        {
            binding.texture_data_type = ShaderTextureData::Float;
        }
        else if (element == "int" || element == "short")
        {
            binding.texture_data_type = ShaderTextureData::Int;
        }
        else if (element == "uint" || element == "ushort")
        {
            binding.texture_data_type = ShaderTextureData::UInt;
        }
        else
        {
            fail(where, "unsupported texture element type '" + element + "'");
        }
        binding.kind = ShaderBindingKind::SampledTexture;
        if (type.args.size() > 1 && !type.args[1].empty())
        {
            const std::string access = type.args[1].back().text;
            if (access == "write" || access == "read_write")
            {
                if (kind->depth)
                {
                    fail(where, "depth textures cannot be written");
                }
                binding.kind = ShaderBindingKind::StorageTexture;
            }
        }
    }

    void reflect_stage_in(const Token& where, const TypeRef& type, ShaderReflection& reflection)
    {
        const StructInfo* info = find_struct(type.name);
        if (info == nullptr)
        {
            fail(where, "[[stage_in]] type '" + type.name + "' is not a struct defined in this shader");
        }
        if (m_input.stage == ShaderStage::Compute)
        {
            fail(where, "compute entry points cannot use [[stage_in]]");
        }
        uint32_t next_location = 0;
        for (const StructMember& member : info->members)
        {
            ShaderStageVariable variable;
            variable.name = member.name;
            variable.type = member.type;
            variable.builtin = is_builtin_output(member.attributes);
            if (m_input.stage == ShaderStage::Vertex)
            {
                const Attribute* attribute = find_attribute(member.attributes, AttributeKind::Attribute);
                if (attribute == nullptr)
                {
                    fail(member.token, "vertex input '" + member.name + "' needs [[attribute(n)]]");
                }
                variable.location = attribute->value;
            }
            else
            {
                variable.location = variable.builtin ? 0 : next_location++;
            }
            reflection.inputs.push_back(variable);
        }
    }

    void reflect_outputs(const FunctionInfo& entry, ShaderReflection& reflection)
    {
        if (entry.return_type.empty())
        {
            fail(entry.token, "entry point '" + entry.name + "' has no return type");
        }
        size_t index = 0;
        const TypeRef type = parse_type_ref(entry.return_type, index);
        if (m_input.stage == ShaderStage::Compute)
        {
            if (type.name != "void")
            {
                fail(entry.token, "kernel '" + entry.name + "' must return void");
            }
            return;
        }
        if (type.name == "void" && m_input.stage == ShaderStage::Pixel)
        {
            return;
        }
        ShaderDataType scalar_type;
        const StructInfo* info = find_struct(type.name);
        if (info != nullptr)
        {
            uint32_t next_location = 0;
            for (const StructMember& member : info->members)
            {
                ShaderStageVariable variable;
                variable.name = member.name;
                variable.type = member.type;
                variable.builtin = is_builtin_output(member.attributes);
                const Attribute* colour = find_attribute(member.attributes, AttributeKind::Colour);
                variable.location = colour != nullptr ? colour->value : (variable.builtin ? 0 : next_location++);
                reflection.outputs.push_back(variable);
            }
        }
        else if (parse_shader_type(type.name, scalar_type))
        {
            const bool vertex = m_input.stage == ShaderStage::Vertex;
            reflection.outputs.push_back({ vertex ? "position" : "colour", 0, scalar_type, vertex });
        }
        else
        {
            fail(entry.token, "unsupported return type '" + type.name + "' for '" + entry.name + "'");
        }
        if (m_input.stage == ShaderStage::Vertex)
        {
            bool has_position = false;
            for (const ShaderStageVariable& output : reflection.outputs)
            {
                has_position = has_position || output.builtin;
            }
            if (!has_position)
            {
                fail(entry.token, "vertex entry '" + entry.name + "' must output a [[position]]");
            }
        }
    }

    void validate_locations(const FunctionInfo& entry, const ShaderReflection& reflection)
    {
        const std::vector<ShaderStageVariable>* lists[] = { &reflection.inputs, &reflection.outputs };
        const char* labels[] = { "input", "output" };
        for (size_t list = 0; list < 2; ++list)
        {
            const bool check = list == 0 ? m_input.stage == ShaderStage::Vertex : m_input.stage == ShaderStage::Pixel;
            if (!check)
            {
                continue;
            }
            std::map<uint32_t, std::string> seen;
            for (const ShaderStageVariable& variable : *lists[list])
            {
                if (variable.builtin)
                {
                    continue;
                }
                const std::map<uint32_t, std::string>::const_iterator it = seen.find(variable.location);
                if (it != seen.end())
                {
                    fail(entry.token, std::string(labels[list]) + " location " + std::to_string(variable.location) + " is used by both '" + it->second + "' and '" + variable.name + "'");
                }
                seen.emplace(variable.location, variable.name);
            }
        }
    }

    const std::vector<Token>& m_tokens;
    const ShaderCompilerInput& m_input;
    size_t m_index = 0;
    std::map<std::string, StructInfo> m_structs;
    std::vector<FunctionInfo> m_functions;
};

std::string stage_define(ShaderStage stage)
{
    switch (stage)
    {
    case ShaderStage::Vertex: return "ORYX_STAGE_VERTEX";
    case ShaderStage::Pixel: return "ORYX_STAGE_PIXEL";
    case ShaderStage::Compute: return "ORYX_STAGE_COMPUTE";
    case ShaderStage::TessControl: return "ORYX_STAGE_TESS_CONTROL";
    case ShaderStage::TessEval: return "ORYX_STAGE_TESS_EVAL";
    }
    return "ORYX_STAGE_UNKNOWN";
}

} // namespace

std::vector<std::string> MslShaderCompiler::dependencies(const ShaderSource& source) const
{
    return shader_include_closure(source.text, [](const std::string&) { return nullptr; });
}

ShaderCompilerOutput MslShaderCompiler::compile(const ShaderCompilerInput& input) const
{
    if (input.target != ShaderBinaryFormat::MslSource)
    {
        throw Error(input.source.name + ": the MSL compiler only emits MSL source");
    }
    MacroMap macros;
    macros["ORYX_MSL"] = "1";
    macros[stage_define(input.stage)] = "1";
    for (const ShaderDefine& define : input.defines)
    {
        macros[define.name] = define.value;
    }

    std::vector<SourceLine> lines;
    std::vector<std::string> stack;
    std::set<std::string> included;
    expand_includes(input.source.name, input.source.text, lines, stack, included, input);

    const std::vector<SourceLine> live = evaluate_conditionals(lines, macros);
    const std::vector<Token> tokens = tokenize(live, macros);

    ShaderCompilerOutput output;
    output.reflection = Parser(tokens, input).run();
    output.format = ShaderBinaryFormat::MslSource;

    // Sorted so equal hashes always yield equal binaries.
    std::string text;
    std::map<std::string, std::string> prelude;
    prelude["ORYX_MSL"] = "1";
    prelude[stage_define(input.stage)] = "1";
    for (const ShaderDefine& define : input.defines)
    {
        prelude[define.name] = define.value;
    }
    for (const std::pair<const std::string, std::string>& define : prelude)
    {
        text += "#define " + define.first + " " + define.second + "\n";
    }
    for (const SourceLine& line : lines)
    {
        text += line.text + "\n";
    }
    output.binary.assign(text.begin(), text.end());
    output.hash = hash_shader_input(input, *this);
    return output;
}

} // namespace oryx
