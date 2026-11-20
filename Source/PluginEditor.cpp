#include "PluginEditor.h"

using namespace juce;

namespace
{
    // Original pixel art: a bundle of three dynamite sticks with a fuse.
    // . empty  y stick cap  H highlight  R red  r shadow  k band highlight  K band  F fuse
    const char* const dynamiteArt[] = {
        "..........F..........",
        ".........F...........",
        ".........F...........",
        "..........F..........",
        ".yyyyy..yyyyy..yyyyy.",
        ".HRRRr..HRRRr..HRRRr.",
        ".HRRRr..HRRRr..HRRRr.",
        ".HRRRr..HRRRr..HRRRr.",
        ".HRRRr..HRRRr..HRRRr.",
        "kkkkkkkkkkkkkkkkkkkkk",
        "KKKKKKKKKKKKKKKKKKKKK",
        ".HRRRr..HRRRr..HRRRr.",
        ".HRRRr..HRRRr..HRRRr.",
        ".HRRRr..HRRRr..HRRRr.",
        ".HRRRr..HRRRr..HRRRr.",
        ".HRRRr..HRRRr..HRRRr.",
        ".rrrrr..rrrrr..rrrrr.",
    };
    constexpr int artW = 21, artH = 17, fuseRows = 4;
    constexpr float basePixel = 9.0f;
    constexpr float dt = 1.0f / 60.0f;

    Colour artColour (char c)
    {
        switch (c)
        {
            case 'y': return Colour (0xffe8d3a2);
            case 'H': return Colour (0xffff6b5b);
            case 'R': return Colour (0xffd62f2f);
            case 'r': return Colour (0xff8e1b1b);
            case 'k': return Colour (0xff4a4a4a);
            case 'K': return Colour (0xff1e1e1e);
            case 'F': return Colour (0xff9a8a74);
            default:  return Colours::transparentBlack;
        }
    }

    const Colour bgDark   { 0xff0e1116 };
    const Colour xpGreen  { 0xff7ed321 };
    const Colour hotRed   { 0xffff3b30 };
    const Colour textDim  { 0xff8a93a6 };
}

//==============================================================================
PixelLookAndFeel::PixelLookAndFeel()
{
    setColour (Slider::textBoxTextColourId,    Colours::white.withAlpha (0.85f));
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Label::textColourId,            textDim);
}

Font PixelLookAndFeel::pixelFont (float size)
{
    return Font (FontOptions (Font::getDefaultMonospacedFontName(), size, Font::bold));
}

Font PixelLookAndFeel::getLabelFont (Label&) { return pixelFont (13.0f); }

void PixelLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                         float pos, float startAngle, float endAngle, Slider&)
{
    const auto area = Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float r = jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto c = area.getCentre();

    // chunky pixel ring: 16 square segments
    const int segs = r > 50.0f ? 28 : 16;
    const int lit = roundToInt (pos * (float) segs);
    const float sq = jmax (4.0f, r * (r > 50.0f ? 0.13f : 0.22f));
    for (int i = 0; i < segs; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * ((float) i + 0.5f) / (float) segs;
        auto p = c.getPointOnCircumference (r - sq * 0.6f, a);
        auto rect = Rectangle<float> (sq, sq).withCentre (p).toNearestInt().toFloat();
        g.setColour (i < lit ? i < segs / 2 ? xpGreen.interpolatedWith (Colour (0xffffd60a), 2.0f * (float) i / (float) segs)
                                    : Colour (0xffffd60a).interpolatedWith (hotRed, 2.0f * (float) i / (float) segs - 1.0f) : Colour (0xff2a2f38));
        g.fillRect (rect);
    }

    // square knob body
    const float kb = r * (r > 50.0f ? 1.05f : 0.95f);
    auto body = Rectangle<float> (kb, kb).withCentre (c).toNearestInt().toFloat();
    g.setColour (Colour (0xff3a3f4a)); g.fillRect (body);
    g.setColour (Colour (0xff555b68)); g.fillRect (body.removeFromTop (3.0f));
    g.setColour (Colour (0xff1c1f26)); g.fillRect (body.removeFromBottom (3.0f));

    const float ang = startAngle + pos * (endAngle - startAngle);
    auto tip = c.getPointOnCircumference (kb * 0.36f, ang);
    g.setColour (Colours::white);
    const float ts = jmax (5.0f, r * 0.12f);
    g.fillRect (Rectangle<float> (ts, ts).withCentre (tip).toNearestInt().toFloat());
}

