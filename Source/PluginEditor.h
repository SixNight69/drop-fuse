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

class DropTNTEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DropTNTEditor (DropTNTProcessor&);
    ~DropTNTEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void explode();

    struct Geometry
    {
        juce::Point<float> topLeft;
        float px = 0.0f, pxY = 0.0f, width = 0.0f;
        std::array<float, 22> colX {};          // x offset of each art column (artW + 1)
        juce::Point<float> sparkPos;
    };
    Geometry getGeometry() const;
    int  burnedFuseRows() const;

    void rebuildBackground();
    void drawSky (juce::Graphics&);
    void drawGround (juce::Graphics&);
    void drawDynamite (juce::Graphics&, const Geometry&, float flash, float alpha);
    void drawParticles (juce::Graphics&);

    struct Particle
    {
        juce::Point<float> pos, vel;
        float life, maxLife, size, gravity;
        juce::Colour colour;
    };

    PixelLookAndFeel lnf;              // destroyed last
    DropTNTProcessor& processor;
    std::atomic<float>* buildParam = nullptr;

    juce::Slider buildSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> buildAttachment;

    juce::Rectangle<int> sceneArea;
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
    juce::Image background;
    float lastPaintedBuild = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DropTNTEditor)
};
