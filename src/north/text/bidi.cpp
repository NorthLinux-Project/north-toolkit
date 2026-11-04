#include "north/text/bidi.hpp"

#include <SheenBidi/SheenBidi.h>

namespace north::text {

std::vector<BidiRun> Bidi::analyze(std::string_view utf8_text) {
    if (utf8_text.empty()) {
        return {};
    }

    SBCodepointSequence sequence{.stringEncoding = SBStringEncodingUTF8,
                                 .stringBuffer = utf8_text.data(),
                                 .stringLength = static_cast<SBUInteger>(utf8_text.size())};

    SBAlgorithmRef algorithm = SBAlgorithmCreate(&sequence);
    if (!algorithm) {
        return {BidiRun{0, utf8_text.size(), 0, TextDirection::LTR}};
    }

    SBParagraphRef paragraph = SBAlgorithmCreateParagraph(
        algorithm, 0, static_cast<SBUInteger>(utf8_text.size()), SBLevelDefaultLTR);
    if (!paragraph) {
        SBAlgorithmRelease(algorithm);
        return {BidiRun{0, utf8_text.size(), 0, TextDirection::LTR}};
    }

    SBLineRef line = SBParagraphCreateLine(paragraph, 0, static_cast<SBUInteger>(utf8_text.size()));
    if (!line) {
        SBParagraphRelease(paragraph);
        SBAlgorithmRelease(algorithm);
        return {BidiRun{0, utf8_text.size(), 0, TextDirection::LTR}};
    }

    SBUInteger count = SBLineGetRunCount(line);
    const SBRun* runs = SBLineGetRunsPtr(line);

    std::vector<BidiRun> result;
    result.reserve(count);

    for (SBUInteger i = 0; i < count; ++i) {
        BidiRun r{};
        r.start = runs[i].offset;
        r.length = runs[i].length;
        r.level = static_cast<uint8_t>(runs[i].level);
        r.direction = (runs[i].level & 1) ? TextDirection::RTL : TextDirection::LTR;
        result.push_back(r);
    }

    SBLineRelease(line);
    SBParagraphRelease(paragraph);
    SBAlgorithmRelease(algorithm);

    if (result.empty()) {
        result.push_back(BidiRun{0, utf8_text.size(), 0, TextDirection::LTR});
    }

    return result;
}

} // namespace north::text
