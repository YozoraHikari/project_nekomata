module projnekomata;
import :core.sfconfig;

namespace projnekomata::sfconfig {

auto Lexer::on(std::string_view text) -> Lexer { return Lexer(text); }

auto Lexer::next() -> LexToken {
    skipWhitespaceAndComments();
    if (m_pos >= m_text.length()) {
        return {LexTokenType::Eof, std::string{}, m_line, m_col};
    }

    auto ch = m_text[m_pos];

    switch (ch) {
        case '[': { m_pos++; m_col++; return {LexTokenType::LSqBrace, 0_i64, m_line, m_col}; }
        case ']': { m_pos++; m_col++; return {LexTokenType::RSqBrace, 0_i64, m_line, m_col}; }
        case '{': { m_pos++; m_col++; return {LexTokenType::LCurlyBrace, 0_i64, m_line, m_col}; }
        case '}': { m_pos++; m_col++; return {LexTokenType::RCurlyBrace, 0_i64, m_line, m_col}; }
        case '"': return lexString();
    }

    if (std::isdigit(ch) || ch == '-') return lexNumber();

    if (std::isalpha(ch) || ch == '_') return lexRawStr();

    panic("unexpected character: {} at line {} column {}", ch, m_line, m_col);
}

auto Lexer::skipWhitespaceAndComments() -> void {
    while (true) {
        while (m_pos < m_text.length() && std::isspace(m_text[m_pos])) {
            if (m_text[m_pos++] == '\n') {
                m_line++;
                m_col = 1;
            } else {
                m_col++;
            }
        }

        if (m_pos + 1 < m_text.length() && m_text[m_pos] == '/' && m_text[m_pos + 1] == '/') {
            while (m_pos < m_text.length() && m_text[m_pos] != '\n') {
                m_pos++;
            }
            m_line++;
            m_col = 1;
        } else {
            break;
        }
    }
}

auto Lexer::lexString() -> LexToken {
    m_pos++; // skip the opening quote
    m_col++;
    std::string str;
    while (m_pos < m_text.length() && m_text[m_pos] != '"') {
        auto ch = m_text[m_pos];
        m_pos++;
        if (ch == '\n') {
            m_line++;
            m_col = 1;
        } else {
            m_col++;
        }

        // look for an escape sequence:
        if (ch == '\\' && m_pos < m_text.length()) {
            auto escaped = m_text[m_pos];
            m_pos++;
            switch (escaped) {
                case 'n': str += '\n'; break;
                case 't': str += '\t'; break;
                case 'r': str += '\r'; break;
                case '\\': str += '\\'; break;
                case '"': str += '"'; break;
                default: {
                    log::warn("unknown escape sequence: '\\{}' at line {} column {}", escaped, m_line, m_col);
                    str += escaped;
                }
            }
            if (escaped == '\n') {
                m_line++;
                m_col = 1;
            } else {
                m_col++;
            }
        } else {
            str += ch;
        }
    }

    // TODO: make a Result
    if (m_pos >= m_text.length()) panic("unterminated string at line {} column {}", m_line, m_col);
    m_pos++; // skip the closing quote
    m_col++;
    return {LexTokenType::String, std::move(str), m_line, m_col};
}

auto Lexer::lexNumber() -> LexToken {
    auto startPos = m_pos;
    auto isFloat = false;

    if (m_text[m_pos] == '-') { m_pos++; m_col++; }
    while (m_pos < m_text.length() && std::isdigit(m_text[m_pos])) { m_pos++; m_col++; }
    if (m_pos < m_text.length() && m_text[m_pos] == '.') {
        m_pos++; m_col++; isFloat = true;
        while (m_pos < m_text.length() && std::isdigit(m_text[m_pos])) { m_pos++; m_col++; }
    }

    auto text = m_text.substr(startPos, m_pos - startPos);

    if (isFloat) {
        f64 value;
        auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec == std::errc{}) {
            return {LexTokenType::Value, value, m_line, m_col};
        }
        panic("invalid float literal: {} ({}) when parsing at line {} column {}", text, std::make_error_code(ec).message(), m_line, m_col);
    }

