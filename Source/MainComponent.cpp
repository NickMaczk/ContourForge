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

    colourProfiles =
    {
        {
            0.0f,
            {
                juce::Colour::fromRGB (24, 24, 28),
                juce::Colour::fromRGB (210, 214, 224),
                juce::Colour::fromRGB (82, 86, 98),
                juce::Colour::fromRGB (155, 160, 174),
                juce::Colour::fromRGB (18, 18, 22)
            }
        }
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
        auto previewControls = getPreviewArea().reduced (18.0f).removeFromTop (56.0f).removeFromRight (260.0f);

        auto shapeRow = previewControls.removeFromTop (24.0f);
        previewControls.removeFromTop (8.0f);
        auto modeRow = previewControls.removeFromTop (24.0f);

        const auto circleButton = shapeRow.removeFromLeft (72.0f);
        shapeRow.removeFromLeft (8.0f);
        const auto squareButton = shapeRow.removeFromLeft (80.0f);

        const auto heightButton = modeRow.removeFromLeft (72.0f);
        modeRow.removeFromLeft (8.0f);
        const auto aoButton = modeRow.removeFromLeft (48.0f);
        modeRow.removeFromLeft (8.0f);
        const auto aoMinusButton = modeRow.removeFromLeft (56.0f);
        modeRow.removeFromLeft (8.0f);
        const auto aoPlusButton = modeRow.removeFromLeft (56.0f);

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

        if (heightButton.contains (mouse))
        {
            previewMode = PreviewMode::heightMap;
            repaint();
            return;
        }

        if (aoButton.contains (mouse))
        {
            previewMode = PreviewMode::ambientOcclusion;
            repaint();
            return;
        }

        if (aoMinusButton.contains (mouse))
        {
            aoPropagation = juce::jlimit (0.04f, 0.80f, aoPropagation - 0.04f);
            previewMode = PreviewMode::ambientOcclusion;
            repaint();
            return;
        }

        if (aoPlusButton.contains (mouse))
        {
            aoPropagation = juce::jlimit (0.04f, 0.80f, aoPropagation + 0.04f);
            previewMode = PreviewMode::ambientOcclusion;
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

juce::Rectangle<float> MainComponent::getColourProfileBarArea() const
{
    auto area = getProfileArea();
    area.removeFromBottom (64.0f);
    return area.removeFromBottom (64.0f).reduced (18.0f, 4.0f);
}

juce::Rectangle<float> MainComponent::getColourPaletteArea() const
{
    auto area = getProfileArea();
    return area.removeFromBottom (64.0f).reduced (18.0f, 8.0f);
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

MainComponent::SampledProfilePoint MainComponent::sampleProfileAt (float x, int colourProfileIndex) const
{
    x = juce::jlimit (0.0f, 1.0f, x);

    const auto safeProfileIndex = juce::jlimit (0, (int) colourProfiles.size() - 1, colourProfileIndex);
    const auto& colours = colourProfiles[(size_t) safeProfileIndex].colours;

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
                juce::jmap (amount, a.y, b.y),
                colours[(size_t) i].interpolatedWith (colours[(size_t) i + 1], amount)
            };
        }
    }

    return
    {
        profilePoints.back().x,
        profilePoints.back().y,
        colours.back()
    };
}

juce::Colour MainComponent::getPointColour (int pointIndex) const
{
    if (colourProfiles.empty())
        return juce::Colours::white;

    const auto safeProfileIndex = juce::jlimit (0, (int) colourProfiles.size() - 1, selectedColourProfileIndex);
    const auto& colours = colourProfiles[(size_t) safeProfileIndex].colours;

    if (colours.empty())
        return juce::Colours::white;

    const auto safePointIndex = juce::jlimit (0, (int) colours.size() - 1, pointIndex);
    return colours[(size_t) safePointIndex];
}

void MainComponent::setPointColour (int pointIndex, juce::Colour colour)
{
    if (colourProfiles.empty())
        return;

    const auto safeProfileIndex = juce::jlimit (0, (int) colourProfiles.size() - 1, selectedColourProfileIndex);
    auto& colours = colourProfiles[(size_t) safeProfileIndex].colours;

    if (colours.empty())
        return;

    const auto safePointIndex = juce::jlimit (0, (int) colours.size() - 1, pointIndex);
    colours[(size_t) safePointIndex] = colour;
}

