#include "ArcGraphView.h"
#include <algorithm>

namespace
{
    struct ArcInfo
    {
        const char* name;
        const char* displayName;
        juce::uint32 colour;
    };

    const ArcInfo kArcInfos[] = {
        { "energy",     "Energy",     0xffff6f61 },
        { "tension",    "Tension",    0xff9b59b6 },
        { "density",    "Density",    0xff3498db },
        { "complexity", "Complexity", 0xff2ecc71 },
        { "coherence",  "Coherence",  0xfff1c40f },
    };

    constexpr int kArcCount = sizeof(kArcInfos) / sizeof(kArcInfos[0]);

    // Smallest "nice" step (in bars) that keeps gridlines readable
    // regardless of how long the piece is - a fixed "every 8 bars" step
    // looked fine at the old fixed 64-bar span, but turns into a single
    // gridline for an 8-bar piece or an unreadable comb for a 500-bar one.
    // Picks the smallest step from a fixed set that still keeps the total
    // gridline count at or under kTargetGridlineCount.
    int gridStepForRange(int span)
    {
        constexpr int kTargetGridlineCount = 12;
        static const int niceSteps[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256 };

        for (int step : niceSteps)
            if (span / step <= kTargetGridlineCount)
                return step;

        return niceSteps[(sizeof(niceSteps) / sizeof(niceSteps[0])) - 1];
    }
}

ArcGraphView::ArcGraphView(ArcSet& arcSetToEdit, const juce::String& captionOverride)
    : arcSet(arcSetToEdit)
{
    addAndMakeVisible(arcSelectorLabel);

    addAndMakeVisible(arcSelectorCombo);
    for (int i = 0; i < kArcCount; ++i)
        arcSelectorCombo.addItem(kArcInfos[i].displayName, i + 1);
    arcSelectorCombo.setSelectedId(1, juce::dontSendNotification);
    arcSelectorCombo.onChange = [this] { arcSelectionChanged(); };

    addAndMakeVisible(captionLabel);
    captionLabel.setText(
        captionOverride.isNotEmpty()
            ? captionOverride
            : juce::String("Drag a point to move it, double-click empty space to add one, right-click a point to "
                            "remove it."),
        juce::dontSendNotification);
    captionLabel.setFont(juce::Font(13.0f, juce::Font::italic));
    captionLabel.setJustificationType(juce::Justification::topLeft);
    captionLabel.setMinimumHorizontalScale(1.0f);

    selectedArcName = kArcInfos[0].name;
    loadWorkingBreakpointsFromArcSet();
}

void ArcGraphView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    if (graphArea.isEmpty())
        return;

    g.setColour(juce::Colours::grey.withAlpha(0.5f));
    g.drawRect(graphArea, 1.0f);

    g.setColour(juce::Colours::grey.withAlpha(0.25f));
    for (int i = 0; i <= 4; ++i)
    {
        const float y = graphArea.getBottom() - (i / 4.0f) * graphArea.getHeight();
        g.drawHorizontalLine((int) y, graphArea.getX(), graphArea.getRight());
    }

    const int gridStep = gridStepForRange(maxBar - kMinBar);
    for (int bar = kMinBar; bar <= maxBar; bar += gridStep)
    {
        const float x = graphArea.getX() + (bar - kMinBar) / float(maxBar - kMinBar) * graphArea.getWidth();
        g.drawVerticalLine((int) x, graphArea.getY(), graphArea.getBottom());
        g.setFont(11.0f);
        g.drawText(juce::String(bar), (int) x - 12, (int) graphArea.getBottom() + 2, 24, 14,
                    juce::Justification::centred);
    }

    for (const auto& info : kArcInfos)
    {
        const bool isSelected = (selectedArcName == info.name);
        const auto points = isSelected ? workingBreakpoints : arcSet.getArc(info.name).getBreakpoints();

        if (points.empty())
            continue;

        auto sortedForDrawing = points;
        std::sort(sortedForDrawing.begin(), sortedForDrawing.end(),
                   [](const ArcBreakpoint& a, const ArcBreakpoint& b) { return a.bar < b.bar; });

        juce::Path path;
        for (size_t i = 0; i < sortedForDrawing.size(); ++i)
        {
            const auto screenPos = toScreen(sortedForDrawing[i]);
            if (i == 0)
                path.startNewSubPath(screenPos);
            else
                path.lineTo(screenPos);
        }

        g.setColour(colourForArc(info.name).withAlpha(isSelected ? 1.0f : 0.35f));
        g.strokePath(path, juce::PathStrokeType(isSelected ? 2.5f : 1.2f));

        if (isSelected)
        {
            for (size_t i = 0; i < workingBreakpoints.size(); ++i)
            {
                const auto screenPos = toScreen(workingBreakpoints[i]);
                const bool isDragged = ((int) i == draggedPointIndex);

                g.setColour(isDragged ? juce::Colours::white : colourForArc(selectedArcName));
                g.fillEllipse(juce::Rectangle<float>(9.0f, 9.0f).withCentre(screenPos));
                g.setColour(juce::Colours::black.withAlpha(0.6f));
                g.drawEllipse(juce::Rectangle<float>(9.0f, 9.0f).withCentre(screenPos), 1.0f);
            }
        }
    }

    int legendY = (int) graphArea.getY() + 6;
    const int legendX = (int) graphArea.getRight() - 112;
    for (const auto& info : kArcInfos)
    {
        const bool isSelected = (selectedArcName == info.name);
        g.setColour(colourForArc(info.name).withAlpha(isSelected ? 1.0f : 0.5f));
        g.fillRect(legendX, legendY, 10, 10);
        g.setColour(juce::Colours::white.withAlpha(isSelected ? 1.0f : 0.6f));
        g.setFont(juce::Font(12.0f, isSelected ? juce::Font::bold : juce::Font::plain));
        g.drawText(info.displayName, legendX + 14, legendY - 2, 96, 14, juce::Justification::centredLeft);
        legendY += 16;
    }
}

