#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    constexpr int   chunkSize = 32;                 // control-rate update interval
    constexpr float gateDuty  = 0.55f;              // fraction of each step that stays open
    constexpr double divisionBeats[] = { 1.0, 0.5, 0.25, 0.125 };

    float ramp (float x, float from, float to) noexcept
    {
        return jlimit (0.0f, 1.0f, (x - from) / (to - from));
    }
}

const char* DropFuseProcessor::divisionName (int idx) noexcept
{
    static const char* names[] = { "1/4", "1/8", "1/16", "1/32" };
    return (idx >= 0 && idx < numDivisions) ? names[idx] : "OFF";
}

DropFuseProcessor::DropFuseProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DropFuseState", createLayout())
{
    buildParam    = apvts.getRawParameterValue ("build");
}

AudioProcessorValueTreeState::ParameterLayout DropFuseProcessor::createLayout()
{
    using P    = AudioParameterFloat;
    using Attr = AudioParameterFloatAttributes;

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<P> (ParameterID { "build", 1 }, "Build",
                                     NormalisableRange<float> (0.0f, 100.0f, 0.01f), 0.0f,
                                     Attr().withStringFromValueFunction ([] (float v, int)
                                     { return String (roundToInt (v)); })));

    return layout;
}

bool DropFuseProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void DropFuseProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    dsp::ProcessSpec spec { sampleRate, (uint32) samplesPerBlock, 2 };

    for (auto* f : { &hpf1, &hpf2 })
    {
        f->prepare (spec);
        f->setType (dsp::StateVariableTPTFilterType::highpass);
        f->setCutoffFrequency (20.0f);
        f->setResonance (1.0f / MathConstants<float>::sqrt2);
        f->reset();
    }

    buildSmooth.reset (sampleRate, 0.02);
    buildSmooth.setCurrentAndTargetValue (buildParam->load() / 100.0f);

    gateGain  = 1.0f;
    gateCoeff = (float) std::exp (-1.0 / (0.002 * sampleRate));   // ~2 ms edges, no clicks

    pitchWindow = (float) (0.045 * sampleRate);
    const int bufSize = nextPowerOfTwo ((int) pitchWindow * 2 + 8);
    pitchMask = bufSize - 1;
    for (auto& b : pitchBuf)
        b.assign ((size_t) bufSize, 0.0f);
    writePos = 0;
    pitchPhase = 0.0f;
    internalPpq = 0.0;
}

float DropFuseProcessor::readDelay (int ch, float delaySamples) const noexcept
{
    const float readPos = (float) writePos - delaySamples;
    const int   i0 = (int) std::floor (readPos);
    const float frac = readPos - (float) i0;
    const auto& b = pitchBuf[ch];
    const float a = b[(size_t) (i0 & pitchMask)];
    const float c = b[(size_t) ((i0 + 1) & pitchMask)];
    return a + frac * (c - a);
}

void DropFuseProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int n = buffer.getNumSamples();
    const int numCh = jmin (numIn, 2);
    if (numCh == 0 || n == 0)
        return;

    // ---- transport (follows Logic's tempo and bar grid) ----
    double bpm = 120.0, ppq = internalPpq;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying())
                if (auto p = pos->getPpqPosition()) ppq = *p;
        }
    const double beatsPerSample = bpm / 60.0 / sr;
    internalPpq = ppq + n * beatsPerSample;

    buildSmooth.setTargetValue (buildParam->load() / 100.0f);

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int start = 0; start < n; start += chunkSize)
    {
        const int len = jmin (chunkSize, n - start);
        const float b = buildSmooth.getNextValue();
        if (len > 1) buildSmooth.skip (len - 1);

        // ---- macro mapping ----
        const float cutoff = 20.0f * std::pow (cutMax / 20.0f, std::pow (b, 1.4f));
        hpf1.setCutoffFrequency (cutoff);
        hpf2.setCutoffFrequency (cutoff);
        hpf1.setResonance (0.707f + 0.9f * b * b);   // DJ-filter style bump near the top

        const float gateDepth = gateMax * ramp (b, 0.05f, 0.30f);
        const int   divIdx = b < 0.05f ? -1 : b < 0.35f ? 0 : b < 0.60f ? 1 : b < 0.85f ? 2 : 3;
        const double div = divIdx >= 0 ? divisionBeats[divIdx] : 1.0;

        const float semis = pitchMax * std::pow (ramp (b, 0.35f, 1.0f), 1.5f);
        const float ratio = std::pow (2.0f, semis / 12.0f);
        const float pitchMix = jlimit (0.0f, 1.0f, semis / 0.25f);
        const float phaseInc = (1.0f - ratio) / pitchWindow;

        const float width = 1.0f - narrow * std::pow (b, 1.2f);

        for (int i = 0; i < len; ++i)
        {
            const int s = start + i;
            float xl = L[s];
            float xr = R != nullptr ? R[s] : xl;

            // 1) pitch rise
            pitchBuf[0][(size_t) writePos] = xl;
            pitchBuf[1][(size_t) writePos] = xr;
            if (pitchMix > 0.0f)
            {
                pitchPhase += phaseInc;
                if (pitchPhase < 0.0f)  pitchPhase += 1.0f;
                if (pitchPhase >= 1.0f) pitchPhase -= 1.0f;
                float p2 = pitchPhase + 0.5f;
                if (p2 >= 1.0f) p2 -= 1.0f;

                const float d1 = 1.0f + pitchPhase * pitchWindow;
                const float d2 = 1.0f + p2 * pitchWindow;
                const float s1 = std::sin (MathConstants<float>::pi * pitchPhase);
                const float g1 = s1 * s1;          // Hann crossfade, g1 + g2 = 1
                const float g2 = 1.0f - g1;

                const float yl = readDelay (0, d1) * g1 + readDelay (0, d2) * g2;
                const float yr = readDelay (1, d1) * g1 + readDelay (1, d2) * g2;
                xl += pitchMix * (yl - xl);
                xr += pitchMix * (yr - xr);
            }
            writePos = (writePos + 1) & pitchMask;

            // 2) low cut, 24 dB/oct
            xl = hpf2.processSample (0, hpf1.processSample (0, xl));
            xr = hpf2.processSample (1, hpf1.processSample (1, xr));

            // 3) stereo narrowing
            if (R != nullptr)
            {
                const float mid  = 0.5f * (xl + xr);
                const float side = 0.5f * (xl - xr) * width;
                xl = mid + side;
                xr = mid - side;
            }

            // 4) tempo-synced gate
            float target = 1.0f;
            if (divIdx >= 0)
            {
                const double pos = ppq + (double) s * beatsPerSample;
                double frac = std::fmod (pos, div) / div;
                if (frac < 0.0) frac += 1.0;
                if (frac >= gateDuty) target = 1.0f - gateDepth;
            }
            gateGain = target + gateCoeff * (gateGain - target);

            L[s] = xl * gateGain;
            if (R != nullptr) R[s] = xr * gateGain;
        }

        hpf1.snapToZero();
        hpf2.snapToZero();

        displayDivision.store (gateDepth > 0.001f ? divIdx : -1);
        displayCutoff.store (cutoff);
        displaySemis.store (semis);
        displayWidth.store (width);
    }
}

void DropFuseProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DropFuseProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* DropFuseProcessor::createEditor()
{
    return new DropFuseEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DropFuseProcessor();
}
