#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "HeadphoneDatabase.h"

namespace mixmate
{

namespace ParamIDs
{
    inline constexpr const char* twoMode    = "twoMode";
    inline constexpr const char* headphones = "headphones";
    inline constexpr const char* monitor    = "monitor";
    inline constexpr const char* solo       = "solo";
    inline constexpr const char* strength   = "strength";
    inline constexpr const char* levelMatch = "levelMatch";
    inline constexpr const char* xSub       = "xoverSub";
    inline constexpr const char* xBass      = "xoverBass";
    inline constexpr const char* xHigh      = "xoverHigh";
    inline constexpr const char* cycleHeadphones = "cycleHeadphones";
    inline constexpr const char* cycleMonitor    = "cycleMonitor";
    inline constexpr const char* cycleSolo       = "cycleSolo";
}

enum HeadphoneState { headphonesOff, headphones1, headphones2 };
enum MonitorState   { monitorStereo, monitorMid, monitorSide };
enum SoloState      { soloOff, soloSub, soloBass, soloMids, soloHighs };

// The three things a key can cycle through.
enum class CycleAction { headphones, monitor, solo };

class MixMateProcessor : public juce::AudioProcessor,
                         private juce::Timer
{
public:
    MixMateProcessor();
    ~MixMateProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const juce::String getName() const override         { return "MixMate"; }
    bool acceptsMidi() const override                   { return false; }
    bool producesMidi() const override                  { return false; }
    bool isMidiEffect() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 0.0; }

    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Everything below is for the message thread (the editor).
    juce::AudioProcessorValueTreeState apvts;
    juce::SharedResourcePointer<HeadphoneDatabase> database;

    int getHeadphoneIndex (int slot) const              { return headphoneIndex[slot].load(); }
    void setHeadphone (int slot, int databaseIndex);

    int getChoice (const char* paramID) const;
    void setParameter (const char* paramID, float plainValue);
    void cycle (CycleAction action);

    juce::KeyPress getKey (CycleAction action) const;
    void setKey (CycleAction action, const juce::KeyPress& key);

    float getUiScale() const;
    void setUiScale (float scale);

    bool getBypassWhenExporting() const;
    void setBypassWhenExporting (bool shouldBypass);

    void saveAsDefault();
    void resetToFactory();
    bool hasSavedDefault() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::ValueTree getSettings();
    juce::ValueTree getSettings() const;
    void applySettingsDefaults();
    void loadCurvesFromSettings();
    void applyStateXml (const juce::XmlElement& xml);
    void syncCycleTriggers();
    void timerCallback() override;

    // Curves handed from the message thread to the audio thread.
    juce::SpinLock curveLock;
    EqCurve pendingCurves[2];
    std::atomic<int> curveVersion { 0 };
    std::atomic<int> headphoneIndex[2] { { -1 }, { -1 } };

    // Last seen values of the key-map trigger parameters.
    bool lastCycleValue[3] { false, false, false };

    //==============================================================================
    // Audio thread state.
    struct EqChain
    {
        std::array<BiquadCoeffs, EqCurve::maxBands> coeffs;
        double state[2][EqCurve::maxBands][2] {};
        int numBands = 0;
        juce::SmoothedValue<float> gain { 1.0f };

        void update (const EqCurve& curve, double sampleRate, float strength, bool levelMatch);
        float process (int channel, float x) noexcept;
    };

    EqChain chains[2];
    EqCurve activeCurves[2];
    int seenCurveVersion = -1;
    float smoothedStrength = 1.0f;
    float appliedStrength = -1.0f;
    bool appliedLevelMatch = true;

    juce::dsp::LinkwitzRileyFilter<float> crossovers[3];
    float smoothedXover[3] { 100.0f, 250.0f, 4000.0f };

    juce::SmoothedValue<float> headphoneGains[3], monitorGains[3], soloGains[5];
    double currentSampleRate = 44100.0;

    std::atomic<float>* twoModeParam = nullptr;
    std::atomic<float>* headphonesParam = nullptr;
    std::atomic<float>* monitorParam = nullptr;
    std::atomic<float>* soloParam = nullptr;
    std::atomic<float>* strengthParam = nullptr;
    std::atomic<float>* levelMatchParam = nullptr;
    std::atomic<float>* xoverParams[3] {};
    std::atomic<bool> bypassWhenExporting { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixMateProcessor)
};

} // namespace mixmate
