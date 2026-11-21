#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Drop TNT v3: one "Build" knob (0-100). Everything squeezes into a point, then explodes.
//   Build up  -> low cut sweeps up (thin, piercing), crunch distortion drives harder,
//                tempo gate 1/4 -> 1/8 -> 1/16 (gentle, level-compensated),
//                DJ-style echo: time shrinks 1/2 beat -> a few ms with rising feedback
//                (pitch glides up into a sharp resonant buzz), room shrinks from big to tiny,
//                compressor squashes harder, stereo collapses to mono.
//   Pull to 0 -> lows, width, dynamics and full groove snap back = the drop.
//
// CPU: at Build 0 (once tails have died) the plugin is a straight pass-through.
class DropTNTProcessor : public juce::AudioProcessor
{
public:
    DropTNTProcessor();
    ~DropTNTProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Live readouts for the GUI
    std::atomic<float> displayCrunchDb  { 0.0f };
    std::atomic<float> displayCutoff    { 20.0f };
    std::atomic<float> displayEchoBeats { 0.0f }; // 0 = echo off
    std::atomic<float> displayRatio     { 1.0f };


private:
    float readEcho (int ch, float delaySamples) const noexcept;

    std::atomic<float>* buildParam = nullptr;

    // Fixed voicing (one-knob design)
    static constexpr float cutMax = 1800.0f;   // low cut at Build 100 (Hz)

    double sr = 44100.0;
    double internalPpq = 0.0;

    juce::SmoothedValue<float> buildSmooth;
    juce::dsp::StateVariableTPTFilter<float> hpf1, hpf2;

    // room (shrinks as Build rises)
    juce::Reverb reverb;
    juce::AudioBuffer<float> revBuffer;
    float revSendSmoothed = 0.0f;

    // DJ-style echo
    std::vector<float> echoBuf[2];
    int   echoMask = 0, echoWrite = 0;
    float echoTime = 1000.0f;          // samples, glides (pitch sweep)
    float echoGlideCoeff = 0.999f;
    float echoSendSmoothed = 0.0f;
    float echoFbHp[2] { 0.0f, 0.0f };  // one-pole high-pass state in the feedback loop
    float echoHpCoeff = 0.97f;


    // gate (1/4 -> 1/8 -> 1/16, gentle depth, level-compensated)
    float gateGain = 1.0f, gateCoeff = 0.99f;

    // compressor ("squash")
    float compEnv = 0.0f, compAtt = 0.9f, compRel = 0.999f;

    float smoothCoeff5ms = 0.99f;
    bool  idle = true;
    int   silentBlocks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DropTNTProcessor)
};