juce::Colour MainComponent::getAutoShadedPointColour (int pointIndex, int colourProfileIndex) const
{
    const auto baseColour = colourProfiles[(size_t) colourProfileIndex].colours[(size_t) pointIndex];

    const auto current = profilePoints[(size_t) pointIndex];

    const auto previousIndex = juce::jmax (0, pointIndex - 1);
    const auto nextIndex = juce::jmin ((int) profilePoints.size() - 1, pointIndex + 1);

    const auto previous = profilePoints[(size_t) previousIndex];
    const auto next = profilePoints[(size_t) nextIndex];

    const auto localSlope = (next.y - previous.y) * 0.5f;
    const auto heightTint = (current.y - 0.5f) * 0.18f;
    const auto slopeTint = localSlope * 0.32f;
    const auto tint = juce::jlimit (-0.35f, 0.35f, heightTint + slopeTint);

    if (tint >= 0.0f)
        return baseColour.interpolatedWith (juce::Colours::white, tint);

    return baseColour.interpolatedWith (juce::Colours::black, -tint);
}

void MainComponent::bakeAutoShadeIntoSelectedProfile()
{
    if (colourProfiles.empty())
        return;

    const auto safeProfileIndex = juce::jlimit (0, (int) colourProfiles.size() - 1, selectedColourProfileIndex);
    auto bakedColours = colourProfiles[(size_t) safeProfileIndex].colours;

    for (int i = 0; i < (int) bakedColours.size(); ++i)
        bakedColours[(size_t) i] = getAutoShadedPointColour (i, safeProfileIndex);

    colourProfiles[(size_t) safeProfileIndex].colours = bakedColours;
}

juce::var MainComponent::createProjectState() const
{
    auto* root = new juce::DynamicObject();

    root->setProperty ("version", 1);
    root->setProperty ("previewShape", previewShape == PreviewShape::circle ? "circle" : "square");
    root->setProperty ("previewMode", previewMode == PreviewMode::ambientOcclusion ? "ambientOcclusion" : "heightMap");
    root->setProperty ("aoPropagation", aoPropagation);
    root->setProperty ("autoShadeEnabled", autoShadeEnabled);
    root->setProperty ("selectedColourProfileIndex", selectedColourProfileIndex);

    juce::Array<juce::var> points;

    for (const auto& point : profilePoints)
    {
        auto* pointObject = new juce::DynamicObject();
        pointObject->setProperty ("x", point.x);
        pointObject->setProperty ("y", point.y);
        points.add (juce::var (pointObject));
    }

    root->setProperty ("profilePoints", juce::var (points));

    juce::Array<juce::var> profiles;

    for (const auto& profile : colourProfiles)
    {
        auto* profileObject = new juce::DynamicObject();
        profileObject->setProperty ("angleDeg", profile.angleDeg);

        juce::Array<juce::var> colours;

        for (const auto& colour : profile.colours)
            colours.add (colour.toDisplayString (true));

        profileObject->setProperty ("colours", juce::var (colours));
        profiles.add (juce::var (profileObject));
    }

    root->setProperty ("colourProfiles", juce::var (profiles));

    return juce::var (root);
}