void ArcGraphView::resized()
{
    auto area = getLocalBounds().reduced(10);

    auto topRow = area.removeFromTop(24);
    arcSelectorLabel.setBounds(topRow.removeFromLeft(100));
    arcSelectorCombo.setBounds(topRow.removeFromLeft(180));

    area.removeFromTop(4);
    captionLabel.setBounds(area.removeFromTop(32));

    area.removeFromTop(6);
    area.removeFromBottom(20); // room for the bar-number labels drawn in paint()
    graphArea = area.toFloat();
}

void ArcGraphView::mouseDown(const juce::MouseEvent& event)
{
    if (!graphArea.contains(event.position))
        return;

    const int hitIndex = findNearbyPointIndex(event.position, workingBreakpoints);

    if (event.mods.isRightButtonDown())
    {
        if (hitIndex >= 0 && workingBreakpoints.size() > 1)
        {
            workingBreakpoints.erase(workingBreakpoints.begin() + hitIndex);
            commitWorkingBreakpoints();
        }
        return;
    }

    draggedPointIndex = hitIndex;
    repaint();
}

void ArcGraphView::mouseDrag(const juce::MouseEvent& event)
{
    if (draggedPointIndex < 0 || draggedPointIndex >= (int) workingBreakpoints.size())
        return;

    workingBreakpoints[(size_t) draggedPointIndex] = toDomain(event.position);
    repaint();
}

void ArcGraphView::mouseUp(const juce::MouseEvent&)
{
    if (draggedPointIndex >= 0)
    {
        commitWorkingBreakpoints();
        draggedPointIndex = -1;
    }
}

void ArcGraphView::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (!graphArea.contains(event.position))
        return;

    if (findNearbyPointIndex(event.position, workingBreakpoints) >= 0)
        return; // too close to an existing point - avoid a near-duplicate

    workingBreakpoints.push_back(toDomain(event.position));
    commitWorkingBreakpoints();
}

void ArcGraphView::refreshFromArcSet()
{
    loadWorkingBreakpointsFromArcSet();
}

void ArcGraphView::setMaxBar(int newMaxBar)
{
    maxBar = juce::jmax(kMinBar + 1, newMaxBar);
    repaint();
}

void ArcGraphView::arcSelectionChanged()
{
    const int index = arcSelectorCombo.getSelectedItemIndex();
    if (index < 0 || index >= kArcCount)
        return;

    selectedArcName = kArcInfos[index].name;
    loadWorkingBreakpointsFromArcSet();

    if (onSelectionChanged)
        onSelectionChanged();
}

void ArcGraphView::commitWorkingBreakpoints()
{
    Arc updated;
    updated.setBreakpoints(workingBreakpoints);
    arcSet.setArc(selectedArcName, updated);
    workingBreakpoints = updated.getBreakpoints();
    repaint();

    if (onEdited)
        onEdited();
}

void ArcGraphView::loadWorkingBreakpointsFromArcSet()
{
    workingBreakpoints = arcSet.getArc(selectedArcName).getBreakpoints();
    draggedPointIndex = -1;
    repaint();
}

juce::Point<float> ArcGraphView::toScreen(const ArcBreakpoint& point) const
{
    const float t = (point.bar - kMinBar) / float(maxBar - kMinBar);
    const float x = graphArea.getX() + t * graphArea.getWidth();
    const float y = graphArea.getBottom() - point.value * graphArea.getHeight();
    return { x, y };
}

ArcBreakpoint ArcGraphView::toDomain(juce::Point<float> screenPos) const
{
    const float t = (screenPos.x - graphArea.getX()) / graphArea.getWidth();
    const int bar = juce::jlimit(kMinBar, maxBar, kMinBar + juce::roundToInt(t * (maxBar - kMinBar)));
    const float value = juce::jlimit(0.0f, 1.0f, (graphArea.getBottom() - screenPos.y) / graphArea.getHeight());
    return { bar, value };
}

int ArcGraphView::findNearbyPointIndex(juce::Point<float> screenPos, const std::vector<ArcBreakpoint>& points) const
{
    constexpr float kHitRadius = 10.0f;
    int bestIndex = -1;
    float bestDistance = kHitRadius;

    for (int i = 0; i < (int) points.size(); ++i)
    {
        const float distance = toScreen(points[(size_t) i]).getDistanceFrom(screenPos);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    return bestIndex;
}

juce::Colour ArcGraphView::colourForArc(const std::string& arcName) const
{
    for (const auto& info : kArcInfos)
    {
        if (arcName == info.name)
            return juce::Colour(info.colour);
    }

    return juce::Colours::white;
}
