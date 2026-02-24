#include "fabric/parser/SyntaxTree.hh"
#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"

namespace fabric {

ASTNode::ASTNode(const Token& token) : token(token) {}

void ASTNode::addChild(std::shared_ptr<ASTNode> child) {
    children.push_back(child);
}

const std::vector<std::shared_ptr<ASTNode>>& ASTNode::getChildren() const {
    return children;
}

const Token& ASTNode::getToken() const {
    return token;
}

TokenType determineTokenType(const std::string& token) {
    // CLI
    if (token.size() > 2 && token.substr(0, 2) == "--") {
        // Check if it's a flag with a value (--option=value)
        size_t equalsPos = token.find('=');
        if (equalsPos != std::string::npos)
            return TokenType::CLIOption;

        // Otherwise it's a simple flag (--flag)
        return TokenType::CLIFlag;
    }
    if (token.size() > 1 && token[0] == '-' && token[1] != '-')
        return TokenType::CLIFlag; // Short options (-f)

    // Control Flow
    if (token == "if")
        return TokenType::KeywordIf;
    if (token == "else")
        return TokenType::KeywordElse;
    if (token == "for")
        return TokenType::KeywordFor;
    if (token == "while")
        return TokenType::KeywordWhile;
    if (token == "return")
        return TokenType::KeywordReturn;
    if (token == "goto")
        return TokenType::KeywordGoto;
    if (token == "break")
        return TokenType::KeywordBreak;
    if (token == "continue")
        return TokenType::KeywordContinue;
    if (token == "switch")
        return TokenType::KeywordSwitch;
    if (token == "case")
        return TokenType::KeywordCase;
    if (token == "default")
        return TokenType::KeywordDefault;
    if (token == "defer")
        return TokenType::KeywordDefer;

    // Error Handling
    if (token == "try")
        return TokenType::KeywordTry;
    if (token == "catch")
        return TokenType::KeywordCatch;
    if (token == "throw")
        return TokenType::KeywordThrow;
    if (token == "finally")
        return TokenType::KeywordFinally;
    if (token == "raise")
        return TokenType::KeywordRaise;
    if (token == "assert")
        return TokenType::KeywordAssert;

    // Data Types
    if (token == "func" || token == "def" || token == "fn" || token == "function")
        return TokenType::KeywordFunction;
    if (token == "struct")
        return TokenType::KeywordStruct;
    if (token == "enum")
        return TokenType::KeywordEnum;
    if (token == "array")
        return TokenType::KeywordArray;
    if (token == "map" || token == "dict")
        return TokenType::KeywordMap;
    if (token == "set")
        return TokenType::KeywordSet;
    if (token == "tuple")
        return TokenType::KeywordTuple;
    if (token == "generic" || token == "template")
        return TokenType::KeywordGeneric;
    if (token == "where")
        return TokenType::KeywordWhere;

    // Object-Oriented Inheritance
    if (token == "class")
        return TokenType::KeywordClass;
    if (token == "interface")
        return TokenType::KeywordInterface;
    if (token == "implements")
        return TokenType::KeywordImplements;
    if (token == "extends")
        return TokenType::KeywordExtends;
    if (token == "self")
        return TokenType::KeywordSelf;
    if (token == "super")
        return TokenType::KeywordSuper;
    if (token == "override")
        return TokenType::KeywordOverride;
    if (token == "abstract")
        return TokenType::KeywordAbstract;
    if (token == "virtual")
        return TokenType::KeywordVirtual;
    if (token == "delegate")
        return TokenType::KeywordDelegate;
    if (token == "event")
        return TokenType::KeywordEvent;

    // Modules
    if (token == "import" || token == "include" || token == "use")
        return TokenType::KeywordImport;
    if (token == "package" || token == "module" || token == "namespace")
        return TokenType::KeywordPackage;
    if (token == "export")
        return TokenType::KeywordExport;
    if (token == "from")
        return TokenType::KeywordFrom;

    // Declarations
    if (token == "const")
        return TokenType::KeywordConst;
    if (token == "let")
        return TokenType::KeywordLet;
    if (token == "var")
        return TokenType::KeywordVar;
    if (token == "type")
        return TokenType::KeywordType;
    if (token == "mut")
        return TokenType::KeywordMut;
    if (token == "unsafe")
        return TokenType::KeywordUnsafe;
    if (token == "static")
        return TokenType::KeywordStatic;

    // Memory Management
    if (token == "new")
        return TokenType::KeywordNew;
    if (token == "delete")
        return TokenType::KeywordDelete;
    if (token == "alloc")
        return TokenType::KeywordAlloc;
    if (token == "free")
        return TokenType::KeywordFree;
    if (token == "move")
        return TokenType::KeywordMove;
    if (token == "borrow")
        return TokenType::KeywordBorrow;

    // Access Modifiers
    if (token == "pub" || token == "public")
        return TokenType::KeywordPublic;
    if (token == "priv" || token == "private")
        return TokenType::KeywordPrivate;
    if (token == "prot" || token == "protected")
        return TokenType::KeywordProtected;
    if (token == "int" || token == "internal")
        return TokenType::KeywordInternal;
    if (token == "final")
        return TokenType::KeywordFinal;

    // Boolean Operators
    if (token == "as")
        return TokenType::KeywordAs;
    if (token == "is")
        return TokenType::KeywordIs;
    if (token == "in")
        return TokenType::KeywordIn;
    if (token == "not")
        return TokenType::KeywordNot;
    if (token == "and")
        return TokenType::KeywordAnd;
    if (token == "or")
        return TokenType::KeywordOr;

    // Functional Programming
    if (token == "lambda" || token == "=>")
        return TokenType::KeywordLambda;
    if (token == "closure")
        return TokenType::KeywordClosure;
    if (token == "curry")
        return TokenType::KeywordCurry;
    if (token == "pipe" || token == "|>")
        return TokenType::KeywordPipe;
    if (token == "compose")
        return TokenType::KeywordCompose;

    // Concurrency
    if (token == "thread")
        return TokenType::KeywordThread;
    if (token == "atomic")
        return TokenType::KeywordAtomic;
    if (token == "sync")
        return TokenType::KeywordSync;
    if (token == "lock")
        return TokenType::KeywordLock;
    if (token == "mutex")
        return TokenType::KeywordMutex;

    // Async
    if (token == "yield")
        return TokenType::KeywordYield;
    if (token == "async")
        return TokenType::KeywordAsync;
    if (token == "await")
        return TokenType::KeywordAwait;

    // Operators
    if (token == "+")
        return TokenType::OperatorPlus;
    if (token == "-")
        return TokenType::OperatorMinus;
    if (token == "*")
        return TokenType::OperatorMultiply;
    if (token == "/")
        return TokenType::OperatorDivide;
    if (token == "%")
        return TokenType::OperatorModulo;
    if (token == "=")
        return TokenType::OperatorAssign;
    if (token == "==")
        return TokenType::OperatorEqual;
    if (token == "!=" || token == "<>")
        return TokenType::OperatorNotEqual;
    if (token == "<")
        return TokenType::OperatorLessThan;
    if (token == ">")
        return TokenType::OperatorGreaterThan;
    if (token == "<=")
        return TokenType::OperatorLessEqual;
    if (token == ">=")
        return TokenType::OperatorGreaterEqual;
    if (token == "**")
        return TokenType::OperatorPower;
    if (token == "&")
        return TokenType::OperatorBitwiseAnd;
    if (token == "|")
        return TokenType::OperatorBitwiseOr;
    if (token == "^")
        return TokenType::OperatorBitwiseXor;
    if (token == "~")
        return TokenType::OperatorBitwiseNot;
    if (token == "<<")
        return TokenType::OperatorShiftLeft;
    if (token == ">>")
        return TokenType::OperatorShiftRight;
    if (token == "+=")
        return TokenType::OperatorAssignAdd;
    if (token == "-=")
        return TokenType::OperatorAssignSubtract;
    if (token == "*=")
        return TokenType::OperatorAssignMultiply;
    if (token == "/=")
        return TokenType::OperatorAssignDivide;
    if (token == "%=")
        return TokenType::OperatorAssignModulo;
    if (token == "&=")
        return TokenType::OperatorAssignBitwiseAnd;
    if (token == "|=")
        return TokenType::OperatorAssignBitwiseOr;
    if (token == "^=")
        return TokenType::OperatorAssignBitwiseXor;
    if (token == "~=")
        return TokenType::OperatorAssignBitwiseNot;
    if (token == "<<=")
        return TokenType::OperatorAssignShiftLeft;
    if (token == ">>=")
        return TokenType::OperatorAssignShiftRight;
    if (token == "**=")
        return TokenType::OperatorAssignPower;
    if (token == "++")
        return TokenType::OperatorIncrement;
    if (token == "--")
        return TokenType::OperatorDecrement;
    if (token == "??")
        return TokenType::OperatorNullCoalesce;
    if (token == "?.")
        return TokenType::OperatorOptionalChaining;
    if (token == "...")
        return TokenType::OperatorSpread;
    if (token == "..=")
        return TokenType::OperatorRangeInclusive;
    if (token == "..")
        return TokenType::OperatorRangeExclusive;
    if (token == "|>")
        return TokenType::OperatorPipeline;

    // Delimiters
    if (token == ";")
        return TokenType::DelimiterSemicolon;
    if (token == ",")
        return TokenType::DelimiterComma;
    if (token == ".")
        return TokenType::DelimiterDot;
    if (token == ":")
        return TokenType::DelimiterColon;
    if (token == "(")
        return TokenType::DelimiterOpenParen;
    if (token == ")")
        return TokenType::DelimiterCloseParen;
    if (token == "{")
        return TokenType::DelimiterOpenBrace;
    if (token == "}")
        return TokenType::DelimiterCloseBrace;
    if (token == "[")
        return TokenType::DelimiterOpenBracket;
    if (token == "]")
        return TokenType::DelimiterCloseBracket;
    if (token == "::")
        return TokenType::DelimiterDoubleColon;
    if (token == "->")
        return TokenType::DelimiterArrow;
    if (token == "=>")
        return TokenType::DelimiterFatArrow;
    if (token == "`")
        return TokenType::DelimiterBacktick;

    // Literals
    if (token == "null" || token == "nil" || token == "None")
        return TokenType::LiteralNull;

    // Check for numeric literals
    if (token.starts_with("0b") && token.find_first_not_of("01", 2) == std::string::npos)
        return TokenType::LiteralBinary;
    if (token.starts_with("0x") && token.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos)
        return TokenType::LiteralHex;
    if (token.starts_with("0o") && token.find_first_not_of("01234567", 2) == std::string::npos)
        return TokenType::LiteralOctal;
    if (token.find_first_not_of("0123456789") == std::string::npos)
        return TokenType::LiteralNumber;
    if (token.find_first_not_of("0123456789.eE+-") == std::string::npos &&
        token.find_first_of(".eE") != std::string::npos)
        return TokenType::LiteralFloat;
    if (token.find_first_not_of("0123456789") == token.length() - 1 && token.back() == 'n')
        return TokenType::LiteralBigInt;

    // String and character literals
    if (token.size() >= 2 && token.front() == '"' && token.back() == '"')
        return TokenType::LiteralString;
    if (token.size() >= 2 && token.front() == '\'' && token.back() == '\'')
        return TokenType::LiteralChar;
    if (token.size() >= 2 && token.front() == '`' && token.back() == '`')
        return TokenType::LiteralTemplate;
    if (token.size() >= 2 && ((token.front() == '/' && token.back() == '/' && token.size() > 2) ||
                              (token.length() > 2 && token[0] == '/' && token[token.length() - 1] == '/' &&
                               token.find_first_of("*/+") == std::string::npos)))
        return TokenType::LiteralRegex;

    // Boolean literals
    if (token == "true" || token == "false")
        return TokenType::LiteralBoolean;

    // Date literals (simple ISO format check)
    if (token.size() == 10 && token[4] == '-' && token[7] == '-' && isdigit(token[0]) && isdigit(token[1]) &&
        isdigit(token[2]) && isdigit(token[3]) && isdigit(token[5]) && isdigit(token[6]) && isdigit(token[8]) &&
        isdigit(token[9]))
        return TokenType::LiteralDate;

    // Preprocessor directives
    if (token == "#include")
        return TokenType::PreprocessorInclude;
    if (token == "#define")
        return TokenType::PreprocessorDefine;
    if (token == "#if")
        return TokenType::PreprocessorIf;
    if (token == "#else")
        return TokenType::PreprocessorElse;
    if (token == "#endif")
        return TokenType::PreprocessorEndif;

    // Meta-programming
    if (token == "quote")
        return TokenType::MetaQuote;
    if (token == "unquote")
        return TokenType::MetaUnquote;
    if (token == "splice")
        return TokenType::MetaSplice;
    if (token == "macro")
        return TokenType::MetaMacro;

    // Comments
    if (token.size() >= 2 && token.substr(0, 2) == "//")
        return TokenType::CommentLine;
    if (token.size() >= 1 && token[0] == '#')
        return TokenType::CommentLine;
    if (token.size() >= 4 && token.substr(0, 2) == "/*" && token.substr(token.size() - 2) == "*/")
        return TokenType::CommentBlock;

    // Whitespace
    if (token == "\n")
        return TokenType::Newline;
    if (token == "\t")
        return TokenType::Tab;
    if (token == "\r")
        return TokenType::CarriageReturn;
    if (token == " ")
        return TokenType::Space;
    if (token.find_first_not_of(" \t\r\n") == std::string::npos)
        return TokenType::Whitespace;

    // Default to identifier
    return TokenType::Identifier;
}

Variant parseValue(const std::string& token, TokenType type) {
    try {
        size_t equalsPos;

        switch (type) {
            case TokenType::CLIFlag:
                return true;
            case TokenType::CLIOption:
                equalsPos = token.find('=');
                if (equalsPos != std::string::npos && equalsPos < token.length() - 1)
                    return token.substr(equalsPos + 1);
                return true;
            case TokenType::LiteralNumber:
                return std::stoi(token);
            case TokenType::LiteralFloat:
                return std::stod(token);
            case TokenType::LiteralString:
                return std::string(token.substr(1, token.length() - 2));
            case TokenType::LiteralBoolean:
                return token == "true";
            case TokenType::LiteralChar:
                return std::string(token.substr(1, token.length() - 2));
            case TokenType::LiteralBinary:
                return std::stoi(token.substr(2), nullptr, 2);
            case TokenType::LiteralHex:
                return std::stoi(token.substr(2), nullptr, 16);
            case TokenType::LiteralOctal:
                return std::stoi(token.substr(2), nullptr, 8);
            case TokenType::LiteralNull:
                return nullptr;
            case TokenType::LiteralTemplate:
                return std::string(token.substr(1, token.length() - 2));
            case TokenType::LiteralRegex:
                return std::string(token.substr(1, token.length() - 2));
            case TokenType::LiteralDate:
                return token; // Store as string, could be parsed later
            case TokenType::LiteralBigInt:
                return token.substr(0, token.length() - 1); // Remove 'n' suffix and store as string
            default:
                return token;
        }
    } catch (const std::exception& e) {
        FABRIC_LOG_ERROR("Error parsing value: {}", e.what());
        return nullptr;
    }
}
} // namespace fabric
