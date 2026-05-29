#include "MainComponent.h"

MainComponent::MainComponent()
{
    setSize (1360, 680);

    profilePoints =
    {
        { 0.00f, 0.10f },
        { 0.16f, 0.82f },
        { 0.36f, 0.48f },
        { 0.62f, 0.66f },
        { 1.00f, 0.18f }
    };
}

MainComponent::~MainComponent() = default;

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (14, 14, 16));

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("ContourForge", 24, 18, 300, 30, juce::Justification::left);

    g.setFont (juce::FontOptions (13.0f));
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawText ("Drag profile points. Double-click to add. Right-click to remove.",
                24, 46, 720, 22, juce::Justification::left);

    auto drawTopButton = [&] (juce::Rectangle<float> button, const juce::String& text)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (button, 6.0f);

        g.setColour (juce::Colours::white.withAlpha (0.38f));
        g.drawRoundedRectangle (button, 6.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.68f));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (text, button, juce::Justification::centred);
    };

    drawTopButton (getSaveButtonArea(), "Save");
    drawTopButton (getLoadButtonArea(), "Load");

    if (statusText.isNotEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.38f));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (statusText, 420, 22, 360, 24, juce::Justification::centredRight);
    }

    drawProfileEditor (g, getProfileArea());
    drawPreview (g, getPreviewArea());
}

void MainComponent::resized()
{
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (auto* selector = dynamic_cast<juce::ColourSelector*> (source))
    {
        baseColour = selector->getCurrentColour();
        previewMode = PreviewMode::material;
        repaint();
    }
}

juce::Rectangle<float> MainComponent::getSaveButtonArea() const
{
    auto r = getLocalBounds().toFloat().reduced (24.0f);
    return { r.getRight() - 172.0f, 20.0f, 78.0f, 28.0f };
}

juce::Rectangle<float> MainComponent::getLoadButtonArea() const
{
    auto r = getLocalBounds().toFloat().reduced (24.0f);
    return { r.getRight() - 86.0f, 20.0f, 78.0f, 28.0f };
}

void MainComponent::showSaveDialog()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Save ContourForge project",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("ContourForge.json"),
        "*.json");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();

            if (file == juce::File{})
                return;

            statusText = saveProjectToFile (file)
                ? "Saved " + file.getFileName()
                : "Save failed";

            repaint();
        });
}

void MainComponent::showLoadDialog()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load ContourForge project",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.json");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();

            if (file == juce::File{})
                return;

            statusText = loadProjectFromFile (file)
                ? "Loaded " + file.getFileName()
                : "Load failed";

            repaint();
        });
}

void MainComponent::mouseDown (const juce::MouseEvent& e)
{
    const auto area = getProfileArea();
    const auto mouse = e.position;

    if (getSaveButtonArea().contains (mouse))
    {
        showSaveDialog();
        return;
    }

    if (getLoadButtonArea().contains (mouse))
    {
        showLoadDialog();
        return;
    }

    {
        auto previewControls = getToolsArea().reduced (18.0f);
        previewControls.removeFromTop (54.0f);

        auto nextToolRow = [&] (float height = 24.0f)
        {
            auto row = previewControls.removeFromTop (height);
            previewControls.removeFromTop (8.0f);
            return row;
        };

        auto shapeRow = nextToolRow();
        const auto circleButton = shapeRow.removeFromLeft (72.0f);
        shapeRow.removeFromLeft (8.0f);
        const auto squareButton = shapeRow.removeFromLeft (80.0f);
        shapeRow.removeFromLeft (8.0f);
        const auto rectButton = shapeRow.removeFromLeft (62.0f);
        shapeRow.removeFromLeft (8.0f);
        const auto gridButton = shapeRow.removeFromLeft (74.0f);

        auto modeRow = nextToolRow();
        const auto heightButton = modeRow.removeFromLeft (60.0f);
        modeRow.removeFromLeft (6.0f);
        const auto normalButton = modeRow.removeFromLeft (64.0f);
        modeRow.removeFromLeft (6.0f);
        const auto materialButton = modeRow.removeFromLeft (64.0f);
        modeRow.removeFromLeft (6.0f);
        const auto cavityButton = modeRow.removeFromLeft (62.0f);
        modeRow.removeFromLeft (6.0f);
        const auto aoButton = modeRow.removeFromLeft (42.0f);

        const auto baseButton = nextToolRow();
        const auto degradationSlider = nextToolRow();
        const auto rangeSlider = nextToolRow();
        const auto elevationSlider = nextToolRow();
        const auto radiusSlider = nextToolRow();

        const auto depthSlider = nextToolRow();
        const auto cavityLayerSlider = nextToolRow();
        const auto aoLayerSlider = nextToolRow();
        const auto shadowLayerSlider = nextToolRow();
        const auto specularLayerSlider = nextToolRow();
        const auto specularCatchSlider = nextToolRow();
        const auto chamferSlider = nextToolRow();
        const auto glossSlider = nextToolRow();

        auto ratioRow = nextToolRow();
        const auto ratioButton = ratioRow.removeFromLeft (96.0f);
        ratioRow.removeFromLeft (6.0f);
        const auto cornersButton = ratioRow.removeFromLeft (94.0f);

        auto shapeGridArea = previewControls.removeFromTop (62.0f).removeFromLeft (62.0f);

        auto setPreviewSliderValue = [&] (PreviewSlider slider, juce::Rectangle<float> sliderArea, float mouseX)
        {
            const auto amount = juce::jlimit (0.0f, 1.0f, (mouseX - sliderArea.getX()) / juce::jmax (1.0f, sliderArea.getWidth()));

            if (slider == PreviewSlider::range)
            {
                if (previewMode == PreviewMode::material)
                    lightAngleDeg = amount * 360.0f;
                else if (previewMode == PreviewMode::cavity)
                    cavityPropagation = 0.04f + amount * (0.80f - 0.04f);
                else
                {
                    ambientOcclusionPropagation = 0.04f + amount * (0.80f - 0.04f);
                    previewMode = PreviewMode::ambientOcclusion;
                }
            }
            else if (slider == PreviewSlider::gloss)
            {
                glossAmount = amount * 2.0f;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::elevation)
            {
                lightElevation = 0.10f + amount * 0.90f;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::depth)
            {
                beautyStrength = amount * 2.0f;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::cavityLayer)
            {
                cavityLayerOpacity = amount;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::aoLayer)
            {
                aoLayerOpacity = amount;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::shadowLayer)
            {
                shadowLayerAmount = amount;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::specularLayer)
            {
                specularLayerAmount = amount;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::specularCatch)
            {
                specularCatchAmount = amount;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::chamferLayer)
            {
                chamferAmount = amount;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::degradation)
            {
                previewQualityDivisor = amount < 0.33f ? 2 : amount < 0.66f ? 4 : 8;
            }
            else if (slider == PreviewSlider::radius)
            {
                cornerRadiusAmount = 0.25f + amount * 0.75f;
                previewShape = PreviewShape::rectangle;
            }

            repaint();
        };

        if (circleButton.contains (mouse))
        {
            previewShape = PreviewShape::rectangle;
            aspectPresetIndex = 0;
            roundedCornerMask = 15;
            cornerRadiusAmount = 1.0f;
            repaint();
            return;
        }

        if (squareButton.contains (mouse))
        {
            previewShape = PreviewShape::rectangle;
            aspectPresetIndex = 0;
            roundedCornerMask = 0;
            repaint();
            return;
        }

        if (rectButton.contains (mouse))
        {
            previewShape = PreviewShape::rectangle;
            roundedCornerMask = 0;
            repaint();
            return;
        }

        if (shapeGridArea.contains (mouse))
        {
            const auto cellW = shapeGridArea.getWidth() / 3.0f;
            const auto cellH = shapeGridArea.getHeight() / 3.0f;

            const auto col = juce::jlimit (0, 2, (int) ((mouse.x - shapeGridArea.getX()) / cellW));
            const auto row = juce::jlimit (0, 2, (int) ((mouse.y - shapeGridArea.getY()) / cellH));

            auto toggleCorner = [&] (int bit)
            {
                roundedCornerMask ^= bit;
            };

            if (row == 0 && col == 0) toggleCorner (1);      // TL
            else if (row == 0 && col == 2) toggleCorner (2); // TR
            else if (row == 2 && col == 2) toggleCorner (4); // BR
            else if (row == 2 && col == 0) toggleCorner (8); // BL
            else if (row == 0 && col == 1) roundedCornerMask = roundedCornerMask == 3  ? 0 : 3;   // top
            else if (row == 2 && col == 1) roundedCornerMask = roundedCornerMask == 12 ? 0 : 12;  // bottom
            else if (row == 1 && col == 0) roundedCornerMask = roundedCornerMask == 9  ? 0 : 9;   // left
            else if (row == 1 && col == 2) roundedCornerMask = roundedCornerMask == 6  ? 0 : 6;   // right
            else if (row == 1 && col == 1) roundedCornerMask = roundedCornerMask == 15 ? 0 : 15;  // all/sharp

            previewShape = PreviewShape::rectangle;
            repaint();
            return;
        }

        if (baseButton.contains (mouse))
        {
            auto selector = std::make_unique<juce::ColourSelector> (
                juce::ColourSelector::showColourAtTop
                | juce::ColourSelector::showSliders
                | juce::ColourSelector::showColourspace);

            selector->setName ("Base Color");
            selector->setCurrentColour (baseColour);
            selector->addChangeListener (this);
            selector->setSize (320, 420);

            juce::CallOutBox::launchAsynchronously (
                std::move (selector),
                baseButton.toNearestInt(),
                this);

            previewMode = PreviewMode::material;
            repaint();
            return;
        }

        if (degradationSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::degradation;
            setPreviewSliderValue (draggedPreviewSlider, degradationSlider, mouse.x);
            return;
        }

        if (gridButton.contains (mouse))
        {
            gridDivisor = gridDivisor == 0 ? 4
                : gridDivisor == 4 ? 8
                : gridDivisor == 8 ? 16
                : gridDivisor == 16 ? 32
                : 0;

            repaint();
            return;
        }

        if (heightButton.contains (mouse))
        {
            previewMode = PreviewMode::heightMap;
            repaint();
            return;
        }

        if (normalButton.contains (mouse))
        {
            previewMode = PreviewMode::normalMap;
            repaint();
            return;
        }

        if (materialButton.contains (mouse))
        {
            previewMode = PreviewMode::material;
            repaint();
            return;
        }

        if (cavityButton.contains (mouse))
        {
            previewMode = PreviewMode::cavity;
            repaint();
            return;
        }

        if (aoButton.contains (mouse))
        {
            previewMode = PreviewMode::ambientOcclusion;
            repaint();
            return;
        }

        if (rangeSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::range;
            setPreviewSliderValue (draggedPreviewSlider, rangeSlider, mouse.x);
            return;
        }

        if (glossSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::gloss;
            setPreviewSliderValue (draggedPreviewSlider, glossSlider, mouse.x);
            return;
        }

        if (elevationSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::elevation;
            setPreviewSliderValue (draggedPreviewSlider, elevationSlider, mouse.x);
            return;
        }

        if (depthSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::depth;
            setPreviewSliderValue (draggedPreviewSlider, depthSlider, mouse.x);
            return;
        }

        if (cavityLayerSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::cavityLayer;
            setPreviewSliderValue (draggedPreviewSlider, cavityLayerSlider, mouse.x);
            return;
        }

        if (aoLayerSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::aoLayer;
            setPreviewSliderValue (draggedPreviewSlider, aoLayerSlider, mouse.x);
            return;
        }

        if (shadowLayerSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::shadowLayer;
            setPreviewSliderValue (draggedPreviewSlider, shadowLayerSlider, mouse.x);
            return;
        }

        if (specularLayerSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::specularLayer;
            setPreviewSliderValue (draggedPreviewSlider, specularLayerSlider, mouse.x);
            return;
        }

        if (specularCatchSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::specularCatch;
            setPreviewSliderValue (draggedPreviewSlider, specularCatchSlider, mouse.x);
            return;
        }

        if (chamferSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::chamferLayer;
            setPreviewSliderValue (draggedPreviewSlider, chamferSlider, mouse.x);
            return;
        }

        if (ratioButton.contains (mouse))
        {
            aspectPresetIndex = (aspectPresetIndex + 1) % 7;
            previewShape = PreviewShape::rectangle;
            repaint();
            return;
        }

        if (cornersButton.contains (mouse))
        {
            const int masks[] { 0, 15, 3, 12, 9, 6, 1, 2, 4, 8 };

            int currentIndex = 0;

            for (int i = 0; i < 10; ++i)
            {
                if (masks[i] == roundedCornerMask)
                {
                    currentIndex = i;
                    break;
                }
            }

            roundedCornerMask = masks[(currentIndex + 1) % 10];
            previewShape = PreviewShape::rectangle;
            repaint();
            return;
        }

        if (radiusSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::radius;
            setPreviewSliderValue (draggedPreviewSlider, radiusSlider, mouse.x);
            return;
        }

    }

    draggedPointIndex = -1;
    selectedPointIndex = -1;

    for (int i = 0; i < (int) profilePoints.size(); ++i)
    {
        if (profileToScreen (profilePoints[(size_t) i], area).getDistanceFrom (mouse) < 12.0f)
        {
            selectedPointIndex = i;

            if (e.mods.isPopupMenu() && i > 0 && i < (int) profilePoints.size() - 1)
            {
                profilePoints.erase (profilePoints.begin() + i);

                selectedPointIndex = -1;
                repaint();
                return;
            }

            draggedPointIndex = i;
            break;
        }
    }

    repaint();
}

void MainComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedPreviewSlider != PreviewSlider::none)
    {
        auto previewControls = getToolsArea().reduced (18.0f);
        previewControls.removeFromTop (54.0f);

        auto nextToolRow = [&] (float height = 24.0f)
        {
            auto row = previewControls.removeFromTop (height);
            previewControls.removeFromTop (8.0f);
            return row;
        };

        nextToolRow(); // shape row
        nextToolRow(); // mode row
        nextToolRow(); // base color

        const auto degradationSlider = nextToolRow();
        const auto rangeSlider = nextToolRow();
        const auto elevationSlider = nextToolRow();
        const auto radiusSlider = nextToolRow();

        const auto depthSlider = nextToolRow();
        const auto cavityLayerSlider = nextToolRow();
        const auto aoLayerSlider = nextToolRow();
        const auto shadowLayerSlider = nextToolRow();
        const auto specularLayerSlider = nextToolRow();
        const auto specularCatchSlider = nextToolRow();
        const auto chamferSlider = nextToolRow();
        const auto glossSlider = nextToolRow();

        juce::Rectangle<float> sliderArea;

        if (draggedPreviewSlider == PreviewSlider::range)
            sliderArea = rangeSlider;
        else if (draggedPreviewSlider == PreviewSlider::gloss)
            sliderArea = glossSlider;
        else if (draggedPreviewSlider == PreviewSlider::elevation)
            sliderArea = elevationSlider;
        else if (draggedPreviewSlider == PreviewSlider::depth)
            sliderArea = depthSlider;
        else if (draggedPreviewSlider == PreviewSlider::cavityLayer)
            sliderArea = cavityLayerSlider;
        else if (draggedPreviewSlider == PreviewSlider::aoLayer)
            sliderArea = aoLayerSlider;
        else if (draggedPreviewSlider == PreviewSlider::shadowLayer)
            sliderArea = shadowLayerSlider;
        else if (draggedPreviewSlider == PreviewSlider::specularLayer)
            sliderArea = specularLayerSlider;
        else if (draggedPreviewSlider == PreviewSlider::specularCatch)
            sliderArea = specularCatchSlider;
        else if (draggedPreviewSlider == PreviewSlider::chamferLayer)
            sliderArea = chamferSlider;
        else if (draggedPreviewSlider == PreviewSlider::degradation)
            sliderArea = degradationSlider;
        else if (draggedPreviewSlider == PreviewSlider::radius)
            sliderArea = radiusSlider;

        const auto amount = juce::jlimit (0.0f, 1.0f, (e.position.x - sliderArea.getX()) / juce::jmax (1.0f, sliderArea.getWidth()));

        if (draggedPreviewSlider == PreviewSlider::range)
        {
            if (previewMode == PreviewMode::material)
                lightAngleDeg = amount * 360.0f;
            else if (previewMode == PreviewMode::cavity)
                cavityPropagation = 0.04f + amount * (0.80f - 0.04f);
            else
            {
                ambientOcclusionPropagation = 0.04f + amount * (0.80f - 0.04f);
                previewMode = PreviewMode::ambientOcclusion;
            }
        }
        else if (draggedPreviewSlider == PreviewSlider::gloss)
        {
            glossAmount = amount * 2.0f;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::elevation)
        {
            lightElevation = 0.10f + amount * 0.90f;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::depth)
        {
            beautyStrength = amount * 2.0f;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::cavityLayer)
        {
            cavityLayerOpacity = amount;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::aoLayer)
        {
            aoLayerOpacity = amount;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::shadowLayer)
        {
            shadowLayerAmount = amount;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::specularLayer)
        {
            specularLayerAmount = amount;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::specularCatch)
        {
            specularCatchAmount = amount;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::chamferLayer)
        {
            chamferAmount = amount;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::degradation)
        {
            previewQualityDivisor = amount < 0.33f ? 2 : amount < 0.66f ? 4 : 8;
        }
        else if (draggedPreviewSlider == PreviewSlider::radius)
        {
            cornerRadiusAmount = 0.25f + amount * 0.75f;
            previewShape = PreviewShape::rectangle;
        }

        repaint();
        return;
    }

    if (draggedPointIndex < 0)
        return;

    profilePoints[(size_t) draggedPointIndex] =
        screenToProfile (e.position, getProfileArea(), draggedPointIndex);

    repaint();
}