//==============================================================================
DropTNTEditor::DropTNTEditor (DropTNTProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);
    buildParam = processor.apvts.getRawParameterValue ("build");

    buildSlider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    buildSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    buildSlider.setMouseDragSensitivity (400);
    buildSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (buildSlider);
    buildAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "build", buildSlider);

    setSize (760, 540);
    startTimerHz (60);
}

DropTNTEditor::~DropTNTEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DropTNTEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (56);
    sceneArea = area.removeFromTop (280);
    groundY = (float) sceneArea.getBottom() - 48.0f;

    readoutArea = area.removeFromTop (34).reduced (20, 0);
    buildArea = area.reduced (20, 10);
    buildSlider.setBounds (buildArea.withSizeKeepingCentre (170, 170));
    rebuildBackground();
}

//==============================================================================
int DropTNTEditor::burnedFuseRows() const
{
    // fuse burns down as Build rises (always leaves the last pixel lit)
    return jlimit (0, fuseRows - 1, (int) (build * fuseRows));
}

DropTNTEditor::Geometry DropTNTEditor::getGeometry() const
{
    // swells as it gets close to blowing, pops back in after respawn
    const float swell = 1.0f + 0.10f * build * build + 0.03f * currentFlash;
    const float px = basePixel * swell * (0.6f + 0.4f * respawn);

    Geometry geo;
    geo.px = px;
    geo.topLeft = { (float) sceneArea.getCentreX() - artW * px * 0.5f + shakeOffset.x,
                    groundY - artH * px + shakeOffset.y };

    const int row = burnedFuseRows();
    int col = 10;
    for (int c = 0; c < artW; ++c)
        if (dynamiteArt[row][c] == 'F') { col = c; break; }
    geo.sparkPos = geo.topLeft + Point<float> (((float) col + 0.5f) * px, ((float) row + 0.3f) * px);
    return geo;
}

void DropTNTEditor::explode()
{
    hiddenTime  = 1.1f;
    screenFlash = 1.0f;
    boomShake   = 20.0f;
    boomText    = 1.0f;
    peak        = 0.0f;

    const auto geo = getGeometry();
    const auto centre = geo.topLeft + Point<float> (artW * geo.px * 0.5f, artH * geo.px * 0.55f);
    const Colour palette[] = { Colours::white, Colour (0xffe0e0e0), Colour (0xffa0a0a0),
                               Colour (0xff5c5c5c), Colour (0xffffc107), Colour (0xffff6f00),
                               Colour (0xffd62f2f), Colour (0xffe8d3a2) };

    for (int i = 0; i < 160; ++i)
    {
        const float ang   = rng.nextFloat() * MathConstants<float>::twoPi;
        const float speed = 120.0f + rng.nextFloat() * 620.0f;
        Particle pt;
        pt.pos = centre + Point<float> (rng.nextFloat() * 40.0f - 20.0f, rng.nextFloat() * 40.0f - 20.0f);
        pt.vel = { std::cos (ang) * speed, std::sin (ang) * speed - 120.0f };
        pt.maxLife = pt.life = 0.4f + rng.nextFloat() * 1.0f;
        pt.size = basePixel * (0.6f + rng.nextFloat() * 1.6f);
        pt.gravity = 420.0f;
        pt.colour = palette[rng.nextInt (8)];
        particles.push_back (pt);
    }
    for (int i = 0; i < 26; ++i)   // slow smoke puffs
    {
        const float ang = rng.nextFloat() * MathConstants<float>::twoPi;
        Particle pt;
        pt.pos = centre;
        pt.vel = { std::cos (ang) * 90.0f, std::sin (ang) * 60.0f - 50.0f };
        pt.maxLife = pt.life = 0.9f + rng.nextFloat() * 0.8f;
        pt.size = basePixel * (2.5f + rng.nextFloat() * 2.5f);
        pt.gravity = -30.0f;
        pt.colour = Colour (0xff6b6b6b);
        particles.push_back (pt);
    }
}

