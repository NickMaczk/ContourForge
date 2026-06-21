#pragma once

#include <JuceHeader.h>

class MainComponent : public juce::Component, private juce::ChangeListener
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
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

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
        square,
        rectangle,
        custom
    };

    enum class PreviewMode
    {
        heightMap,
        normalMap,
        material,
        cavity,
        ambientOcclusion
    };

    enum class PreviewSlider
    {
        none,
        range,
        gloss,
        elevation,
        depth,
        cavityLayer,
        aoLayer,
        cavityBounce,
        shadowLayer,
        dropShadow,
        dropShadowLength,
        dropShadowSoftness,
        dropShadowFalloff,
        contourAoBlur,
        contourAoOpacity,
        specularLayer,
        specularCatch,
        chamferLayer,
        degradation,
        radius
    };

    std::vector<ProfilePoint> profilePoints;

    PreviewShape previewShape = PreviewShape::circle;
    PreviewMode previewMode = PreviewMode::heightMap;
    float cavityPropagation = 0.22f;
    float ambientOcclusionPropagation = 0.18f;
    juce::Colour baseColour = juce::Colour::fromRGB (170, 146, 105);
    float lightAngleDeg = 225.0f;
    float lightElevation = 0.62f;
    float glossAmount = 0.45f;
    float beautyStrength = 1.0f;
    float cavityLayerOpacity = 1.0f;
    float aoLayerOpacity = 1.0f;
    float cavityBounceAmount = 0.0f;
    float shadowLayerAmount = 0.0f;
    float dropShadowAmount = 0.0f;
    float dropShadowLength = 0.40f;   // 0..1, shadow reach (overrides elevation)
    float dropShadowSoftness = 0.30f; // 0..1, how fast the penumbra widens
    float dropShadowFalloff = 0.30f;  // 0..1, how fast the shadow fades from contact to tip
    float contourAoBlur = 0.0f;       // 0..1, blur radius of the contour AO band
    float contourAoOpacity = 0.0f;    // 0..1, strength of the contour AO band
    float specularLayerAmount = 1.0f;
    float specularCatchAmount = 0.60f;
    float chamferAmount = 0.0f;
    int gridDivisor = 0;
    float aspectRatioX = 3.0f; // free aspect ratio, default 3:1
    float aspectRatioY = 1.0f;
    int roundedCornerMask = 0; // TL=1, TR=2, BR=4, BL=8
    float cornerRadiusAmount = 0.50f;
    int previewQualityDivisor = 2;

    float toolsScrollOffset = 0.0f;  // vertical scroll of the tools panel
    float toolsContentHeight = 0.0f; // measured each paint, for scroll clamping
    bool draggingToolsScrollbar = false;

    // Custom shape loaded from SVG: a normalised distance-from-edge field
    // (-1 outside, 0..1 inside) sampled in place of the analytic shapes.
    std::vector<float> customShapeField;
    int customFieldW = 0;
    int customFieldH = 0;
    juce::String customShapeSvg; // kept so the shape persists in saved projects

    juce::String statusText;
    std::unique_ptr<juce::FileChooser> fileChooser;

    int draggedPointIndex = -1;
    int selectedPointIndex = -1;
    PreviewSlider draggedPreviewSlider = PreviewSlider::none;

    juce::Rectangle<float> getProfileArea() const;
    juce::Rectangle<float> getPreviewArea() const;
    juce::Rectangle<float> getToolsArea() const;
    juce::Rectangle<float> getToolsViewport() const;
    juce::Rectangle<float> getToolsScrollTrack() const;
    juce::Rectangle<float> getToolsScrollThumb() const;
    void setToolsScrollFromMouseY (float y);
    juce::Rectangle<float> getSaveButtonArea() const;
    juce::Rectangle<float> getLoadButtonArea() const;
    juce::Rectangle<float> getExportButtonArea() const;

    juce::Point<float> profileToScreen (const ProfilePoint&, juce::Rectangle<float>) const;
    ProfilePoint screenToProfile (juce::Point<float>, juce::Rectangle<float>, int pointIndex) const;

    SampledProfilePoint sampleProfileAt (float x) const;

    juce::var createProjectState() const;
    bool applyProjectState (const juce::var& state);
    void showSaveDialog();
    void showLoadDialog();
    bool saveProjectToFile (const juce::File& file) const;
    bool loadProjectFromFile (const juce::File& file);

    void showExportDialog();

    void showCustomShapeDialog();
    bool loadCustomShapeFromSvg (const juce::String& svgText);

    // castShadowMode: 0 = none, 1 = shape composited over its cast shadow,
    // 2 = cast shadow only (alpha). renderRegion is the geometry-space rectangle
    // the image spans; when empty it defaults to the shape's bounding box.
    juce::Image renderMap (PreviewMode mode,
                           juce::Rectangle<float> shapeArea,
                           int pixelWidth,
                           int pixelHeight,
                           bool transparentBackground,
                           int supersample = 1,
                           juce::Rectangle<float> renderRegion = {},
                           int castShadowMode = 0) const;

    struct CastShadowParams
    {
        float lengthPx = 0.0f;     // how far the shadow reaches, in shape pixels
        float halfSpreadRad = 0.0f; // angular half-width of the light cone (penumbra)
        float dirX = 0.0f;         // shadow direction (screen space), away from light
        float dirY = 0.0f;
    };

    // Geometry-derived cast shadow: direction from the light angle, length and
    // softness from elevation (high sun = short/sharp, low sun = long/soft).
    CastShadowParams castShadowParams (float shapeSize) const;

    void drawProfileEditor (juce::Graphics&, juce::Rectangle<float>);
    void drawPreview (juce::Graphics&, juce::Rectangle<float>);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
