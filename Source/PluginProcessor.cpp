#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    constexpr int   segmentSize = 512;
    constexpr int   chunkSize   = 32;
    constexpr int   maxChunks   = segmentSize / chunkSize;

    constexpr float echoWet  = 0.7f;
    constexpr float roomWet  = 0.8f;
    constexpr float compThresholdDb = -20.0f;

    float ramp (float x, float from, float to) noexcept
    {
        return jlimit (0.0f, 1.0f, (x - from) / (to - from));
    }
    float dbToGain (float db) noexcept { return std::exp (db * 0.115129255f); }
    float gainToDb (float g) noexcept  { return 8.68588964f * std::log (g + 1.0e-9f); }
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
    revBuffer.setSize (2, segmentSize, false, true, false);

    const int echoSize = nextPowerOfTwo ((int) (sampleRate * 1.0) + 8);   // up to 1 s (1/2 beat at 30 BPM)
    echoMask = echoSize - 1;
    for (auto& b : echoBuf) b.assign ((size_t) echoSize, 0.0f);
    echoWrite = 0;
    echoTime = (float) (0.25 * sampleRate);
    echoGlideCoeff = (float) std::exp (-1.0 / (0.08 * sampleRate));          // ~80 ms time glide = pitch sweep
    echoHpCoeff = (float) (1.0 - std::exp (-MathConstants<double>::twoPi * 150.0 / sampleRate));
    echoFbHp[0] = echoFbHp[1] = 0.0f;

    buildSmooth.reset (sampleRate, 0.02);
    buildSmooth.setCurrentAndTargetValue (buildParam->load() / 100.0f);

    smoothCoeff5ms = (float) std::exp (-1.0 / (0.005 * sampleRate));
    gateGain  = 1.0f;
    gateCoeff = (float) std::exp (-1.0 / (0.002 * sampleRate));
    compAtt = (float) std::exp (-1.0 / (0.001 * sampleRate));
    compRel = (float) std::exp (-1.0 / (0.08 * sampleRate));
    compEnv = 0.0f;

    revSendSmoothed = echoSendSmoothed = 0.0f;
    internalPpq = 0.0;
    idle = true;
    silentBlocks = 1000;
}

