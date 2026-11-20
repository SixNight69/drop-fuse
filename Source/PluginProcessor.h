#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Drop Fuse: one "Build" macro (0-100) for build-ups.
//   Build up  -> tempo-synced volume gate gets denser (1/4 > 1/8 > 1/16 > 1/32),
//                resonant low cut sweeps up, pitch rises, stereo narrows to mono.
//   Pull to 0 -> everything snaps back: full lows, full width = the drop hits.
class DropFuseProcessor : public juce::AudioProcessor
{
public:
    DropFuseProcessor();
    ~DropFuseProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.0; }

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
    std::atomic<int>   displayDivision { -1 };   // -1 = gate off, 0 = 1/4, 1 = 1/8, 2 = 1/16, 3 = 1/32
    std::atomic<float> displayCutoff   { 20.0f };
    std::atomic<float> displaySemis    { 0.0f };
    std::atomic<float> displayWidth    { 1.0f };

    static constexpr int numDivisions = 4;
    static const char* divisionName (int idx) noexcept;

private:
    float readDelay (int ch, float delaySamples) const noexcept;

    std::atomic<float>* buildParam    = nullptr;

    // Fixed voicing (one-knob design)
    static constexpr float gateMax  = 1.0f;     // gate fully closes
    static constexpr float cutMax   = 1200.0f;  // low cut at Build 100 (Hz)
    static constexpr float pitchMax = 5.0f;     // semitones at Build 100
    static constexpr float narrow   = 1.0f;     // fully mono at Build 100

    double sr = 44100.0;
    double internalPpq = 0.0;

    juce::SmoothedValue<float> buildSmooth;
    juce::dsp::StateVariableTPTFilter<float> hpf1, hpf2;

    float gateGain = 1.0f;
    float gateCoeff = 0.99f;

    // delay-line pitch shifter (two crossfaded taps)
    std::vector<float> pitchBuf[2];
    int   pitchMask = 0;
    int   writePos = 0;
    float pitchPhase = 0.0f;
    float pitchWindow = 1764.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DropFuseProcessor)
};
