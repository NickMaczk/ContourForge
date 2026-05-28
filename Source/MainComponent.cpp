#include "MainComponent.h"

MainComponent::MainComponent()
{
    setSize (1000, 620);

    profilePoints =
    {
        { 0.00f, 0.10f, juce::Colour::fromRGB (24, 24, 28) },
        { 0.16f, 0.82f, juce::Colour::fromRGB (210, 214, 224) },
        { 0.36f, 0.48f, juce::Colour::fromRGB (82, 86, 98) },
        { 0.62f, 0.66f, juce::Colour::fromRGB (155, 160, 174) },
        { 1.00f, 0.18f, juce::Colour::fromRGB (18, 18, 22) }
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
    g.drawText ("Drag the profile points. The preview is generated from the same profile.",
                24, 46, 560, 22, juce::Justification::left);

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

    draggedPointIndex = -1;

    for (int i = 0; i < (int) profilePoints.size(); ++i)
    {
        if (profileToScreen (profilePoints[(size_t) i], area).getDistanceFrom (mouse) < 12.0f)
        {
            draggedPointIndex = i;
            break;
        }
    }
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
    area = area.reduced (28.0f);
    return
    {
        area.getX() + p.x * area.getWidth(),
        area.getBottom() - p.y * area.getHeight()
    };
}

MainComponent::ProfilePoint MainComponent::screenToProfile (juce::Point<float> p,
                                                            juce::Rectangle<float> area,
                                                            int pointIndex) const
{
    area = area.reduced (28.0f);

    auto x = (p.x - area.getX()) / area.getWidth();
    auto y = (area.getBottom() - p.y) / area.getHeight();

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

MainComponent::ProfilePoint MainComponent::sampleProfileAt (float x) const
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
                juce::jmap (amount, a.y, b.y),
                a.colour.interpolatedWith (b.colour, amount)
            };
        }
    }

    return profilePoints.back();
}

void MainComponent::drawProfileEditor (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (juce::Colour::fromRGB (22, 22, 26));
    g.fillRoundedRectangle (area, 14.0f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (area, 14.0f, 1.0f);

    auto graph = area.reduced (28.0f);

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

        g.setColour (profilePoints[(size_t) i].colour);
        g.fillEllipse (pt.x - 7.0f, pt.y - 7.0f, 14.0f, 14.0f);

        g.setColour (juce::Colours::white.withAlpha (isDragged ? 0.95f : 0.5f));
        g.drawEllipse (pt.x - 7.0f, pt.y - 7.0f, 14.0f, 14.0f, isDragged ? 2.0f : 1.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText ("Geometry + colour profile", area.reduced (18.0f).removeFromTop (24.0f),
                juce::Justification::left);
}

void MainComponent::drawPreview (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (juce::Colour::fromRGB (22, 22, 26));
    g.fillRoundedRectangle (area, 14.0f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (area, 14.0f, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText ("Pseudo-3D preview", area.reduced (18.0f).removeFromTop (24.0f),
                juce::Justification::left);

    auto shapeArea = area.reduced (70.0f, 82.0f);
    const auto maxInset = juce::jmin (shapeArea.getWidth(), shapeArea.getHeight()) * 0.46f;

    auto getContour = [&] (const ProfilePoint& p)
    {
        return shapeArea.reduced (p.x * maxInset);
    };

    auto getCornerRadius = [] (const ProfilePoint& p)
    {
        return juce::jlimit (8.0f, 72.0f, 72.0f - p.x * 42.0f);
    };

    constexpr int numBands = 64;

    for (int i = 0; i < numBands; ++i)
    {
        const auto x0 = (float) i / (float) numBands;
        const auto x1 = (float) (i + 1) / (float) numBands;

        const auto a = sampleProfileAt (x0);
        const auto b = sampleProfileAt (x1);

        const auto outer = getContour (a);
        const auto inner = getContour (b);

        juce::Path band;
        band.setUsingNonZeroWinding (false);
        band.addRoundedRectangle (outer, getCornerRadius (a));
        band.addRoundedRectangle (inner, getCornerRadius (b));

        g.setColour (a.colour.interpolatedWith (b.colour, 0.5f));
        g.fillPath (band);
    }

    const auto inner = getContour (profilePoints.back());

    g.setColour (profilePoints.back().colour);
    g.fillRoundedRectangle (inner, getCornerRadius (profilePoints.back()));

    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (getContour (profilePoints.front()), getCornerRadius (profilePoints.front()), 1.0f);
}
