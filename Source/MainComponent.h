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

    struct SampledProfilePoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    enum class PreviewShape
    {
        circle,
        square
    };

    enum class PreviewMode
    {
        heightMap,
        cavity
    };

    std::vector<ProfilePoint> profilePoints;

    PreviewShape previewShape = PreviewShape::circle;
    PreviewMode previewMode = PreviewMode::heightMap;
    float cavityPropagation = 0.22f;

    juce::String statusText;
    std::unique_ptr<juce::FileChooser> fileChooser;

    int draggedPointIndex = -1;
    int selectedPointIndex = -1;

    juce::Rectangle<float> getProfileArea() const;
    juce::Rectangle<float> getPreviewArea() const;
    juce::Rectangle<float> getSaveButtonArea() const;
    juce::Rectangle<float> getLoadButtonArea() const;

    juce::Point<float> profileToScreen (const ProfilePoint&, juce::Rectangle<float>) const;
    ProfilePoint screenToProfile (juce::Point<float>, juce::Rectangle<float>, int pointIndex) const;

    SampledProfilePoint sampleProfileAt (float x) const;

    juce::var createProjectState() const;
    bool applyProjectState (const juce::var& state);
    void showSaveDialog();
    void showLoadDialog();
    bool saveProjectToFile (const juce::File& file) const;
    bool loadProjectFromFile (const juce::File& file);

    void drawProfileEditor (juce::Graphics&, juce::Rectangle<float>);
    void drawPreview (juce::Graphics&, juce::Rectangle<float>);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
