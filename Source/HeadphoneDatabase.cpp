#include "HeadphoneDatabase.h"
#include "BinaryData.h"

namespace mixmate
{

namespace
{
    // Shown at the top of the list, in this order.
    const char* const popularNames[] =
    {
        "Sennheiser HD 600",
        "Sennheiser HD 650",
        "Sennheiser HD 800 S",
        "Sennheiser HD 280 Pro",
        "Sennheiser HD 25",
        "Beyerdynamic DT 770 Pro (80 Ohm)",
        "Beyerdynamic DT 990 Pro",
        "Beyerdynamic DT 880 (250 Ohm)",
        "Beyerdynamic DT 1990 Pro (analytical earpads)",
        "Audio-Technica ATH-M50x",
        "Audio-Technica ATH-M40x",
        "Sony MDR-7506",
        "AKG K371",
        "AKG K702",
        "AKG K240 Studio",
        "Neumann NDH20",
        "Focal Listen Professional",
        "Apple AirPods Pro 2 (ANC mode)",
        "Apple AirPods Max",
        "Sony WH-1000XM4",
    };

    FilterType filterTypeFromCode (juce::juce_wchar c)
    {
        if (c == 'L') return FilterType::lowShelf;
        if (c == 'H') return FilterType::highShelf;
        return FilterType::peak;
    }
}

HeadphoneDatabase::HeadphoneDatabase()
{
    const juce::String text (juce::CharPointer_UTF8 (BinaryData::headphones_tsv), (size_t) BinaryData::headphones_tsvSize);

    juce::StringArray lines;
    lines.addLines (text);
    headphones.reserve ((size_t) lines.size());

    for (const auto& line : lines)
    {
        juce::StringArray cols;
        cols.addTokens (line, "\t", {});
        if (cols.size() < 5)
            continue;

        Headphone hp;
        hp.name = cols[0];
        hp.source = cols[1];
        hp.type = cols[2];
        hp.curve.preampDb = cols[3].getFloatValue();

        juce::StringArray filters;
        filters.addTokens (cols[4], ",", {});
        for (const auto& f : filters)
        {
            if (hp.curve.numBands >= EqCurve::maxBands)
                break;

            juce::StringArray parts;
            parts.addTokens (f, " ", {});
            if (parts.size() != 4)
                continue;

            auto& band = hp.curve.bands[(size_t) hp.curve.numBands++];
            band.type = filterTypeFromCode (parts[0][0]);
            band.freq = parts[1].getFloatValue();
            band.gainDb = parts[2].getFloatValue();
            band.q = juce::jmax (0.05f, parts[3].getFloatValue());
        }

        headphones.push_back (std::move (hp));
    }

    std::sort (headphones.begin(), headphones.end(), [] (const Headphone& a, const Headphone& b)
    {
        return a.name.compareNatural (b.name) < 0;
    });

    for (size_t i = 0; i < headphones.size(); ++i)
        byName[headphones[i].name] = (int) i;

    for (auto* name : popularNames)
        if (auto index = indexOf (name); index >= 0)
            popular.push_back (index);
}

int HeadphoneDatabase::indexOf (const juce::String& name) const
{
    auto it = byName.find (name);
    return it != byName.end() ? it->second : -1;
}

} // namespace mixmate