void DropTNTEditor::timerCallback()
{
    build = jlimit (0.0f, 1.0f, buildParam->load() / 100.0f);

    // blink: faster and brighter as Build rises
    flashPhase += dt * (0.6f + build * build * 13.0f);
    flashPhase -= std::floor (flashPhase);
    currentFlash = (build > 0.02f && flashPhase < 0.5f) ? 0.15f + 0.75f * build : 0.0f;

    // drop detection: was high recently, now pulled down -> BOOM
    peak = jmax (build, peak - dt * 0.35f);
    if (hiddenTime <= 0.0f && peak > 0.55f && build < 0.08f)
        explode();

    // shake
    boomShake = jmax (0.0f, boomShake - dt * 45.0f);
    const float shakeAmt = (hiddenTime > 0.0f ? 0.0f : build * build * 6.0f) + boomShake;
    shakeOffset = { (rng.nextFloat() * 2.0f - 1.0f) * shakeAmt, (rng.nextFloat() * 2.0f - 1.0f) * shakeAmt * 0.6f };

    if (hiddenTime > 0.0f)
    {
        hiddenTime -= dt;
        if (hiddenTime <= 0.0f) respawn = 0.0f;
    }
    respawn = jmin (1.0f, respawn + dt * 5.0f);
    screenFlash = jmax (0.0f, screenFlash - dt * 2.2f);
    boomText = jmax (0.0f, boomText - dt * 1.1f);

    // fuse sparks
    if (hiddenTime <= 0.0f && build > 0.01f)
    {
        const auto spark = getGeometry().sparkPos;
        const int count = (int) (build * 4.0f + rng.nextFloat());
        for (int i = 0; i < count; ++i)
        {
            Particle pt;
            pt.pos = spark;
            pt.vel = { (rng.nextFloat() * 2.0f - 1.0f) * (60.0f + 160.0f * build),
                       -(40.0f + rng.nextFloat() * (120.0f + 200.0f * build)) };
            pt.maxLife = pt.life = 0.2f + rng.nextFloat() * 0.35f;
            pt.size = 3.0f + rng.nextFloat() * 4.0f;
            pt.gravity = 380.0f;
            pt.colour = rng.nextBool() ? Colour (0xffffd60a) : Colour (0xffff8c00);
            particles.push_back (pt);
        }
    }

    for (auto& pt : particles)
    {
        pt.vel.y += pt.gravity * dt;
        pt.vel *= 0.985f;
        pt.pos += pt.vel * dt;
        pt.life -= dt;
    }
    particles.erase (std::remove_if (particles.begin(), particles.end(),
                                     [] (const Particle& pt) { return pt.life <= 0.0f; }),
                     particles.end());
    if (particles.size() > 600)
        particles.erase (particles.begin(), particles.begin() + (long) (particles.size() - 600));

    // only redraw when something is actually moving (idle GUI = ~0 CPU)
    const bool animating = build > 0.001f || ! particles.empty() || screenFlash > 0.0f || boomText > 0.0f
                           || hiddenTime > 0.0f || respawn < 1.0f || boomShake > 0.0f;
    const int div = processor.displayDivision.load();
    if (animating || std::abs (build - lastPaintedBuild) > 1.0e-4f || div != lastPaintedDivision)
    {
        lastPaintedBuild = build;
        lastPaintedDivision = div;
        repaint();
    }
}

//==============================================================================
void DropTNTEditor::drawSky (Graphics& g)
{
    // night sky in coarse colour bands (pixel gradient)
    const Colour top (0xff121a33), bottom (0xff2b2350);
    constexpr int bands = 10;
    const float bandH = (groundY - (float) sceneArea.getY()) / bands;
    for (int i = 0; i < bands; ++i)
    {
        g.setColour (top.interpolatedWith (bottom, (float) i / (bands - 1)));
        g.fillRect (Rectangle<float> ((float) sceneArea.getX(), (float) sceneArea.getY() + bandH * (float) i,
                                      (float) sceneArea.getWidth(), bandH + 1.0f));
    }

    Random starRng (1234);
    for (int i = 0; i < 40; ++i)
    {
        const float sx = (float) sceneArea.getX() + starRng.nextFloat() * (float) sceneArea.getWidth();
        const float sy = (float) sceneArea.getY() + starRng.nextFloat() * (groundY - (float) sceneArea.getY() - 60.0f);
        const float sz = starRng.nextBool() ? 3.0f : 2.0f;
        g.setColour (Colours::white.withAlpha (0.35f + 0.5f * starRng.nextFloat()));
        g.fillRect (Rectangle<float> (std::round (sx), std::round (sy), sz, sz));
    }
}

