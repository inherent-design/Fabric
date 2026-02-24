#pragma once

#include "fabric/core/Types.hh"

namespace fabric {

// Enum for token types with payloads for literals
enum class TokenType {
    // CLI
    CLIFlag,       // --flag (valueless flag)
    CLIOption,     // --option=value or --option value
    CLIPositional, // positional argument
    CLICommand,    // command in command-based CLIs

    // Control Flow
    KeywordIf,       // if
    KeywordElse,     // else
    KeywordFor,      // for
    KeywordWhile,    // while
    KeywordReturn,   // return
    KeywordGoto,     // goto
    KeywordBreak,    // break
    KeywordContinue, // continue
    KeywordSwitch,   // switch
    KeywordCase,     // case
    KeywordDefault,  // default
    KeywordDefer,    // defer (Go-like)

    // Error Handling
    KeywordTry,     // try
    KeywordCatch,   // catch
    KeywordThrow,   // throw
    KeywordFinally, // finally
    KeywordRaise,   // raise
    KeywordAssert,  // assert

    // Data Types
    KeywordFunction, // func, def, fn, function
    KeywordStruct,   // struct
    KeywordEnum,     // enum
    KeywordArray,    // array
    KeywordMap,      // map, dict
    KeywordSet,      // set
    KeywordTuple,    // tuple
    KeywordGeneric,  // generic, template
    KeywordWhere,    // where (constraints)

    // Object-Oriented Inheritance
    KeywordClass,      // class
    KeywordInterface,  // interface
    KeywordImplements, // implements
    KeywordExtends,    // extends
    KeywordSelf,       // self
    KeywordSuper,      // super
    KeywordOverride,   // override
    KeywordAbstract,   // abstract
    KeywordVirtual,    // virtual
    KeywordDelegate,   // delegate
    KeywordEvent,      // event

    // Modules
    KeywordImport,  // import, include, use
    KeywordPackage, // package, module, namespace
    KeywordExport,  // export
    KeywordFrom,    // from

    // Declarations
    KeywordConst,  // const
    KeywordLet,    // let
    KeywordVar,    // var
    KeywordType,   // type
    KeywordMut,    // mut
    KeywordUnsafe, // unsafe
    KeywordStatic, // static

    // Memory Management
    KeywordNew,    // new
    KeywordDelete, // delete
    KeywordAlloc,  // alloc
    KeywordFree,   // free
    KeywordMove,   // move
    KeywordBorrow, // borrow (Rust-like)

    // Access Modifiers
    KeywordPublic,    // pub
    KeywordPrivate,   // priv
    KeywordProtected, // prot
    KeywordInternal,  // int
    KeywordFinal,     // final

    // Boolean Operators
    KeywordAs,  // as
    KeywordIs,  // is
    KeywordIn,  // in
    KeywordNot, // not
    KeywordAnd, // and
    KeywordOr,  // or

    // Functional Programming
    KeywordLambda,  // lambda, =>
    KeywordClosure, // closure
    KeywordCurry,   // curry
    KeywordPipe,    // pipe, |>
    KeywordCompose, // compose

    // Concurrency
    KeywordThread, // thread
    KeywordAtomic, // atomic
    KeywordSync,   // sync
    KeywordLock,   // lock
    KeywordMutex,  // mutex

    // Async
    KeywordYield, // yield
    KeywordAsync, // async
    KeywordAwait, // await

    // Operators
    OperatorPlus,             // +
    OperatorMinus,            // -
    OperatorMultiply,         // *
    OperatorDivide,           // /
    OperatorModulo,           // %
    OperatorAssign,           // =
    OperatorEqual,            // ==
    OperatorNotEqual,         // !=, <>
    OperatorLessThan,         // <
    OperatorGreaterThan,      // >
    OperatorLessEqual,        // <=
    OperatorGreaterEqual,     // >=
    OperatorPower,            // **
    OperatorBitwiseAnd,       // &
    OperatorBitwiseOr,        // |
    OperatorBitwiseXor,       // ^
    OperatorBitwiseNot,       // ~
    OperatorShiftLeft,        // <<
    OperatorShiftRight,       // >>
    OperatorAssignAdd,        // +=
    OperatorAssignSubtract,   // -=
    OperatorAssignMultiply,   // *=
    OperatorAssignDivide,     // /=
    OperatorAssignModulo,     // %=
    OperatorAssignBitwiseAnd, // &=
    OperatorAssignBitwiseOr,  // |=
    OperatorAssignBitwiseXor, // ^=
    OperatorAssignBitwiseNot, // ~=
    OperatorAssignShiftLeft,  // <<=
    OperatorAssignShiftRight, // >>=
    OperatorAssignPower,      // **=
    OperatorIncrement,        // ++
    OperatorDecrement,        // --
    OperatorNullCoalesce,     // ??
    OperatorOptionalChaining, // ?.
    OperatorSpread,           // ...
    OperatorRangeInclusive,   // ..=
    OperatorRangeExclusive,   // ..
    OperatorPipeline,         // |>

    // Delimiters
    DelimiterSemicolon,    // ;
    DelimiterComma,        // ,
    DelimiterDot,          // .
    DelimiterColon,        // :
    DelimiterOpenParen,    // (
    DelimiterCloseParen,   // )
    DelimiterOpenBrace,    // {
    DelimiterCloseBrace,   // }
    DelimiterOpenBracket,  // [
    DelimiterCloseBracket, // ]
    DelimiterDoubleColon,  // ::
    DelimiterArrow,        // ->
    DelimiterFatArrow,     // =>
    DelimiterBacktick,     // `

    // Literals
    LiteralNull,     // null, nil, None
    LiteralNumber,   // num(42)
    LiteralString,   // str("hello")
    LiteralBoolean,  // bool(true)
    LiteralFloat,    // float(3.14)
    LiteralChar,     // char('a')
    LiteralRegex,    // regex(/pattern/)
    LiteralDate,     // date(2023-01-01)
    LiteralTemplate, // template(`string ${expr}`)
    LiteralBinary,   // binary(0b101)
    LiteralHex,      // hex(0xFF)
    LiteralOctal,    // octal(0o77)
    LiteralBigInt,   // bigint(1234567890123456789n)

    // Preprocessor
    PreprocessorInclude, // #include
    PreprocessorDefine,  // #define
    PreprocessorIf,      // #if
    PreprocessorElse,    // #else
    PreprocessorEndif,   // #endif

    // Meta-programming
    MetaQuote,   // quote
    MetaUnquote, // unquote
    MetaSplice,  // splice
    MetaMacro,   // macro

    // Identifiers
    Identifier,

    // Comments
    CommentLine,  // //, #
    CommentBlock, // /* */

    // Whitespace
    Whitespace,
    Newline,
    Tab,
    CarriageReturn,
    Space,

    // End of File
    EndOfFile,
};

// Token structure with type and value
struct Token {
    TokenType type;
    Variant value;

    Token(TokenType type, Variant value = nullptr) : type(type), value(value) {}

    Token() : type(TokenType::EndOfFile), value(nullptr) {}
};
} // namespace fabric
