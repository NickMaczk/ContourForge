#include "MainComponent.h"

namespace
{
    // Separable box blur over a float coverage buffer, clamped at the edges.
    // A few passes approximate a Gaussian. Used for the contour AO band.
    void boxBlur1D (const std::vector<float>& src, std::vector<float>& dst,
                    int w, int h, int radius, bool horizontal)
    {
        const auto norm = 1.0f / (float) (2 * radius + 1);

        if (horizontal)
        {
            for (int y = 0; y < h; ++y)
            {
                const auto row = y * w;
                float sum = 0.0f;

                for (int i = -radius; i <= radius; ++i)
                    sum += src[(size_t) (row + juce::jlimit (0, w - 1, i))];

                for (int x = 0; x < w; ++x)
                {
                    dst[(size_t) (row + x)] = sum * norm;
                    sum += src[(size_t) (row + juce::jlimit (0, w - 1, x + radius + 1))]
                         - src[(size_t) (row + juce::jlimit (0, w - 1, x - radius))];
                }
            }
        }
        else
        {
            for (int x = 0; x < w; ++x)
            {
                float sum = 0.0f;

                for (int i = -radius; i <= radius; ++i)
                    sum += src[(size_t) (juce::jlimit (0, h - 1, i) * w + x)];

                for (int y = 0; y < h; ++y)
                {
                    dst[(size_t) (y * w + x)] = sum * norm;
                    sum += src[(size_t) (juce::jlimit (0, h - 1, y + radius + 1) * w + x)]
                         - src[(size_t) (juce::jlimit (0, h - 1, y - radius) * w + x)];
                }
            }
        }
    }

    void blurCoverageMask (std::vector<float>& buffer, int w, int h, int radius, int passes)
    {
        if (radius < 1 || w < 1 || h < 1)
            return;

        std::vector<float> scratch (buffer.size());

        for (int p = 0; p < passes; ++p)
        {
            boxBlur1D (buffer, scratch, w, h, radius, true);
            boxBlur1D (scratch, buffer, w, h, radius, false);
        }
    }

    // Felzenszwalb 1D squared-distance transform over a strided slice of f.
    void distanceTransform1D (std::vector<float>& f, int n, int stride, int offset)
    {
        constexpr float inf = 1.0e20f;

        std::vector<float> result ((size_t) n);
        std::vector<int> v ((size_t) n);
        std::vector<float> z ((size_t) n + 1);

        int k = 0;
        v[0] = 0;
        z[0] = -inf;
        z[1] = inf;

        for (int q = 1; q < n; ++q)
        {
            const auto fq = f[(size_t) (offset + q * stride)];
            float s;

            while (true)
            {
                const auto fv = f[(size_t) (offset + v[(size_t) k] * stride)];
                s = ((fq + (float) q * q) - (fv + (float) v[(size_t) k] * v[(size_t) k]))
                    / (2.0f * (float) q - 2.0f * (float) v[(size_t) k]);

                if (s <= z[(size_t) k] && k > 0)
                    --k;
                else
                    break;
            }

            ++k;
            v[(size_t) k] = q;
            z[(size_t) k] = s;
            z[(size_t) k + 1] = inf;
        }

        k = 0;

        for (int q = 0; q < n; ++q)
        {
            while (z[(size_t) k + 1] < (float) q)
                ++k;

            const auto d = (float) (q - v[(size_t) k]);
            result[(size_t) q] = d * d + f[(size_t) (offset + v[(size_t) k] * stride)];
        }

        for (int q = 0; q < n; ++q)
            f[(size_t) (offset + q * stride)] = result[(size_t) q];
    }

    // Inward-offset profile field: profileX = distance-to-edge / max-distance.
    // Iso-contours are the outline pushed inward by a constant distance, so the steps
    // are spread evenly and the ridge follows the shape's skeleton (a line for a bar,
    // not a point). -1 marks outside pixels.
    std::vector<float> buildProfileField (const std::vector<unsigned char>& inside, int w, int h)
    {
        constexpr float inf = 1.0e20f;

        std::vector<float> f ((size_t) w * (size_t) h);

        for (size_t i = 0; i < f.size(); ++i)
            f[i] = inside[i] ? inf : 0.0f; // distance to the nearest outside pixel

        for (int x = 0; x < w; ++x)
            distanceTransform1D (f, h, w, x);

        for (int y = 0; y < h; ++y)
            distanceTransform1D (f, w, 1, y * w);

        float maxDistSq = 0.0f;

        for (size_t i = 0; i < f.size(); ++i)
            if (inside[i])
                maxDistSq = juce::jmax (maxDistSq, f[i]);

        const auto maxDist = juce::jmax (1.0f, std::sqrt (maxDistSq));

        std::vector<float> field ((size_t) w * (size_t) h);

        for (size_t i = 0; i < field.size(); ++i)
            field[i] = inside[i] ? std::sqrt (f[i]) / maxDist : 0.0f; // outside 0 for the blur

        // A gentle smoothing makes the gradient C1-continuous (no normal-map facets at
        // cell edges) and dissolves tiny medial-axis specks. Negligible corner softening
        // at this resolution. Outside pixels are restored to the -1 sentinel afterwards.
        blurCoverageMask (field, w, h, 2, 2);

        for (size_t i = 0; i < field.size(); ++i)
            if (! inside[i])
                field[i] = -1.0f;

        return field;
    }

    // Trim trailing zeros so a ratio reads "3:1" / "2.35:1" rather than "3.00:1.00".
    juce::String formatRatioValue (float v)
    {
        auto s = juce::String (v, 2);

        if (s.containsChar ('.'))
            s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");

        return s;
    }

    struct ExportOptions
    {
        bool wantHeight = true;
        bool wantNormal = true;
        bool wantBeauty = true;
        bool wantCavity = false;
        bool wantAo     = false;
        int  width      = 1024;
        int  heightPx   = 1024;
        int  grid       = 0; // 0 = off; otherwise snap each canvas up to a multiple of this
    };

    class ExportOptionsComponent : public juce::Component
    {
    public:
        std::function<void (ExportOptions)> onExport;
        std::function<void()> onCancel;

        explicit ExportOptionsComponent (ExportOptions defaults)
        {
            auto addToggle = [this] (juce::ToggleButton& button, const juce::String& text, bool on)
            {
                button.setButtonText (text);
                button.setToggleState (on, juce::dontSendNotification);
                button.setColour (juce::ToggleButton::textColourId, juce::Colours::white.withAlpha (0.85f));
                button.setColour (juce::ToggleButton::tickColourId, juce::Colours::white.withAlpha (0.85f));
                button.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::white.withAlpha (0.35f));
                addAndMakeVisible (button);
            };

            addToggle (heightToggle, "Height", defaults.wantHeight);
            addToggle (normalToggle, "Normal", defaults.wantNormal);
            addToggle (beautyToggle, "Beauty", defaults.wantBeauty);
            addToggle (cavityToggle, "Cavity", defaults.wantCavity);
            addToggle (aoToggle,     "Ambient Occlusion", defaults.wantAo);

            auto setupLabel = [this] (juce::Label& label, const juce::String& text)
            {
                label.setText (text, juce::dontSendNotification);
                label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
                addAndMakeVisible (label);
            };

            setupLabel (mapsLabel, "Maps");
            setupLabel (sizeLabel, "Size (px)");
            setupLabel (gridLabel, "Snap to grid");

            gridBox.addItem ("Off", 1);

            for (auto step : { 8, 16, 32, 64, 128, 256 })
                gridBox.addItem (juce::String (step), step);

            gridBox.setSelectedId (defaults.grid > 1 ? defaults.grid : 1, juce::dontSendNotification);
            addAndMakeVisible (gridBox);

            auto setupEditor = [this] (juce::TextEditor& editor, int value)
            {
                editor.setInputRestrictions (4, "0123456789");
                editor.setText (juce::String (value), juce::dontSendNotification);
                editor.setJustification (juce::Justification::centred);
                addAndMakeVisible (editor);
            };

            setupEditor (widthEditor, defaults.width);
            setupEditor (heightEditor, defaults.heightPx);

            timesLabel.setText (juce::String::fromUTF8 ("\xC3\x97"), juce::dontSendNotification);
            timesLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
            timesLabel.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (timesLabel);

