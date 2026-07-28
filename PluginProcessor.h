#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>
#include "PitchTracker.h"
#include "KeyDetector.h"

// ─────────────────────────────────────────────────────────────────────────
//  Uncertain Voice — build de validation v0 (SPEC §10, étape 1)
//  L'audio n'est PAS modifié : il passe tel quel. Le plugin analyse en
//  parallèle pour valider le tracker de pitch + la détection de gamme.
//  La correction (centre/oscillation) viendra une fois le suiveur validé.
// ─────────────────────────────────────────────────────────────────────────
class UncertainVoiceProcessor : public juce::AudioProcessor
{
public:
    UncertainVoiceProcessor();
    ~UncertainVoiceProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    // ── interface pour l'éditeur (thread message) ──────────────────────
    static constexpr int kTrace = 512;

    struct TraceSnapshot
    {
        std::array<float, kTrace> midi {};   // NaN = non-voisé
        int count = 0;
    };

    TraceSnapshot     getTrace() const;
    KeyDetector::Key  getKey() const;
    void              resetAnalysis();

private:
    double hostSr = 44100.0;
    int    decim  = 1;
    int    window = 1024;
    int    hop    = 256;

    PitchTracker tracker;
    KeyDetector  keyDet;

    float lpState = 0.0f, lpCoeff = 0.0f;
    int   decCount = 0;

    std::vector<float> ring, linWin;
    int ringPos = 0, ringFilled = 0, hopCount = 0;

    std::array<std::atomic<float>, kTrace> traceMidi;
    std::atomic<int> traceWrite { 0 };

    std::atomic<int>   keyRoot  { 9 };
    std::atomic<bool>  keyMinor { true };
    std::atomic<float> keyConf  { 0.0f };
    mutable juce::SpinLock keyPcsLock;
    std::array<int, 7> keyPcs { { 9, 11, 0, 2, 4, 5, 7 } };

    void analyseWindow();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UncertainVoiceProcessor)
};
