#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

UncertainVoiceProcessor::UncertainVoiceProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (auto& a : traceMidi) a.store (std::nanf (""));
}

void UncertainVoiceProcessor::prepareToPlay (double sampleRate, int)
{
    hostSr = sampleRate;
    decim  = juce::jmax (1, (int) std::round (sampleRate / 16000.0));
    const double dsr = sampleRate / decim;

    window = 1024;
    hop    = 256;

    tracker.prepare (dsr, window);
    tracker.setRange (65.0f, 1000.0f);
    keyDet.reset();

    ring.assign ((size_t) window, 0.0f);
    linWin.assign ((size_t) window, 0.0f);
    ringPos = ringFilled = hopCount = decCount = 0;

    // one-pole LPF ~ Nyquist décimée
    const double fc = dsr * 0.45;
    lpCoeff = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * fc / sampleRate);
    lpState = 0.0f;
}

bool UncertainVoiceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void UncertainVoiceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = buffer.getNumChannels();
    const int numSm = buffer.getNumSamples();

    // --- passe d'analyse (ne touche pas à l'audio) ---
    const float* L = buffer.getReadPointer (0);
    const float* R = numCh > 1 ? buffer.getReadPointer (1) : nullptr;

    for (int i = 0; i < numSm; ++i)
    {
        const float mono = R ? 0.5f * (L[i] + R[i]) : L[i];

        // filtrage anti-alias puis décimation
        lpState = mono + lpCoeff * (lpState - mono);
        if (++decCount >= decim)
        {
            decCount = 0;
            ring[(size_t) ringPos] = lpState;
            ringPos = (ringPos + 1) % window;
            if (ringFilled < window) ++ringFilled;

            if (++hopCount >= hop && ringFilled >= window)
            {
                hopCount = 0;
                analyseWindow();
            }
        }
    }

    // audio inchangé (validation build) — rien à écrire.
    juce::ignoreUnused (numCh);
}

void UncertainVoiceProcessor::analyseWindow()
{
    // linéarise le buffer circulaire
    for (int i = 0; i < window; ++i)
        linWin[(size_t) i] = ring[(size_t) ((ringPos + i) % window)];

    const auto res = tracker.process (linWin.data());

    float midi = std::nanf ("");
    if (res.voiced)
    {
        midi = 69.0f + 12.0f * std::log2 (res.f0 / 440.0f);
        keyDet.addFrame (res.f0, res.clarity);
    }

    // écrit dans la trace circulaire
    const int w = traceWrite.load();
    traceMidi[(size_t) w].store (midi);
    traceWrite.store ((w + 1) % kTrace);

    // met à jour la gamme estimée
    const auto k = keyDet.estimate();
    keyRoot.store (k.root);
    keyMinor.store (k.minor);
    keyConf.store (k.confidence);
    {
        const juce::SpinLock::ScopedLockType sl (keyPcsLock);
        keyPcs = k.pcs;
    }
}

UncertainVoiceProcessor::TraceSnapshot UncertainVoiceProcessor::getTrace() const
{
    TraceSnapshot s;
    const int w = traceWrite.load();
    for (int i = 0; i < kTrace; ++i)
    {
        const int idx = (w + i) % kTrace;   // du plus ancien au plus récent
        s.midi[(size_t) i] = traceMidi[(size_t) idx].load();
    }
    s.count = kTrace;
    return s;
}

KeyDetector::Key UncertainVoiceProcessor::getKey() const
{
    KeyDetector::Key k;
    k.root = keyRoot.load();
    k.minor = keyMinor.load();
    k.confidence = keyConf.load();
    {
        const juce::SpinLock::ScopedLockType sl (keyPcsLock);
        k.pcs = keyPcs;
    }
    return k;
}

void UncertainVoiceProcessor::resetAnalysis()
{
    keyDet.reset();
    for (auto& a : traceMidi) a.store (std::nanf (""));
    traceWrite.store (0);
    ringFilled = ringPos = hopCount = 0;
}

juce::AudioProcessorEditor* UncertainVoiceProcessor::createEditor()
{
    return new UncertainVoiceEditor (*this);
}

// point d'entrée VST3
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UncertainVoiceProcessor();
}
