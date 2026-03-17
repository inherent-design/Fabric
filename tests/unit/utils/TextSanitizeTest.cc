#include "fabric/utils/TextSanitize.hh"

#include <gtest/gtest.h>

using namespace fabric::utils;

// --- rmlEscape ---

TEST(RmlEscape, EmptyString) {
    EXPECT_EQ(rmlEscape(""), "");
}

TEST(RmlEscape, NoSpecialChars) {
    EXPECT_EQ(rmlEscape("Hello World 123"), "Hello World 123");
}

TEST(RmlEscape, LessThan) {
    EXPECT_EQ(rmlEscape("a < b"), "a &lt; b");
}

TEST(RmlEscape, GreaterThan) {
    EXPECT_EQ(rmlEscape("a > b"), "a &gt; b");
}

TEST(RmlEscape, Ampersand) {
    EXPECT_EQ(rmlEscape("a & b"), "a &amp; b");
}

TEST(RmlEscape, DoubleQuote) {
    EXPECT_EQ(rmlEscape("say \"hi\""), "say &quot;hi&quot;");
}

TEST(RmlEscape, SingleQuote) {
    EXPECT_EQ(rmlEscape("it's"), "it&#39;s");
}

TEST(RmlEscape, AllSpecialChars) {
    EXPECT_EQ(rmlEscape("<div class=\"x\">&'test</div>"), "&lt;div class=&quot;x&quot;&gt;&amp;&#39;test&lt;/div&gt;");
}

TEST(RmlEscape, HtmlInjection) {
    EXPECT_EQ(rmlEscape("<script>alert(1)</script>"), "&lt;script&gt;alert(1)&lt;/script&gt;");
}

TEST(RmlEscape, UnicodePassthrough) {
    std::string input = "Hello \xC3\xA9\xC3\xA8";
    EXPECT_EQ(rmlEscape(input), input);
}

TEST(RmlEscape, EntityNotDoubleEscaped) {
    EXPECT_EQ(rmlEscape("&amp;"), "&amp;amp;");
}

TEST(RmlEscape, OnlySpecialChars) {
    EXPECT_EQ(rmlEscape("<>&\"'"), "&lt;&gt;&amp;&quot;&#39;");
}

// --- sqlEscape ---

TEST(SqlEscape, EmptyString) {
    EXPECT_EQ(sqlEscape(""), "");
}

TEST(SqlEscape, NoSpecialChars) {
    EXPECT_EQ(sqlEscape("hello world"), "hello world");
}

TEST(SqlEscape, SingleQuote) {
    EXPECT_EQ(sqlEscape("it's"), "it''s");
}

TEST(SqlEscape, Backslash) {
    EXPECT_EQ(sqlEscape("path\\to"), "path\\\\to");
}

TEST(SqlEscape, MultipleSingleQuotes) {
    EXPECT_EQ(sqlEscape("can't won't"), "can''t won''t");
}

TEST(SqlEscape, Mixed) {
    EXPECT_EQ(sqlEscape("it's a \\path"), "it''s a \\\\path");
}

TEST(SqlEscape, UnicodePassthrough) {
    std::string input = "caf\xC3\xA9";
    EXPECT_EQ(sqlEscape(input), input);
}