            exportButton.onClick = [this]
            {
                ExportOptions result;
                result.wantHeight = heightToggle.getToggleState();
                result.wantNormal = normalToggle.getToggleState();
                result.wantBeauty = beautyToggle.getToggleState();
                result.wantCavity = cavityToggle.getToggleState();
                result.wantAo     = aoToggle.getToggleState();
                result.width    = juce::jlimit (1, 8192, widthEditor.getText().getIntValue());
                result.heightPx = juce::jlimit (1, 8192, heightEditor.getText().getIntValue());

                const auto gridId = gridBox.getSelectedId();
                result.grid = gridId > 1 ? gridId : 0;

                if (onExport != nullptr)
                    onExport (result);
            };

            cancelButton.onClick = [this]
            {
                if (onCancel != nullptr)
                    onCancel();
            };

            addAndMakeVisible (exportButton);
            addAndMakeVisible (cancelButton);

            setSize (300, 372);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (18);

            mapsLabel.setBounds (area.removeFromTop (20));
            area.removeFromTop (4);

            for (auto* toggle : { &heightToggle, &normalToggle, &beautyToggle, &cavityToggle, &aoToggle })
            {
                toggle->setBounds (area.removeFromTop (26));
                area.removeFromTop (2);
            }

            area.removeFromTop (12);
            sizeLabel.setBounds (area.removeFromTop (20));
            area.removeFromTop (4);

            auto sizeRow = area.removeFromTop (28);
            widthEditor.setBounds (sizeRow.removeFromLeft (96));
            timesLabel.setBounds (sizeRow.removeFromLeft (28));
            heightEditor.setBounds (sizeRow.removeFromLeft (96));

            area.removeFromTop (12);
            gridLabel.setBounds (area.removeFromTop (20));
            area.removeFromTop (4);
            gridBox.setBounds (area.removeFromTop (26).removeFromLeft (120));

            area.removeFromTop (16);
            auto buttonRow = area.removeFromTop (30);
            cancelButton.setBounds (buttonRow.removeFromRight (90));
            buttonRow.removeFromRight (8);
            exportButton.setBounds (buttonRow.removeFromRight (90));
        }

    private:
        juce::ToggleButton heightToggle, normalToggle, beautyToggle, cavityToggle, aoToggle;
        juce::Label mapsLabel, sizeLabel, timesLabel, gridLabel;
        juce::TextEditor widthEditor, heightEditor;
        juce::ComboBox gridBox;
        juce::TextButton exportButton { "Export" }, cancelButton { "Cancel" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportOptionsComponent)
    };

    // Colour picker with a visual colourspace plus proper 0-255 R/G/B and 6-digit hex
    // fields. JUCE's built-in ColourSelector hex label is an 8-digit ARGB field that
    // mangles plain RGB entry, so we drive the numeric side ourselves.
    class ColourPickerComponent : public juce::Component,
                                  private juce::ChangeListener
    {
    public:
        std::function<void (juce::Colour)> onColourChange;

        explicit ColourPickerComponent (juce::Colour initial)
            : selector (juce::ColourSelector::showColourspace, 4, 7)
        {
            addAndMakeVisible (selector);
            selector.setCurrentColour (initial, juce::dontSendNotification);
            selector.addChangeListener (this);

            auto setupField = [this] (juce::TextEditor& editor, const juce::String& allowed,
                                      int maxLen, std::function<void()> commit)
            {
                editor.setInputRestrictions (maxLen, allowed);
                editor.setJustification (juce::Justification::centred);
                editor.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (38, 38, 44));
                editor.setColour (juce::TextEditor::textColourId, juce::Colours::white.withAlpha (0.9f));
                editor.onReturnKey  = commit;
                editor.onFocusLost  = commit;
                addAndMakeVisible (editor);
            };

            setupField (redEditor,   "0123456789", 3, [this] { applyRgb(); });
            setupField (greenEditor, "0123456789", 3, [this] { applyRgb(); });
            setupField (blueEditor,  "0123456789", 3, [this] { applyRgb(); });
            setupField (hexEditor,   "0123456789ABCDEFabcdef", 6, [this] { applyHex(); });

            auto setupLabel = [this] (juce::Label& label, const juce::String& text)
            {
                label.setText (text, juce::dontSendNotification);
                label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
                label.setJustificationType (juce::Justification::centred);
                addAndMakeVisible (label);
            };

            setupLabel (redLabel, "R");
            setupLabel (greenLabel, "G");
            setupLabel (blueLabel, "B");
            setupLabel (hexLabel, "Hex #");

            updateFields (initial);
            setSize (300, 330);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (12);

            selector.setBounds (area.removeFromTop (200));
            area.removeFromTop (12);

            auto rgbRow = area.removeFromTop (44);
            const auto cellWidth = rgbRow.getWidth() / 3;

            auto layoutField = [] (juce::Rectangle<int> cell, juce::Label& label, juce::TextEditor& editor)
            {
                label.setBounds (cell.removeFromTop (16));
                editor.setBounds (cell.removeFromTop (26).reduced (6, 0));
            };

            layoutField (rgbRow.removeFromLeft (cellWidth), redLabel, redEditor);
            layoutField (rgbRow.removeFromLeft (cellWidth), greenLabel, greenEditor);
            layoutField (rgbRow, blueLabel, blueEditor);

            area.removeFromTop (10);
            auto hexRow = area.removeFromTop (26);
            hexLabel.setBounds (hexRow.removeFromLeft (52));
            hexEditor.setBounds (hexRow.removeFromLeft (110));
        }

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            const auto colour = selector.getCurrentColour();
            updateFields (colour);

            if (onColourChange != nullptr)
                onColourChange (colour);
        }

        void updateFields (juce::Colour colour)
        {
            if (updating)
                return;

            const juce::ScopedValueSetter<bool> guard (updating, true);

            redEditor.setText   (juce::String ((int) colour.getRed()),   juce::dontSendNotification);
            greenEditor.setText (juce::String ((int) colour.getGreen()), juce::dontSendNotification);
            blueEditor.setText  (juce::String ((int) colour.getBlue()),  juce::dontSendNotification);
            hexEditor.setText   (colour.toDisplayString (false),         juce::dontSendNotification);
        }

        void commitColour (juce::Colour colour)
        {
            {
                const juce::ScopedValueSetter<bool> guard (updating, true);
                selector.setCurrentColour (colour, juce::dontSendNotification);
            }

            updateFields (colour);

            if (onColourChange != nullptr)
                onColourChange (colour);
        }

        void applyRgb()
        {
            if (updating)
                return;

            const auto r = (juce::uint8) juce::jlimit (0, 255, redEditor.getText().getIntValue());
            const auto g = (juce::uint8) juce::jlimit (0, 255, greenEditor.getText().getIntValue());
            const auto b = (juce::uint8) juce::jlimit (0, 255, blueEditor.getText().getIntValue());

            commitColour (juce::Colour::fromRGB (r, g, b));
        }

        void applyHex()
        {
            if (updating)
                return;

            const auto value = hexEditor.getText().getHexValue32();

            commitColour (juce::Colour::fromRGB (
                (juce::uint8) ((value >> 16) & 0xff),
                (juce::uint8) ((value >> 8)  & 0xff),
                (juce::uint8) (value & 0xff)));
        }

        juce::ColourSelector selector;
        juce::TextEditor redEditor, greenEditor, blueEditor, hexEditor;
        juce::Label redLabel, greenLabel, blueLabel, hexLabel;
        bool updating = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColourPickerComponent)
    };

    // Free aspect-ratio entry: X and Y numeric fields, live.
    class AspectRatioComponent : public juce::Component
    {
    public:
        std::function<void (float, float)> onChange;

        AspectRatioComponent (float x, float y)
        {
            auto setupField = [this] (juce::TextEditor& editor, float value)
            {
                editor.setInputRestrictions (6, "0123456789.");
                editor.setText (formatRatioValue (value), juce::dontSendNotification);
                editor.setJustification (juce::Justification::centred);
                editor.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (38, 38, 44));
                editor.setColour (juce::TextEditor::textColourId, juce::Colours::white.withAlpha (0.9f));
                editor.onTextChange = [this] { apply(); };
                addAndMakeVisible (editor);
            };

            setupField (xEditor, x);
            setupField (yEditor, y);

            colonLabel.setText (":", juce::dontSendNotification);
            colonLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
            colonLabel.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (colonLabel);

            setSize (170, 54);
        }

        void resized() override
        {
            auto row = getLocalBounds().reduced (12, 14);
            const auto colonWidth = 16;
            const auto fieldWidth = (row.getWidth() - colonWidth) / 2;

            xEditor.setBounds (row.removeFromLeft (fieldWidth));
            colonLabel.setBounds (row.removeFromLeft (colonWidth));
            yEditor.setBounds (row.removeFromLeft (fieldWidth));
        }

    private:
        void apply()
        {
            const auto x = juce::jlimit (0.1f, 100.0f, xEditor.getText().getFloatValue());
            const auto y = juce::jlimit (0.1f, 100.0f, yEditor.getText().getFloatValue());

            if (onChange != nullptr)
                onChange (x, y);
        }

        juce::TextEditor xEditor, yEditor;
        juce::Label colonLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AspectRatioComponent)
    };
}

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

    drawTopButton (getExportButtonArea(), "Export");
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

