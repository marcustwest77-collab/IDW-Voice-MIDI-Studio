#pragma once
#include <JuceHeader.h>

// Custom LookAndFeel for IDW Voice MIDI Studio.
// Replaces JUCE's stock rotary knob with a brushed-metal bezel, a gold value
// arc, and a gold pointer -- matching the "black gloss / gold trim" UI spec
// in Resources/UI_SPEC.md. Colours are read via findColour() so the existing
// theme.setColour(...) calls in PluginEditor.cpp still control the palette.
class IDWLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(4.0f);
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto centre = bounds.getCentre();
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        const auto fillColour = slider.findColour(juce::Slider::rotarySliderFillColourId);
        const juce::Colour trackBackground(0xff1b2130);

        // Brushed-metal bezel: a top-lit radial-ish gradient disc.
        juce::ColourGradient bezelGrad(juce::Colour(0xff2b3549), centre.x, bounds.getY(),
                                        juce::Colour(0xff11151d), centre.x, bounds.getBottom(), false);
        g.setGradientFill(bezelGrad);
        g.fillEllipse(bounds);
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.drawEllipse(bounds.reduced(0.5f), 1.0f);

        // Background track for the full sweep range.
        const float arcRadius = radius - 6.0f;
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(trackBackground);
        g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Gold value arc from start to the current position.
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(fillColour);
        g.strokePath(valueArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Pointer line.
        juce::Path pointer;
        const float pointerLength = radius * 0.62f;
        const float pointerThickness = 2.6f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -radius + 5.0f, pointerThickness, pointerLength, 1.3f);
        pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
        g.setColour(fillColour);
        g.fillPath(pointer);

        // Centre cap.
        auto cap = juce::Rectangle<float>(radius * 0.34f, radius * 0.34f).withCentre(centre);
        g.setColour(juce::Colour(0xff0b0e14));
        g.fillEllipse(cap);
        g.setColour(fillColour.withAlpha(0.55f));
        g.drawEllipse(cap, 1.0f);
    }
};
