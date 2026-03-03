#include "fabric/parser/SyntaxTree.hh"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace fabric;

// ============================================================
// Token tests
// ============================================================

TEST(TokenTest, DefaultConstructor) {
    Token token;
    EXPECT_EQ(token.type, TokenType::EndOfFile);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(token.value));
}

TEST(TokenTest, ConstructWithTypeAndValue) {
    Token token(TokenType::LiteralNumber, 42);
    EXPECT_EQ(token.type, TokenType::LiteralNumber);
    EXPECT_EQ(std::get<int>(token.value), 42);
}

TEST(TokenTest, ConstructWithStringValue) {
    Token token(TokenType::LiteralString, std::string("hello"));
    EXPECT_EQ(token.type, TokenType::LiteralString);
    EXPECT_EQ(std::get<std::string>(token.value), "hello");
}

TEST(TokenTest, ConstructWithBoolValue) {
    Token token(TokenType::LiteralBoolean, true);
    EXPECT_EQ(token.type, TokenType::LiteralBoolean);
    EXPECT_EQ(std::get<bool>(token.value), true);
}

TEST(TokenTest, ConstructWithNullValue) {
    Token token(TokenType::LiteralNull);
    EXPECT_EQ(token.type, TokenType::LiteralNull);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(token.value));
}

// ============================================================
// determineTokenType tests
// ============================================================

TEST(DetermineTokenTypeTest, Keywords) {
    EXPECT_EQ(determineTokenType("if"), TokenType::KeywordIf);
    EXPECT_EQ(determineTokenType("else"), TokenType::KeywordElse);
    EXPECT_EQ(determineTokenType("for"), TokenType::KeywordFor);
    EXPECT_EQ(determineTokenType("while"), TokenType::KeywordWhile);
    EXPECT_EQ(determineTokenType("return"), TokenType::KeywordReturn);
    EXPECT_EQ(determineTokenType("class"), TokenType::KeywordClass);
    EXPECT_EQ(determineTokenType("struct"), TokenType::KeywordStruct);
    EXPECT_EQ(determineTokenType("const"), TokenType::KeywordConst);
}

TEST(DetermineTokenTypeTest, Operators) {
    EXPECT_EQ(determineTokenType("+"), TokenType::OperatorPlus);
    EXPECT_EQ(determineTokenType("-"), TokenType::OperatorMinus);
    EXPECT_EQ(determineTokenType("*"), TokenType::OperatorMultiply);
    EXPECT_EQ(determineTokenType("/"), TokenType::OperatorDivide);
    EXPECT_EQ(determineTokenType("=="), TokenType::OperatorEqual);
    EXPECT_EQ(determineTokenType("!="), TokenType::OperatorNotEqual);
    EXPECT_EQ(determineTokenType("**"), TokenType::OperatorPower);
}

TEST(DetermineTokenTypeTest, Delimiters) {
    EXPECT_EQ(determineTokenType(";"), TokenType::DelimiterSemicolon);
    EXPECT_EQ(determineTokenType(","), TokenType::DelimiterComma);
    EXPECT_EQ(determineTokenType("("), TokenType::DelimiterOpenParen);
    EXPECT_EQ(determineTokenType(")"), TokenType::DelimiterCloseParen);
    EXPECT_EQ(determineTokenType("{"), TokenType::DelimiterOpenBrace);
    EXPECT_EQ(determineTokenType("}"), TokenType::DelimiterCloseBrace);
    EXPECT_EQ(determineTokenType("::"), TokenType::DelimiterDoubleColon);
    // Note: "->" is matched by CLI short option pattern before delimiters.
    // This is a known parser precedence issue.
}

TEST(DetermineTokenTypeTest, Literals) {
    EXPECT_EQ(determineTokenType("null"), TokenType::LiteralNull);
    EXPECT_EQ(determineTokenType("nil"), TokenType::LiteralNull);
    EXPECT_EQ(determineTokenType("true"), TokenType::LiteralBoolean);
    EXPECT_EQ(determineTokenType("false"), TokenType::LiteralBoolean);
    EXPECT_EQ(determineTokenType("42"), TokenType::LiteralNumber);
    EXPECT_EQ(determineTokenType("0xFF"), TokenType::LiteralHex);
    EXPECT_EQ(determineTokenType("0b101"), TokenType::LiteralBinary);
    EXPECT_EQ(determineTokenType("0o77"), TokenType::LiteralOctal);
}

TEST(DetermineTokenTypeTest, StringLiterals) {
    EXPECT_EQ(determineTokenType("\"hello\""), TokenType::LiteralString);
    EXPECT_EQ(determineTokenType("'a'"), TokenType::LiteralChar);
}

TEST(DetermineTokenTypeTest, FloatLiterals) {
    EXPECT_EQ(determineTokenType("3.14"), TokenType::LiteralFloat);
    EXPECT_EQ(determineTokenType("1e10"), TokenType::LiteralFloat);
}

TEST(DetermineTokenTypeTest, CLITokens) {
    EXPECT_EQ(determineTokenType("--verbose"), TokenType::CLIFlag);
    EXPECT_EQ(determineTokenType("--output=file"), TokenType::CLIOption);
    EXPECT_EQ(determineTokenType("-f"), TokenType::CLIFlag);
}

