#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace mixmate
{

namespace
{
    const juce::Identifier settingsType ("Settings");
    const juce::Identifier hp1Prop ("headphone1");
    const juce::Identifier hp2Prop ("headphone2");
    const juce::Identifier keyHeadphonesProp ("keyHeadphones");
    const juce::Identifier keyMonitorProp ("keyMonitor");
    const juce::Identifier keySoloProp ("keySolo");
    const juce::Identifier uiScaleProp ("uiScale");
    const juce::Identifier exportBypassProp ("bypassWhenExporting");

    const char* const defaultHeadphone1 = "Sennheiser HD 600";
    const char* const defaultHeadphone2 = "Beyerdynamic DT 770 Pro (80 Ohm)";

    const char* const cycleParamIDs[3] = { ParamIDs::cycleHeadphones, ParamIDs::cycleMonitor, ParamIDs::cycleSolo };

    const juce::Identifier& keyProp (CycleAction action)
    {
        switch (action)
        {
            case CycleAction::monitor:  return keyMonitorProp;
            case CycleAction::solo:     return keySoloProp;
            case CycleAction::headphones:
            default:                    return keyHeadphonesProp;
        }
    }

    juce::KeyPress defaultKey (CycleAction action)
    {
        switch (action)
        {
            case CycleAction::monitor:  return juce::KeyPress ('m');
            case CycleAction::solo:     return juce::KeyPress ('b');
            case CycleAction::headphones:
            default:                    return juce::KeyPress ('n');
        }
    }

    juce::PropertiesFile::Options defaultsFileOptions()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "MixMate";
        options.folderName = "MixMate";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        return options;
    }

    juce::NormalisableRange<float> frequencyRange (float lo, float hi)
    {
        juce::NormalisableRange<float> range (lo, hi);
        range.setSkewForCentre (std::sqrt (lo * hi));
        return range;
    }

    juce::String formatHz (float value, int)
    {
        return value >= 1000.0f ? juce::String (value / 1000.0f, 1) + " kHz"
                                : juce::String (juce::roundToInt (value)) + " Hz";
    }
}

//==============================================================================
MixMateProcessor::MixMateProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MixMate", createLayout())
{
    twoModeParam    = apvts.getRawParameterValue (ParamIDs::twoMode);
    headphonesParam = apvts.getRawParameterValue (ParamIDs::headphones);
    monitorParam    = apvts.getRawParameterValue (ParamIDs::monitor);
    soloParam       = apvts.getRawParameterValue (ParamIDs::solo);
    strengthParam   = apvts.getRawParameterValue (ParamIDs::strength);
    levelMatchParam = apvts.getRawParameterValue (ParamIDs::levelMatch);
    xoverParams[0]  = apvts.getRawParameterValue (ParamIDs::xSub);
    xoverParams[1]  = apvts.getRawParameterValue (ParamIDs::xBass);
    xoverParams[2]  = apvts.getRawParameterValue (ParamIDs::xHigh);

    applySettingsDefaults();

    // A saved default makes every new MixMate start with your own setup.
    juce::PropertiesFile defaults (defaultsFileOptions());
    if (auto xml = defaults.getXmlValue ("state"))
        applyStateXml (*xml);
    else
        loadCurvesFromSettings();

    syncCycleTriggers();
    startTimerHz (30);
}

MixMateProcessor::~MixMateProcessor()
{
    stopTimer();
}

juce::AudioProcessorValueTreeState::ParameterLayout MixMateProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::twoMode, 1 }, "Two Headphones", true));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::headphones, 1 }, "Headphones",
                                                              StringArray { "Off", "Headphone 1", "Headphone 2" }, 0));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::monitor, 1 }, "Monitor",
                                                              StringArray { "Stereo", "Mid", "Side" }, 0));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::solo, 1 }, "Solo",
                                                              StringArray { "Off", "Sub", "Bass", "Mids", "Highs" }, 0));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::strength, 1 }, "Strength",
                                                             NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
                                                             AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::levelMatch, 1 }, "Level Match", true));

    auto hzAttributes = AudioParameterFloatAttributes().withStringFromValueFunction (formatHz);
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::xSub, 1 }, "Sub / Bass",
                                                             frequencyRange (40.0f, 200.0f), 100.0f, hzAttributes));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::xBass, 1 }, "Bass / Mids",
                                                             frequencyRange (150.0f, 1000.0f), 250.0f, hzAttributes));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::xHigh, 1 }, "Mids / Highs",
                                                             frequencyRange (1000.0f, 12000.0f), 4000.0f, hzAttributes));

    // Map these to a key or MIDI button in your DAW: every change moves to the next option.
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::cycleHeadphones, 1 }, "Cycle Headphones", false));
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::cycleMonitor, 1 }, "Cycle Monitor", false));
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::cycleSolo, 1 }, "Cycle Solo", false));

    return { params.begin(), params.end() };
}