float DropTNTProcessor::readEcho (int ch, float delaySamples) const noexcept
{
    const float readPos = (float) echoWrite - delaySamples;
    const int   i0 = (int) std::floor (readPos);
    const float frac = readPos - (float) i0;
    const auto& b = echoBuf[ch];
    const float a = b[(size_t) (i0 & echoMask)];
    const float c = b[(size_t) ((i0 + 1) & echoMask)];
    return a + frac * (c - a);
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

    // ---- transport ----
    double bpm = 120.0, ppq = internalPpq;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying())
                if (auto p = pos->getPpqPosition()) ppq = *p;
        }
    bpm = jlimit (30.0, 400.0, bpm);
    const double beatsPerSample = bpm / 60.0 / sr;
    const double samplesPerBeat = sr * 60.0 / bpm;
    internalPpq = ppq + n * beatsPerSample;

    const float target = buildParam->load() / 100.0f;
    buildSmooth.setTargetValue (target);

    // ---- idle bypass ----
    if (target <= 0.0f && ! buildSmooth.isSmoothing() && silentBlocks > 8)
    {
        if (! idle)
        {
            idle = true;
            displayCrunchDb.store (0.0f);
            displayCutoff.store (20.0f);
            displayEchoBeats.store (0.0f);
            displayRatio.store (1.0f);
        }
        return;
    }
    if (idle)
    {
        idle = false;
        silentBlocks = 0;
        hpf1.reset(); hpf2.reset();
        reverb.reset();
        for (auto& b : echoBuf) std::fill (b.begin(), b.end(), 0.0f);
        echoFbHp[0] = echoFbHp[1] = 0.0f;
        echoTime = (float) (0.5 * samplesPerBeat);
        compEnv = 0.0f;
        gateGain = 1.0f;
        revSendSmoothed = echoSendSmoothed = 0.0f;
    }

    const bool stereo = numCh > 1;
    float* L = buffer.getWritePointer (0);
    float* R = stereo ? buffer.getWritePointer (1) : nullptr;
    float* rL = revBuffer.getWritePointer (0);
    float* rR = revBuffer.getWritePointer (1);
    const float maxEcho = (float) echoMask - 4.0f;

    float widths[maxChunks + 1], ratios[maxChunks + 1];
    float tailPeak = 0.0f;
    float lastSends = 0.0f;

    for (int segStart = 0; segStart < n; segStart += segmentSize)
    {
        const int segLen = jmin (segmentSize, n - segStart);
        float room = 0.8f;

        // ---- pass 1: low cut, crunch, gate, echo, fill room send ----
        for (int c = 0, start = 0; start < segLen; ++c, start += chunkSize)
        {
            const int len = jmin (chunkSize, segLen - start);
            const float b = buildSmooth.getNextValue();
            if (len > 1) buildSmooth.skip (len - 1);

            const float cutoff = 20.0f * std::pow (cutMax / 20.0f, std::pow (b, 1.4f));
            hpf1.setCutoffFrequency (cutoff);
            hpf2.setCutoffFrequency (cutoff);
            hpf1.setResonance (0.707f + 1.1f * b * b);

            // crunch: drive rises 0 -> +20 dB, output level-compensated
            const float crunch   = std::pow (ramp (b, 0.03f, 1.0f), 0.8f);
            const float drive    = dbToGain (20.0f * crunch);
            const float driveMix = ramp (b, 0.0f, 0.10f);              // fades in, no jump at the start
            const float driveComp = 1.0f / (0.7f * std::tanh (drive * 0.35f) / 0.35f + 0.3f * jmin (drive, 1.0f / 0.35f));

            // gate: 1/4 -> 1/8 -> 1/16 across 0-100, depth grows gently, open part compensated
            const bool   gateOn = b >= 0.02f;
            const double gateDiv = b < 0.34f ? 1.0 : b < 0.67f ? 0.5 : 0.25;
            const float  gateDepth = 0.8f * std::pow (ramp (b, 0.02f, 0.70f), 1.3f);
            const float  gateEnergy = 0.5f + 0.5f * (1.0f - gateDepth) * (1.0f - gateDepth);
            const float  gateComp = std::pow (gateEnergy, -0.35f);

            // echo: time 1/2 beat -> 1/128 beat (a few ms = resonant buzz), feedback up
            const float e = std::pow (ramp (b, 0.15f, 1.0f), 0.9f);
            const float echoBeats = 0.5f * std::exp2 (-6.0f * e);
            const float echoTarget = jlimit (8.0f, maxEcho, (float) (echoBeats * samplesPerBeat));
            const float echoSend = 0.7f * ramp (b, 0.10f, 0.50f);
            const float echoFb = 0.30f + 0.60f * ramp (b, 0.20f, 1.0f);

            // room: big -> tiny
            const float revSend = 0.6f * std::pow (ramp (b, 0.05f, 0.5f), 0.8f) * (1.0f - 0.6f * ramp (b, 0.6f, 1.0f));
            room = 0.80f - 0.60f * b;

            widths[c] = 1.0f - std::pow (b, 1.2f);
            ratios[c] = 1.0f + 7.0f * ramp (b, 0.10f, 1.0f);
            lastSends = echoSend + revSend;

            for (int i = 0; i < len; ++i)
            {
                const int k = start + i, s = segStart + k;
                float xl = hpf2.processSample (0, hpf1.processSample (0, L[s]));
                float xr = stereo ? hpf2.processSample (1, hpf1.processSample (1, R[s])) : xl;

                if (driveMix > 0.0f)
                {
                    // 70% tanh saturation + 30% hard clip = crunch
                    const float dl = xl * drive, dr = xr * drive;
                    const float sl = (0.7f * std::tanh (dl) + 0.3f * jlimit (-1.0f, 1.0f, dl)) * driveComp;
                    const float sr2 = (0.7f * std::tanh (dr) + 0.3f * jlimit (-1.0f, 1.0f, dr)) * driveComp;
                    xl += driveMix * (sl - xl);
                    xr += driveMix * (sr2 - xr);
                }

                float g = 1.0f;
                if (gateOn)
                {
                    double frac = std::fmod (ppq + (double) s * beatsPerSample, gateDiv) / gateDiv;
                    if (frac < 0.0) frac += 1.0;
                    g = frac < 0.5 ? gateComp : gateComp * (1.0f - gateDepth);
                }
                gateGain = g + gateCoeff * (gateGain - g);
                xl *= gateGain;
                xr *= gateGain;

                echoSendSmoothed = echoSend + smoothCoeff5ms * (echoSendSmoothed - echoSend);
                revSendSmoothed  = revSend  + smoothCoeff5ms * (revSendSmoothed  - revSend);
                echoTime = echoTarget + echoGlideCoeff * (echoTime - echoTarget);

                // echo (feedback through a 150 Hz high-pass + soft clip = stable at high feedback)
                const float yl = readEcho (0, echoTime);
                const float yr = stereo ? readEcho (1, echoTime) : yl;
                echoFbHp[0] += echoHpCoeff * (yl - echoFbHp[0]);
                echoFbHp[1] += echoHpCoeff * (yr - echoFbHp[1]);
                echoBuf[0][(size_t) echoWrite] = xl * echoSendSmoothed + std::tanh (echoFb * (yl - echoFbHp[0]));
                echoBuf[1][(size_t) echoWrite] = xr * echoSendSmoothed + std::tanh (echoFb * (yr - echoFbHp[1]));
                echoWrite = (echoWrite + 1) & echoMask;

                tailPeak = jmax (tailPeak, std::abs (yl));

                rL[k] = xl * revSendSmoothed;
                rR[k] = xr * revSendSmoothed;
                L[s] = xl + yl * echoWet;
                if (stereo) R[s] = xr + yr * echoWet;
            }

            displayCrunchDb.store (20.0f * crunch * driveMix);
            displayCutoff.store (cutoff);
            displayEchoBeats.store (echoSend > 0.001f ? echoBeats : 0.0f);
            displayRatio.store (ratios[c]);
        }

        // ---- room ----
        Reverb::Parameters rp;
        rp.roomSize = room;
        rp.damping  = 0.25f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width    = 1.0f;
        reverb.setParameters (rp);
        if (stereo) reverb.processStereo (rL, rR, segLen);
        else        reverb.processMono (rL, segLen);

        // ---- pass 2: add room, squash, collapse to mono ----
        for (int c = 0, start = 0; start < segLen; ++c, start += chunkSize)
        {
            const int len = jmin (chunkSize, segLen - start);
            const float w = widths[c];
            const float slope = 1.0f - 1.0f / ratios[c];
            const float makeup = -compThresholdDb * slope * 0.4f;

            for (int i = 0; i < len; ++i)
            {
                const int k = start + i, s = segStart + k;
                float yl = L[s] + rL[k] * roomWet;
                float yr = stereo ? R[s] + rR[k] * roomWet : yl;
                tailPeak = jmax (tailPeak, std::abs (rL[k]));

                // compressor (stereo-linked peak detector)
                const float level = jmax (std::abs (yl), std::abs (yr));
                compEnv = level + (level > compEnv ? compAtt : compRel) * (compEnv - level);
                const float over = gainToDb (compEnv) - compThresholdDb;
                const float gain = dbToGain (makeup - (over > 0.0f ? over * slope : 0.0f));
                yl *= gain;
                yr *= gain;

                if (stereo)
                {
                    const float mid  = 0.5f * (yl + yr);
                    const float side = 0.5f * (yl - yr) * w;
                    L[s] = mid + side;
                    R[s] = mid - side;
                }
                else
                {
                    L[s] = yl;
                }
            }
        }

        hpf1.snapToZero();
        hpf2.snapToZero();
    }

    if (lastSends <= 0.0f && tailPeak < 1.0e-4f) ++silentBlocks;
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
