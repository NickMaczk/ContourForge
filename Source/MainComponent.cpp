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
    g.drawText ("Drag points. Double-click to add. Right-click to remove. Duplicate colour profiles below.",
                24, 46, 720, 22, juce::Justification::left);

    drawProfileEditor (g, getProfileArea());
    drawPreview (g, getPreviewArea());
}

void MainComponent::resized()
{
}

void MainComponent::mouseDown (const juce::MouseEvent& e)
{
    const auto area = getProfileArea();
    const auto mouse = e.position;

    {
        auto previewArea = getPreviewArea();
        auto shapeButtons = previewArea.reduced (18.0f).removeFromTop (24.0f);
        shapeButtons = shapeButtons.removeFromRight (148.0f);

        const auto circleButton = shapeButtons.removeFromLeft (68.0f);
        shapeButtons.removeFromLeft (8.0f);
        const auto squareButton = shapeButtons.removeFromLeft (72.0f);

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
    }

    {
        auto previewArea = getPreviewArea();
        auto shapeArea = previewArea.reduced (84.0f, 92.0f);
        const auto centre = shapeArea.getCentre();
        const auto outerRadius = juce::jmin (shapeArea.getWidth(), shapeArea.getHeight()) * 0.5f;

        auto pointAt = [&] (float radius, float angleDeg)
        {
            const auto radians = (angleDeg - 90.0f) * juce::MathConstants<float>::pi / 180.0f;

            return juce::Point<float>
            {
                centre.x + std::cos (radians) * radius,
                centre.y + std::sin (radians) * radius
            };
        };

        for (int i = 0; i < (int) colourProfiles.size(); ++i)
        {
            const auto marker = pointAt (outerRadius + 18.0f, colourProfiles[(size_t) i].angleDeg);

            if (marker.getDistanceFrom (mouse) <= 14.0f)
            {
                selectedColourProfileIndex = i;
                draggedColourProfileIndex = i;
                repaint();
                return;
            }
        }
    }

    if (getColourProfileBarArea().contains (mouse))
    {
        auto bar = getColourProfileBarArea();

        constexpr float pillHeight = 24.0f;
        constexpr float gap = 8.0f;

        auto selectorRow = bar.removeFromTop (pillHeight);
        bar.removeFromTop (gap);
        auto actionRow = bar.removeFromTop (pillHeight);

        const auto previousPill = juce::Rectangle<float> (selectorRow.getX(), selectorRow.getY(), 32.0f, pillHeight);
        const auto profilePill = juce::Rectangle<float> (previousPill.getRight() + gap, selectorRow.getY(), 126.0f, pillHeight);
        const auto nextPill = juce::Rectangle<float> (profilePill.getRight() + gap, selectorRow.getY(), 32.0f, pillHeight);

        const auto duplicatePill = juce::Rectangle<float> (actionRow.getX(), actionRow.getY(), 46.0f, pillHeight);
        const auto deletePill = juce::Rectangle<float> (duplicatePill.getRight() + gap, actionRow.getY(), 46.0f, pillHeight);
        const auto minusPill = juce::Rectangle<float> (deletePill.getRight() + gap, actionRow.getY(), 58.0f, pillHeight);
        const auto plusPill = juce::Rectangle<float> (minusPill.getRight() + gap, actionRow.getY(), 58.0f, pillHeight);

        auto selectRelativeProfile = [&] (int delta)
        {
            if (colourProfiles.empty())
                return;

            selectedColourProfileIndex += delta;

            if (selectedColourProfileIndex < 0)
                selectedColourProfileIndex = (int) colourProfiles.size() - 1;

            if (selectedColourProfileIndex >= (int) colourProfiles.size())
                selectedColourProfileIndex = 0;

            repaint();
        };

        auto moveSelectedProfileAngle = [&] (float delta)
        {
            if (colourProfiles.empty())
                return;

            auto& angle = colourProfiles[(size_t) selectedColourProfileIndex].angleDeg;
            angle += delta;

            while (angle < 0.0f)
                angle += 360.0f;

            while (angle >= 360.0f)
                angle -= 360.0f;

            repaint();
        };

        if (previousPill.contains (mouse))
        {
            selectRelativeProfile (-1);
            return;
        }

        if (nextPill.contains (mouse))
        {
            selectRelativeProfile (1);
            return;
        }

        if (duplicatePill.contains (mouse) && ! colourProfiles.empty())
        {
            auto clone = colourProfiles[(size_t) selectedColourProfileIndex];
            clone.angleDeg += 90.0f;

            while (clone.angleDeg >= 360.0f)
                clone.angleDeg -= 360.0f;

            colourProfiles.push_back (clone);
            selectedColourProfileIndex = (int) colourProfiles.size() - 1;

            repaint();
            return;
        }

        if (deletePill.contains (mouse) && colourProfiles.size() > 1)
        {
            colourProfiles.erase (colourProfiles.begin() + selectedColourProfileIndex);
            selectedColourProfileIndex = juce::jlimit (0, (int) colourProfiles.size() - 1, selectedColourProfileIndex);

            repaint();
            return;
        }

        if (minusPill.contains (mouse))
        {
            moveSelectedProfileAngle (-15.0f);
            return;
        }

        if (plusPill.contains (mouse))
        {
            moveSelectedProfileAngle (15.0f);
            return;
        }

        return;
    }

    if (selectedPointIndex >= 0 && getColourPaletteArea().contains (mouse))
    {
        const auto colours = getPaletteColours();
        auto paletteArea = getColourPaletteArea();
        paletteArea.removeFromTop (18.0f);

        constexpr float swatchSize = 24.0f;
        constexpr float gap = 8.0f;

        for (int i = 0; i < (int) colours.size(); ++i)
        {
            const auto swatch = juce::Rectangle<float> (
                paletteArea.getX() + (swatchSize + gap) * (float) i,
                paletteArea.getY(),
                swatchSize,
                swatchSize);

            if (swatch.contains (mouse))
            {
                setPointColour (selectedPointIndex, colours[(size_t) i]);
                repaint();
                return;
            }
        }

        return;
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

                for (auto& colourProfile : colourProfiles)
                    colourProfile.colours.erase (colourProfile.colours.begin() + i);

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
    if (draggedColourProfileIndex >= 0)
    {
        auto previewArea = getPreviewArea();
        auto shapeArea = previewArea.reduced (84.0f, 92.0f);
        const auto side = juce::jmin (shapeArea.getWidth(), shapeArea.getHeight());
        shapeArea = shapeArea.withSizeKeepingCentre (side, side);

        const auto centre = shapeArea.getCentre();
        const auto dx = e.position.x - centre.x;
        const auto dy = e.position.y - centre.y;

        auto angle = std::atan2 (dy, dx) * 180.0f / juce::MathConstants<float>::pi + 90.0f;

        while (angle < 0.0f)
            angle += 360.0f;

        while (angle >= 360.0f)
            angle -= 360.0f;

        colourProfiles[(size_t) draggedColourProfileIndex].angleDeg = angle;

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
    draggedColourProfileIndex = -1;
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
    graph.removeFromBottom (144.0f);
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

    std::vector<juce::Colour> insertedColours;

    for (int profileIndex = 0; profileIndex < (int) colourProfiles.size(); ++profileIndex)
        insertedColours.push_back (sampleProfileAt (x, profileIndex).colour);

    ProfilePoint newPoint;
    newPoint.x = x;
    newPoint.y = y;

    profilePoints.insert (profilePoints.begin() + insertIndex, newPoint);

    for (int profileIndex = 0; profileIndex < (int) colourProfiles.size(); ++profileIndex)
        colourProfiles[(size_t) profileIndex].colours.insert (
            colourProfiles[(size_t) profileIndex].colours.begin() + insertIndex,
            insertedColours[(size_t) profileIndex]);

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
    graph.removeFromBottom (144.0f);
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
    graph.removeFromBottom (144.0f);
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
    graph.removeFromBottom (144.0f);
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

        g.setColour (getPointColour (i));
        g.fillEllipse (pt.x - 7.0f, pt.y - 7.0f, 14.0f, 14.0f);

        g.setColour (juce::Colours::white.withAlpha ((isDragged || isSelected) ? 0.95f : 0.5f));
        g.drawEllipse (pt.x - 7.0f, pt.y - 7.0f, 14.0f, 14.0f, (isDragged || isSelected) ? 2.0f : 1.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText ("Geometry + colour profile", area.reduced (18.0f).removeFromTop (24.0f),
                juce::Justification::left);

    drawColourProfileBar (g, getColourProfileBarArea());
    drawColourPalette (g, getColourPaletteArea());
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
    g.drawText ("Colour-profile preview", titleRow.removeFromLeft (280.0f),
                juce::Justification::left);

    auto shapeButtons = titleRow.removeFromRight (148.0f);
    const auto circleButton = shapeButtons.removeFromLeft (68.0f);
    shapeButtons.removeFromLeft (8.0f);
    const auto squareButton = shapeButtons.removeFromLeft (72.0f);

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

    auto forwardAngleDistance = [&] (float from, float to)
    {
        return normaliseAngle (to - from);
    };

    auto colourAt = [&] (float x, float angleDeg)
    {
        if (colourProfiles.empty())
            return juce::Colours::white;

        if (colourProfiles.size() == 1)
            return sampleProfileAt (x, 0).colour;

        angleDeg = normaliseAngle (angleDeg);

        int previousIndex = 0;
        int nextIndex = 0;

        auto bestPreviousDistance = 360.0f;
        auto bestNextDistance = 360.0f;

        for (int i = 0; i < (int) colourProfiles.size(); ++i)
        {
            const auto profileAngle = normaliseAngle (colourProfiles[(size_t) i].angleDeg);

            const auto previousDistance = forwardAngleDistance (profileAngle, angleDeg);
            const auto nextDistance = forwardAngleDistance (angleDeg, profileAngle);

            if (previousDistance < bestPreviousDistance)
            {
                bestPreviousDistance = previousDistance;
                previousIndex = i;
            }

            if (nextDistance < bestNextDistance)
            {
                bestNextDistance = nextDistance;
                nextIndex = i;
            }
        }

        if (previousIndex == nextIndex)
            return sampleProfileAt (x, previousIndex).colour;

        const auto totalDistance = bestPreviousDistance + bestNextDistance;
        const auto amount = totalDistance <= 0.0001f ? 0.0f : bestPreviousDistance / totalDistance;

        return sampleProfileAt (x, previousIndex).colour
            .interpolatedWith (sampleProfileAt (x, nextIndex).colour, amount);
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

    constexpr int numBands = 64;
    constexpr int numAngles = 96;

    for (int bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        const auto x0 = (float) bandIndex / (float) numBands;
        const auto x1 = (float) (bandIndex + 1) / (float) numBands;

        const auto a = sampleProfileAt (x0, selectedColourProfileIndex);
        const auto b = sampleProfileAt (x1, selectedColourProfileIndex);

        for (int angleIndex = 0; angleIndex < numAngles; ++angleIndex)
        {
            const auto angle0 = 360.0f * (float) angleIndex / (float) numAngles;
            const auto angle1 = 360.0f * (float) (angleIndex + 1) / (float) numAngles;
            const auto midAngle = (angle0 + angle1) * 0.5f;
            const auto midX = (x0 + x1) * 0.5f;

            juce::Path wedge;
            wedge.startNewSubPath (shapePointAt (x0, angle0));
            wedge.lineTo (shapePointAt (x0, angle1));
            wedge.lineTo (shapePointAt (x1, angle1));
            wedge.lineTo (shapePointAt (x1, angle0));
            wedge.closeSubPath();

            auto colour = colourAt (midX, midAngle);

            const auto midHeight = (a.y + b.y) * 0.5f;
            const auto slope = b.y - a.y;

            const auto heightTint = (midHeight - 0.5f) * 0.18f;
            const auto slopeTint = slope * 0.32f;
            const auto tint = juce::jlimit (-0.35f, 0.35f, heightTint + slopeTint);

            if (tint >= 0.0f)
                colour = colour.interpolatedWith (juce::Colours::white, tint);
            else
                colour = colour.interpolatedWith (juce::Colours::black, -tint);

            g.setColour (colour);
            g.fillPath (wedge);
        }
    }

    g.setColour (juce::Colours::white.withAlpha (0.12f));

    if (previewShape == PreviewShape::circle)
        g.drawEllipse (shapeArea, 1.0f);
    else
        g.drawRect (shapeArea, 1.0f);

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));

    for (int i = 0; i < (int) colourProfiles.size(); ++i)
    {
        const auto angle = colourProfiles[(size_t) i].angleDeg;
        const auto marker = circularPointAt (outerRadius + 18.0f, angle);
        const auto colour = sampleProfileAt (0.18f, i).colour;
        const auto isSelected = i == selectedColourProfileIndex;

        g.setColour (colour);
        g.fillEllipse (marker.x - 7.0f, marker.y - 7.0f, 14.0f, 14.0f);

        g.setColour (juce::Colours::white.withAlpha (isSelected ? 0.95f : 0.45f));
        g.drawEllipse (marker.x - 7.0f, marker.y - 7.0f, 14.0f, 14.0f, isSelected ? 2.0f : 1.0f);

        g.drawText ("P" + juce::String (i + 1),
                    juce::Rectangle<float> (marker.x - 14.0f, marker.y - 26.0f, 28.0f, 16.0f),
                    juce::Justification::centred);
    }
}

