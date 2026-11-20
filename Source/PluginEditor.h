#pragma once

#include "PluginProcessor.h"

class PixelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PixelLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;

    static juce::Font pixelFont (float size);
};

class DropFuseEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DropFuseEditor (DropFuseProcessor&);
    ~DropFuseEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void explode();

    struct Geometry { juce::Point<float> topLeft; float px; juce::Point<float> sparkPos; };
    Geometry getGeometry() const;
    int  burnedFuseRows() const;

    void drawSky (juce::Graphics&);
    void drawGround (juce::Graphics&);
    void drawDynamite (juce::Graphics&, const Geometry&, float flash, float alpha);
    void drawParticles (juce::Graphics&);
    void drawReadout (juce::Graphics&);

    struct Particle
    {
        juce::Point<float> pos, vel;
        float life, maxLife, size, gravity;
        juce::Colour colour;
    };

    PixelLookAndFeel lnf;              // destroyed last
    DropFuseProcessor& processor;
    std::atomic<float>* buildParam = nullptr;

    juce::Slider buildSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> buildAttachment;

    juce::Rectangle<int> sceneArea, buildArea, readoutArea;
    float groundY = 0.0f;

    std::vector<Particle> particles;
    juce::Random rng;

    float build = 0.0f;
    float flashPhase = 0.0f, currentFlash = 0.0f;
    float peak = 0.0f;
    float screenFlash = 0.0f;
    float boomShake = 0.0f;
    float hiddenTime = 0.0f;
    float respawn = 1.0f;
    float boomText = 0.0f;
    juce::Point<float> shakeOffset;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DropFuseEditor)
};
