#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Drop TNT v2: one "Build" knob (0-100) for build-ups.
//   Build up  -> resonant low cut sweeps up, tempo-synced gate gets denser
//                (1/4 > 1/8 > 1/16 > 1/32), a post-gate reverb send fills the gaps,
//                stereo narrows to mono.
//   Pull to 0 -> lows + width + full groove snap back; the reverb tail rings into the drop.
//
// CPU: at Build 0 (once the reverb tail has died) the plugin is a straight pass-through.
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
    std::atomic<int>   displayDivision { -1 };   // -1 = off, 0 = 1/4, 1 = 1/8, 2 = 1/16, 3 = 1/32
    std::atomic<float> displayCutoff   { 20.0f };
    std::atomic<float> displayReverb   { 0.0f };
    std::atomic<float> displayWidth    { 1.0f };

    static const char* divisionName (int idx) noexcept;

private:
    std::atomic<float>* buildParam = nullptr;

    // Fixed voicing (one-knob design)
    static constexpr float cutMax     = 1200.0f;  // low cut at Build 100 (Hz)
    static constexpr float sendMax    = 0.85f;    // reverb send at Build 100
    static constexpr float gateFloor  = 0.0f;     // gate closes fully, reverb fills the gap

    double sr = 44100.0;
    double internalPpq = 0.0;

    juce::SmoothedValue<float> buildSmooth;
    juce::dsp::StateVariableTPTFilter<float> hpf1, hpf2;
    juce::Reverb reverb;
    juce::AudioBuffer<float> sendBuffer;

    float gateGain = 1.0f;
    float gateCoeff = 0.99f;
    float sendSmoothed = 0.0f;
    bool  idle = true;
    int   silentBlocks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DropTNTProcessor)
};
