#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    constexpr int   segmentSize = 512;   // internal processing segment (send buffer size)
    constexpr int   chunkSize   = 32;    // control-rate update interval inside a segment
    constexpr int   maxChunks   = segmentSize / chunkSize;
    constexpr float gateDuty    = 0.55f;
    constexpr double divisionBeats[] = { 1.0, 0.5, 0.25, 0.125 };

    float ramp (float x, float from, float to) noexcept
    {
        return jlimit (0.0f, 1.0f, (x - from) / (to - from));
    }
}

const char* DropTNTProcessor::divisionName (int idx) noexcept
{
    static const char* names[] = { "1/4", "1/8", "1/16", "1/32" };
    return (idx >= 0 && idx < 4) ? names[idx] : "OFF";
}

DropTNTProcessor::DropTNTProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DropTNTState", createLayout())
{
    buildParam = apvts.getRawParameterValue ("build");
}

AudioProcessorValueTreeState::ParameterLayout DropTNTProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "build", 1 }, "Build", NormalisableRange<float> (0.0f, 100.0f, 0.01f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return String (roundToInt (v)); })));
    return layout;
}

bool DropTNTProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void DropTNTProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    dsp::ProcessSpec spec { sampleRate, (uint32) jmin (samplesPerBlock, segmentSize), 2 };

    for (auto* f : { &hpf1, &hpf2 })
    {
        f->prepare (spec);
        f->setType (dsp::StateVariableTPTFilterType::highpass);
        f->setCutoffFrequency (20.0f);
        f->setResonance (1.0f / MathConstants<float>::sqrt2);
        f->reset();
    }

    reverb.setSampleRate (sampleRate);
    reverb.reset();
    sendBuffer.setSize (2, segmentSize, false, true, false);

    buildSmooth.reset (sampleRate, 0.02);
    buildSmooth.setCurrentAndTargetValue (buildParam->load() / 100.0f);

    gateGain  = 1.0f;
    gateCoeff = (float) std::exp (-1.0 / (0.002 * sampleRate));   // ~2 ms gate edges
    sendSmoothed = 0.0f;
    internalPpq = 0.0;
    idle = true;
    silentBlocks = 1000;
}

void DropTNTProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
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

    // ---- transport (follows the host tempo and bar grid) ----
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

    const float target = buildParam->load() / 100.0f;
    buildSmooth.setTargetValue (target);

    // ---- idle bypass: Build at 0 and reverb tail gone -> pass audio straight through ----
    if (target <= 0.0f && ! buildSmooth.isSmoothing() && silentBlocks > 8)
    {
        if (! idle)
        {
            idle = true;
            displayDivision.store (-1);
            displayCutoff.store (20.0f);
            displayReverb.store (0.0f);
            displayWidth.store (1.0f);
        }
        return;
    }
    if (idle)
    {
        idle = false;
        silentBlocks = 0;
        hpf1.reset(); hpf2.reset();
        reverb.reset();
        gateGain = 1.0f;
        sendSmoothed = 0.0f;
    }

    const float sendCoeff = (float) std::exp (-1.0 / (0.005 * sr));
    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    float* sL = sendBuffer.getWritePointer (0);
    float* sR = sendBuffer.getWritePointer (1);

    float widths[maxChunks + 1];
    float reverbPeak = 0.0f;
    float lastSend = 0.0f;

    for (int segStart = 0; segStart < n; segStart += segmentSize)
    {
        const int segLen = jmin (segmentSize, n - segStart);
        float lastRoom = 0.55f;

        // ---- pass 1: low cut + gate, fill reverb send ----
        for (int c = 0, start = 0; start < segLen; ++c, start += chunkSize)
        {
            const int len = jmin (chunkSize, segLen - start);
            const float b = buildSmooth.getNextValue();
            if (len > 1) buildSmooth.skip (len - 1);

            const float cutoff = 20.0f * std::pow (cutMax / 20.0f, std::pow (b, 1.4f));
            hpf1.setCutoffFrequency (cutoff);
            hpf2.setCutoffFrequency (cutoff);
            hpf1.setResonance (0.707f + 0.9f * b * b);

            const float gateDepth = (1.0f - gateFloor) * ramp (b, 0.05f, 0.30f);
            const int   divIdx = b < 0.05f ? -1 : b < 0.35f ? 0 : b < 0.60f ? 1 : b < 0.85f ? 2 : 3;
            const double div = divIdx >= 0 ? divisionBeats[divIdx] : 1.0;

            const float sendTarget = sendMax * std::pow (ramp (b, 0.05f, 1.0f), 0.8f);
            widths[c] = 1.0f - std::pow (b, 1.2f);
            lastRoom = 0.55f + 0.37f * b;
            lastSend = sendTarget;

            for (int i = 0; i < len; ++i)
            {
                const int s = segStart + start + i;
                float xl = hpf2.processSample (0, hpf1.processSample (0, L[s]));
                float xr = R != nullptr ? hpf2.processSample (1, hpf1.processSample (1, R[s])) : xl;

                float g = 1.0f;
                if (divIdx >= 0)
                {
                    double frac = std::fmod (ppq + (double) s * beatsPerSample, div) / div;
                    if (frac < 0.0) frac += 1.0;
                    if (frac >= gateDuty) g = 1.0f - gateDepth;
                }
                gateGain = g + gateCoeff * (gateGain - g);
                sendSmoothed = sendTarget + sendCoeff * (sendSmoothed - sendTarget);

                xl *= gateGain;
                xr *= gateGain;
                sL[start + i] = xl * sendSmoothed;
                sR[start + i] = xr * sendSmoothed;
                L[s] = xl;
                if (R != nullptr) R[s] = xr;
            }

            displayDivision.store (gateDepth > 0.001f ? divIdx : -1);
            displayCutoff.store (cutoff);
            displayReverb.store (sendTarget / sendMax);
            displayWidth.store (widths[c]);
        }

        // ---- reverb on the send (post-gate: the tail fills the gaps) ----
        Reverb::Parameters rp;
        rp.roomSize = lastRoom;
        rp.damping  = 0.45f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width    = 1.0f;
        reverb.setParameters (rp);
        if (R != nullptr) reverb.processStereo (sL, sR, segLen);
        else              reverb.processMono (sL, segLen);

        // ---- pass 2: add reverb, narrow stereo ----
        for (int c = 0, start = 0; start < segLen; ++c, start += chunkSize)
        {
            const int len = jmin (chunkSize, segLen - start);
            const float w = widths[c];
            for (int i = 0; i < len; ++i)
            {
                const int k = start + i, s = segStart + k;
                const float wl = sL[k] * 0.8f;
                reverbPeak = jmax (reverbPeak, std::abs (wl));
                if (R != nullptr)
                {
                    const float wr = sR[k] * 0.8f;
                    const float yl = L[s] + wl, yr = R[s] + wr;
                    const float mid  = 0.5f * (yl + yr);
                    const float side = 0.5f * (yl - yr) * w;
                    L[s] = mid + side;
                    R[s] = mid - side;
                }
                else
                {
                    L[s] += wl;
                }
            }
        }

        hpf1.snapToZero();
        hpf2.snapToZero();
    }

    // reverb tail tracking for the idle bypass
    if (lastSend <= 0.0f && reverbPeak < 1.0e-4f) ++silentBlocks;
    else silentBlocks = 0;
}

void DropTNTProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DropTNTProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* DropTNTProcessor::createEditor()
{
    return new DropTNTEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DropTNTProcessor();
}
