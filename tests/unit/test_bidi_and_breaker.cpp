#include "north/text/bidi.hpp"
#include "north/text/line_breaker.hpp"

#include <gtest/gtest.h>

namespace north::text::testing {

TEST(BidiTest, AnalyzesPureLtrText) {
    auto runs = Bidi::analyze("North Toolkit modern C++20 UI framework");
    ASSERT_FALSE(runs.empty());
    EXPECT_EQ(runs.size(), 1u);
    EXPECT_EQ(runs[0].direction, TextDirection::LTR);
    EXPECT_EQ(runs[0].level, 0u);
}

TEST(BidiTest, AnalyzesPureRtlText) {
    // Arabic "مرحبا" (Marhaban)
    std::string_view arabic_text = "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7";
    auto runs = Bidi::analyze(arabic_text);
    ASSERT_FALSE(runs.empty());
    EXPECT_EQ(runs[0].direction, TextDirection::RTL);
    EXPECT_EQ(runs[0].level & 1, 1u);
}

TEST(BidiTest, SplitsMixedBidirectionalText) {
    // English + Arabic + English
    std::string mixed = "Hello \xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7 World";
    auto runs = Bidi::analyze(mixed);
    ASSERT_GE(runs.size(), 2u);

    bool has_ltr = false;
    bool has_rtl = false;
    for (const auto& r : runs) {
        if (r.direction == TextDirection::LTR)
            has_ltr = true;
        if (r.direction == TextDirection::RTL)
            has_rtl = true;
    }
    EXPECT_TRUE(has_ltr);
    EXPECT_TRUE(has_rtl);
}

TEST(LineBreakerTest, IdentifiesBreakOpportunitiesOnSpaces) {
    std::string text = "First Second Third";
    auto breaks = LineBreaker::get_breaks(text);
    ASSERT_EQ(breaks.size(), text.size());

    // Characters inside "First" should be NoBreak
    EXPECT_EQ(breaks[0], BreakAction::NoBreak);
    EXPECT_EQ(breaks[1], BreakAction::NoBreak);

    // Space character position or boundary allows break
    size_t space1 = text.find(' ');
    ASSERT_NE(space1, std::string::npos);
    EXPECT_TRUE(breaks[space1] == BreakAction::AllowBreak ||
                breaks[space1 + 1] == BreakAction::AllowBreak);
}

TEST(LineBreakerTest, MandatoryNewlineBreaks) {
    std::string text = "Line1\nLine2";
    auto breaks = LineBreaker::get_breaks(text);
    ASSERT_EQ(breaks.size(), text.size());

    size_t nl = text.find('\n');
    ASSERT_NE(nl, std::string::npos);
    EXPECT_EQ(breaks[nl], BreakAction::MustBreak);
}

} // namespace north::text::testing
