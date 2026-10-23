// Checks MixMate's audio processing and saves screenshots of the UI.
// Usage: MixMateTests [screenshot output folder]

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

using namespace mixmate;

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  pass  " : "  FAIL  ") << what << std::endl;
        if (! ok)
            ++failures;
    }

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    void setParam (MixMateProcessor& p, const char* id, float plainValue)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
    }

    // Feeds a sine (left, right amplitudes) for a second and returns output RMS per channel
    // over the last quarter second.
    std::pair<double, double> runSine (MixMateProcessor& p, double freq, float ampL, float ampR, double seconds = 1.0)
    {
        p.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        const int total = (int) (seconds * sampleRate);
        const int measureFrom = total - (int) (0.25 * sampleRate);
        double sumL = 0, sumR = 0;
        int count = 0;

        for (int start = 0; start < total; start += blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const auto s = (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (start + i) / sampleRate);
                buffer.setSample (0, i, ampL * s);
                buffer.setSample (1, i, ampR * s);
            }
            p.processBlock (buffer, midi);
            for (int i = 0; i < blockSize; ++i)
                if (start + i >= measureFrom)
                {
                    sumL += buffer.getSample (0, i) * buffer.getSample (0, i);
                    sumR += buffer.getSample (1, i) * buffer.getSample (1, i);
                    ++count;
                }
        }

        return { std::sqrt (sumL / count), std::sqrt (sumR / count) };
    }

    double gainDb (double rms, double amp)
    {
        return juce::Decibels::gainToDecibels (rms / (amp / std::sqrt (2.0)), -200.0);
    }

    void pumpMessages (int ms)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
    }

    void saveSnapshot (juce::Component& c, const juce::File& folder, const juce::String& name)
    {
        if (folder == juce::File())
            return;
        auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 2.0f);
        auto file = folder.getChildFile (name + ".png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::cout << "  saved " << file.getFullPathName() << std::endl;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File shots = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File();
    if (shots != juce::File())
        shots.createDirectory();

    // Start without any saved default from an earlier run.
    { MixMateProcessor p; p.resetToFactory(); }

    std::cout << "Database" << std::endl;
    {
        MixMateProcessor p;
        check (p.database->size() > 5000, "loads " + juce::String (p.database->size()) + " headphones");
        check (p.database->getPopular().size() == 20, "all 20 popular headphones found");
        check (p.database->indexOf ("Sennheiser HD 650") >= 0, "finds Sennheiser HD 650");
    }

    std::cout << "Off = untouched audio" << std::endl;
    {
        MixMateProcessor p;
        auto [l, r] = runSine (p, 1000.0, 0.5f, 0.25f);
        check (std::abs (gainDb (l, 0.5f)) < 0.01 && std::abs (gainDb (r, 0.25f)) < 0.01, "left and right unchanged");
    }

    std::cout << "Headphone correction matches the AutoEQ curve" << std::endl;
    {
        MixMateProcessor p;
        const int hd650 = p.database->indexOf ("Sennheiser HD 650");
        p.setHeadphone (0, hd650);
        setParam (p, ParamIDs::headphones, headphones1);
        const auto& curve = p.database->get (hd650).curve;

        for (double f : { 40.0, 118.0, 1000.0, 3169.0, 8800.0 })
        {
            const double expected = curveResponseDb (curve, f, 1.0f, sampleRate) - curveLoudnessDb (curve, 1.0f, sampleRate);
            auto [l, r] = runSine (p, f, 0.25f, 0.25f);
            const double got = gainDb (l, 0.25f);
            check (std::abs (got - expected) < 0.1, juce::String (f) + " Hz: " + juce::String (got, 2) + " dB (expected " + juce::String (expected, 2) + ")");
        }

        setParam (p, ParamIDs::strength, 50.0f);
        const double expectedHalf = curveResponseDb (curve, 40.0, 0.5f, sampleRate) - curveLoudnessDb (curve, 0.5f, sampleRate);
        auto [l, r] = runSine (p, 40.0, 0.25f, 0.25f, 2.0);
        check (std::abs (gainDb (l, 0.25f) - expectedHalf) < 0.1, "50% strength at 40 Hz: " + juce::String (gainDb (l, 0.25f), 2) + " dB (expected " + juce::String (expectedHalf, 2) + ")");

        setParam (p, ParamIDs::strength, 100.0f);
        setParam (p, ParamIDs::levelMatch, 0.0f);
        auto [l2, r2] = runSine (p, 1000.0, 0.25f, 0.25f);
        const double noPreamp = curveResponseDb (curve, 1000.0, 1.0f, sampleRate);
        check (std::abs (gainDb (l2, 0.25f) - noPreamp) < 0.1, "level match off removes the gain compensation");
    }

    std::cout << "Level match keeps music at the same loudness" << std::endl;
    {
        // Pink noise has music's average spectrum. Compare its level (100 Hz - 10 kHz,
        // via an RMS of band-passed audio) with MixMate off and on for each popular headphone.
        auto pinkNoiseLevelDb = [] (MixMateProcessor& p)
        {
            p.prepareToPlay (sampleRate, blockSize);
            juce::Random random (1234);
            double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
            juce::dsp::IIR::Filter<float> hp, lp;
            hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 100.0);
            lp.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 10000.0);
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            double sum = 0;
            long count = 0;
            for (int block = 0; block < 400; ++block)
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    // Paul Kellet's pink noise filter.
                    const double white = random.nextFloat() * 2.0 - 1.0;
                    b0 = 0.99886 * b0 + white * 0.0555179; b1 = 0.99332 * b1 + white * 0.0750759;
                    b2 = 0.96900 * b2 + white * 0.1538520; b3 = 0.86650 * b3 + white * 0.3104856;
                    b4 = 0.55000 * b4 + white * 0.5329522; b5 = -0.7616 * b5 - white * 0.0168980;
                    const auto pink = (float) (0.05 * (b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362));
                    b6 = white * 0.115926;
                    buffer.setSample (0, i, pink);
                    buffer.setSample (1, i, pink);
                }
                p.processBlock (buffer, midi);
                for (int i = 0; i < blockSize; ++i)
                {
                    const float y = lp.processSample (hp.processSample (buffer.getSample (0, i)));
                    if (block >= 40) { sum += y * y; ++count; }
                }
            }
            return 10.0 * std::log10 (sum / (double) count);
        };

        MixMateProcessor reference;
        const double off = pinkNoiseLevelDb (reference);
        double worst = 0;
        for (auto index : reference.database->getPopular())
        {
            MixMateProcessor p;
            p.setHeadphone (0, index);
            setParam (p, ParamIDs::headphones, headphones1);
            const double diff = pinkNoiseLevelDb (p) - off;
            const auto& hp = p.database->get (index);
            std::cout << "        " << juce::String (diff, 2).paddedLeft (' ', 6) << " dB  (was "
                      << juce::String (hp.curve.preampDb, 1)
                      << " dB preamp)  " << hp.name << std::endl;
            worst = juce::jmax (worst, std::abs (diff));
        }
        check (worst < 1.0, "all popular headphones within 1 dB of off (worst " + juce::String (worst, 2) + " dB)");
    }

    std::cout << "Headphone 2 and one-headphone mode" << std::endl;
    {
        MixMateProcessor p;
        const int dt770 = p.database->indexOf ("Beyerdynamic DT 770 Pro (80 Ohm)");
        p.setHeadphone (1, dt770);
        setParam (p, ParamIDs::headphones, headphones2);
        const auto& curve = p.database->get (dt770).curve;
        const double expected = curveResponseDb (curve, 5000.0, 1.0f, sampleRate) - curveLoudnessDb (curve, 1.0f, sampleRate);
        auto [l, r] = runSine (p, 5000.0, 0.25f, 0.25f);
        check (std::abs (gainDb (l, 0.25f) - expected) < 0.1, "headphone 2 uses its own curve");

        setParam (p, ParamIDs::twoMode, 0.0f);
        const auto& curve1 = p.database->get (p.getHeadphoneIndex (0)).curve;
        const double expected1 = curveResponseDb (curve1, 5000.0, 1.0f, sampleRate) - curveLoudnessDb (curve1, 1.0f, sampleRate);
        auto [l1, r1] = runSine (p, 5000.0, 0.25f, 0.25f);
        check (std::abs (gainDb (l1, 0.25f) - expected1) < 0.1, "one-headphone mode plays headphone 1 instead of 2");
    }

    std::cout << "Mid / side" << std::endl;
    {
        MixMateProcessor p;
        setParam (p, ParamIDs::monitor, monitorMid);
        auto [l, r] = runSine (p, 1000.0, 0.5f, 0.0f);
        check (std::abs (gainDb (l, 0.25f)) < 0.05 && std::abs (gainDb (r, 0.25f)) < 0.05, "mid: left-only signal comes out of both sides at half level");

        setParam (p, ParamIDs::monitor, monitorSide);
        auto [sl, sr] = runSine (p, 1000.0, 0.5f, 0.5f);
        check (sl < 1.0e-5 && sr < 1.0e-5, "side: identical left and right cancel out");

        auto [dl, dr] = runSine (p, 1000.0, 0.5f, -0.5f);
        check (std::abs (gainDb (dl, 0.5f)) < 0.05 && std::abs (gainDb (dr, 0.5f)) < 0.05, "side: opposite left and right come through");

        // Side stays stereo: a left-only signal comes out with the right side inverted.
        p.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        for (int rep = 0; rep < 40; ++rep)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                buffer.setSample (0, i, 0.5f * (float) std::sin (0.1 * (rep * blockSize + i)));
                buffer.setSample (1, i, 0.0f);
            }
            p.processBlock (buffer, midi);
        }
        double dot = 0;
        for (int i = 0; i < blockSize; ++i)
            dot += buffer.getSample (0, i) * buffer.getSample (1, i);
        check (dot < 0.0 && std::abs (buffer.getSample (0, 100) + buffer.getSample (1, 100)) < 1.0e-6,
               "side: plays in stereo (right is the inverse of left)");
    }

    std::cout << "Band solo" << std::endl;
    {
        MixMateProcessor p;
        struct Case { int band; double inside; double outside; const char* name; };
        for (auto c : { Case { soloSub, 40.0, 1000.0, "sub" }, Case { soloBass, 160.0, 3000.0, "bass" },
                        Case { soloMids, 1000.0, 60.0, "mids" }, Case { soloHighs, 10000.0, 200.0, "highs" } })
        {
            setParam (p, ParamIDs::solo, (float) c.band);
            auto [in, inR] = runSine (p, c.inside, 0.5f, 0.5f);
            auto [out, outR] = runSine (p, c.outside, 0.5f, 0.5f);
            // The bass band is narrow (100-250 Hz), so its centre sits a little lower.
            check (gainDb (in, 0.5f) > (c.band == soloBass ? -3.5 : -1.5) && gainDb (out, 0.5f) < -30.0,
                   juce::String (c.name) + ": " + juce::String (c.inside) + " Hz at " + juce::String (gainDb (in, 0.5f), 1)
                       + " dB, " + juce::String (c.outside) + " Hz at " + juce::String (gainDb (out, 0.5f), 1) + " dB");
        }

        setParam (p, ParamIDs::solo, soloSub);
        setParam (p, ParamIDs::xSub, 60.0f);
        auto [moved, movedR] = runSine (p, 100.0, 0.5f, 0.5f);
        check (gainDb (moved, 0.5f) < -6.0, "moving the sub crossover to 60 Hz takes 100 Hz out of the sub band");
    }

    std::cout << "Exporting" << std::endl;
    {
        MixMateProcessor p;
        p.setHeadphone (0, p.database->indexOf ("Sennheiser HD 650"));
        setParam (p, ParamIDs::headphones, headphones1);
        setParam (p, ParamIDs::monitor, monitorSide);
        p.setNonRealtime (true);
        auto [l, r] = runSine (p, 1000.0, 0.5f, 0.5f);
        check (std::abs (gainDb (l, 0.5f)) < 0.01, "everything bypassed while the DAW exports");

        p.setBypassWhenExporting (false);
        auto [l2, r2] = runSine (p, 1000.0, 0.5f, 0.5f);
        check (l2 < 1.0e-5, "still active when export bypass is switched off");
    }

    std::cout << "Saving and loading" << std::endl;
    {
        juce::MemoryBlock state;
        {
            MixMateProcessor p;
            p.setHeadphone (0, p.database->indexOf ("AKG K371"));
            p.setHeadphone (1, p.database->indexOf ("Sony MDR-7506"));
            p.setKey (CycleAction::headphones, juce::KeyPress ('h'));
            setParam (p, ParamIDs::headphones, headphones2);
            setParam (p, ParamIDs::strength, 70.0f);
            setParam (p, ParamIDs::xSub, 90.0f);
            p.getStateInformation (state);
        }
        MixMateProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        check (q.database->get (q.getHeadphoneIndex (0)).name == "AKG K371", "headphone 1 restored");
        check (q.database->get (q.getHeadphoneIndex (1)).name == "Sony MDR-7506", "headphone 2 restored");
        check (q.getChoice (ParamIDs::headphones) == headphones2, "selected headphone restored");
        check (q.getKey (CycleAction::headphones) == juce::KeyPress ('h'), "shortcut key restored");
        check (std::abs (q.apvts.getRawParameterValue (ParamIDs::strength)->load() - 70.0f) < 0.01f, "strength restored");
        check (std::abs (q.apvts.getRawParameterValue (ParamIDs::xSub)->load() - 90.0f) < 0.5f, "crossover restored");
        pumpMessages (100);
        check (q.getChoice (ParamIDs::headphones) == headphones2, "loading a project does not trigger a cycle");
    }

    std::cout << "Save as default" << std::endl;
    {
        {
            MixMateProcessor p;
            p.setHeadphone (0, p.database->indexOf ("Audio-Technica ATH-M50x"));
            p.saveAsDefault();
        }
        {
            MixMateProcessor p;
            check (p.database->get (p.getHeadphoneIndex (0)).name == "Audio-Technica ATH-M50x", "new instance starts with the saved default");
            p.resetToFactory();
        }
        MixMateProcessor p;
        check (p.database->get (p.getHeadphoneIndex (0)).name == "Sennheiser HD 600", "reset brings back the factory headphones");
        check (! p.hasSavedDefault(), "reset removes the saved default");
    }

    std::cout << "Cycling (DAW key map / MIDI)" << std::endl;
    {
        MixMateProcessor p;
        int seen[4];
        for (int i = 0; i < 4; ++i)
        {
            seen[i] = p.getChoice (ParamIDs::headphones);
            setParam (p, ParamIDs::cycleHeadphones, (i % 2 == 0) ? 1.0f : 0.0f);
            pumpMessages (100);
        }
        check (seen[0] == 0 && seen[1] == 1 && seen[2] == 2 && seen[3] == 0, "headphones: off, 1, 2, off");

        setParam (p, ParamIDs::headphones, headphonesOff);
        setParam (p, ParamIDs::twoMode, 0.0f);
        pumpMessages (100);
        setParam (p, ParamIDs::cycleHeadphones, 1.0f); pumpMessages (100);
        const int a = p.getChoice (ParamIDs::headphones);
        setParam (p, ParamIDs::cycleHeadphones, 0.0f); pumpMessages (100);
        const int b = p.getChoice (ParamIDs::headphones);
        check (a == 1 && b == 0, "one-headphone mode: off, 1, off");

        int ms[4];
        for (int i = 0; i < 4; ++i) { ms[i] = p.getChoice (ParamIDs::monitor); p.cycle (CycleAction::monitor); }
        check (ms[0] == 0 && ms[1] == 1 && ms[2] == 2 && ms[3] == 0, "mid/side: stereo, mid, side, stereo");

        int solo[6];
        for (int i = 0; i < 6; ++i) { solo[i] = p.getChoice (ParamIDs::solo); p.cycle (CycleAction::solo); }
        check (solo[0] == 0 && solo[4] == 4 && solo[5] == 0, "solo: off, sub, bass, mids, highs, off");
    }

    std::cout << "Interface" << std::endl;
    {
        MixMateProcessor p;
        p.setHeadphone (0, p.database->indexOf ("Sennheiser HD 650"));
        p.setHeadphone (1, p.database->indexOf ("Beyerdynamic DT 770 Pro (80 Ohm)"));
        setParam (p, ParamIDs::headphones, headphones1);

        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        check (editor->getWidth() == 500 && editor->getHeight() == 200, "editor is 500 x 200");
        pumpMessages (100);
        saveSnapshot (*editor, shots, "1-main");

        // Shortcut key cycles headphones.
        editor->keyPressed (juce::KeyPress ('n'));
        check (p.getChoice (ParamIDs::headphones) == headphones2, "pressing N moves to headphone 2");

        setParam (p, ParamIDs::monitor, monitorSide);
        setParam (p, ParamIDs::solo, soloBass);
        pumpMessages (100);
        saveSnapshot (*editor, shots, "2-side-bass-solo");

        setParam (p, ParamIDs::monitor, monitorStereo);
        setParam (p, ParamIDs::solo, soloOff);
        setParam (p, ParamIDs::headphones, headphonesOff);
        setParam (p, ParamIDs::twoMode, 0.0f);
        pumpMessages (100);
        saveSnapshot (*editor, shots, "3-one-headphone-mode-off");
        setParam (p, ParamIDs::twoMode, 1.0f);

        // Open the picker and the settings through their own components.
        juce::Array<juce::Component*> overlays;
        for (auto* child : editor->getChildren())
            overlays.addArray (child->getChildren());

        for (auto* c : overlays)
                if (auto* hp = dynamic_cast<HeadphonePicker*> (c))
                {
                    hp->open (0, p.getHeadphoneIndex (0));
                    pumpMessages (50);
                    saveSnapshot (*editor, shots, "4-picker");
                    hp->close();
                }
                else if (auto* s = dynamic_cast<SettingsPanel*> (c))
                {
                    s->open();
                    pumpMessages (50);
                    saveSnapshot (*editor, shots, "5-settings");
                    s->close();
                }

        p.setUiScale (1.5f);
        std::unique_ptr<juce::AudioProcessorEditor> big (p.createEditor());
        check (big->getWidth() == 750 && big->getHeight() == 300, "150% size is 750 x 300");
    }

    std::cout << (failures == 0 ? "\nAll tests passed" : "\n" + juce::String (failures) + " test(s) FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