void MainComponent::mouseUp (const juce::MouseEvent&)
{
    draggedPointIndex = -1;
    draggedPreviewSlider = PreviewSlider::none;
    repaint();
}

void MainComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto area = getProfileArea();

    if (! area.contains (e.position))
        return;

    for (const auto& point : profilePoints)
        if (profileToScreen (point, area).getDistanceFrom (e.position) < 12.0f)
            return;

    auto graph = area;
    graph.removeFromBottom (28.0f);
    graph = graph.reduced (28.0f);

    if (! graph.contains (e.position))
        return;

    auto x = juce::jlimit (0.0f, 1.0f, (e.position.x - graph.getX()) / graph.getWidth());
    auto y = juce::jlimit (0.0f, 1.0f, (graph.getBottom() - e.position.y) / graph.getHeight());

    if (gridDivisor > 0)
    {
        x = juce::jlimit (0.0f, 1.0f, std::round (x * (float) gridDivisor) / (float) gridDivisor);
        y = juce::jlimit (0.0f, 1.0f, std::round (y * (float) gridDivisor) / (float) gridDivisor);
    }

    if (x <= 0.02f || x >= 0.98f)
        return;

    auto insertIndex = 1;

    while (insertIndex < (int) profilePoints.size() && profilePoints[(size_t) insertIndex].x < x)
        ++insertIndex;

    if (x - profilePoints[(size_t) insertIndex - 1].x < 0.025f)
        return;

    if (profilePoints[(size_t) insertIndex].x - x < 0.025f)
        return;

    ProfilePoint newPoint;
    newPoint.x = x;
    newPoint.y = y;

    profilePoints.insert (profilePoints.begin() + insertIndex, newPoint);

    selectedPointIndex = insertIndex;
    draggedPointIndex = insertIndex;

    repaint();
}

juce::Rectangle<float> MainComponent::getProfileArea() const
{
    auto r = getLocalBounds().toFloat().reduced (24.0f);
    r.removeFromTop (72.0f);
    return r.removeFromLeft (getWidth() * 0.30f).reduced (0.0f, 12.0f);
}

juce::Rectangle<float> MainComponent::getPreviewArea() const
{
    auto r = getLocalBounds().toFloat().reduced (24.0f);
    r.removeFromTop (72.0f);
    r.removeFromLeft (getWidth() * 0.30f);
    r.removeFromRight (540.0f);
    return r.reduced (18.0f, 12.0f);
}

juce::Rectangle<float> MainComponent::getToolsArea() const
{
    auto r = getLocalBounds().toFloat().reduced (24.0f);
    r.removeFromTop (72.0f);
    return r.removeFromRight (540.0f).reduced (18.0f, 12.0f);
}

juce::Point<float> MainComponent::profileToScreen (const ProfilePoint& p, juce::Rectangle<float> area) const
{
    auto graph = area;
    graph.removeFromBottom (28.0f);
    graph = graph.reduced (28.0f);

    return
    {
        graph.getX() + p.x * graph.getWidth(),
        graph.getBottom() - p.y * graph.getHeight()
    };
}

MainComponent::ProfilePoint MainComponent::screenToProfile (juce::Point<float> p,
                                                            juce::Rectangle<float> area,
                                                            int pointIndex) const
{
    auto graph = area;
    graph.removeFromBottom (28.0f);
    graph = graph.reduced (28.0f);

    auto x = (p.x - graph.getX()) / graph.getWidth();
    auto y = (graph.getBottom() - p.y) / graph.getHeight();

    auto snapToGrid = [&] (float value)
    {
        if (gridDivisor <= 0)
            return value;

        return juce::jlimit (0.0f, 1.0f,
            std::round (value * (float) gridDivisor) / (float) gridDivisor);
    };

    const auto isFirst = pointIndex == 0;
    const auto isLast = pointIndex == (int) profilePoints.size() - 1;

    if (isFirst)
        x = 0.0f;
    else if (isLast)
        x = 1.0f;
    else
    {
        const auto minX = profilePoints[(size_t) pointIndex - 1].x + 0.025f;
        const auto maxX = profilePoints[(size_t) pointIndex + 1].x - 0.025f;
        x = juce::jlimit (minX, maxX, snapToGrid (x));
    }

    ProfilePoint result = profilePoints[(size_t) pointIndex];
    result.x = juce::jlimit (0.0f, 1.0f, x);
    result.y = snapToGrid (juce::jlimit (0.0f, 1.0f, y));
    return result;
}

MainComponent::SampledProfilePoint MainComponent::sampleProfileAt (float x) const
{
    x = juce::jlimit (0.0f, 1.0f, x);

    for (int i = 0; i < (int) profilePoints.size() - 1; ++i)
    {
        const auto& a = profilePoints[(size_t) i];
        const auto& b = profilePoints[(size_t) i + 1];

        if (x >= a.x && x <= b.x)
        {
            const auto amount = (x - a.x) / juce::jmax (0.0001f, b.x - a.x);

            return
            {
                x,
                juce::jmap (amount, a.y, b.y)
            };
        }
    }

    return
    {
        profilePoints.back().x,
        profilePoints.back().y
    };
}

juce::var MainComponent::createProjectState() const
{
    auto* root = new juce::DynamicObject();

    root->setProperty ("version", 2);
    root->setProperty ("previewShape",
        previewShape == PreviewShape::circle ? "circle"
        : previewShape == PreviewShape::rectangle ? "rectangle"
        : "square");
    root->setProperty ("previewMode",
        previewMode == PreviewMode::normalMap ? "normalMap"
        : previewMode == PreviewMode::material ? "material"
        : previewMode == PreviewMode::cavity ? "cavity"
        : previewMode == PreviewMode::ambientOcclusion ? "ambientOcclusion"
        : "heightMap");

    root->setProperty ("cavityPropagation", cavityPropagation);
    root->setProperty ("ambientOcclusionPropagation", ambientOcclusionPropagation);
    root->setProperty ("baseColour", baseColour.toDisplayString (true));
    root->setProperty ("lightAngleDeg", lightAngleDeg);
    root->setProperty ("lightElevation", lightElevation);
    root->setProperty ("glossAmount", glossAmount);
    root->setProperty ("beautyStrength", beautyStrength);
    root->setProperty ("cavityLayerOpacity", cavityLayerOpacity);
    root->setProperty ("aoLayerOpacity", aoLayerOpacity);
    root->setProperty ("shadowLayerAmount", shadowLayerAmount);
    root->setProperty ("specularLayerAmount", specularLayerAmount);
    root->setProperty ("specularCatchAmount", specularCatchAmount);
    root->setProperty ("chamferAmount", chamferAmount);
    root->setProperty ("previewQualityDivisor", previewQualityDivisor);
    root->setProperty ("gridDivisor", gridDivisor);
    root->setProperty ("aspectPresetIndex", aspectPresetIndex);
    root->setProperty ("roundedCornerMask", roundedCornerMask);
    root->setProperty ("cornerRadiusAmount", cornerRadiusAmount);

    juce::Array<juce::var> points;

    for (const auto& point : profilePoints)
    {
        auto* pointObject = new juce::DynamicObject();
        pointObject->setProperty ("x", point.x);
        pointObject->setProperty ("y", point.y);
        points.add (juce::var (pointObject));
    }

    root->setProperty ("profilePoints", juce::var (points));

    return juce::var (root);
}