bool MainComponent::applyProjectState (const juce::var& state)
{
    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return false;

    auto* pointsArray = root->getProperty ("profilePoints").getArray();
    auto* profilesArray = root->getProperty ("colourProfiles").getArray();

    if (pointsArray == nullptr || profilesArray == nullptr)
        return false;

    if (pointsArray->size() < 2 || profilesArray->isEmpty())
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

    std::vector<ColourProfile> loadedProfiles;

    for (const auto& profileVar : *profilesArray)
    {
        auto* profileObject = profileVar.getDynamicObject();

        if (profileObject == nullptr)
            return false;

        auto* coloursArray = profileObject->getProperty ("colours").getArray();

        if (coloursArray == nullptr || coloursArray->size() != (int) loadedPoints.size())
            return false;

        ColourProfile profile;
        profile.angleDeg = (float) (double) profileObject->getProperty ("angleDeg");

        while (profile.angleDeg < 0.0f)
            profile.angleDeg += 360.0f;

        while (profile.angleDeg >= 360.0f)
            profile.angleDeg -= 360.0f;

        for (const auto& colourVar : *coloursArray)
            profile.colours.push_back (juce::Colour::fromString (colourVar.toString()));

        loadedProfiles.push_back (profile);
    }

    profilePoints = std::move (loadedPoints);
    colourProfiles = std::move (loadedProfiles);

    previewShape = root->getProperty ("previewShape").toString() == "square"
        ? PreviewShape::square
        : PreviewShape::circle;

    previewMode = root->getProperty ("previewMode").toString() == "ambientOcclusion"
        ? PreviewMode::ambientOcclusion
        : PreviewMode::heightMap;

    aoPropagation = juce::jlimit (0.04f, 0.80f, (float) (double) root->getProperty ("aoPropagation"));

    autoShadeEnabled = (bool) root->getProperty ("autoShadeEnabled");

    selectedColourProfileIndex = juce::jlimit (
        0,
        (int) colourProfiles.size() - 1,
        (int) root->getProperty ("selectedColourProfileIndex"));

    selectedPointIndex = -1;
    draggedPointIndex = -1;
    draggedColourProfileIndex = -1;

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

std::vector<juce::Colour> MainComponent::getPaletteColours() const
{
    return
    {
        juce::Colour::fromRGB (16, 16, 18),
        juce::Colour::fromRGB (42, 43, 48),
        juce::Colour::fromRGB (85, 88, 98),
        juce::Colour::fromRGB (140, 145, 158),
        juce::Colour::fromRGB (220, 224, 232),
        juce::Colour::fromRGB (80, 130, 255),
        juce::Colour::fromRGB (240, 80, 86),
        juce::Colour::fromRGB (245, 184, 80)
    };
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

void MainComponent::drawColourProfileBar (juce::Graphics& g, juce::Rectangle<float> area)
{
    constexpr float pillHeight = 24.0f;
    constexpr float gap = 8.0f;

    auto selectorRow = area.removeFromTop (pillHeight);
    area.removeFromTop (gap);
    auto actionRow = area.removeFromTop (pillHeight);

    const auto previousPill = juce::Rectangle<float> (selectorRow.getX(), selectorRow.getY(), 32.0f, pillHeight);
    const auto profilePill = juce::Rectangle<float> (previousPill.getRight() + gap, selectorRow.getY(), 126.0f, pillHeight);
    const auto nextPill = juce::Rectangle<float> (profilePill.getRight() + gap, selectorRow.getY(), 32.0f, pillHeight);

    const auto duplicatePill = juce::Rectangle<float> (actionRow.getX(), actionRow.getY(), 46.0f, pillHeight);
    const auto deletePill = juce::Rectangle<float> (duplicatePill.getRight() + gap, actionRow.getY(), 46.0f, pillHeight);
    const auto minusPill = juce::Rectangle<float> (deletePill.getRight() + gap, actionRow.getY(), 58.0f, pillHeight);
    const auto plusPill = juce::Rectangle<float> (minusPill.getRight() + gap, actionRow.getY(), 58.0f, pillHeight);

    auto drawButton = [&] (juce::Rectangle<float> button, const juce::String& text, bool enabled = true, bool selected = false)
    {
        g.setColour (juce::Colours::white.withAlpha (selected ? 0.18f : (enabled ? 0.07f : 0.03f)));
        g.fillRoundedRectangle (button, 6.0f);

        g.setColour (juce::Colours::white.withAlpha (selected ? 0.85f : (enabled ? 0.35f : 0.14f)));
        g.drawRoundedRectangle (button, 6.0f, selected ? 2.0f : 1.0f);

        g.setColour (juce::Colours::white.withAlpha (enabled ? 0.62f : 0.22f));
        g.drawText (text, button.reduced (7.0f, 0.0f), juce::Justification::centred);
    };

    const auto hasProfiles = ! colourProfiles.empty();

    drawButton (previousPill, "<", hasProfiles);
    drawButton (nextPill, ">", hasProfiles);

    juce::String profileText = "No profile";

    if (hasProfiles)
    {
        const auto indexText = juce::String (selectedColourProfileIndex + 1) + "/" + juce::String ((int) colourProfiles.size());
        const auto angleText = juce::String ((int) colourProfiles[(size_t) selectedColourProfileIndex].angleDeg) + "deg";
        profileText = "P" + indexText + "  " + angleText;
    }

    drawButton (profilePill, profileText, hasProfiles, true);

    drawButton (duplicatePill, "+P", hasProfiles);
    drawButton (deletePill, "Del", colourProfiles.size() > 1);
    drawButton (minusPill, "-15deg", hasProfiles);
    drawButton (plusPill, "+15deg", hasProfiles);
}

void MainComponent::drawColourPalette (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (juce::Colours::white.withAlpha (0.45f));

    const auto label = selectedPointIndex >= 0
        ? "Selected point colour"
        : "Select a point to edit its colour";

    g.drawText (label, area.removeFromTop (18.0f), juce::Justification::left);

    const auto colours = getPaletteColours();

    constexpr float swatchSize = 24.0f;
    constexpr float gap = 8.0f;

    for (int i = 0; i < (int) colours.size(); ++i)
    {
        const auto swatch = juce::Rectangle<float> (
            area.getX() + (swatchSize + gap) * (float) i,
            area.getY(),
            swatchSize,
            swatchSize);

        g.setColour (colours[(size_t) i]);
        g.fillRoundedRectangle (swatch, 5.0f);

        const auto isActive = selectedPointIndex >= 0
            && getPointColour (selectedPointIndex).getARGB() == colours[(size_t) i].getARGB();

        g.setColour (juce::Colours::white.withAlpha (isActive ? 0.95f : 0.25f));
        g.drawRoundedRectangle (swatch, 5.0f, isActive ? 2.0f : 1.0f);
    }
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

    auto previewControls = area.reduced (18.0f).removeFromTop (56.0f).removeFromRight (260.0f);

    auto shapeRow = previewControls.removeFromTop (24.0f);
    previewControls.removeFromTop (8.0f);
    auto modeRow = previewControls.removeFromTop (24.0f);

    const auto circleButton = shapeRow.removeFromLeft (72.0f);
    shapeRow.removeFromLeft (8.0f);
    const auto squareButton = shapeRow.removeFromLeft (80.0f);

    const auto heightButton = modeRow.removeFromLeft (72.0f);
    modeRow.removeFromLeft (8.0f);
    const auto aoButton = modeRow.removeFromLeft (48.0f);
    modeRow.removeFromLeft (8.0f);
    const auto aoMinusButton = modeRow.removeFromLeft (56.0f);
    modeRow.removeFromLeft (8.0f);
    const auto aoPlusButton = modeRow.removeFromLeft (56.0f);

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
    drawShapeButton (heightButton, "Height", previewMode == PreviewMode::heightMap);
    drawShapeButton (aoButton, "AO", previewMode == PreviewMode::ambientOcclusion);
    drawShapeButton (aoMinusButton, "AO-", false);
    drawShapeButton (aoPlusButton, "AO+", false);

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
        const auto sample = sampleProfileAt (profileX, 0);
        return juce::jlimit (0.0f, 1.0f, sample.y);
    };

    auto ambientOcclusionAt = [&] (float profileX)
    {
        const auto current = heightValueAt (profileX);

        const auto radius = aoPropagation;
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
        if (previewMode == PreviewMode::ambientOcclusion)
        {
            const auto value = ambientOcclusionAt (profileX);
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

    auto applyAutoShade = [&] (juce::Colour colour, float x)
    {
        if (! autoShadeEnabled)
            return colour;

        constexpr float sampleStep = 1.0f / 128.0f;

        const auto x0 = juce::jlimit (0.0f, 1.0f, x - sampleStep);
        const auto x1 = juce::jlimit (0.0f, 1.0f, x + sampleStep);

        const auto current = sampleProfileAt (x, selectedColourProfileIndex);
        const auto a = sampleProfileAt (x0, selectedColourProfileIndex);
        const auto b = sampleProfileAt (x1, selectedColourProfileIndex);

        const auto heightTint = (current.y - 0.5f) * 0.18f;
        const auto slopeTint = (b.y - a.y) * 0.32f;
        const auto tint = juce::jlimit (-0.35f, 0.35f, heightTint + slopeTint);

        if (tint >= 0.0f)
            return colour.interpolatedWith (juce::Colours::white, tint);

        return colour.interpolatedWith (juce::Colours::black, -tint);
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