void DropTNTEditor::drawGround (Graphics& g)
{
    const float p = basePixel;
    const int cols = (int) std::ceil ((float) sceneArea.getWidth() / p);
    const int rows = (int) std::ceil (((float) sceneArea.getBottom() - groundY) / p);
    Random groundRng (77);

    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
        {
            const float n = groundRng.nextFloat();
            Colour col;
            if (r == 0)      col = n < 0.3f ? Colour (0xff4c9a2a) : Colour (0xff5fb832);
            else if (r == 1) col = n < 0.5f ? Colour (0xff4c9a2a) : Colour (0xff7a5536);
            else             col = n < 0.2f ? Colour (0xff5e3f27) : n < 0.35f ? Colour (0xff8a6342) : Colour (0xff7a5536);
            g.setColour (col);
            g.fillRect (Rectangle<float> ((float) sceneArea.getX() + (float) c * p,
                                          groundY + (float) r * p, p, p));
        }
}

void DropTNTEditor::rebuildBackground()
{
    // sky, stars and ground never change: draw them once (at 2x for sharp pixels on Retina)
    constexpr float scale = 2.0f;
    background = Image (Image::RGB, sceneArea.getWidth() * (int) scale, sceneArea.getHeight() * (int) scale, true);
    Graphics bg (background);
    bg.addTransform (AffineTransform::translation ((float) -sceneArea.getX(), (float) -sceneArea.getY()).scaled (scale));
    drawSky (bg);
    drawGround (bg);
}

void DropTNTEditor::drawDynamite (Graphics& g, const Geometry& geo, float flash, float alpha)
{
    const int burned = burnedFuseRows();
    for (int r = 0; r < artH; ++r)
    {
        if (r < burned) continue;   // burned part of the fuse is gone
        for (int c = 0; c < artW; ++c)
        {
            const char ch = dynamiteArt[r][c];
            if (ch == '.') continue;
            auto col = artColour (ch);
            if (ch != 'F') col = col.interpolatedWith (Colours::white, flash);
            g.setColour (col.withMultipliedAlpha (alpha));
            g.fillRect (Rectangle<float> (geo.topLeft.x + (float) c * geo.px, geo.topLeft.y + (float) r * geo.px,
                                          geo.px + 0.5f, geo.px + 0.5f));
        }
    }

    // glowing fuse tip
    if (build > 0.01f)
    {
        const float s = geo.px * (0.9f + 0.4f * currentFlash);
        g.setColour (Colour (0xffffd60a).withMultipliedAlpha (alpha));
        g.fillRect (Rectangle<float> (s, s).withCentre (geo.sparkPos));
        g.setColour (Colours::white.withMultipliedAlpha (alpha));
        g.fillRect (Rectangle<float> (s * 0.45f, s * 0.45f).withCentre (geo.sparkPos));
    }
}

void DropTNTEditor::drawParticles (Graphics& g)
{
    for (const auto& pt : particles)
    {
        const float t = pt.life / pt.maxLife;
        const float s = std::round (pt.size * (0.4f + 0.6f * t));
        g.setColour (pt.colour.withMultipliedAlpha (jmin (1.0f, t * 1.5f)));
        g.fillRect (Rectangle<float> (std::round (pt.pos.x - s * 0.5f), std::round (pt.pos.y - s * 0.5f), s, s));
    }
}

void DropTNTEditor::drawReadout (Graphics& g)
{
    const int div = processor.displayDivision.load();
    const float cut = processor.displayCutoff.load();
    const float rev = processor.displayReverb.load();
    const float width = processor.displayWidth.load();

    auto cutText = cut >= 1000.0f ? String (cut / 1000.0f, 2) + "kHz" : String (roundToInt (cut)) + "Hz";
    auto items = StringArray {
        String ("GATE ") + DropTNTProcessor::divisionName (div),
        "LOW CUT " + cutText,
        "REVERB " + String (roundToInt (rev * 100.0f)) + "%",
        "WIDTH " + String (roundToInt (width * 100.0f)) + "%"
    };

    auto area = readoutArea;
    const int w = area.getWidth() / items.size();
    g.setFont (PixelLookAndFeel::pixelFont (14.0f));
    for (int i = 0; i < items.size(); ++i)
    {
        auto cell = area.removeFromLeft (w);
        g.setColour (Colour (0xff181c24));
        g.fillRect (cell.reduced (3, 4));
        g.setColour (i == 0 && div >= 0 ? Colour (0xffffd60a) : Colours::white.withAlpha (0.8f));
        g.drawText (items[i], cell, Justification::centred);
    }
}