bool MixMateProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void MixMateProcessor::EqChain::update (const EqCurve& curve, double sampleRate, float strength, bool levelMatch)
{
    numBands = curve.numBands;
    for (int i = 0; i < numBands; ++i)
        coeffs[(size_t) i] = BiquadCoeffs::make (curve.bands[(size_t) i], sampleRate, strength);

    gain.setTargetValue (levelMatch ? juce::Decibels::decibelsToGain (curve.preampDb * strength) : 1.0f);
}

float MixMateProcessor::EqChain::process (int channel, float input) noexcept
{
    double x = input;
    auto& s = state[channel];

    for (int i = 0; i < numBands; ++i)
    {
        const auto& c = coeffs[(size_t) i];
        const double y = c.b0 * x + s[i][0];
        s[i][0] = c.b1 * x - c.a1 * y + s[i][1];
        s[i][1] = c.b2 * x - c.a2 * y;
        x = y;
    }

    return (float) x;
}

void MixMateProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    for (auto& chain : chains)
    {
        std::memset (chain.state, 0, sizeof (chain.state));
        chain.gain.reset (sampleRate, 0.05);
    }

    appliedStrength = -1.0f;   // force coefficient update
    smoothedStrength = strengthParam->load() / 100.0f;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    for (int i = 0; i < 3; ++i)
    {
        crossovers[i].prepare (spec);
        smoothedXover[i] = xoverParams[i]->load();
        crossovers[i].setCutoffFrequency (smoothedXover[i]);
    }

    auto resetGains = [sampleRate] (auto& gains, int count, int selected)
    {
        for (int i = 0; i < count; ++i)
        {
            gains[i].reset (sampleRate, 0.03);
            gains[i].setCurrentAndTargetValue (i == selected ? 1.0f : 0.0f);
        }
    };

    resetGains (headphoneGains, 3, 0);
    resetGains (monitorGains, 3, 0);
    resetGains (soloGains, 5, 0);
}

void MixMateProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (2, buffer.getNumChannels());

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // Keep the correction out of bounces and exports.
    if (isNonRealtime() && bypassWhenExporting.load())
        return;

    if (numSamples == 0 || numChannels == 0)
        return;

    // Pick up new headphone curves from the editor.
    if (curveVersion.load() != seenCurveVersion)
    {
        const juce::SpinLock::ScopedTryLockType lock (curveLock);
        if (lock.isLocked())
        {
            activeCurves[0] = pendingCurves[0];
            activeCurves[1] = pendingCurves[1];
            seenCurveVersion = curveVersion.load();
            appliedStrength = -1.0f;
        }
    }

    // Strength glides per block; coefficients are recalculated while it moves.
    const float blockSeconds = (float) numSamples / (float) currentSampleRate;
    const float glide = 1.0f - std::exp (-blockSeconds / 0.05f);
    const float targetStrength = strengthParam->load() / 100.0f;
    smoothedStrength += (targetStrength - smoothedStrength) * glide;
    if (std::abs (targetStrength - smoothedStrength) < 0.0005f)
        smoothedStrength = targetStrength;

    const bool levelMatch = levelMatchParam->load() > 0.5f;
    if (! juce::exactlyEqual (smoothedStrength, appliedStrength) || levelMatch != appliedLevelMatch)
    {
        for (int i = 0; i < 2; ++i)
            chains[i].update (activeCurves[i], currentSampleRate, smoothedStrength, levelMatch);
        appliedStrength = smoothedStrength;
        appliedLevelMatch = levelMatch;
    }

    // Crossovers, kept in order so the bands never overlap.
    float xTargets[3] = { xoverParams[0]->load(), xoverParams[1]->load(), xoverParams[2]->load() };
    xTargets[1] = juce::jmax (xTargets[1], xTargets[0] * 1.25f);
    xTargets[2] = juce::jmax (xTargets[2], xTargets[1] * 1.25f);
    for (int i = 0; i < 3; ++i)
    {
        if (! juce::exactlyEqual (smoothedXover[i], xTargets[i]))
        {
            smoothedXover[i] += (xTargets[i] - smoothedXover[i]) * glide;
            if (std::abs (xTargets[i] - smoothedXover[i]) < 0.5f)
                smoothedXover[i] = xTargets[i];
            crossovers[i].setCutoffFrequency (smoothedXover[i]);
        }
    }

    // Selected options, crossfaded so switching never clicks.
    int headphoneState = juce::roundToInt (headphonesParam->load());
    if (headphoneState == headphones2 && twoModeParam->load() < 0.5f)
        headphoneState = headphones1;
    const int monitorState = juce::roundToInt (monitorParam->load());
    const int soloState = juce::roundToInt (soloParam->load());

    for (int i = 0; i < 3; ++i) headphoneGains[i].setTargetValue (i == headphoneState ? 1.0f : 0.0f);
    for (int i = 0; i < 3; ++i) monitorGains[i].setTargetValue (i == monitorState ? 1.0f : 0.0f);
    for (int i = 0; i < 5; ++i) soloGains[i].setTargetValue (i == soloState ? 1.0f : 0.0f);

    float* left = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        const float gOff = headphoneGains[0].getNextValue();
        const float g1 = headphoneGains[1].getNextValue();
        const float g2 = headphoneGains[2].getNextValue();
        const float c1 = chains[0].gain.getNextValue();
        const float c2 = chains[1].gain.getNextValue();

        float out[2];
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float x = ch == 0 ? left[n] : right[n];
            out[ch] = gOff * x
                    + g1 * c1 * chains[0].process (ch, x)
                    + g2 * c2 * chains[1].process (ch, x);
        }

        // Mono / mid / side.
        const float gStereo = monitorGains[0].getNextValue();
        const float gMid = monitorGains[1].getNextValue();
        const float gSide = monitorGains[2].getNextValue();

        if (numChannels > 1)
        {
            const float mid = 0.5f * (out[0] + out[1]);
            const float side = 0.5f * (out[0] - out[1]);
            const float shared = gMid * mid + gSide * side;
            out[0] = gStereo * out[0] + shared;
            out[1] = gStereo * out[1] + shared;
        }
        else
        {
            out[0] *= gStereo + gMid;
        }

        // Band solo.
        const float sOff = soloGains[0].getNextValue();
        const float sSub = soloGains[1].getNextValue();
        const float sBass = soloGains[2].getNextValue();
        const float sMids = soloGains[3].getNextValue();
        const float sHighs = soloGains[4].getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float sub, rest1, bass, rest2, mids, highs;
            crossovers[0].processSample (ch, out[ch], sub, rest1);
            crossovers[1].processSample (ch, rest1, bass, rest2);
            crossovers[2].processSample (ch, rest2, mids, highs);

            out[ch] = sOff * out[ch] + sSub * sub + sBass * bass + sMids * mids + sHighs * highs;
        }

        left[n] = out[0];
        if (right != nullptr)
            right[n] = out[1];
    }

    for (auto& c : crossovers)
        c.snapToZero();
}

//==============================================================================
juce::ValueTree MixMateProcessor::getSettings()
{
    return apvts.state.getOrCreateChildWithName (settingsType, nullptr);
}

juce::ValueTree MixMateProcessor::getSettings() const
{
    return apvts.state.getChildWithName (settingsType);
}

void MixMateProcessor::applySettingsDefaults()
{
    auto settings = getSettings();
    auto setIfMissing = [&settings] (const juce::Identifier& id, const juce::var& value)
    {
        if (! settings.hasProperty (id))
            settings.setProperty (id, value, nullptr);
    };

    setIfMissing (hp1Prop, defaultHeadphone1);
    setIfMissing (hp2Prop, defaultHeadphone2);
    setIfMissing (keyHeadphonesProp, defaultKey (CycleAction::headphones).getTextDescription());
    setIfMissing (keyMonitorProp, defaultKey (CycleAction::monitor).getTextDescription());
    setIfMissing (keySoloProp, defaultKey (CycleAction::solo).getTextDescription());
    setIfMissing (uiScaleProp, 1.0f);
    setIfMissing (exportBypassProp, true);

    bypassWhenExporting = (bool) settings[exportBypassProp];
}

void MixMateProcessor::loadCurvesFromSettings()
{
    auto settings = getSettings();
    const juce::Identifier* props[2] = { &hp1Prop, &hp2Prop };
    const char* fallbacks[2] = { defaultHeadphone1, defaultHeadphone2 };

    for (int slot = 0; slot < 2; ++slot)
    {
        auto index = database->indexOf (settings[*props[slot]].toString());
        if (index < 0)
            index = database->indexOf (fallbacks[slot]);
        setHeadphone (slot, index);
    }
}

void MixMateProcessor::setHeadphone (int slot, int databaseIndex)
{
    jassert (slot == 0 || slot == 1);
    if (databaseIndex < 0 || databaseIndex >= database->size())
        return;

    const auto& hp = database->get (databaseIndex);
    getSettings().setProperty (slot == 0 ? hp1Prop : hp2Prop, hp.name, nullptr);
    headphoneIndex[slot] = databaseIndex;

    {
        const juce::SpinLock::ScopedLockType lock (curveLock);
        pendingCurves[slot] = hp.curve;
    }
    ++curveVersion;
}

