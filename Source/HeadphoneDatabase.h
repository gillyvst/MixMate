#pragma once

#include <juce_core/juce_core.h>
#include <unordered_map>
#include "EqMath.h"

namespace mixmate
{

struct Headphone
{
    juce::String name;
    juce::String source;   // who measured it, e.g. "oratory1990"
    juce::String type;     // "over-ear", "in-ear" or "earbud"
    EqCurve curve;
};

// All AutoEQ curves bundled with the plugin. Loaded once and shared by every instance.
class HeadphoneDatabase
{
public:
    HeadphoneDatabase();

    int size() const                              { return (int) headphones.size(); }
    const Headphone& get (int index) const        { return headphones[(size_t) index]; }

    // Index of a headphone by exact name, or -1.
    int indexOf (const juce::String& name) const;

    // Indices of the popular headphones, in display order.
    const std::vector<int>& getPopular() const    { return popular; }

private:
    std::vector<Headphone> headphones;   // sorted A-Z
    std::vector<int> popular;
    std::unordered_map<juce::String, int> byName;
};

} // namespace mixmate