bool MainComponent::applyProjectState (const juce::var& state)
{
    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return false;

    auto* pointsArray = root->getProperty ("profilePoints").getArray();

    if (pointsArray == nullptr || pointsArray->size() < 2)
        return false;

    std::vector<ProfilePoint> loadedPoints;

    for (const auto& pointVar : *pointsArray)
    {
        auto* pointObject = pointVar.getDynamicObject();

        if (pointObject == nullptr)
            return false;

        ProfilePoint point;
        point.x = juce::jlimit (0.0f, 1.0f, (float) (double) pointObject->getProperty ("x"));
        point.y = juce::jlimit (0.0f, 1.0f, (float) (double) pointObject->getProperty ("y"));

        loadedPoints.push_back (point);
    }

    loadedPoints.front().x = 0.0f;
    loadedPoints.back().x = 1.0f;

    profilePoints = std::move (loadedPoints);

    const auto previewShapeText = root->getProperty ("previewShape").toString();

    previewShape = previewShapeText == "rectangle"
        ? PreviewShape::rectangle
        : previewShapeText == "square"
            ? PreviewShape::square
            : PreviewShape::circle;

    const auto previewModeText = root->getProperty ("previewMode").toString();

    previewMode = previewModeText == "normalMap"
        ? PreviewMode::normalMap
        : previewModeText == "material"
            ? PreviewMode::material
            : previewModeText == "cavity"
            ? PreviewMode::cavity
            : previewModeText == "ambientOcclusion"
                ? PreviewMode::ambientOcclusion
                : PreviewMode::heightMap;

    cavityPropagation = juce::jlimit (0.04f, 0.80f, (float) (double) root->getProperty ("cavityPropagation"));

    const auto loadedAmbientOcclusionPropagation = root->getProperty ("ambientOcclusionPropagation");

    if (! loadedAmbientOcclusionPropagation.isVoid())
        ambientOcclusionPropagation = juce::jlimit (0.04f, 0.80f, (float) (double) loadedAmbientOcclusionPropagation);

    const auto loadedBaseColour = root->getProperty ("baseColour");

    if (! loadedBaseColour.isVoid())
        baseColour = juce::Colour::fromString (loadedBaseColour.toString());

    const auto loadedLightAngleDeg = root->getProperty ("lightAngleDeg");

    if (! loadedLightAngleDeg.isVoid())
        lightAngleDeg = (float) (double) loadedLightAngleDeg;

    while (lightAngleDeg < 0.0f)
        lightAngleDeg += 360.0f;

    while (lightAngleDeg >= 360.0f)
        lightAngleDeg -= 360.0f;

    const auto loadedLightElevation = root->getProperty ("lightElevation");

    if (! loadedLightElevation.isVoid())
        lightElevation = juce::jlimit (0.10f, 1.0f, (float) (double) loadedLightElevation);

    const auto loadedGlossAmount = root->getProperty ("glossAmount");

    if (! loadedGlossAmount.isVoid())
        glossAmount = juce::jlimit (0.0f, 2.0f, (float) (double) loadedGlossAmount);

    const auto loadedBeautyStrength = root->getProperty ("beautyStrength");

    if (! loadedBeautyStrength.isVoid())
        beautyStrength = juce::jlimit (0.0f, 2.0f, (float) (double) loadedBeautyStrength);

    const auto loadedCavityLayerOpacity = root->getProperty ("cavityLayerOpacity");

    if (! loadedCavityLayerOpacity.isVoid())
        cavityLayerOpacity = juce::jlimit (0.0f, 1.0f, (float) (double) loadedCavityLayerOpacity);

    const auto loadedAoLayerOpacity = root->getProperty ("aoLayerOpacity");

    if (! loadedAoLayerOpacity.isVoid())
        aoLayerOpacity = juce::jlimit (0.0f, 1.0f, (float) (double) loadedAoLayerOpacity);

    const auto loadedShadowLayerAmount = root->getProperty ("shadowLayerAmount");

    if (! loadedShadowLayerAmount.isVoid())
        shadowLayerAmount = juce::jlimit (0.0f, 1.0f, (float) (double) loadedShadowLayerAmount);

    const auto loadedSpecularLayerAmount = root->getProperty ("specularLayerAmount");

    if (! loadedSpecularLayerAmount.isVoid())
        specularLayerAmount = juce::jlimit (0.0f, 1.0f, (float) (double) loadedSpecularLayerAmount);

    const auto loadedSpecularCatchAmount = root->getProperty ("specularCatchAmount");

    if (! loadedSpecularCatchAmount.isVoid())
        specularCatchAmount = juce::jlimit (0.0f, 1.0f, (float) (double) loadedSpecularCatchAmount);

    const auto loadedChamferAmount = root->getProperty ("chamferAmount");

    if (! loadedChamferAmount.isVoid())
        chamferAmount = juce::jlimit (0.0f, 1.0f, (float) (double) loadedChamferAmount);

    const auto loadedPreviewQualityDivisor = (int) root->getProperty ("previewQualityDivisor");

    if (loadedPreviewQualityDivisor == 2 || loadedPreviewQualityDivisor == 4 || loadedPreviewQualityDivisor == 8)
        previewQualityDivisor = loadedPreviewQualityDivisor;

    const auto loadedGridDivisor = (int) root->getProperty ("gridDivisor");

    if (loadedGridDivisor == 0 || loadedGridDivisor == 4 || loadedGridDivisor == 8
        || loadedGridDivisor == 16 || loadedGridDivisor == 32)
        gridDivisor = loadedGridDivisor;

    const auto loadedAspectPresetIndex = (int) root->getProperty ("aspectPresetIndex");

    if (loadedAspectPresetIndex >= 0 && loadedAspectPresetIndex < 7)
        aspectPresetIndex = loadedAspectPresetIndex;

    const auto loadedRoundedCornerMask = (int) root->getProperty ("roundedCornerMask");

    if (loadedRoundedCornerMask >= 0 && loadedRoundedCornerMask <= 15)
        roundedCornerMask = loadedRoundedCornerMask;

    const auto loadedCornerRadiusAmount = root->getProperty ("cornerRadiusAmount");

    if (! loadedCornerRadiusAmount.isVoid())
        cornerRadiusAmount = juce::jlimit (0.25f, 1.0f, (float) (double) loadedCornerRadiusAmount);

    selectedPointIndex = -1;
    draggedPointIndex = -1;

    return true;
}

bool MainComponent::saveProjectToFile (const juce::File& file) const
{
    const auto target = file.hasFileExtension ("json")
        ? file
        : file.withFileExtension ("json");

    return target.replaceWithText (juce::JSON::toString (createProjectState(), true));
}

bool MainComponent::loadProjectFromFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    if (parsed.isVoid())
        return false;

    return applyProjectState (parsed);
}