    i64 value;
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec == std::errc{}) {
        return {LexTokenType::Value, value, m_line, m_col};
    }
    panic("invalid integer literal: {} ({}) when parsing at line {} column {}", text, std::make_error_code(ec).message(), m_line, m_col);
}

auto Lexer::lexRawStr() -> LexToken {
    auto startPos = m_pos;

    while (m_pos < m_text.length() && (std::isalnum(m_text[m_pos]) || m_text[m_pos] == '_')) { m_pos++; m_col++; }

    std::string str(m_text.substr(startPos, m_pos - startPos));

    return {LexTokenType::Value, std::move(str), m_line, m_col};
}

auto Parser::create(std::string_view text) -> Parser {
    return Parser(Lexer::on(text));
}

auto Parser::parse() -> ConfigData {
    advance();
    auto data = DataNode{Unique<HashMap<std::string, DataNode>>::from(HashMap<std::string, DataNode>::create())};
    while (m_currentToken.type != LexTokenType::Eof) {
        auto [key, value] = parseMember();
        data.asObject()->insert(std::move(key), std::move(value));
    }
    return ConfigData(std::move(data));
}

auto Parser::advance() -> void {
    m_currentToken = m_lexer.next();
}

auto Parser::expectToken(LexTokenType type) -> void {
    if (m_currentToken.type != type) {
        panic("line {}, column {}: expected token of type {}, got {}", m_currentToken.line, m_currentToken.col, lexTokenTypeToString(type), lexTokenTypeToString(m_currentToken.type));
    }
}

auto Parser::parseMember() -> std::pair<std::string, DataNode> {
    expectToken(LexTokenType::Value);
    auto key = acquireInto<std::string>(m_currentToken.value);
    advance();

    return {std::move(key), parseValue()};
}

auto Parser::parseValue() -> DataNode {
    switch (m_currentToken.type) {
        case LexTokenType::Value: {
            // See if the variant has an int/float type first:
            if (matches<i64>(m_currentToken.value)) {
                auto value = acquireInto<i64>(m_currentToken.value);
                advance();
                return DataNode{value};
            }
            if (matches<f64>(m_currentToken.value)) {
                auto value = acquireInto<f64>(m_currentToken.value);
                advance();
                return DataNode{value};
            }

            // Boolean deduction

            auto val = DataNode{false};
            auto& str = acquireInto<std::string>(m_currentToken.value);
            if (str == "true") {
                val = DataNode{true};
            } else if (str != "false") {
                panic("line {}, column {}: expected an int/float/bool/string/array/object", m_currentToken.line, m_currentToken.col);
            }
            advance();
            return val;
        }
        case LexTokenType::String: {
            auto str = acquireInto<std::string>(m_currentToken.value);
            advance();
            return DataNode{std::move(str)};
        }
        case LexTokenType::LSqBrace: {
            advance(); // skip opening bracket
            auto val = DataNode{Unique<Vec<DataNode>>::from(Vec<DataNode>::create())};
            while (m_currentToken.type != LexTokenType::RSqBrace) {
                val.asArray()->emplace(parseValue());
            }
            advance(); // skip closing bracket
            return val;
        }
        case LexTokenType::LCurlyBrace: {
            advance(); // skip opening brace
            auto val = DataNode{Unique<HashMap<std::string, DataNode>>::from(HashMap<std::string, DataNode>::create())};
            while (m_currentToken.type != LexTokenType::RCurlyBrace) {
                auto prop = parseMember();
                val.asObject()->insert(std::move(prop.first), std::move(prop.second));
            }
            advance(); // skip closing brace
            return val;
        }
        default: {
            panic("line {}, column {}: expected an int/float/bool/string/array/object", m_currentToken.line, m_currentToken.col);
        }
    }
}

auto ConfigData::parse(std::string_view text) -> ConfigData {
    auto parser = Parser::create(text);
    return parser.parse();
}
}