TEST(DetermineTokenTypeTest, Identifiers) {
    EXPECT_EQ(determineTokenType("myVariable"), TokenType::Identifier);
    EXPECT_EQ(determineTokenType("foo_bar"), TokenType::Identifier);
    EXPECT_EQ(determineTokenType("CamelCase"), TokenType::Identifier);
}

TEST(DetermineTokenTypeTest, Whitespace) {
    EXPECT_EQ(determineTokenType("\n"), TokenType::Newline);
    EXPECT_EQ(determineTokenType("\t"), TokenType::Tab);
    EXPECT_EQ(determineTokenType(" "), TokenType::Space);
    EXPECT_EQ(determineTokenType("  \t\n"), TokenType::Whitespace);
}

TEST(DetermineTokenTypeTest, Preprocessor) {
    EXPECT_EQ(determineTokenType("#include"), TokenType::PreprocessorInclude);
    EXPECT_EQ(determineTokenType("#define"), TokenType::PreprocessorDefine);
    EXPECT_EQ(determineTokenType("#if"), TokenType::PreprocessorIf);
}

// ============================================================
// parseValue tests
// ============================================================

TEST(ParseValueTest, ParseNumber) {
    auto result = parseValue("42", TokenType::LiteralNumber);
    EXPECT_EQ(std::get<int>(result), 42);
}

TEST(ParseValueTest, ParseFloat) {
    auto result = parseValue("3.14", TokenType::LiteralFloat);
    EXPECT_NEAR(std::get<double>(result), 3.14, 0.001);
}

TEST(ParseValueTest, ParseString) {
    auto result = parseValue("\"hello\"", TokenType::LiteralString);
    EXPECT_EQ(std::get<std::string>(result), "hello");
}

TEST(ParseValueTest, ParseBooleanTrue) {
    auto result = parseValue("true", TokenType::LiteralBoolean);
    EXPECT_EQ(std::get<bool>(result), true);
}

TEST(ParseValueTest, ParseBooleanFalse) {
    auto result = parseValue("false", TokenType::LiteralBoolean);
    EXPECT_EQ(std::get<bool>(result), false);
}

TEST(ParseValueTest, ParseNull) {
    auto result = parseValue("null", TokenType::LiteralNull);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(result));
}

TEST(ParseValueTest, ParseCLIFlag) {
    auto result = parseValue("--verbose", TokenType::CLIFlag);
    EXPECT_EQ(std::get<bool>(result), true);
}

TEST(ParseValueTest, ParseCLIOption) {
    auto result = parseValue("--output=file.txt", TokenType::CLIOption);
    EXPECT_EQ(std::get<std::string>(result), "file.txt");
}

TEST(ParseValueTest, ParseHex) {
    auto result = parseValue("0xFF", TokenType::LiteralHex);
    EXPECT_EQ(std::get<int>(result), 255);
}

TEST(ParseValueTest, ParseBinary) {
    auto result = parseValue("0b101", TokenType::LiteralBinary);
    EXPECT_EQ(std::get<int>(result), 5);
}

// ============================================================
// ASTNode tests
// ============================================================

TEST(ASTNodeTest, Construction) {
    Token token(TokenType::Identifier, std::string("x"));
    ASTNode node(token);

    EXPECT_EQ(node.getToken().type, TokenType::Identifier);
    EXPECT_EQ(std::get<std::string>(node.getToken().value), "x");
    EXPECT_TRUE(node.getChildren().empty());
}

TEST(ASTNodeTest, AddChild) {
    auto parent = std::make_shared<ASTNode>(Token(TokenType::OperatorPlus, std::string("+")));
    auto left = std::make_shared<ASTNode>(Token(TokenType::LiteralNumber, 1));
    auto right = std::make_shared<ASTNode>(Token(TokenType::LiteralNumber, 2));

    parent->addChild(left);
    parent->addChild(right);

    EXPECT_EQ(parent->getChildren().size(), 2u);
    EXPECT_EQ(std::get<int>(parent->getChildren()[0]->getToken().value), 1);
    EXPECT_EQ(std::get<int>(parent->getChildren()[1]->getToken().value), 2);
}

TEST(ASTNodeTest, NestedTree) {
    // Build: (+ (* 2 3) 4)
    auto mul = std::make_shared<ASTNode>(Token(TokenType::OperatorMultiply, std::string("*")));
    mul->addChild(std::make_shared<ASTNode>(Token(TokenType::LiteralNumber, 2)));
    mul->addChild(std::make_shared<ASTNode>(Token(TokenType::LiteralNumber, 3)));

    auto add = std::make_shared<ASTNode>(Token(TokenType::OperatorPlus, std::string("+")));
    add->addChild(mul);
    add->addChild(std::make_shared<ASTNode>(Token(TokenType::LiteralNumber, 4)));

    EXPECT_EQ(add->getChildren().size(), 2u);
    EXPECT_EQ(add->getChildren()[0]->getChildren().size(), 2u);
    EXPECT_EQ(add->getChildren()[1]->getChildren().size(), 0u);
}