void DropTNTEditor::paint (Graphics& g)
{
    g.fillAll (bgDark);

    // header
    g.setFont (PixelLookAndFeel::pixelFont (28.0f));
    g.setColour (Colours::black);
    g.drawText ("DROP TNT", 23, 13, 300, 34, Justification::centredLeft);
    g.setColour (Colours::white);
    g.drawText ("DROP TNT", 20, 10, 300, 34, Justification::centredLeft);
    g.setFont (PixelLookAndFeel::pixelFont (12.0f));
    g.setColour (textDim);
    g.drawText ("build-up macro // draw automation 0 -> 100, pull to 0 at the drop",
                getLocalBounds().withHeight (56).reduced (20, 0), Justification::centredRight);

    // scene
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (sceneArea);
        g.setImageResamplingQuality (Graphics::lowResamplingQuality);
        g.drawImage (background, sceneArea.toFloat());

        // red glow builds up behind the dynamite
        if (build > 0.0f && hiddenTime <= 0.0f)
        {
            const auto c = Point<float> ((float) sceneArea.getCentreX(), groundY - 70.0f);
            g.setGradientFill (ColourGradient (hotRed.withAlpha (0.35f * build * build), c.x, c.y,
                                               hotRed.withAlpha (0.0f), c.x + 260.0f, c.y, true));
            g.fillRect (sceneArea.withBottom ((int) groundY));
        }

        // scorch mark after the blast
        if (hiddenTime > 0.0f)
        {
            g.setColour (Colours::black.withAlpha (0.45f));
            g.fillRect (Rectangle<float> (12.0f * basePixel, basePixel)
                            .withCentre ({ (float) sceneArea.getCentreX(), groundY + basePixel * 0.5f }));
        }
        if (hiddenTime <= 0.0f)
            drawDynamite (g, getGeometry(), currentFlash, respawn);
        drawParticles (g);

        if (boomText > 0.0f)
        {
            const float sz = 64.0f + (1.0f - boomText) * 40.0f;
            g.setFont (PixelLookAndFeel::pixelFont (sz));
            auto r = sceneArea.toFloat().withTrimmedBottom (90.0f);
            g.setColour (Colours::black.withAlpha (boomText));
            g.drawText ("DROP!", r.translated (5.0f, 5.0f), Justification::centred);
            g.setColour (Colour (0xffffd60a).withAlpha (boomText));
            g.drawText ("DROP!", r, Justification::centred);
        }
    }
    g.setColour (Colours::black);
    g.drawRect (sceneArea, 3);

    drawReadout (g);

    // build value
    const int value = roundToInt (build * 100.0f);
    g.setFont (PixelLookAndFeel::pixelFont (24.0f));
    g.setColour (value > 85 ? hotRed : value > 50 ? Colour (0xffffd60a) : xpGreen);
    auto knobBounds = buildSlider.getBounds();
    auto left  = buildArea.withRight (knobBounds.getX() - 20);
    auto right = buildArea.withLeft (knobBounds.getRight() + 20);
    g.drawText ("BUILD", left.withTrimmedBottom (left.getHeight() / 2), Justification::bottomRight);
    g.setFont (PixelLookAndFeel::pixelFont (52.0f));
    g.drawText (String (value).paddedLeft ('0', 3), left.withTrimmedTop (left.getHeight() / 2), Justification::topRight);

    g.setFont (PixelLookAndFeel::pixelFont (12.0f));
    g.setColour (textDim);
    g.drawFittedText ("DRAG UP / DOWN\nDOUBLE-CLICK = 0\n\nAUTOMATE: BUILD", right, Justification::centredLeft, 4);

    if (screenFlash > 0.0f)
    {
        g.setColour (Colours::white.withAlpha (screenFlash * 0.85f));
        g.fillAll();
    }
}
