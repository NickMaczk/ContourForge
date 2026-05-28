#include "MainComponent.h"

MainComponent::MainComponent()
{
    setSize (1000, 620);

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
        auto previewControls = getPreviewArea().reduced (18.0f).removeFromTop (56.0f).removeFromRight (500.0f);

        auto shapeRow = previewControls.removeFromTop (24.0f);
        previewControls.removeFromTop (8.0f);
        auto modeRow = previewControls.removeFromTop (24.0f);

        const auto circleButton = shapeRow.removeFromLeft (72.0f);
        shapeRow.removeFromLeft (8.0f);
        const auto squareButton = shapeRow.removeFromLeft (80.0f);
        shapeRow.removeFromLeft (8.0f);
        const auto baseButton = shapeRow.removeFromLeft (76.0f);

        const auto heightButton = modeRow.removeFromLeft (60.0f);
        modeRow.removeFromLeft (6.0f);
        const auto normalButton = modeRow.removeFromLeft (64.0f);
        modeRow.removeFromLeft (6.0f);
        const auto materialButton = modeRow.removeFromLeft (64.0f);
        modeRow.removeFromLeft (6.0f);
        const auto cavityButton = modeRow.removeFromLeft (62.0f);
        modeRow.removeFromLeft (6.0f);
        const auto aoButton = modeRow.removeFromLeft (42.0f);
        modeRow.removeFromLeft (6.0f);
        const auto rangeMinusButton = modeRow.removeFromLeft (48.0f);
        modeRow.removeFromLeft (6.0f);
        const auto rangePlusButton = modeRow.removeFromLeft (48.0f);

        if (circleButton.contains (mouse))
        {
            previewShape = PreviewShape::circle;
            repaint();
            return;
        }

        if (squareButton.contains (mouse))
        {
            previewShape = PreviewShape::square;
            repaint();
            return;
        }

        if (baseButton.contains (mouse))
        {
            const std::vector<juce::Colour> baseColours
            {
                juce::Colour::fromRGB (170, 146, 105),
                juce::Colour::fromRGB (120, 120, 126),
                juce::Colour::fromRGB (205, 205, 198),
                juce::Colour::fromRGB (96, 72, 54),
                juce::Colour::fromRGB (98, 125, 150),
                juce::Colour::fromRGB (150, 82, 74),
                juce::Colour::fromRGB (34, 34, 38)
            };

            int nextIndex = 0;

            for (int i = 0; i < (int) baseColours.size(); ++i)
            {
                if (baseColours[(size_t) i].getARGB() == baseColour.getARGB())
                {
                    nextIndex = (i + 1) % (int) baseColours.size();
                    break;
                }
            }

            baseColour = baseColours[(size_t) nextIndex];
            previewMode = PreviewMode::material;
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

        if (rangeMinusButton.contains (mouse))
        {
            if (previewMode == PreviewMode::cavity)
                cavityPropagation = juce::jlimit (0.04f, 0.80f, cavityPropagation - 0.04f);
            else
            {
                ambientOcclusionPropagation = juce::jlimit (0.04f, 0.80f, ambientOcclusionPropagation - 0.04f);
                previewMode = PreviewMode::ambientOcclusion;
            }

            repaint();
            return;
        }

        if (rangePlusButton.contains (mouse))
        {
            if (previewMode == PreviewMode::cavity)
                cavityPropagation = juce::jlimit (0.04f, 0.80f, cavityPropagation + 0.04f);
            else
            {
                ambientOcclusionPropagation = juce::jlimit (0.04f, 0.80f, ambientOcclusionPropagation + 0.04f);
                previewMode = PreviewMode::ambientOcclusion;
            }

            repaint();
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
    if (draggedPointIndex < 0)
        return;

    profilePoints[(size_t) draggedPointIndex] =
        screenToProfile (e.position, getProfileArea(), draggedPointIndex);

    repaint();
}

void MainComponent::mouseUp (const juce::MouseEvent&)
{
    draggedPointIndex = -1;
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

    const auto x = juce::jlimit (0.0f, 1.0f, (e.position.x - graph.getX()) / graph.getWidth());
    const auto y = juce::jlimit (0.0f, 1.0f, (graph.getBottom() - e.position.y) / graph.getHeight());

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
    return r.removeFromLeft (getWidth() * 0.42f).reduced (0.0f, 12.0f);
}

juce::Rectangle<float> MainComponent::getPreviewArea() const
{
    auto r = getLocalBounds().toFloat().reduced (24.0f);
    r.removeFromTop (72.0f);
    r.removeFromLeft (getWidth() * 0.42f);
    return r.reduced (18.0f, 12.0f);
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
        x = juce::jlimit (minX, maxX, x);
    }

    ProfilePoint result = profilePoints[(size_t) pointIndex];
    result.x = juce::jlimit (0.0f, 1.0f, x);
    result.y = juce::jlimit (0.0f, 1.0f, y);
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
    root->setProperty ("previewShape", previewShape == PreviewShape::circle ? "circle" : "square");
    root->setProperty ("previewMode",
        previewMode == PreviewMode::normalMap ? "normalMap"
        : previewMode == PreviewMode::material ? "material"
        : previewMode == PreviewMode::cavity ? "cavity"
        : previewMode == PreviewMode::ambientOcclusion ? "ambientOcclusion"
        : "heightMap");

    root->setProperty ("cavityPropagation", cavityPropagation);
    root->setProperty ("ambientOcclusionPropagation", ambientOcclusionPropagation);
    root->setProperty ("baseColour", baseColour.toDisplayString (true));

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

    previewShape = root->getProperty ("previewShape").toString() == "square"
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

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    for (int i = 0; i <= 4; ++i)
    {
        const auto y = graph.getY() + graph.getHeight() * i / 4.0f;
        g.drawHorizontalLine ((int) y, graph.getX(), graph.getRight());
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

    auto titleRow = area.reduced (18.0f).removeFromTop (24.0f);

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText ("Shape preview", titleRow.removeFromLeft (280.0f),
                juce::Justification::left);

    auto previewControls = area.reduced (18.0f).removeFromTop (56.0f).removeFromRight (500.0f);

    auto shapeRow = previewControls.removeFromTop (24.0f);
    previewControls.removeFromTop (8.0f);
    auto modeRow = previewControls.removeFromTop (24.0f);

    const auto circleButton = shapeRow.removeFromLeft (72.0f);
    shapeRow.removeFromLeft (8.0f);
    const auto squareButton = shapeRow.removeFromLeft (80.0f);
    shapeRow.removeFromLeft (8.0f);
    const auto baseButton = shapeRow.removeFromLeft (76.0f);

    const auto heightButton = modeRow.removeFromLeft (60.0f);
    modeRow.removeFromLeft (6.0f);
    const auto normalButton = modeRow.removeFromLeft (64.0f);
    modeRow.removeFromLeft (6.0f);
    const auto materialButton = modeRow.removeFromLeft (64.0f);
    modeRow.removeFromLeft (6.0f);
    const auto cavityButton = modeRow.removeFromLeft (62.0f);
    modeRow.removeFromLeft (6.0f);
    const auto aoButton = modeRow.removeFromLeft (42.0f);
    modeRow.removeFromLeft (6.0f);
    const auto rangeMinusButton = modeRow.removeFromLeft (48.0f);
    modeRow.removeFromLeft (6.0f);
    const auto rangePlusButton = modeRow.removeFromLeft (48.0f);

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

    drawShapeButton (circleButton, "Circle", previewShape == PreviewShape::circle);
    drawShapeButton (squareButton, "Square", previewShape == PreviewShape::square);

    g.setColour (baseColour.withAlpha (0.82f));
    g.fillRoundedRectangle (baseButton, 6.0f);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawRoundedRectangle (baseButton, 6.0f, 1.0f);

    g.setColour (baseColour.getBrightness() < 0.42f
        ? juce::Colours::white.withAlpha (0.72f)
        : juce::Colours::black.withAlpha (0.62f));

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("Base", baseButton.reduced (6.0f, 0.0f), juce::Justification::centred);
    drawShapeButton (heightButton, "Height", previewMode == PreviewMode::heightMap);
    drawShapeButton (normalButton, "Normal", previewMode == PreviewMode::normalMap);
    drawShapeButton (materialButton, "Beauty", previewMode == PreviewMode::material);
    drawShapeButton (cavityButton, "Cavity", previewMode == PreviewMode::cavity);
    drawShapeButton (aoButton, "AO", previewMode == PreviewMode::ambientOcclusion);
    drawShapeButton (rangeMinusButton, "Range-", false);
    drawShapeButton (rangePlusButton, "Range+", false);

    auto shapeArea = area.reduced (84.0f, 92.0f);
    const auto side = juce::jmin (shapeArea.getWidth(), shapeArea.getHeight());
    shapeArea = shapeArea.withSizeKeepingCentre (side, side);

    const auto centre = shapeArea.getCentre();
    const auto outerRadius = side * 0.5f;

    auto normaliseAngle = [] (float angle)
    {
        while (angle < 0.0f)
            angle += 360.0f;

        while (angle >= 360.0f)
            angle -= 360.0f;

        return angle;
    };

    auto heightValueAt = [&] (float profileX)
    {
        const auto sample = sampleProfileAt (profileX);
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

    juce::Image previewImage (
        juce::Image::ARGB,
        imageBounds.getWidth(),
        imageBounds.getHeight(),
        true);

    auto sampleHeightAtPixel = [&] (float pixelX, float pixelY, float& height)
    {
        const auto dx = pixelX - centre.x;
        const auto dy = pixelY - centre.y;
        const auto distance = std::sqrt (dx * dx + dy * dy);

        if (distance <= 0.0001f)
        {
            height = heightValueAt (1.0f);
            return true;
        }

        const auto angleDeg = normaliseAngle (
            std::atan2 (dy, dx) * 180.0f / juce::MathConstants<float>::pi + 90.0f);

        float edgeDistance = outerRadius;

        if (previewShape == PreviewShape::square)
        {
            const auto radians = (angleDeg - 90.0f) * juce::MathConstants<float>::pi / 180.0f;
            const auto dirX = std::cos (radians);
            const auto dirY = std::sin (radians);

            const auto halfW = shapeArea.getWidth() * 0.5f;
            const auto halfH = shapeArea.getHeight() * 0.5f;

            const auto tx = std::abs (dirX) < 0.0001f ? 999999.0f : halfW / std::abs (dirX);
            const auto ty = std::abs (dirY) < 0.0001f ? 999999.0f : halfH / std::abs (dirY);

            edgeDistance = juce::jmin (tx, ty);
        }

        if (distance > edgeDistance)
            return false;

        const auto profileX = juce::jlimit (0.0f, 1.0f, 1.0f - distance / edgeDistance);
        height = heightValueAt (profileX);
        return true;
    };

    auto ambientOcclusionAtPixel = [&] (float pixelX, float pixelY, float currentHeight)
    {
        const auto radiusPx = juce::jmap (ambientOcclusionPropagation, 0.04f, 0.80f, 4.0f, 56.0f);

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
        constexpr float offset = 1.5f;
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

        constexpr float lx = -0.38f;
        constexpr float ly = -0.58f;
        constexpr float lz = 0.72f;

        const auto lightLength = std::sqrt (lx * lx + ly * ly + lz * lz);
        const auto dot = juce::jlimit (0.0f, 1.0f, (nx * lx + ny * ly + nz * lz) / lightLength);

        const auto heightLift = 0.86f + heightValueAt (profileX) * 0.22f;
        const auto cavityShade = 0.70f + cavityAt (profileX) * 0.30f;
        const auto aoShade = 0.66f + ambientOcclusionAtPixel (pixelX, pixelY, currentHeight) * 0.34f;
        const auto diffuse = 0.34f + dot * 0.78f;

        const auto shade = juce::jlimit (0.0f, 1.35f, diffuse * heightLift * cavityShade * aoShade);
        const auto spec = std::pow (dot, 34.0f) * 0.18f;

        return juce::Colour::fromFloatRGBA (
            juce::jlimit (0.0f, 1.0f, baseColour.getFloatRed()   * shade + spec),
            juce::jlimit (0.0f, 1.0f, baseColour.getFloatGreen() * shade + spec),
            juce::jlimit (0.0f, 1.0f, baseColour.getFloatBlue()  * shade + spec),
            1.0f);
    };

    for (int y = 0; y < previewImage.getHeight(); ++y)
    {
        for (int x = 0; x < previewImage.getWidth(); ++x)
        {
            const auto pixelX = (float) imageBounds.getX() + (float) x + 0.5f;
            const auto pixelY = (float) imageBounds.getY() + (float) y + 0.5f;

            const auto dx = pixelX - centre.x;
            const auto dy = pixelY - centre.y;
            const auto distance = std::sqrt (dx * dx + dy * dy);

            if (distance <= 0.0001f)
            {
                auto colour = colourAt (1.0f, 0.0f);

                if (previewMode == PreviewMode::normalMap)
                    colour = normalColourAtPixel (pixelX, pixelY, heightValueAt (1.0f));
                else if (previewMode == PreviewMode::material)
                    colour = materialColourAtPixel (pixelX, pixelY, 1.0f, heightValueAt (1.0f));
                else if (previewMode == PreviewMode::ambientOcclusion)
                {
                    const auto value = ambientOcclusionAtPixel (pixelX, pixelY, heightValueAt (1.0f));
                    colour = juce::Colour::fromFloatRGBA (value, value, value, 1.0f);
                }

                previewImage.setPixelAt (x, y, colour);
                continue;
            }

            const auto angleDeg = normaliseAngle (
                std::atan2 (dy, dx) * 180.0f / juce::MathConstants<float>::pi + 90.0f);

            float edgeDistance = outerRadius;

            if (previewShape == PreviewShape::square)
            {
                const auto radians = (angleDeg - 90.0f) * juce::MathConstants<float>::pi / 180.0f;
                const auto dirX = std::cos (radians);
                const auto dirY = std::sin (radians);

                const auto halfW = shapeArea.getWidth() * 0.5f;
                const auto halfH = shapeArea.getHeight() * 0.5f;

                const auto tx = std::abs (dirX) < 0.0001f ? 999999.0f : halfW / std::abs (dirX);
                const auto ty = std::abs (dirY) < 0.0001f ? 999999.0f : halfH / std::abs (dirY);

                edgeDistance = juce::jmin (tx, ty);
            }

            if (distance > edgeDistance)
                continue;

            const auto profileX = juce::jlimit (0.0f, 1.0f, 1.0f - distance / edgeDistance);

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

            previewImage.setPixelAt (x, y, colour);
        }
    }

    g.drawImageAt (previewImage, imageBounds.getX(), imageBounds.getY());

    g.setColour (juce::Colours::white.withAlpha (0.12f));

    if (previewShape == PreviewShape::circle)
        g.drawEllipse (shapeArea, 1.0f);
    else
        g.drawRect (shapeArea, 1.0f);


}