int MixMateProcessor::getChoice (const char* paramID) const
{
    return juce::roundToInt (apvts.getRawParameterValue (paramID)->load());
}

void MixMateProcessor::setParameter (const char* paramID, float plainValue)
{
    if (auto* param = apvts.getParameter (paramID))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
        param->endChangeGesture();
    }
}

void MixMateProcessor::cycle (CycleAction action)
{
    switch (action)
    {
        case CycleAction::headphones:
        {
            const int count = getChoice (ParamIDs::twoMode) != 0 ? 3 : 2;
            setParameter (ParamIDs::headphones, (float) ((getChoice (ParamIDs::headphones) + 1) % count));
            break;
        }
        case CycleAction::monitor:
            setParameter (ParamIDs::monitor, (float) ((getChoice (ParamIDs::monitor) + 1) % 3));
            break;
        case CycleAction::solo:
            setParameter (ParamIDs::solo, (float) ((getChoice (ParamIDs::solo) + 1) % 5));
            break;
    }
}

void MixMateProcessor::syncCycleTriggers()
{
    for (int i = 0; i < 3; ++i)
        lastCycleValue[i] = apvts.getRawParameterValue (cycleParamIDs[i])->load() > 0.5f;
}

void MixMateProcessor::timerCallback()
{
    // Each change of a trigger parameter (from a DAW key or MIDI mapping) is one step.
    const CycleAction actions[3] = { CycleAction::headphones, CycleAction::monitor, CycleAction::solo };
    for (int i = 0; i < 3; ++i)
    {
        const bool value = apvts.getRawParameterValue (cycleParamIDs[i])->load() > 0.5f;
        if (value != lastCycleValue[i])
        {
            lastCycleValue[i] = value;
            cycle (actions[i]);
        }
    }

    // Leaving two-headphone mode while on headphone 2 falls back to headphone 1.
    if (getChoice (ParamIDs::twoMode) == 0 && getChoice (ParamIDs::headphones) == headphones2)
        setParameter (ParamIDs::headphones, (float) headphones1);
}

//==============================================================================
juce::KeyPress MixMateProcessor::getKey (CycleAction action) const
{
    auto key = juce::KeyPress::createFromDescription (getSettings()[keyProp (action)].toString());
    return key.isValid() ? key : defaultKey (action);
}

void MixMateProcessor::setKey (CycleAction action, const juce::KeyPress& key)
{
    getSettings().setProperty (keyProp (action), key.getTextDescription(), nullptr);
}

float MixMateProcessor::getUiScale() const
{
    return juce::jlimit (1.0f, 2.0f, (float) getSettings().getProperty (uiScaleProp, 1.0f));
}

void MixMateProcessor::setUiScale (float scale)
{
    getSettings().setProperty (uiScaleProp, scale, nullptr);
}

bool MixMateProcessor::getBypassWhenExporting() const
{
    return bypassWhenExporting.load();
}

void MixMateProcessor::setBypassWhenExporting (bool shouldBypass)
{
    bypassWhenExporting = shouldBypass;
    getSettings().setProperty (exportBypassProp, shouldBypass, nullptr);
}

void MixMateProcessor::saveAsDefault()
{
    juce::PropertiesFile defaults (defaultsFileOptions());
    if (auto xml = apvts.copyState().createXml())
    {
        defaults.setValue ("state", xml.get());
        defaults.saveIfNeeded();
    }
}

bool MixMateProcessor::hasSavedDefault() const
{
    juce::PropertiesFile defaults (defaultsFileOptions());
    return defaults.containsKey ("state");
}

void MixMateProcessor::resetToFactory()
{
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
            ranged->endChangeGesture();
        }

    getSettings().removeAllProperties (nullptr);
    applySettingsDefaults();
    loadCurvesFromSettings();
    syncCycleTriggers();

    juce::PropertiesFile defaults (defaultsFileOptions());
    defaults.removeValue ("state");
    defaults.saveIfNeeded();
}

//==============================================================================
void MixMateProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MixMateProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        applyStateXml (*xml);
}

void MixMateProcessor::applyStateXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (xml));
    applySettingsDefaults();
    loadCurvesFromSettings();
    syncCycleTriggers();
}

juce::AudioProcessorEditor* MixMateProcessor::createEditor()
{
    return new MixMateEditor (*this);
}

} // namespace mixmate

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new mixmate::MixMateProcessor();
}
