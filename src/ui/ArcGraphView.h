#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <string>
#include <functional>
#include "../policy/Arc.h"
#include "../policy/ArcSet.h"

// The interactive arc-breakpoint graph editor - drag a point to move it,
// double-click empty space to add one, right-click a point to remove it.
// This IS the authoring surface for an ArcSet's 5 arcs (see
// docs/composer_mastermind_design.md's v1.0 section), not a separate
// feature layered on top of a form.
//
// Operates on an injected ArcSet& rather than always reaching into
// ComposerCore, so the same component serves two roles: the live editor
// inside ui/BlueprintView's "Arcs" inner tab (given ComposerCore's own
// ArcSet), and a preview/editor for a not-yet-committed candidate ArcSet
// (given a scratch instance owned by ui/GenerateView) - see
// docs/composer_mastermind_design.md's v1.1 section for why generated
// proposals need to be inspectable/editable before anything touches live
// state. Both roles reuse every interaction unchanged; only which ArcSet
// gets read/written differs.
class ArcGraphView : public juce::Component
{
public:
    // captionOverride replaces the default "drag/double-click/right-click"
    // help text when non-empty - used by the preview role to say
    // "not yet committed" instead of the live-editing caption.
    explicit ArcGraphView(ArcSet& arcSetToEdit, const juce::String& captionOverride = {});

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    // Re-pulls the currently-selected arc's breakpoints from arcSet -
    // needed after the underlying ArcSet was replaced wholesale from
    // outside (e.g. ui/GenerateView reassigning its candidate ArcSet after
    // a new Generate/Discard), which this component has no other way to
    // notice since it caches the selected arc's points for editing.
    void refreshFromArcSet();

    // Widens or narrows the graph's own bar axis - defaults to 1..64 (every
    // existing caller: the live "Arcs" tab, GenerateView's draft preview),
    // but a real piece isn't always 64 bars (docs/composer_mastermind_design.md's
    // whole-piece blueprints can run much longer or shorter). Clamped to a
    // sane minimum span so the axis never collapses to a single point.
    // ui/BlueprintArcCurveView calls this per selected blueprint, sized to
    // that blueprint's own actual section span.
    void setMaxBar(int newMaxBar);

    const std::string& getSelectedArcName() const { return selectedArcName; }

    // Fired after the dimension-selector combo changes (no data touched).
    std::function<void()> onSelectionChanged;

    // Fired after a drag/add/delete actually commits new breakpoints for
    // the selected arc. ui/BlueprintArcCurveView uses these two separately
    // (rather than one combined callback) to tell "you switched which
    // dimension you're looking at" apart from "you just turned a derived
    // curve into a saved one" - see that component for why the distinction
    // matters.
    std::function<void()> onEdited;

private:
    void arcSelectionChanged();
    void commitWorkingBreakpoints();
    void loadWorkingBreakpointsFromArcSet();

    // Screen <-> (bar, value) domain conversion within the graph area.
    juce::Point<float> toScreen(const ArcBreakpoint& point) const;
    ArcBreakpoint toDomain(juce::Point<float> screenPos) const;
    int findNearbyPointIndex(juce::Point<float> screenPos, const std::vector<ArcBreakpoint>& points) const;

    juce::Colour colourForArc(const std::string& arcName) const;

    ArcSet& arcSet;

    juce::ComboBox arcSelectorCombo;
    juce::Label arcSelectorLabel { {}, "Editing Arc" };
    juce::Label captionLabel;

    juce::Rectangle<float> graphArea;
    static constexpr int kMinBar = 1;
    static constexpr int kDefaultMaxBar = 64;
    int maxBar = kDefaultMaxBar;

    std::string selectedArcName;
    std::vector<ArcBreakpoint> workingBreakpoints;
    int draggedPointIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArcGraphView)
};
