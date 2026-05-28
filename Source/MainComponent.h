#pragma once

#include <JuceHeader.h>

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    struct ProfilePoint
    {
        float x = 0.0f; // 0..1, from outside to inside
        float y = 0.0f; // 0..1, profile height
    };

    struct ColourProfile
    {
        float angleDeg = 0.0f;
        std::vector<juce::Colour> colours;
    };

    struct SampledProfilePoint
    {
        float x = 0.0f;
        float y = 0.0f;
        juce::Colour colour;
    };

    enum class PreviewShape
    {
        circle,
        square
    };

    std::vector<ProfilePoint> profilePoints;
    std::vector<ColourProfile> colourProfiles;

    PreviewShape previewShape = PreviewShape::circle;
    bool autoShadeEnabled = true;

    int selectedColourProfileIndex = 0;
    int draggedColourProfileIndex = -1;
    int draggedPointIndex = -1;
    int selectedPointIndex = -1;

    juce::Rectangle<float> getProfileArea() const;
    juce::Rectangle<float> getPreviewArea() const;
    juce::Rectangle<float> getColourProfileBarArea() const;
    juce::Rectangle<float> getColourPaletteArea() const;

    juce::Point<float> profileToScreen (const ProfilePoint&, juce::Rectangle<float>) const;
    ProfilePoint screenToProfile (juce::Point<float>, juce::Rectangle<float>, int pointIndex) const;

    SampledProfilePoint sampleProfileAt (float x, int colourProfileIndex) const;
    juce::Colour getPointColour (int pointIndex) const;
    void setPointColour (int pointIndex, juce::Colour colour);
    juce::Colour getAutoShadedPointColour (int pointIndex, int colourProfileIndex) const;
    void bakeAutoShadeIntoSelectedProfile();

    std::vector<juce::Colour> getPaletteColours() const;

    void drawProfileEditor (juce::Graphics&, juce::Rectangle<float>);
    void drawColourProfileBar (juce::Graphics&, juce::Rectangle<float>);
    void drawColourPalette (juce::Graphics&, juce::Rectangle<float>);
    void drawPreview (juce::Graphics&, juce::Rectangle<float>);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