void MainComponent::drawProfileEditor (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (juce::Colour::fromRGB (22, 22, 26));
    g.fillRoundedRectangle (area, 14.0f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (area, 14.0f, 1.0f);

    auto graph = area;
    graph.removeFromBottom (28.0f);
    graph = graph.reduced (28.0f);

    const auto gridSteps = gridDivisor > 0 ? gridDivisor : 4;

    g.setColour (juce::Colours::white.withAlpha (gridDivisor > 0 ? 0.10f : 0.08f));

    for (int i = 0; i <= gridSteps; ++i)
    {
        const auto y = graph.getY() + graph.getHeight() * i / (float) gridSteps;
        g.drawHorizontalLine ((int) y, graph.getX(), graph.getRight());
    }

    if (gridDivisor > 0)
    {
        for (int i = 0; i <= gridDivisor; ++i)
        {
            const auto x = graph.getX() + graph.getWidth() * i / (float) gridDivisor;
            g.drawVerticalLine ((int) x, graph.getY(), graph.getBottom());
        }
    }

    juce::Path curve;

    for (int i = 0; i < (int) profilePoints.size(); ++i)
    {
        const auto pt = profileToScreen (profilePoints[(size_t) i], area);

        if (i == 0)
            curve.startNewSubPath (pt);
        else
            curve.lineTo (pt);
    }

    g.setColour (juce::Colours::white.withAlpha (0.75f));
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    for (int i = 0; i < (int) profilePoints.size(); ++i)
    {
        const auto pt = profileToScreen (profilePoints[(size_t) i], area);
        const auto isDragged = i == draggedPointIndex;
        const auto isSelected = i == selectedPointIndex;

        g.setColour (juce::Colours::white.withAlpha (0.82f));
        g.fillEllipse (pt.x - 7.0f, pt.y - 7.0f, 14.0f, 14.0f);

        g.setColour (juce::Colours::white.withAlpha ((isDragged || isSelected) ? 0.95f : 0.5f));
        g.drawEllipse (pt.x - 7.0f, pt.y - 7.0f, 14.0f, 14.0f, (isDragged || isSelected) ? 2.0f : 1.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText ("Geometry profile", area.reduced (18.0f).removeFromTop (24.0f),
                juce::Justification::left);
}

void MainComponent::drawPreview (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (juce::Colour::fromRGB (22, 22, 26));
    g.fillRoundedRectangle (area, 14.0f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (area, 14.0f, 1.0f);

    const auto toolsArea = getToolsArea();

    g.setColour (juce::Colour::fromRGB (22, 22, 26));
    g.fillRoundedRectangle (toolsArea, 14.0f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (toolsArea, 14.0f, 1.0f);

    auto titleRow = area.reduced (18.0f).removeFromTop (24.0f);

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText ("Shape preview", titleRow.removeFromLeft (280.0f),
                juce::Justification::left);

    juce::String modeText = "Height";

    if (previewMode == PreviewMode::normalMap)
        modeText = "Normal";
    else if (previewMode == PreviewMode::material)
        modeText = "Beauty";
    else if (previewMode == PreviewMode::cavity)
        modeText = "Cavity";
    else if (previewMode == PreviewMode::ambientOcclusion)
        modeText = "AO";

    auto getAspectText = [&]()
    {
        switch (aspectPresetIndex)
        {
            case 0: return juce::String ("1:1");
            case 1: return juce::String ("2:1");
            case 2: return juce::String ("3:1");
            case 3: return juce::String ("4:3");
            case 4: return juce::String ("3:4");
            case 5: return juce::String ("1:2");
            case 6: return juce::String ("2:4");
            default: break;
        }

        return juce::String ("3:1");
    };

    auto getCornerText = [&]()
    {
        switch (roundedCornerMask)
        {
            case 0: return juce::String ("Sharp");
            case 15: return juce::String ("All");
            case 3: return juce::String ("Top");
            case 12: return juce::String ("Bottom");
            case 9: return juce::String ("Left");
            case 6: return juce::String ("Right");
            case 1: return juce::String ("TL");
            case 2: return juce::String ("TR");
            case 4: return juce::String ("BR");
            case 8: return juce::String ("BL");
            default: break;
        }

        return juce::String ("Custom");
    };

    const auto infoText =
        modeText
        + " | Light " + juce::String (juce::roundToInt (lightAngleDeg)) + juce::String::fromUTF8 ("\xC2\xB0")
        + " | Elev " + juce::String (juce::roundToInt (lightElevation * 100.0f)) + "%"
        + " | Gloss " + juce::String (juce::roundToInt (glossAmount * 50.0f)) + "%"
        + " | Height " + juce::String (juce::roundToInt (beautyStrength * 50.0f)) + "%"
        + " | Grid " + (gridDivisor == 0 ? juce::String ("Off") : "/" + juce::String (gridDivisor))
        + " | Ratio " + getAspectText()
        + " | Corners " + getCornerText()
        + " | Rad " + juce::String (juce::roundToInt (cornerRadiusAmount * 100.0f)) + "%"
        + " | Drag /" + juce::String (previewQualityDivisor);

    auto infoRow = toolsArea.reduced (18.0f).removeFromTop (54.0f).removeFromBottom (18.0f);

    g.setColour (juce::Colours::white.withAlpha (0.26f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (infoText, infoRow, juce::Justification::left);

    auto previewControls = getToolsArea().reduced (18.0f);
        previewControls.removeFromTop (54.0f);

        auto nextToolRow = [&] (float height = 24.0f)
        {
            auto row = previewControls.removeFromTop (height);
            previewControls.removeFromTop (8.0f);
            return row;
        };

        auto shapeRow = nextToolRow();
        const auto circleButton = shapeRow.removeFromLeft (72.0f);
        shapeRow.removeFromLeft (8.0f);
        const auto squareButton = shapeRow.removeFromLeft (80.0f);
        shapeRow.removeFromLeft (8.0f);
        const auto rectButton = shapeRow.removeFromLeft (62.0f);
        shapeRow.removeFromLeft (8.0f);
        const auto gridButton = shapeRow.removeFromLeft (74.0f);

        auto modeRow = nextToolRow();
        const auto heightButton = modeRow.removeFromLeft (60.0f);
        modeRow.removeFromLeft (6.0f);
        const auto normalButton = modeRow.removeFromLeft (64.0f);
        modeRow.removeFromLeft (6.0f);
        const auto materialButton = modeRow.removeFromLeft (64.0f);
        modeRow.removeFromLeft (6.0f);
        const auto cavityButton = modeRow.removeFromLeft (62.0f);
        modeRow.removeFromLeft (6.0f);
        const auto aoButton = modeRow.removeFromLeft (42.0f);

        const auto baseButton = nextToolRow();
        const auto degradationSlider = nextToolRow();
        const auto rangeSlider = nextToolRow();
        const auto elevationSlider = nextToolRow();
        const auto radiusSlider = nextToolRow();

        const auto depthSlider = nextToolRow();
        const auto cavityLayerSlider = nextToolRow();
        const auto aoLayerSlider = nextToolRow();
        const auto shadowLayerSlider = nextToolRow();
        const auto specularLayerSlider = nextToolRow();
        const auto specularCatchSlider = nextToolRow();
        const auto chamferSlider = nextToolRow();
        const auto glossSlider = nextToolRow();

        auto ratioRow = nextToolRow();
        const auto ratioButton = ratioRow.removeFromLeft (96.0f);
        ratioRow.removeFromLeft (6.0f);
        const auto cornersButton = ratioRow.removeFromLeft (94.0f);

        auto shapeGridArea = previewControls.removeFromTop (62.0f).removeFromLeft (62.0f);

    auto drawShapeButton = [&] (juce::Rectangle<float> button, const juce::String& text, bool selected)
    {
        g.setColour (juce::Colours::white.withAlpha (selected ? 0.18f : 0.07f));
        g.fillRoundedRectangle (button, 6.0f);

        g.setColour (juce::Colours::white.withAlpha (selected ? 0.85f : 0.35f));
        g.drawRoundedRectangle (button, 6.0f, selected ? 2.0f : 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.62f));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (text, button.reduced (6.0f, 0.0f), juce::Justification::centred);
    };

    auto drawPreviewSlider = [&] (juce::Rectangle<float> slider, const juce::String& text, float amount)
    {
        amount = juce::jlimit (0.0f, 1.0f, amount);

        g.setColour (juce::Colours::white.withAlpha (0.055f));
        g.fillRoundedRectangle (slider, 6.0f);

        auto fill = slider;
        fill.setWidth (slider.getWidth() * amount);

        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.fillRoundedRectangle (fill, 6.0f);

        g.setColour (juce::Colours::white.withAlpha (0.30f));
        g.drawRoundedRectangle (slider, 6.0f, 1.0f);

        const auto thumbX = slider.getX() + slider.getWidth() * amount;
        const auto thumb = juce::Rectangle<float> (thumbX - 3.0f, slider.getY() + 3.0f, 6.0f, slider.getHeight() - 6.0f);

        g.setColour (juce::Colours::white.withAlpha (0.54f));
        g.fillRoundedRectangle (thumb, 3.0f);

        g.setColour (juce::Colours::white.withAlpha (0.70f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (text, slider.reduced (7.0f, 0.0f), juce::Justification::centredLeft);
    };

    const auto isCirclePreset = previewShape == PreviewShape::rectangle
        && aspectPresetIndex == 0
        && roundedCornerMask == 15
        && cornerRadiusAmount >= 0.875f;

    drawShapeButton (circleButton, "Circle", previewShape == PreviewShape::circle || isCirclePreset);
    drawShapeButton (squareButton, "Square", previewShape == PreviewShape::rectangle && aspectPresetIndex == 0 && roundedCornerMask == 0);
    drawShapeButton (rectButton, "Rect", previewShape == PreviewShape::rectangle && aspectPresetIndex != 0 && roundedCornerMask == 0);
    drawShapeButton (gridButton, gridDivisor == 0 ? "Grid Off" : "Grid /" + juce::String (gridDivisor), gridDivisor > 0);

    drawShapeButton (heightButton, "Height", previewMode == PreviewMode::heightMap);
    drawShapeButton (normalButton, "Normal", previewMode == PreviewMode::normalMap);
    drawShapeButton (materialButton, "Beauty", previewMode == PreviewMode::material);
    drawShapeButton (cavityButton, "Cavity", previewMode == PreviewMode::cavity);
    drawShapeButton (aoButton, "AO", previewMode == PreviewMode::ambientOcclusion);

    g.setColour (baseColour.withAlpha (0.82f));
    g.fillRoundedRectangle (baseButton, 6.0f);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawRoundedRectangle (baseButton, 6.0f, 1.0f);

    g.setColour (baseColour.getBrightness() < 0.42f
        ? juce::Colours::white.withAlpha (0.72f)
        : juce::Colours::black.withAlpha (0.62f));

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("Base Color", baseButton.reduced (6.0f, 0.0f), juce::Justification::centred);

    const auto degradationAmount = previewQualityDivisor == 2 ? 0.0f
        : previewQualityDivisor == 4 ? 0.5f
        : 1.0f;

    const auto rangeAmount = previewMode == PreviewMode::material
        ? lightAngleDeg / 360.0f
        : previewMode == PreviewMode::cavity
            ? (cavityPropagation - 0.04f) / (0.80f - 0.04f)
            : (ambientOcclusionPropagation - 0.04f) / (0.80f - 0.04f);

    const auto rangeText = previewMode == PreviewMode::material
        ? "Light " + juce::String (juce::roundToInt (lightAngleDeg))
        : "Range " + juce::String (juce::roundToInt (rangeAmount * 100.0f)) + "%";

    drawPreviewSlider (degradationSlider, "Degradation /" + juce::String (previewQualityDivisor), degradationAmount);
    drawPreviewSlider (rangeSlider, rangeText, rangeAmount);
    drawPreviewSlider (elevationSlider, "Elevation " + juce::String (juce::roundToInt (lightElevation * 100.0f)) + "%", (lightElevation - 0.10f) / 0.90f);
    drawPreviewSlider (radiusSlider, "Radius " + juce::String (juce::roundToInt (cornerRadiusAmount * 100.0f)) + "%", (cornerRadiusAmount - 0.25f) / 0.75f);

    drawPreviewSlider (depthSlider, "Height layer " + juce::String (juce::roundToInt (beautyStrength * 50.0f)) + "%", beautyStrength / 2.0f);
    drawPreviewSlider (cavityLayerSlider, "Cavity layer " + juce::String (juce::roundToInt (cavityLayerOpacity * 100.0f)) + "%", cavityLayerOpacity);
    drawPreviewSlider (aoLayerSlider, "AO layer " + juce::String (juce::roundToInt (aoLayerOpacity * 100.0f)) + "%", aoLayerOpacity);
    drawPreviewSlider (shadowLayerSlider, "Shadow " + juce::String (juce::roundToInt (shadowLayerAmount * 100.0f)) + "%", shadowLayerAmount);
    drawPreviewSlider (specularLayerSlider, "Specular " + juce::String (juce::roundToInt (specularLayerAmount * 100.0f)) + "%", specularLayerAmount);
    drawPreviewSlider (specularCatchSlider, "Spec catch " + juce::String (juce::roundToInt (specularCatchAmount * 100.0f)) + "%", specularCatchAmount);
    drawPreviewSlider (chamferSlider, "Mini chamfer " + juce::String (juce::roundToInt (chamferAmount * 100.0f)) + "%", chamferAmount);
    drawPreviewSlider (glossSlider, "Glossiness " + juce::String (juce::roundToInt (glossAmount * 50.0f)) + "%", glossAmount / 2.0f);

    drawShapeButton (ratioButton, "Ratio " + getAspectText(), previewShape == PreviewShape::rectangle);
    drawShapeButton (cornersButton, "Corner " + getCornerText(), roundedCornerMask != 0);
    drawPreviewSlider (radiusSlider, "Rad " + juce::String (juce::roundToInt (cornerRadiusAmount * 100.0f)), (cornerRadiusAmount - 0.25f) / 0.75f);

    auto drawShapeGridCell = [&] (juce::Rectangle<float> cell, bool active, bool circular)
    {
        g.setColour (juce::Colours::white.withAlpha (0.055f));
        g.fillRoundedRectangle (cell, 4.0f);

        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawRoundedRectangle (cell, 4.0f, active ? 1.4f : 1.0f);

        auto mark = cell.reduced (5.0f);

        g.setColour (juce::Colours::white.withAlpha (0.28f));

        if (circular)
            g.drawEllipse (mark, active ? 1.4f : 1.0f);
        else
            g.drawRect (mark, active ? 1.4f : 1.0f);
    };

    const auto gap = 3.0f;
    const auto cellW = (shapeGridArea.getWidth() - gap * 2.0f) / 3.0f;
    const auto cellH = (shapeGridArea.getHeight() - gap * 2.0f) / 3.0f;

    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            const auto cell = juce::Rectangle<float>
            {
                shapeGridArea.getX() + (float) col * (cellW + gap),
                shapeGridArea.getY() + (float) row * (cellH + gap),
                cellW,
                cellH
            };

            int bit = 0;
            bool active = false;
            bool circular = false;

            if (row == 0 && col == 0) bit = 1;
            else if (row == 0 && col == 2) bit = 2;
            else if (row == 2 && col == 2) bit = 4;
            else if (row == 2 && col == 0) bit = 8;

            if (bit != 0)
            {
                active = (roundedCornerMask & bit) != 0;
                circular = active;
            }
            else if (row == 1 && col == 1)
            {
                active = roundedCornerMask == 15;
                circular = active;
            }
            else if (row == 0 && col == 1)
            {
                active = (roundedCornerMask & 3) == 3;
                circular = active;
            }
            else if (row == 2 && col == 1)
            {
                active = (roundedCornerMask & 12) == 12;
                circular = active;
            }
            else if (row == 1 && col == 0)
            {
                active = (roundedCornerMask & 9) == 9;
                circular = active;
            }
            else if (row == 1 && col == 2)
            {
                active = (roundedCornerMask & 6) == 6;
                circular = active;
            }

            drawShapeGridCell (cell, active, circular);
        }
    }

    auto shapeArea = area.reduced (46.0f, 58.0f);
    shapeArea.removeFromTop (34.0f);

    auto getAspectRatio = [&]()
    {
        switch (aspectPresetIndex)
        {
            case 0: return 1.0f / 1.0f;
            case 1: return 2.0f / 1.0f;
            case 2: return 3.0f / 1.0f;
            case 3: return 4.0f / 3.0f;
            case 4: return 3.0f / 4.0f;
            case 5: return 1.0f / 2.0f;
            case 6: return 2.0f / 4.0f;
            default: break;
        }

        return 3.0f / 1.0f;
    };

    if (previewShape == PreviewShape::rectangle)
    {
        const auto ratio = getAspectRatio();
        const auto availableRatio = shapeArea.getWidth() / juce::jmax (1.0f, shapeArea.getHeight());

        if (availableRatio > ratio)
            shapeArea = shapeArea.withSizeKeepingCentre (shapeArea.getHeight() * ratio, shapeArea.getHeight());
        else
            shapeArea = shapeArea.withSizeKeepingCentre (shapeArea.getWidth(), shapeArea.getWidth() / ratio);
    }
    else
    {
        const auto side = juce::jmin (shapeArea.getWidth(), shapeArea.getHeight());
        shapeArea = shapeArea.withSizeKeepingCentre (side, side);
    }

    const auto centre = shapeArea.getCentre();
    const auto outerRadius = juce::jmin (shapeArea.getWidth(), shapeArea.getHeight()) * 0.5f;

    auto normaliseAngle = [] (float angle)
    {
        while (angle < 0.0f)
            angle += 360.0f;

        while (angle >= 360.0f)
            angle -= 360.0f;

        return angle;
    };

    std::vector<ProfilePoint> renderProfilePoints;

    {
        const auto chamfer = juce::jlimit (0.0f, 1.0f, chamferAmount);

        if (chamfer <= 0.0001f || profilePoints.size() < 3)
        {
            renderProfilePoints = profilePoints;
        }
        else
        {
            renderProfilePoints.reserve (profilePoints.size() * 2);
            renderProfilePoints.push_back (profilePoints.front());

            const auto cutAmount = chamfer * 0.45f;

            auto lerpPoint = [] (const ProfilePoint& from, const ProfilePoint& to, float amount)
            {
                ProfilePoint result;
                result.x = from.x + (to.x - from.x) * amount;
                result.y = from.y + (to.y - from.y) * amount;
                return result;
            };

            for (int i = 1; i < (int) profilePoints.size() - 1; ++i)
            {
                const auto& previous = profilePoints[(size_t) i - 1];
                const auto& current  = profilePoints[(size_t) i];
                const auto& next     = profilePoints[(size_t) i + 1];

                const auto leftPoint  = lerpPoint (current, previous, cutAmount);
                const auto rightPoint = lerpPoint (current, next, cutAmount);

                renderProfilePoints.push_back (leftPoint);
                renderProfilePoints.push_back (rightPoint);
            }

            renderProfilePoints.push_back (profilePoints.back());
        }
    }

    auto sampleRenderProfileAt = [&] (float x)
    {
        x = juce::jlimit (0.0f, 1.0f, x);

        for (int i = 0; i < (int) renderProfilePoints.size() - 1; ++i)
        {
            const auto& a = renderProfilePoints[(size_t) i];
            const auto& b = renderProfilePoints[(size_t) i + 1];

            if (x >= a.x && x <= b.x)
            {
                const auto amount = (x - a.x) / juce::jmax (0.0001f, b.x - a.x);

                return SampledProfilePoint
                {
                    x,
                    juce::jmap (amount, a.y, b.y)
                };
            }
        }

        return SampledProfilePoint
        {
            renderProfilePoints.back().x,
            renderProfilePoints.back().y
        };
    };

    auto heightValueAt = [&] (float profileX)
    {
        const auto sample = sampleRenderProfileAt (profileX);
        return juce::jlimit (0.0f, 1.0f, sample.y);
    };

    auto cavityAt = [&] (float profileX)
    {
        const auto current = heightValueAt (profileX);

        const auto radius = cavityPropagation;
        constexpr int sampleCount = 18;

        float occlusion = 0.0f;

        for (int i = 1; i <= sampleCount; ++i)
        {
            const auto step = radius * (float) i / (float) sampleCount;

            const auto leftX = juce::jlimit (0.0f, 1.0f, profileX - step);
            const auto rightX = juce::jlimit (0.0f, 1.0f, profileX + step);

            const auto leftHeight = heightValueAt (leftX);
            const auto rightHeight = heightValueAt (rightX);

            const auto distanceFalloff = 1.0f - (float) i / (float) sampleCount;

            occlusion += juce::jmax (0.0f, leftHeight - current) * distanceFalloff;
            occlusion += juce::jmax (0.0f, rightHeight - current) * distanceFalloff;
        }

        occlusion = juce::jlimit (0.0f, 1.0f, occlusion * 0.42f);

        return 1.0f - occlusion;
    };

    auto colourAt = [&] (float profileX, float)
    {
        if (previewMode == PreviewMode::cavity)
        {
            const auto value = cavityAt (profileX);
            return juce::Colour::fromFloatRGBA (value, value, value, 1.0f);
        }

        const auto value = juce::jlimit (0.0f, 1.0f, 0.12f + heightValueAt (profileX) * 0.84f);
        return juce::Colour::fromFloatRGBA (value, value, value, 1.0f);
    };

    auto circularPointAt = [&] (float radius, float angleDeg)
    {
        const auto radians = (angleDeg - 90.0f) * juce::MathConstants<float>::pi / 180.0f;

        return juce::Point<float>
        {
            centre.x + std::cos (radians) * radius,
            centre.y + std::sin (radians) * radius
        };
    };

    auto shapePointAt = [&] (float x, float angleDeg)
    {
        const auto scale = 1.0f - x;
        const auto radians = (angleDeg - 90.0f) * juce::MathConstants<float>::pi / 180.0f;

        const auto dirX = std::cos (radians);
        const auto dirY = std::sin (radians);

        if (previewShape == PreviewShape::circle)
            return juce::Point<float>
            {
                centre.x + dirX * outerRadius * scale,
                centre.y + dirY * outerRadius * scale
            };

        const auto halfW = shapeArea.getWidth() * 0.5f;
        const auto halfH = shapeArea.getHeight() * 0.5f;

        const auto tx = std::abs (dirX) < 0.0001f ? 999999.0f : halfW / std::abs (dirX);
        const auto ty = std::abs (dirY) < 0.0001f ? 999999.0f : halfH / std::abs (dirY);
        const auto t = juce::jmin (tx, ty) * scale;

        return juce::Point<float>
        {
            centre.x + dirX * t,
            centre.y + dirY * t
        };
    };

    const auto imageBounds = shapeArea.getSmallestIntegerContainer();

    const auto isFastPreview = draggedPointIndex >= 0 || draggedPreviewSlider != PreviewSlider::none;
    const auto renderScale = isFastPreview ? 1.0f / (float) previewQualityDivisor : 1.0f;

    const auto renderWidth = juce::jmax (1, juce::roundToInt ((float) imageBounds.getWidth() * renderScale));
    const auto renderHeight = juce::jmax (1, juce::roundToInt ((float) imageBounds.getHeight() * renderScale));

    juce::Image previewImage (
        juce::Image::ARGB,
        renderWidth,
        renderHeight,
        true);

    {
        juce::Graphics imageGraphics (previewImage);
        imageGraphics.fillAll (juce::Colour::fromRGB (22, 22, 26));
    }

    auto profileXAtPixel = [&] (float pixelX, float pixelY, float& profileX)
    {
        if (previewShape == PreviewShape::square || previewShape == PreviewShape::rectangle)
        {
            if (! shapeArea.contains (pixelX, pixelY))
                return false;

            const auto w = shapeArea.getWidth();
            const auto h = shapeArea.getHeight();
            const auto side = juce::jmin (w, h);
            const auto halfSide = side * 0.5f;

            const auto localX = pixelX - shapeArea.getX();
            const auto localY = pixelY - shapeArea.getY();

            auto virtualX = localX;
            auto virtualY = localY;

            if (w > h)
            {
                if (localX < halfSide)
                    virtualX = localX;
                else if (localX > w - halfSide)
                    virtualX = side - (w - localX);
                else
                    virtualX = halfSide;

                virtualY = localY;
            }
            else if (h > w)
            {
                virtualX = localX;

                if (localY < halfSide)
                    virtualY = localY;
                else if (localY > h - halfSide)
                    virtualY = side - (h - localY);
                else
                    virtualY = halfSide;
            }

            virtualX = juce::jlimit (0.0f, side, virtualX);
            virtualY = juce::jlimit (0.0f, side, virtualY);

            const auto radius = juce::jlimit (0.0f, halfSide, halfSide * cornerRadiusAmount);

            auto cornerDistanceToBorder = [&] (float cx, float cy)
            {
                const auto dx = virtualX - cx;
                const auto dy = virtualY - cy;
                const auto d = std::sqrt (dx * dx + dy * dy);

                if (d > radius)
                    return -1.0f;

                return radius - d;
            };

            auto distanceToEdge = juce::jmin (
                juce::jmin (virtualX, side - virtualX),
                juce::jmin (virtualY, side - virtualY));

            if (roundedCornerMask != 0 && radius > 0.0001f)
            {
                if ((roundedCornerMask & 1) != 0 && virtualX < radius && virtualY < radius)
                {
                    const auto d = cornerDistanceToBorder (radius, radius);

                    if (d < 0.0f)
                        return false;

                    distanceToEdge = d;
                }
                else if ((roundedCornerMask & 2) != 0 && virtualX > side - radius && virtualY < radius)
                {
                    const auto d = cornerDistanceToBorder (side - radius, radius);

                    if (d < 0.0f)
                        return false;

                    distanceToEdge = d;
                }
                else if ((roundedCornerMask & 4) != 0 && virtualX > side - radius && virtualY > side - radius)
                {
                    const auto d = cornerDistanceToBorder (side - radius, side - radius);

                    if (d < 0.0f)
                        return false;

                    distanceToEdge = d;
                }
                else if ((roundedCornerMask & 8) != 0 && virtualX < radius && virtualY > side - radius)
                {
                    const auto d = cornerDistanceToBorder (radius, side - radius);

                    if (d < 0.0f)
                        return false;

                    distanceToEdge = d;
                }
            }

            profileX = juce::jlimit (0.0f, 1.0f, distanceToEdge / juce::jmax (1.0f, halfSide));
            return true;
        }

        const auto dx = pixelX - centre.x;
        const auto dy = pixelY - centre.y;
        const auto distance = std::sqrt (dx * dx + dy * dy);

        if (distance > outerRadius)
            return false;

        profileX = juce::jlimit (0.0f, 1.0f, 1.0f - distance / outerRadius);
        return true;
    };

    auto sampleHeightAtPixel = [&] (float pixelX, float pixelY, float& height)
    {
        float profileX = 0.0f;

        if (! profileXAtPixel (pixelX, pixelY, profileX))
            return false;

        height = heightValueAt (profileX);
        return true;
    };

    auto ambientOcclusionAtPixel = [&] (float pixelX, float pixelY, float currentHeight)
    {
        const auto shapeSize = juce::jmax (1.0f, juce::jmin (shapeArea.getWidth(), shapeArea.getHeight()));

        // Size-relative AO, so small and large renders keep the same visual behaviour.
        const auto radiusPx = shapeSize * juce::jmap (
            ambientOcclusionPropagation,
            0.04f, 0.80f,
            0.012f, 0.180f);

        constexpr int ringCount = 4;
        constexpr int directionCount = 12;

        float occlusion = 0.0f;
        float weightSum = 0.0001f;

        for (int ring = 1; ring <= ringCount; ++ring)
        {
            const auto ringAmount = (float) ring / (float) ringCount;
            const auto radius = radiusPx * ringAmount;
            const auto ringFalloff = 1.0f - ringAmount * 0.72f;

            for (int direction = 0; direction < directionCount; ++direction)
            {
                const auto angle = juce::MathConstants<float>::twoPi
                    * ((float) direction / (float) directionCount)
                    + (float) ring * 0.31f;

                float sampleHeight = 0.0f;

                const auto hasSample = sampleHeightAtPixel (
                    pixelX + std::cos (angle) * radius,
                    pixelY + std::sin (angle) * radius,
                    sampleHeight);

                if (! hasSample)
                    continue;

                constexpr float heightBias = 0.045f;
                const auto higherNeighbour = juce::jmax (0.0f, sampleHeight - currentHeight - heightBias);

                const auto contribution = juce::jlimit (0.0f, 1.0f, higherNeighbour * 2.4f);
                occlusion += contribution * ringFalloff;
                weightSum += ringFalloff;
            }
        }

        const auto normalized = juce::jlimit (0.0f, 0.78f, (occlusion / weightSum) * 2.8f);
        return 1.0f - normalized;
    };

    auto normalAtPixel = [&] (float pixelX, float pixelY, float currentHeight,
                              float& nx, float& ny, float& nz)
    {
        const auto shapeSize = juce::jmax (1.0f, juce::jmin (shapeArea.getWidth(), shapeArea.getHeight()));

        // Keep normal/specular behaviour consistent when the preview is rendered small or large.
        const auto offset = juce::jlimit (0.35f, 24.0f, shapeSize * 0.0125f);
        constexpr float strength = 3.2f;

        float left = currentHeight;
        float right = currentHeight;
        float up = currentHeight;
        float down = currentHeight;

        sampleHeightAtPixel (pixelX - offset, pixelY, left);
        sampleHeightAtPixel (pixelX + offset, pixelY, right);
        sampleHeightAtPixel (pixelX, pixelY - offset, up);
        sampleHeightAtPixel (pixelX, pixelY + offset, down);

        nx = (left - right) * strength;
        ny = (down - up) * strength;
        nz = 1.0f;

        const auto length = juce::jmax (0.0001f, std::sqrt (nx * nx + ny * ny + nz * nz));

        nx /= length;
        ny /= length;
        nz /= length;
    };

    auto normalColourAtPixel = [&] (float pixelX, float pixelY, float currentHeight)
    {
        float nx = 0.0f;
        float ny = 0.0f;
        float nz = 1.0f;

        normalAtPixel (pixelX, pixelY, currentHeight, nx, ny, nz);

        return juce::Colour::fromFloatRGBA (
            nx * 0.5f + 0.5f,
            ny * 0.5f + 0.5f,
            nz * 0.5f + 0.5f,
            1.0f);
    };

    auto materialColourAtPixel = [&] (float pixelX, float pixelY, float profileX, float currentHeight)
    {
        float nx = 0.0f;
        float ny = 0.0f;
        float nz = 1.0f;

        normalAtPixel (pixelX, pixelY, currentHeight, nx, ny, nz);

        auto applyLayerOpacity = [] (float layerValue, float opacity)
        {
            layerValue = juce::jlimit (0.0f, 1.0f, layerValue);
            opacity = juce::jlimit (0.0f, 1.0f, opacity);

            return 1.0f + (layerValue - 1.0f) * opacity;
        };

        const auto heightOpacity = juce::jlimit (0.0f, 1.0f, beautyStrength / 2.0f);
        const auto cavityOpacity = cavityLayerOpacity;
        const auto aoOpacity = aoLayerOpacity;

        const auto heightLayer = applyLayerOpacity (
            juce::jlimit (0.0f, 1.0f, 0.08f + heightValueAt (profileX) * 0.92f),
            heightOpacity);

        const auto cavityLayer = applyLayerOpacity (cavityAt (profileX), cavityOpacity);
        const auto aoLayer = applyLayerOpacity (ambientOcclusionAtPixel (pixelX, pixelY, currentHeight), aoOpacity);

        const auto lightRadians = (lightAngleDeg - 90.0f) * juce::MathConstants<float>::pi / 180.0f;
        const auto lz = juce::jmap (lightElevation, 0.10f, 1.0f, 0.20f, 0.95f);
        const auto sideAmount = std::sqrt (juce::jmax (0.0f, 1.0f - lz * lz));

        const auto lx = std::cos (lightRadians) * sideAmount;
        const auto ly = std::sin (lightRadians) * sideAmount;

        auto directionalShadowAtPixel = [&]()
        {
            if (shadowLayerAmount <= 0.0001f)
                return 1.0f;

            constexpr int stepCount = 18;

            const auto shapeSize = juce::jmax (1.0f, juce::jmin (shapeArea.getWidth(), shapeArea.getHeight()));
            const auto shadowLengthPx = shapeSize * juce::jmap (lightElevation, 0.10f, 1.0f, 0.42f, 0.08f);

            const auto horizontalLight = juce::jmax (0.0001f, sideAmount);
            const auto lightSlope = (lz / horizontalLight) * 0.38f;

            float strongestShadow = 0.0f;

            for (int i = 1; i <= stepCount; ++i)
            {
                const auto t = (float) i / (float) stepCount;
                const auto distance = shadowLengthPx * t;

                float sampleHeight = 0.0f;

                const auto hasSample = sampleHeightAtPixel (
                    pixelX + lx * distance,
                    pixelY - ly * distance,
                    sampleHeight);

                if (! hasSample)
                    continue;

                const auto distanceInShape = distance / shapeSize;

                // Height ray test: farther samples must be proportionally higher
                // to block the incoming light.
                const auto rayHeight = currentHeight + distanceInShape * lightSlope + 0.018f;
                const auto blocker = sampleHeight - rayHeight;

                if (blocker <= 0.0f)
                    continue;

                const auto softness = 1.0f - t * 0.58f;
                const auto cast = juce::jlimit (0.0f, 1.0f, blocker * 7.5f) * softness;

                strongestShadow = juce::jmax (strongestShadow, cast);
            }

            return 1.0f - strongestShadow * shadowLayerAmount;
        };

        const auto shadowLayer = directionalShadowAtPixel();
        const auto layerShade = juce::jlimit (0.0f, 1.0f, heightLayer * cavityLayer * aoLayer * shadowLayer);

        const auto hx = lx;
        const auto hy = ly;
        const auto hz = lz + 1.0f;
        const auto hLength = juce::jmax (0.0001f, std::sqrt (hx * hx + hy * hy + hz * hz));

        const auto specDot = juce::jlimit (0.0f, 1.0f,
            (nx * hx + ny * hy + nz * hz) / hLength);

        const auto gloss01 = juce::jlimit (0.0f, 1.0f, glossAmount / 2.0f);

        // Artistic specular catch.
        // Lower threshold = highlights appear from wider / less perfect angles.
        const auto specularCatchThreshold = juce::jmap (specularCatchAmount, 0.0f, 1.0f, 0.55f, 0.02f);

        const auto caughtSpecDot = juce::jlimit (0.0f, 1.0f,
            (specDot - specularCatchThreshold) / (1.0f - specularCatchThreshold));

        const auto specPower = juce::jmap (gloss01, 0.0f, 1.0f, 2.0f, 64.0f);
        const auto specStrength = juce::jmap (gloss01, 0.0f, 1.0f, 0.65f, 3.6f);

        const auto specular = std::pow (caughtSpecDot, specPower) * specularLayerAmount * specStrength;

        return juce::Colour::fromFloatRGBA (
            juce::jlimit (0.0f, 1.0f, baseColour.getFloatRed()   * layerShade + specular),
            juce::jlimit (0.0f, 1.0f, baseColour.getFloatGreen() * layerShade + specular),
            juce::jlimit (0.0f, 1.0f, baseColour.getFloatBlue()  * layerShade + specular),
            1.0f);
    };


    {
        juce::Image::BitmapData previewPixels (previewImage, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < previewImage.getHeight(); ++y)
    {
        for (int x = 0; x < previewImage.getWidth(); ++x)
        {
            const auto pixelX = (float) imageBounds.getX()
                + ((float) x + 0.5f) * (float) imageBounds.getWidth() / (float) previewImage.getWidth();

            const auto pixelY = (float) imageBounds.getY()
                + ((float) y + 0.5f) * (float) imageBounds.getHeight() / (float) previewImage.getHeight();

            const auto dx = pixelX - centre.x;
            const auto dy = pixelY - centre.y;

            const auto angleDeg = normaliseAngle (
                std::atan2 (dy, dx) * 180.0f / juce::MathConstants<float>::pi + 90.0f);

            float profileX = 0.0f;

            if (! profileXAtPixel (pixelX, pixelY, profileX))
                continue;

            auto colour = colourAt (profileX, angleDeg);

            if (previewMode == PreviewMode::normalMap)
                colour = normalColourAtPixel (pixelX, pixelY, heightValueAt (profileX));
            else if (previewMode == PreviewMode::material)
                colour = materialColourAtPixel (pixelX, pixelY, profileX, heightValueAt (profileX));
            else if (previewMode == PreviewMode::ambientOcclusion)
            {
                const auto value = ambientOcclusionAtPixel (pixelX, pixelY, heightValueAt (profileX));
                colour = juce::Colour::fromFloatRGBA (value, value, value, 1.0f);
            }

            auto* pixel = reinterpret_cast<juce::PixelARGB*> (previewPixels.getPixelPointer (x, y));
            pixel->setARGB (colour.getAlpha(), colour.getRed(), colour.getGreen(), colour.getBlue());
        }
    }

    }

    g.setOpacity (1.0f);
    g.setColour (juce::Colours::white);

    g.drawImage (
        previewImage,
        imageBounds.getX(),
        imageBounds.getY(),
        imageBounds.getWidth(),
        imageBounds.getHeight(),
        0,
        0,
        previewImage.getWidth(),
        previewImage.getHeight(),
        false);

    g.setColour (juce::Colours::white.withAlpha (0.12f));

    if (previewShape == PreviewShape::circle)
        g.drawEllipse (shapeArea, 1.0f);
    else if (roundedCornerMask == 15)
        g.drawRoundedRectangle (shapeArea, juce::jmin (shapeArea.getWidth(), shapeArea.getHeight()) * 0.5f * cornerRadiusAmount, 1.0f);
    else
        g.drawRect (shapeArea, 1.0f);


}