juce::Rectangle<float> MainComponent::getExportButtonArea() const
{
    auto r = getLocalBounds().toFloat().reduced (24.0f);
    return { r.getRight() - 258.0f, 20.0f, 78.0f, 28.0f };
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

    if (getExportButtonArea().contains (mouse))
    {
        showExportDialog();
        return;
    }

    if (! getToolsScrollThumb().isEmpty() && getToolsScrollTrack().contains (mouse))
    {
        draggingToolsScrollbar = true;
        setToolsScrollFromMouseY (mouse.y);
        repaint();
        return;
    }

    if (getToolsViewport().contains (mouse))
    {
        auto previewControls = getToolsArea().reduced (18.0f);
        previewControls.removeFromTop (54.0f);
        previewControls.removeFromRight (12.0f); // gutter for the scrollbar
        previewControls.translate (0.0f, -toolsScrollOffset);
        previewControls.setHeight (10000.0f); // unbounded so rows aren't clamped to the panel

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
        const auto customButton = shapeRow.removeFromLeft (70.0f);
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
        const auto cavityBounceSlider = nextToolRow();
        const auto shadowLayerSlider = nextToolRow();
        const auto dropShadowSlider = nextToolRow();
        const auto dropShadowLengthSlider = nextToolRow();
        const auto dropShadowSoftnessSlider = nextToolRow();
        const auto dropShadowFalloffSlider = nextToolRow();
        const auto contourAoBlurSlider = nextToolRow();
        const auto contourAoOpacitySlider = nextToolRow();
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
            else if (slider == PreviewSlider::cavityBounce)
            {
                cavityBounceAmount = amount;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::shadowLayer)
            {
                shadowLayerAmount = amount;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::dropShadow)
            {
                dropShadowAmount = amount;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::dropShadowLength)
            {
                dropShadowLength = amount;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::dropShadowSoftness)
            {
                dropShadowSoftness = amount;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::dropShadowFalloff)
            {
                dropShadowFalloff = amount;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::contourAoBlur)
            {
                contourAoBlur = amount;
                previewMode = PreviewMode::material;
            }
            else if (slider == PreviewSlider::contourAoOpacity)
            {
                contourAoOpacity = amount;
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
            aspectRatioX = 1.0f;
            aspectRatioY = 1.0f;
            roundedCornerMask = 15;
            cornerRadiusAmount = 1.0f;
            repaint();
            return;
        }

        if (squareButton.contains (mouse))
        {
            previewShape = PreviewShape::rectangle;
            aspectRatioX = 1.0f;
            aspectRatioY = 1.0f;
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

        if (customButton.contains (mouse))
        {
            // Re-activate an already-loaded shape with a single tap; otherwise (or when
            // already showing it) open the loader to pick / replace the SVG.
            if (! customShapeField.empty() && previewShape != PreviewShape::custom)
            {
                previewShape = PreviewShape::custom;
                aspectRatioX = customFieldW >= customFieldH
                    ? juce::jlimit (0.1f, 100.0f, (float) customFieldW / (float) juce::jmax (1, customFieldH)) : 1.0f;
                aspectRatioY = customFieldH >  customFieldW
                    ? juce::jlimit (0.1f, 100.0f, (float) customFieldH / (float) juce::jmax (1, customFieldW)) : 1.0f;
                repaint();
            }
            else
            {
                showCustomShapeDialog();
            }

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
            auto picker = std::make_unique<ColourPickerComponent> (baseColour);

            picker->onColourChange = [this] (juce::Colour colour)
            {
                baseColour = colour;
                previewMode = PreviewMode::material;
                repaint();
            };

            juce::CallOutBox::launchAsynchronously (
                std::move (picker),
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

        if (cavityBounceSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::cavityBounce;
            setPreviewSliderValue (draggedPreviewSlider, cavityBounceSlider, mouse.x);
            return;
        }

        if (shadowLayerSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::shadowLayer;
            setPreviewSliderValue (draggedPreviewSlider, shadowLayerSlider, mouse.x);
            return;
        }

        if (dropShadowSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::dropShadow;
            setPreviewSliderValue (draggedPreviewSlider, dropShadowSlider, mouse.x);
            return;
        }

        if (dropShadowLengthSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::dropShadowLength;
            setPreviewSliderValue (draggedPreviewSlider, dropShadowLengthSlider, mouse.x);
            return;
        }

        if (dropShadowSoftnessSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::dropShadowSoftness;
            setPreviewSliderValue (draggedPreviewSlider, dropShadowSoftnessSlider, mouse.x);
            return;
        }

        if (dropShadowFalloffSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::dropShadowFalloff;
            setPreviewSliderValue (draggedPreviewSlider, dropShadowFalloffSlider, mouse.x);
            return;
        }

        if (contourAoBlurSlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::contourAoBlur;
            setPreviewSliderValue (draggedPreviewSlider, contourAoBlurSlider, mouse.x);
            return;
        }

        if (contourAoOpacitySlider.contains (mouse))
        {
            draggedPreviewSlider = PreviewSlider::contourAoOpacity;
            setPreviewSliderValue (draggedPreviewSlider, contourAoOpacitySlider, mouse.x);
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
            auto ratio = std::make_unique<AspectRatioComponent> (aspectRatioX, aspectRatioY);

            ratio->onChange = [this] (float x, float y)
            {
                aspectRatioX = x;
                aspectRatioY = y;
                previewShape = PreviewShape::rectangle;
                repaint();
            };

            juce::CallOutBox::launchAsynchronously (
                std::move (ratio),
                ratioButton.toNearestInt(),
                this);

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
    if (draggingToolsScrollbar)
    {
        setToolsScrollFromMouseY (e.position.y);
        repaint();
        return;
    }

    if (draggedPreviewSlider != PreviewSlider::none)
    {
        auto previewControls = getToolsArea().reduced (18.0f);
        previewControls.removeFromTop (54.0f);
        previewControls.removeFromRight (12.0f); // gutter for the scrollbar
        previewControls.translate (0.0f, -toolsScrollOffset);
        previewControls.setHeight (10000.0f); // unbounded so rows aren't clamped to the panel

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
        const auto cavityBounceSlider = nextToolRow();
        const auto shadowLayerSlider = nextToolRow();
        const auto dropShadowSlider = nextToolRow();
        const auto dropShadowLengthSlider = nextToolRow();
        const auto dropShadowSoftnessSlider = nextToolRow();
        const auto dropShadowFalloffSlider = nextToolRow();
        const auto contourAoBlurSlider = nextToolRow();
        const auto contourAoOpacitySlider = nextToolRow();
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
        else if (draggedPreviewSlider == PreviewSlider::cavityBounce)
            sliderArea = cavityBounceSlider;
        else if (draggedPreviewSlider == PreviewSlider::shadowLayer)
            sliderArea = shadowLayerSlider;
        else if (draggedPreviewSlider == PreviewSlider::dropShadow)
            sliderArea = dropShadowSlider;
        else if (draggedPreviewSlider == PreviewSlider::dropShadowLength)
            sliderArea = dropShadowLengthSlider;
        else if (draggedPreviewSlider == PreviewSlider::dropShadowSoftness)
            sliderArea = dropShadowSoftnessSlider;
        else if (draggedPreviewSlider == PreviewSlider::dropShadowFalloff)
            sliderArea = dropShadowFalloffSlider;
        else if (draggedPreviewSlider == PreviewSlider::contourAoBlur)
            sliderArea = contourAoBlurSlider;
        else if (draggedPreviewSlider == PreviewSlider::contourAoOpacity)
            sliderArea = contourAoOpacitySlider;
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
        else if (draggedPreviewSlider == PreviewSlider::cavityBounce)
        {
            cavityBounceAmount = amount;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::shadowLayer)
        {
            shadowLayerAmount = amount;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::dropShadow)
        {
            dropShadowAmount = amount;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::dropShadowLength)
        {
            dropShadowLength = amount;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::dropShadowSoftness)
        {
            dropShadowSoftness = amount;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::dropShadowFalloff)
        {
            dropShadowFalloff = amount;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::contourAoBlur)
        {
            contourAoBlur = amount;
            previewMode = PreviewMode::material;
        }
        else if (draggedPreviewSlider == PreviewSlider::contourAoOpacity)
        {
            contourAoOpacity = amount;
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
    draggingToolsScrollbar = false;
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

juce::Rectangle<float> MainComponent::getToolsViewport() const
{
    // The scrollable region of the tools panel (everything below the fixed info row).
    auto inner = getToolsArea().reduced (18.0f);
    inner.removeFromTop (54.0f);
    return inner;
}

void MainComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (! getToolsArea().contains (e.position))
        return;

    const auto maxScroll = juce::jmax (0.0f, toolsContentHeight - getToolsViewport().getHeight());

    toolsScrollOffset = juce::jlimit (0.0f, maxScroll, toolsScrollOffset - wheel.deltaY * 80.0f);
    repaint();
}

juce::Rectangle<float> MainComponent::getToolsScrollTrack() const
{
    const auto viewport = getToolsViewport();
    const auto innerRight = getToolsArea().reduced (18.0f).getRight();

    // The reserved gutter on the right of the controls.
    return { innerRight - 12.0f, viewport.getY(), 12.0f, viewport.getHeight() };
}

juce::Rectangle<float> MainComponent::getToolsScrollThumb() const
{
    const auto viewport = getToolsViewport();
    const auto maxScroll = juce::jmax (0.0f, toolsContentHeight - viewport.getHeight());

    if (maxScroll <= 0.5f)
        return {}; // everything fits; no scrollbar needed

    const auto track = getToolsScrollTrack();
    const auto thumbHeight = juce::jmax (32.0f,
        viewport.getHeight() * viewport.getHeight() / juce::jmax (1.0f, toolsContentHeight));

    const auto t = toolsScrollOffset / maxScroll;
    const auto thumbY = viewport.getY() + t * (viewport.getHeight() - thumbHeight);

    return { track.getX() + 2.0f, thumbY, track.getWidth() - 4.0f, thumbHeight };
}

void MainComponent::setToolsScrollFromMouseY (float y)
{
    const auto viewport = getToolsViewport();
    const auto maxScroll = juce::jmax (0.0f, toolsContentHeight - viewport.getHeight());

    if (maxScroll <= 0.0f)
    {
        toolsScrollOffset = 0.0f;
        return;
    }

    const auto thumbHeight = juce::jmax (32.0f,
        viewport.getHeight() * viewport.getHeight() / juce::jmax (1.0f, toolsContentHeight));

    // Centre the thumb on the stylus, then map its position to the scroll range.
    const auto t = juce::jlimit (0.0f, 1.0f,
        (y - viewport.getY() - thumbHeight * 0.5f) / juce::jmax (1.0f, viewport.getHeight() - thumbHeight));

    toolsScrollOffset = t * maxScroll;
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
        : previewShape == PreviewShape::custom ? "custom"
        : "square");

    if (customShapeSvg.isNotEmpty())
        root->setProperty ("customShapeSvg", customShapeSvg);
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
    root->setProperty ("cavityBounceAmount", cavityBounceAmount);
    root->setProperty ("shadowLayerAmount", shadowLayerAmount);
    root->setProperty ("dropShadowAmount", dropShadowAmount);
    root->setProperty ("dropShadowLength", dropShadowLength);
    root->setProperty ("dropShadowSoftness", dropShadowSoftness);
    root->setProperty ("dropShadowFalloff", dropShadowFalloff);
    root->setProperty ("contourAoBlur", contourAoBlur);
    root->setProperty ("contourAoOpacity", contourAoOpacity);
    root->setProperty ("specularLayerAmount", specularLayerAmount);
    root->setProperty ("specularCatchAmount", specularCatchAmount);
    root->setProperty ("chamferAmount", chamferAmount);
    root->setProperty ("previewQualityDivisor", previewQualityDivisor);
    root->setProperty ("gridDivisor", gridDivisor);
    root->setProperty ("aspectRatioX", aspectRatioX);
    root->setProperty ("aspectRatioY", aspectRatioY);
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

    // Rebuild a saved custom silhouette before resolving the shape.
    const auto loadedCustomSvg = root->getProperty ("customShapeSvg").toString();

    if (loadedCustomSvg.isNotEmpty())
        loadCustomShapeFromSvg (loadedCustomSvg);

    previewShape = previewShapeText == "rectangle"
        ? PreviewShape::rectangle
        : previewShapeText == "square"
            ? PreviewShape::square
            : previewShapeText == "custom"
                ? (customShapeField.empty() ? PreviewShape::circle : PreviewShape::custom)
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

    const auto loadedCavityBounceAmount = root->getProperty ("cavityBounceAmount");

    if (! loadedCavityBounceAmount.isVoid())
        cavityBounceAmount = juce::jlimit (0.0f, 1.0f, (float) (double) loadedCavityBounceAmount);

    const auto loadedShadowLayerAmount = root->getProperty ("shadowLayerAmount");

    if (! loadedShadowLayerAmount.isVoid())
        shadowLayerAmount = juce::jlimit (0.0f, 1.0f, (float) (double) loadedShadowLayerAmount);

    const auto loadedDropShadowAmount = root->getProperty ("dropShadowAmount");

    if (! loadedDropShadowAmount.isVoid())
        dropShadowAmount = juce::jlimit (0.0f, 1.0f, (float) (double) loadedDropShadowAmount);

    const auto loadedDropShadowLength = root->getProperty ("dropShadowLength");

    if (! loadedDropShadowLength.isVoid())
        dropShadowLength = juce::jlimit (0.0f, 1.0f, (float) (double) loadedDropShadowLength);

    const auto loadedDropShadowSoftness = root->getProperty ("dropShadowSoftness");

    if (! loadedDropShadowSoftness.isVoid())
        dropShadowSoftness = juce::jlimit (0.0f, 1.0f, (float) (double) loadedDropShadowSoftness);

    const auto loadedDropShadowFalloff = root->getProperty ("dropShadowFalloff");

    if (! loadedDropShadowFalloff.isVoid())
        dropShadowFalloff = juce::jlimit (0.0f, 1.0f, (float) (double) loadedDropShadowFalloff);

    const auto loadedContourAoBlur = root->getProperty ("contourAoBlur");

    if (! loadedContourAoBlur.isVoid())
        contourAoBlur = juce::jlimit (0.0f, 1.0f, (float) (double) loadedContourAoBlur);

    const auto loadedContourAoOpacity = root->getProperty ("contourAoOpacity");

    if (! loadedContourAoOpacity.isVoid())
        contourAoOpacity = juce::jlimit (0.0f, 1.0f, (float) (double) loadedContourAoOpacity);

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

    const auto loadedAspectRatioX = root->getProperty ("aspectRatioX");
    const auto loadedAspectRatioY = root->getProperty ("aspectRatioY");

    if (! loadedAspectRatioX.isVoid() && ! loadedAspectRatioY.isVoid())
    {
        aspectRatioX = juce::jlimit (0.1f, 100.0f, (float) (double) loadedAspectRatioX);
        aspectRatioY = juce::jlimit (0.1f, 100.0f, (float) (double) loadedAspectRatioY);
    }
    else
    {
        // Migrate the old fixed preset index from older project files.
        const float presetRatios[][2] { {1,1}, {2,1}, {3,1}, {4,3}, {3,4}, {1,2}, {2,4} };
        const auto idx = (int) root->getProperty ("aspectPresetIndex");

        if (idx >= 0 && idx < 7)
        {
            aspectRatioX = presetRatios[idx][0];
            aspectRatioY = presetRatios[idx][1];
        }
    }

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

bool MainComponent::loadCustomShapeFromSvg (const juce::String& svgText)
{
    const auto xml = juce::parseXML (svgText);

    if (xml == nullptr)
        return false;

    auto drawable = juce::Drawable::createFromSVG (*xml);

    if (drawable == nullptr)
        return false;

    const auto bounds = drawable->getDrawableBounds();

    if (bounds.isEmpty())
        return false;

    // Rasterise the silhouette onto a fixed grid, keeping the SVG's aspect ratio.
    // A high resolution captures curves/diagonals finely; bilinear sampling at render
    // time then smooths the distance field between cells.
    constexpr int longSide = 1024;
    const auto aspect = bounds.getWidth() / juce::jmax (0.0001f, bounds.getHeight());

    const auto shapeW = aspect >= 1.0f ? longSide : juce::jmax (1, juce::roundToInt (longSide * aspect));
    const auto shapeH = aspect >= 1.0f ? juce::jmax (1, juce::roundToInt (longSide / aspect)) : longSide;

    juce::Image shapeImage (juce::Image::ARGB, shapeW, shapeH, true);

    {
        juce::Graphics g (shapeImage);
        drawable->drawWithin (g, juce::Rectangle<float> (0.0f, 0.0f, (float) shapeW, (float) shapeH),
                              juce::RectanglePlacement::centred, 1.0f);
    }

    // Pad with a transparent margin so every edge of the silhouette (not just the
    // corners) has background to measure against in the distance transform.
    const auto margin = 8;
    const auto fieldW = shapeW + margin * 2;
    const auto fieldH = shapeH + margin * 2;

    std::vector<unsigned char> inside ((size_t) fieldW * (size_t) fieldH, 0);

    {
        const juce::Image::BitmapData pixels (shapeImage, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < shapeH; ++y)
            for (int x = 0; x < shapeW; ++x)
                if (pixels.getPixelColour (x, y).getAlpha() > 127)
                    inside[(size_t) ((y + margin) * fieldW + (x + margin))] = 1;
    }

    auto field = buildProfileField (inside, fieldW, fieldH);

    // Reject empty / fully-transparent SVGs.
    if (std::none_of (field.begin(), field.end(), [] (float v) { return v >= 0.0f; }))
        return false;

    customShapeField = std::move (field);
    customFieldW = fieldW;
    customFieldH = fieldH;
    customShapeSvg = svgText;

    previewShape = PreviewShape::custom;

    // Aspect follows the padded field so the margin shows as an even border.
    const auto fieldAspect = (float) fieldW / (float) fieldH;

    if (fieldAspect >= 1.0f) { aspectRatioX = juce::jlimit (0.1f, 100.0f, fieldAspect); aspectRatioY = 1.0f; }
    else                     { aspectRatioX = 1.0f; aspectRatioY = juce::jlimit (0.1f, 100.0f, 1.0f / fieldAspect); }

    return true;
}

void MainComponent::showCustomShapeDialog()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load custom shape (SVG)",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.svg");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();

            if (file == juce::File{})
                return;

            statusText = loadCustomShapeFromSvg (file.loadFileAsString())
                ? "Loaded shape " + file.getFileName()
                : "Shape load failed";

            repaint();
        });
}

void MainComponent::showExportDialog()
{
    auto aspectRatioForExport = [this]()
    {
        if (previewShape != PreviewShape::rectangle)
            return 1.0f;

        return aspectRatioX / juce::jmax (0.0001f, aspectRatioY);
    };

    ExportOptions defaults;
    defaults.width = 1024;
    defaults.heightPx = juce::jmax (1, juce::roundToInt (1024.0f / aspectRatioForExport()));

    // Pre-select the map that is currently being previewed.
    defaults.wantHeight = previewMode == PreviewMode::heightMap;
    defaults.wantNormal = previewMode == PreviewMode::normalMap;
    defaults.wantBeauty = previewMode == PreviewMode::material;
    defaults.wantCavity = previewMode == PreviewMode::cavity;
    defaults.wantAo     = previewMode == PreviewMode::ambientOcclusion;

    auto content = std::make_unique<ExportOptionsComponent> (defaults);
    auto* rawContent = content.get();

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (content.release());
    options.dialogTitle = "Export maps";
    options.dialogBackgroundColour = juce::Colour::fromRGB (28, 28, 32);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    auto* window = options.launchAsync();

    auto closeWindow = [window]()
    {
        if (window != nullptr)
            juce::MessageManager::callAsync ([window] { delete window; });
    };

    rawContent->onCancel = closeWindow;

    rawContent->onExport = [this, closeWindow] (ExportOptions chosen)
    {
        closeWindow();

        fileChooser = std::make_unique<juce::FileChooser> (
            "Export ContourForge maps",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("ContourForge.png"),
            "*.png");

        fileChooser->launchAsync (
            juce::FileBrowserComponent::saveMode
                | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::warnAboutOverwriting,
            [this, chosen] (const juce::FileChooser& chooser)
            {
                const auto file = chooser.getResult();

                if (file == juce::File{})
                    return;

                const auto directory = file.getParentDirectory();

                // If the user picked an already-exported file to overwrite (e.g. "name_beauty.png"),
                // strip the trailing map suffix so we don't double it up ("name_beauty_beauty.png").
                auto baseName = file.getFileNameWithoutExtension();

                for (const auto* suffix : { "_height", "_normal", "_beauty", "_cavity", "_ao" })
                {
                    if (baseName.endsWithIgnoreCase (suffix))
                    {
                        baseName = baseName.dropLastCharacters ((int) juce::String (suffix).length());
                        break;
                    }
                }

                const juce::Rectangle<float> shapeArea (
                    0.0f, 0.0f, (float) chosen.width, (float) chosen.heightPx);

                // Supersample more for small exports, less for large ones to keep
                // the (synchronous) render time reasonable.
                const auto maxDimension = juce::jmax (chosen.width, chosen.heightPx);
                const auto supersample = maxDimension <= 1024 ? 3 : 2;

                int written = 0;

                auto saveImage = [&] (const juce::Image& image, const juce::String& suffix)
                {
                    auto outputFile = directory.getChildFile (baseName + "_" + suffix + ".png");
                    outputFile.deleteFile();

                    juce::FileOutputStream stream (outputFile);

                    if (stream.openedOk())
                    {
                        juce::PNGImageFormat png;

                        if (png.writeImageToStream (image, stream))
                            ++written;
                    }
                };

                // Renders one map over a given content region (geometry space). When a grid is
                // requested, the canvas is ceil'd up to the next multiple of the grid and the
                // content is re-centred, so the extra becomes even transparent margin.
                auto exportMap = [&] (PreviewMode mode, juce::Rectangle<float> contentRegion,
                                      int castShadowMode, const juce::String& suffix)
                {
                    auto canvasWidth  = juce::roundToInt (contentRegion.getWidth());
                    auto canvasHeight = juce::roundToInt (contentRegion.getHeight());

                    if (chosen.grid > 1)
                    {
                        auto snapUp = [grid = chosen.grid] (int value)
                        {
                            return ((value + grid - 1) / grid) * grid;
                        };

                        canvasWidth  = snapUp (canvasWidth);
                        canvasHeight = snapUp (canvasHeight);
                    }

                    // Expand the region symmetrically to the (possibly snapped) canvas size so
                    // the content stays centred.
                    const auto extraX = (float) canvasWidth  - contentRegion.getWidth();
                    const auto extraY = (float) canvasHeight - contentRegion.getHeight();

                    const juce::Rectangle<float> region (
                        contentRegion.getX() - extraX * 0.5f,
                        contentRegion.getY() - extraY * 0.5f,
                        contentRegion.getWidth()  + extraX,
                        contentRegion.getHeight() + extraY);

                    saveImage (renderMap (mode, shapeArea, canvasWidth, canvasHeight,
                                          true, supersample, region, castShadowMode), suffix);
                };

                auto writeOne = [&] (PreviewMode mode, const juce::String& suffix, bool wanted)
                {
                    if (wanted)
                        exportMap (mode, shapeArea, 0, suffix);
                };

                writeOne (PreviewMode::heightMap,        "height", chosen.wantHeight);
                writeOne (PreviewMode::normalMap,        "normal", chosen.wantNormal);
                writeOne (PreviewMode::cavity,           "cavity", chosen.wantCavity);
                writeOne (PreviewMode::ambientOcclusion, "ao",     chosen.wantAo);

                const auto dropShadowOn = chosen.wantBeauty && dropShadowAmount > 0.0001f;
                const auto contourAoOn  = chosen.wantBeauty && contourAoOpacity > 0.0001f;

                if (chosen.wantBeauty)
                {
                    // The shadow and the contour AO band both spill outside the shape, so pad
                    // the canvas uniformly by whichever reaches furthest. The shape stays dead
                    // centre and the canvas grows symmetrically around it.
                    auto pad = 0;

                    if (dropShadowOn)
                    {
                        const auto params = castShadowParams ((float) juce::jmin (chosen.width, chosen.heightPx));
                        const auto spread = params.lengthPx * std::sin (params.halfSpreadRad) + 3.0f;
                        const auto extentX = std::abs (params.dirX * params.lengthPx);
                        const auto extentY = std::abs (params.dirY * params.lengthPx);

                        pad = juce::jmax (pad, juce::roundToInt (juce::jmax (extentX, extentY) + spread));
                    }

                    if (contourAoOn)
                    {
                        const auto reach = (float) juce::jmin (chosen.width, chosen.heightPx) * contourAoBlur * 0.30f + 3.0f;
                        pad = juce::jmax (pad, juce::roundToInt (reach));
                    }

                    const juce::Rectangle<float> beautyRegion (
                        (float) -pad, (float) -pad,
                        (float) (chosen.width    + pad * 2),
                        (float) (chosen.heightPx + pad * 2));

                    exportMap (PreviewMode::material, beautyRegion, dropShadowOn ? 1 : 0, "beauty");

                    if (dropShadowOn)
                        exportMap (PreviewMode::material, beautyRegion, 2, "shadow");
                }

                statusText = written > 0
                    ? "Exported " + juce::String (written) + (written == 1 ? " map" : " maps")
                    : "Export failed";

                repaint();
            });
    };
}

juce::Image MainComponent::renderMap (PreviewMode mode,
                                      juce::Rectangle<float> shapeArea,
                                      int pixelWidth,
                                      int pixelHeight,
                                      bool transparentBackground,
                                      int supersample,
                                      juce::Rectangle<float> renderRegion,
                                      int castShadowMode) const
{
    const auto centre = shapeArea.getCentre();
    const auto outerRadius = juce::jmin (shapeArea.getWidth(), shapeArea.getHeight()) * 0.5f;

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

    auto colourAt = [&] (float profileX)
    {
        if (mode == PreviewMode::cavity)
        {
            const auto value = cavityAt (profileX);
            return juce::Colour::fromFloatRGBA (value, value, value, 1.0f);
        }

        const auto value = juce::jlimit (0.0f, 1.0f, 0.12f + heightValueAt (profileX) * 0.84f);
        return juce::Colour::fromFloatRGBA (value, value, value, 1.0f);
    };

    auto profileXAtPixel = [&] (float pixelX, float pixelY, float& profileX)
    {
        if (previewShape == PreviewShape::custom)
        {
            if (customShapeField.empty() || customFieldW <= 0 || customFieldH <= 0)
                return false;

            const auto u = (pixelX - shapeArea.getX()) / juce::jmax (0.0001f, shapeArea.getWidth());
            const auto v = (pixelY - shapeArea.getY()) / juce::jmax (0.0001f, shapeArea.getHeight());

            if (u < 0.0f || u >= 1.0f || v < 0.0f || v >= 1.0f)
                return false;

            auto sampleField = [&] (int sx, int sy)
            {
                sx = juce::jlimit (0, customFieldW - 1, sx);
                sy = juce::jlimit (0, customFieldH - 1, sy);
                return customShapeField[(size_t) (sy * customFieldW + sx)];
            };

            // Inside/outside from the nearest cell (the silhouette stays crisp; render
            // supersampling anti-aliases it).
            const auto nearestX = juce::jlimit (0, customFieldW - 1, (int) (u * (float) customFieldW));
            const auto nearestY = juce::jlimit (0, customFieldH - 1, (int) (v * (float) customFieldH));

            if (sampleField (nearestX, nearestY) < 0.0f)
                return false;

            // Bilinear sample of the distance for a smooth bevel (outside cells clamp to
            // the edge value 0 so the gradient stays continuous near the silhouette).
            const auto fxF = u * (float) customFieldW - 0.5f;
            const auto fyF = v * (float) customFieldH - 0.5f;

            const auto x0 = (int) std::floor (fxF);
            const auto y0 = (int) std::floor (fyF);
            const auto tx = fxF - (float) x0;
            const auto ty = fyF - (float) y0;

            auto positive = [] (float s) { return s < 0.0f ? 0.0f : s; };

            const auto s00 = positive (sampleField (x0,     y0));
            const auto s10 = positive (sampleField (x0 + 1, y0));
            const auto s01 = positive (sampleField (x0,     y0 + 1));
            const auto s11 = positive (sampleField (x0 + 1, y0 + 1));

            const auto top = s00 * (1.0f - tx) + s10 * tx;
            const auto bottom = s01 * (1.0f - tx) + s11 * tx;

            profileX = top * (1.0f - ty) + bottom * ty;
            return true;
        }

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

        const auto rawCavity = cavityAt (profileX);
        const auto rawAo = ambientOcclusionAtPixel (pixelX, pixelY, currentHeight);

        const auto cavityLayer = applyLayerOpacity (rawCavity, cavityOpacity);
        const auto aoLayer = applyLayerOpacity (rawAo, aoOpacity);

        const auto lightRadians = (lightAngleDeg - 90.0f) * juce::MathConstants<float>::pi / 180.0f;
        const auto lz = juce::jmap (lightElevation, 0.10f, 1.0f, 0.20f, 0.95f);
        const auto sideAmount = std::sqrt (juce::jmax (0.0f, 1.0f - lz * lz));

        const auto lx = std::cos (lightRadians) * sideAmount;
        const auto ly = std::sin (lightRadians) * sideAmount;

        const auto lightLength = juce::jmax (0.0001f, std::sqrt (lx * lx + ly * ly + lz * lz));
        const auto ndotl = juce::jlimit (0.0f, 1.0f, (nx * lx + ny * ly + nz * lz) / lightLength);

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

        // Artistic face fill: prevents grazing/steep light angles from collapsing faces too dark.
        const auto faceFill = 0.12f;
        const auto rawLayerShade = heightLayer * cavityLayer * aoLayer * shadowLayer;
        const auto layerShade = juce::jlimit (0.0f, 1.0f, faceFill + rawLayerShade * (1.0f - faceFill));

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

        // Additive bounce in creases when the area is lit.
        // AO stays dark-only; this is the separate light-gathering layer.
        const auto creaseAmount = juce::jlimit (0.0f, 1.0f,
            (1.0f - rawCavity) * 0.65f + (1.0f - rawAo) * 0.35f);

        const auto litCrease = std::pow (ndotl, 0.55f) * creaseAmount;
        const auto cavityLight = litCrease * cavityBounceAmount * 0.55f;

        return juce::Colour::fromFloatRGBA (
            juce::jlimit (0.0f, 1.0f, baseColour.getFloatRed()   * layerShade + cavityLight + specular),
            juce::jlimit (0.0f, 1.0f, baseColour.getFloatGreen() * layerShade + cavityLight + specular),
            juce::jlimit (0.0f, 1.0f, baseColour.getFloatBlue()  * layerShade + cavityLight + specular),
            1.0f);
    };

    // Geometry-derived cast shadow: march a small cone of rays from a ground pixel
    // toward the light and test against the heightfield. The cone gives a sharp
    // contact shadow that softens with distance (area-light penumbra).
    const auto shadowParams = castShadowParams (juce::jmin (shapeArea.getWidth(), shapeArea.getHeight()));
    const auto shadowShapeSize = juce::jmax (1.0f, juce::jmin (shapeArea.getWidth(), shapeArea.getHeight()));
    const auto shadowLightRadians = (lightAngleDeg - 90.0f) * juce::MathConstants<float>::pi / 180.0f;

    // The ray climbs so that the tallest geometry (height 1) casts exactly the chosen
    // length: slope = shapeSize / lengthPx. Length therefore controls reach directly,
    // with no dependence on the sun's elevation.
    const auto shadowSlope = shadowShapeSize / juce::jmax (1.0f, shadowParams.lengthPx);

    auto castShadowDensity = [&] (float px, float py)
    {
        if (dropShadowAmount <= 0.0001f)
            return 0.0f;

        constexpr int rays = 4;
        constexpr int steps = 20;

        // Contact falloff: 0 = uniform density along the whole shadow, higher values
        // fade it from a dark contact edge to a faint tip.
        const auto falloffExponent = dropShadowFalloff * 4.0f;

        float occlusion = 0.0f;

        for (int r = 0; r < rays; ++r)
        {
            const auto t = rays == 1 ? 0.5f : (float) r / (float) (rays - 1);
            const auto angle = shadowLightRadians + (t - 0.5f) * 2.0f * shadowParams.halfSpreadRad;

            const auto towardLightX = std::cos (angle);
            const auto towardLightY = std::sin (angle);

            float rayBlock = 0.0f;
            float blockDistance = 0.0f;

            for (int i = 1; i <= steps; ++i)
            {
                const auto distance = shadowParams.lengthPx * (float) i / (float) steps;

                float sampleProfileX = 0.0f;

                if (! profileXAtPixel (px + towardLightX * distance,
                                       py - towardLightY * distance,
                                       sampleProfileX))
                    continue;

                const auto sampleHeight = heightValueAt (sampleProfileX);
                const auto rayHeight = (distance / shadowShapeSize) * shadowSlope;
                const auto blocker = juce::jlimit (0.0f, 1.0f, (sampleHeight - rayHeight) * 9.0f);

                if (blocker > rayBlock)
                {
                    rayBlock = blocker;
                    blockDistance = distance;
                }
            }

            // Fade by how far along the shadow this point's occluder sits.
            const auto distanceFraction = juce::jlimit (0.0f, 1.0f,
                blockDistance / juce::jmax (1.0f, shadowParams.lengthPx));
            const auto falloffWeight = std::pow (1.0f - distanceFraction, falloffExponent);

            occlusion += rayBlock * falloffWeight;
        }

        return occlusion / (float) rays;
    };

    const auto imageBounds = (renderRegion.isEmpty() ? shapeArea : renderRegion).getSmallestIntegerContainer();

    const auto outWidth  = juce::jmax (1, pixelWidth);
    const auto outHeight = juce::jmax (1, pixelHeight);

    // Supersample: render at a higher internal resolution and downscale with a
    // high-quality filter. The per-pixel shader point-samples with binary alpha
    // at the silhouette, so without this the export edges/shading alias badly.
    const auto ss = juce::jlimit (1, 4, supersample);

    juce::Image image (
        juce::Image::ARGB,
        outWidth * ss,
        outHeight * ss,
        true);

    if (! transparentBackground)
    {
        juce::Graphics imageGraphics (image);
        imageGraphics.fillAll (juce::Colour::fromRGB (22, 22, 26));
    }

    // Contour AO: a soft dark band straddling the silhouette. Build a coverage mask
    // (1 inside the shape, 0 outside), blur it, then use 4*c*(1-c) so the darkening
    // peaks at the edge (c~0.5) and fades to nothing deep inside (1) and far out (0).
    const auto contourAoOn = mode == PreviewMode::material
        && castShadowMode != 2
        && contourAoOpacity > 0.0001f;

    std::vector<float> contourMask;

    if (contourAoOn)
    {
        const auto maskW = image.getWidth();
        const auto maskH = image.getHeight();

        contourMask.resize ((size_t) maskW * (size_t) maskH, 0.0f);

        for (int y = 0; y < maskH; ++y)
        {
            for (int x = 0; x < maskW; ++x)
            {
                const auto px = (float) imageBounds.getX()
                    + ((float) x + 0.5f) * (float) imageBounds.getWidth() / (float) maskW;
                const auto py = (float) imageBounds.getY()
                    + ((float) y + 0.5f) * (float) imageBounds.getHeight() / (float) maskH;

                float ignored = 0.0f;
                contourMask[(size_t) (y * maskW + x)] = profileXAtPixel (px, py, ignored) ? 1.0f : 0.0f;
            }
        }

        const auto geomToPixel = (float) maskW / juce::jmax (1.0f, (float) imageBounds.getWidth());
        const auto radius = juce::roundToInt (shadowShapeSize * (contourAoBlur * 0.30f) * geomToPixel);

        blurCoverageMask (contourMask, maskW, maskH, juce::jmax (1, radius), 3);
    }

    auto contourBand = [&] (int x, int y)
    {
        if (! contourAoOn)
            return 0.0f;

        const auto c = contourMask[(size_t) (y * image.getWidth() + x)];
        return juce::jlimit (0.0f, 1.0f, 4.0f * c * (1.0f - c) * contourAoOpacity);
    };

    {
        juce::Image::BitmapData previewPixels (image, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < image.getHeight(); ++y)
        {
            for (int x = 0; x < image.getWidth(); ++x)
            {
                const auto pixelX = (float) imageBounds.getX()
                    + ((float) x + 0.5f) * (float) imageBounds.getWidth() / (float) image.getWidth();

                const auto pixelY = (float) imageBounds.getY()
                    + ((float) y + 0.5f) * (float) imageBounds.getHeight() / (float) image.getHeight();

                float profileX = 0.0f;
                const auto inside = profileXAtPixel (pixelX, pixelY, profileX);

                auto* pixel = reinterpret_cast<juce::PixelARGB*> (previewPixels.getPixelPointer (x, y));

                // Cast-shadow-only pass: every pixel carries the ground shadow alpha.
                if (castShadowMode == 2)
                {
                    const auto density = castShadowDensity (pixelX, pixelY);

                    if (density <= 0.0001f)
                        continue;

                    const auto alpha = (juce::uint8) juce::roundToInt (
                        juce::jlimit (0.0f, 1.0f, density * dropShadowAmount) * 255.0f);

                    pixel->setARGB (alpha, 0, 0, 0);
                    continue;
                }

                if (! inside)
                {
                    // Ground pixel: combine the cast shadow with the outer half of the
                    // contour AO band (both are dark, transparent occlusion on the ground).
                    auto groundAlpha = 0.0f;

                    if (castShadowMode == 1)
                        groundAlpha = juce::jlimit (0.0f, 1.0f, castShadowDensity (pixelX, pixelY) * dropShadowAmount);

                    const auto band = contourBand (x, y);

                    if (band > 0.0001f)
                        groundAlpha = 1.0f - (1.0f - groundAlpha) * (1.0f - band);

                    if (groundAlpha > 0.0001f)
                        pixel->setARGB ((juce::uint8) juce::roundToInt (groundAlpha * 255.0f), 0, 0, 0);

                    continue;
                }

                auto colour = colourAt (profileX);

                if (mode == PreviewMode::normalMap)
                    colour = normalColourAtPixel (pixelX, pixelY, heightValueAt (profileX));
                else if (mode == PreviewMode::material)
                {
                    colour = materialColourAtPixel (pixelX, pixelY, profileX, heightValueAt (profileX));

                    // Inner half of the contour AO band: darken the surface near the edge.
                    const auto band = contourBand (x, y);

                    if (band > 0.0001f)
                        colour = juce::Colour::fromFloatRGBA (
                            colour.getFloatRed()   * (1.0f - band),
                            colour.getFloatGreen() * (1.0f - band),
                            colour.getFloatBlue()  * (1.0f - band),
                            colour.getFloatAlpha());
                }
                else if (mode == PreviewMode::ambientOcclusion)
                {
                    const auto value = ambientOcclusionAtPixel (pixelX, pixelY, heightValueAt (profileX));
                    colour = juce::Colour::fromFloatRGBA (value, value, value, 1.0f);
                }

                pixel->setARGB (colour.getAlpha(), colour.getRed(), colour.getGreen(), colour.getBlue());
            }
        }
    }

    if (ss > 1)
        return image.rescaled (outWidth, outHeight, juce::Graphics::highResamplingQuality);

    return image;
}

MainComponent::CastShadowParams MainComponent::castShadowParams (float shapeSize) const
{
    const auto lightRadians = (lightAngleDeg - 90.0f) * juce::MathConstants<float>::pi / 180.0f;

    CastShadowParams params;

    // Length and softness are driven by their own sliders (independent of the sun).
    params.lengthPx = shapeSize * juce::jmap (dropShadowLength, 0.0f, 1.0f, 0.05f, 1.20f);
    params.halfSpreadRad = juce::jmap (dropShadowSoftness, 0.0f, 1.0f, 0.0f, 0.40f);

    // Direction still follows the light angle. Marching toward the light samples
    // (x + cos, y - sin), so the away-from-light screen direction is (-cos, +sin).
    params.dirX = -std::cos (lightRadians);
    params.dirY =  std::sin (lightRadians);

    return params;
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
        return formatRatioValue (aspectRatioX) + ":" + formatRatioValue (aspectRatioY);
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
        previewControls.removeFromRight (12.0f); // gutter for the scrollbar
        previewControls.translate (0.0f, -toolsScrollOffset);
        previewControls.setHeight (10000.0f); // unbounded so rows aren't clamped to the panel

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
        const auto customButton = shapeRow.removeFromLeft (70.0f);
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
        const auto cavityBounceSlider = nextToolRow();
        const auto shadowLayerSlider = nextToolRow();
        const auto dropShadowSlider = nextToolRow();
        const auto dropShadowLengthSlider = nextToolRow();
        const auto dropShadowSoftnessSlider = nextToolRow();
        const auto dropShadowFalloffSlider = nextToolRow();
        const auto contourAoBlurSlider = nextToolRow();
        const auto contourAoOpacitySlider = nextToolRow();
        const auto specularLayerSlider = nextToolRow();
        const auto specularCatchSlider = nextToolRow();
        const auto chamferSlider = nextToolRow();
        const auto glossSlider = nextToolRow();

        auto ratioRow = nextToolRow();
        const auto ratioButton = ratioRow.removeFromLeft (96.0f);
        ratioRow.removeFromLeft (6.0f);
        const auto cornersButton = ratioRow.removeFromLeft (94.0f);

        auto shapeGridArea = previewControls.removeFromTop (62.0f).removeFromLeft (62.0f);

    // Total height of all controls (used to clamp the scroll offset). previewControls.getY()
    // is the bottom of the consumed rows; un-shift it by the current scroll to get the height.
    toolsContentHeight = previewControls.getY() - getToolsViewport().getY() + toolsScrollOffset;

    // Clip everything below to the panel so scrolled controls don't bleed into the header
    // or outside the panel. The block scopes the clip to the controls only.
    {
    juce::Graphics::ScopedSaveState toolsClip (g);
    g.reduceClipRegion (getToolsViewport().getSmallestIntegerContainer());

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

    const auto squareAspect = juce::approximatelyEqual (aspectRatioX, aspectRatioY);

    const auto isCirclePreset = previewShape == PreviewShape::rectangle
        && squareAspect
        && roundedCornerMask == 15
        && cornerRadiusAmount >= 0.875f;

    drawShapeButton (circleButton, "Circle", previewShape == PreviewShape::circle || isCirclePreset);
    drawShapeButton (squareButton, "Square", previewShape == PreviewShape::rectangle && squareAspect && roundedCornerMask == 0);
    drawShapeButton (rectButton, "Rect", previewShape == PreviewShape::rectangle && ! squareAspect && roundedCornerMask == 0);
    drawShapeButton (customButton, customShapeField.empty() ? "Custom..." : "Custom", previewShape == PreviewShape::custom);
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
    drawPreviewSlider (cavityBounceSlider, "Cavity light " + juce::String (juce::roundToInt (cavityBounceAmount * 100.0f)) + "%", cavityBounceAmount);
    drawPreviewSlider (shadowLayerSlider, "Shadow " + juce::String (juce::roundToInt (shadowLayerAmount * 100.0f)) + "%", shadowLayerAmount);
    drawPreviewSlider (dropShadowSlider, "Drop shadow " + juce::String (juce::roundToInt (dropShadowAmount * 100.0f)) + "%", dropShadowAmount);
    drawPreviewSlider (dropShadowLengthSlider, "Shadow length " + juce::String (juce::roundToInt (dropShadowLength * 100.0f)) + "%", dropShadowLength);
    drawPreviewSlider (dropShadowSoftnessSlider, "Shadow softness " + juce::String (juce::roundToInt (dropShadowSoftness * 100.0f)) + "%", dropShadowSoftness);
    drawPreviewSlider (dropShadowFalloffSlider, "Shadow falloff " + juce::String (juce::roundToInt (dropShadowFalloff * 100.0f)) + "%", dropShadowFalloff);
    drawPreviewSlider (contourAoBlurSlider, "Contour AO blur " + juce::String (juce::roundToInt (contourAoBlur * 100.0f)) + "%", contourAoBlur);
    drawPreviewSlider (contourAoOpacitySlider, "Contour AO " + juce::String (juce::roundToInt (contourAoOpacity * 100.0f)) + "%", contourAoOpacity);
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
    } // end tools clip scope

    // Draggable scrollbar (stylus-friendly): drawn unclipped over the panel gutter.
    if (const auto thumb = getToolsScrollThumb(); ! thumb.isEmpty())
    {
        const auto track = getToolsScrollTrack();

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (track.reduced (3.0f, 0.0f), 3.0f);

        g.setColour (juce::Colours::white.withAlpha (draggingToolsScrollbar ? 0.55f : 0.30f));
        g.fillRoundedRectangle (thumb, 3.0f);
    }

    auto shapeArea = area.reduced (46.0f, 58.0f);
    shapeArea.removeFromTop (34.0f);

    auto getAspectRatio = [&]()
    {
        return aspectRatioX / juce::jmax (0.0001f, aspectRatioY);
    };

    if (previewShape == PreviewShape::rectangle || previewShape == PreviewShape::custom)
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

    // The cast shadow needs the shape's alpha as its silhouette, so render with a
    // transparent background when it is active. The preview panel behind is the same
    // colour as the usual fill, so non-shadow modes look identical either way.
    const auto useDropShadow = previewMode == PreviewMode::material && dropShadowAmount > 0.0001f;
    const auto useContourAo  = previewMode == PreviewMode::material && contourAoOpacity > 0.0001f;
    const auto needsGround   = useDropShadow || useContourAo;

    // When the shadow or contour AO is active, render a region padded around the shape so
    // those effects have room to spill outside the silhouette (shadow is directional, the
    // contour band is symmetric).
    auto renderRegion = shapeArea;
    auto castShadowMode = useDropShadow ? 1 : 0;

    if (needsGround)
    {
        auto padLeft = 0.0f, padRight = 0.0f, padTop = 0.0f, padBottom = 0.0f;

        if (useDropShadow)
        {
            const auto params = castShadowParams (juce::jmin (shapeArea.getWidth(), shapeArea.getHeight()));
            const auto spread = params.lengthPx * std::sin (params.halfSpreadRad) + 3.0f;

            const auto extentX = params.dirX * params.lengthPx;
            const auto extentY = params.dirY * params.lengthPx;

            padLeft   = juce::jmax (0.0f, -extentX) + spread;
            padRight  = juce::jmax (0.0f,  extentX) + spread;
            padTop    = juce::jmax (0.0f, -extentY) + spread;
            padBottom = juce::jmax (0.0f,  extentY) + spread;
        }

        if (useContourAo)
        {
            const auto reach = juce::jmin (shapeArea.getWidth(), shapeArea.getHeight()) * contourAoBlur * 0.30f + 3.0f;
            padLeft   = juce::jmax (padLeft,   reach);
            padRight  = juce::jmax (padRight,  reach);
            padTop    = juce::jmax (padTop,    reach);
            padBottom = juce::jmax (padBottom, reach);
        }

        renderRegion = { shapeArea.getX() - padLeft,
                         shapeArea.getY() - padTop,
                         shapeArea.getWidth()  + padLeft + padRight,
                         shapeArea.getHeight() + padTop + padBottom };
    }

    const auto imageBounds = renderRegion.getSmallestIntegerContainer();

    const auto isFastPreview = draggedPointIndex >= 0 || draggedPreviewSlider != PreviewSlider::none;
    const auto renderScale = isFastPreview ? 1.0f / (float) previewQualityDivisor : 1.0f;

    const auto renderWidth = juce::jmax (1, juce::roundToInt ((float) imageBounds.getWidth() * renderScale));
    const auto renderHeight = juce::jmax (1, juce::roundToInt ((float) imageBounds.getHeight() * renderScale));

    const auto previewImage = renderMap (previewMode, shapeArea, renderWidth, renderHeight,
                                         needsGround, 1, renderRegion, castShadowMode);

    g.setOpacity (1.0f);
    g.setColour (juce::Colours::white);

    {
        // Keep the (possibly oversized) shadow within the preview panel.
        juce::Graphics::ScopedSaveState savedState (g);
        g.reduceClipRegion (area.getSmallestIntegerContainer());

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
    }

    g.setColour (juce::Colours::white.withAlpha (0.12f));

    if (previewShape == PreviewShape::custom)
    {
        // No analytic outline for a custom silhouette.
    }
    else if (previewShape == PreviewShape::circle)
        g.drawEllipse (shapeArea, 1.0f);
    else if (roundedCornerMask == 15)
        g.drawRoundedRectangle (shapeArea, juce::jmin (shapeArea.getWidth(), shapeArea.getHeight()) * 0.5f * cornerRadiusAmount, 1.0f);
    else
        g.drawRect (shapeArea, 1.0f);


}

