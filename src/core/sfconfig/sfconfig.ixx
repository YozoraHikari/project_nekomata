export module projnekomata:core.sfconfig;
import projnekomata.cs;
import std;
import :core.overloaded;

export namespace projnekomata::sfconfig {

struct DataNode;
using DataNodeList = Vec<DataNode>;
using DataNodeMap = HashMap<std::string, DataNode>;

struct DataNode {
    using Type = FlatVariant<
        bool,                           // matches true / false
        i64,                            // matches signed ints
        f64,                            // matches floats
        std::string,                    // matches strings (in quotes)
        Unique<DataNodeList>,           // matches arrays (in [] braces)
        Unique<DataNodeMap>             // matches objects (in {} braces)
    >;

    Type value;

    DataNode() = default;
    DataNode(Type v) : value(std::move(v)) {}

    [[nodiscard]] constexpr auto isObject() const -> bool { return matches<Unique<DataNodeMap>>(value); }
    [[nodiscard]] constexpr auto isArray() const -> bool { return matches<Unique<DataNodeList>>(value); }
    [[nodiscard]] constexpr auto isString() const -> bool { return matches<std::string>(value); }
    [[nodiscard]] constexpr auto isI64() const -> bool { return matches<i64>(value); }
    [[nodiscard]] constexpr auto isF64() const -> bool { return matches<f64>(value); }
    [[nodiscard]] constexpr auto isBool() const -> bool { return matches<bool>(value); }

    [[nodiscard]] constexpr auto asObject() -> Unique<DataNodeMap>& { return acquireInto<Unique<DataNodeMap>>(value); }
    [[nodiscard]] constexpr auto asArray() -> Unique<DataNodeList>& { return acquireInto<Unique<DataNodeList>>(value); }
    [[nodiscard]] constexpr auto asString() -> std::string& { return acquireInto<std::string>(value); }
    [[nodiscard]] constexpr auto asI64() -> i64& { return acquireInto<i64>(value); }
    [[nodiscard]] constexpr auto asF64() -> f64& { return acquireInto<f64>(value); }
    [[nodiscard]] constexpr auto asBool() -> bool& { return acquireInto<bool>(value); }

    [[nodiscard]] constexpr auto operator[](const std::string& key) -> Option<std::reference_wrapper<DataNode>> {
        return asObject()->get(key);
    }

    [[nodiscard]] constexpr auto operator[](usize index) -> Option<std::reference_wrapper<DataNode>> {
        return asArray()->at(index);
    }
};

class ConfigData;


using LexValue = FlatVariant<std::string, f64, i64>; // bool is deduced from strings

enum class LexTokenType {
    Value,
    String,

    LSqBrace,
    RSqBrace,
    LCurlyBrace,
    RCurlyBrace,

    Eof,
};

auto lexTokenTypeToString(LexTokenType type) -> std::string_view {
    switch (type) {
        case LexTokenType::Value: return "Value";
        case LexTokenType::String: return "String";
        case LexTokenType::LSqBrace: return "[";
        case LexTokenType::RSqBrace: return "]";
        case LexTokenType::LCurlyBrace: return "{";
        case LexTokenType::RCurlyBrace: return "}";
        case LexTokenType::Eof: return "<eof>";
    }
}

struct LexToken {
    LexTokenType type;
    LexValue value;
    usize line;
    usize col;
};

class Lexer {
public:
    static auto on(std::string_view text) -> Lexer;

    auto next() -> LexToken;

private:
    explicit Lexer(std::string_view text) : m_text(text) {}

    auto skipWhitespaceAndComments() -> void;

    auto lexString() -> LexToken;
    auto lexNumber() -> LexToken;
    auto lexRawStr() -> LexToken;

    std::string_view m_text;
    usize m_pos = 0;
    usize m_line = 1;
    usize m_col = 1;
};

class Parser {
public:
    static auto create(std::string_view text) -> Parser;
    auto parse() -> ConfigData;

private:
    Lexer m_lexer;
    LexToken m_currentToken = LexToken{LexTokenType::Eof, 0_i64, 0, 0};

    explicit Parser(Lexer lexer) : m_lexer(lexer) {}

    auto advance() -> void;
    auto expectToken(LexTokenType type) -> void;

    auto parseMember() -> std::pair<std::string, DataNode>;
    auto parseValue() -> DataNode;
};

class ConfigData {
public:
    ConfigData(DataNode&& data) : m_rootData(std::move(data)) {}

    static auto parse(std::string_view text) -> ConfigData;

    auto get(const std::string& key) -> Option<std::reference_wrapper<DataNode>> {
        return m_rootData.asObject()->get(key);
    }

private:

    DataNode m_rootData;
};

}
