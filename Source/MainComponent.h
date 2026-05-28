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
        juce::Colour colour;
    };

    std::vector<ProfilePoint> profilePoints;
    int draggedPointIndex = -1;
    int selectedPointIndex = -1;

    juce::Rectangle<float> getProfileArea() const;
    juce::Rectangle<float> getPreviewArea() const;

    juce::Point<float> profileToScreen (const ProfilePoint&, juce::Rectangle<float>) const;
    ProfilePoint screenToProfile (juce::Point<float>, juce::Rectangle<float>, int pointIndex) const;

    ProfilePoint sampleProfileAt (float x) const;

    void drawProfileEditor (juce::Graphics&, juce::Rectangle<float>);
    void drawPreview (juce::Graphics&, juce::Rectangle<float>);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
