#include "EditorView.h"
#include "../plugin/PluginProcessor.h"

namespace
{
    constexpr int kStatusBarHeight = 28;
    constexpr int kTimerIntervalMs = 300;
}

EditorView::EditorView(ComposerMastermindAudioProcessor& processor)
    : processorRef(processor),
      instanceView(processor, [this](const juce::String& text) { setStatusText(text); }),
      sceneListView(processor, [this](const juce::String& text) { setStatusText(text); }),
      debugPanel(processor, [this](const juce::String& text) { setStatusText(text); }),
      blueprintView(processor, [this](const juce::String& text) { setStatusText(text); }),
      presetLibraryView(processor, [this](const juce::String& text) { setStatusText(text); }),
      generateView(processor, [this](const juce::String& text) { setStatusText(text); }),
      modulatorTargetView(processor, [this](const juce::String& text) { setStatusText(text); }),
      patternAwarenessView(processor, [this](const juce::String& text) { setStatusText(text); }),
      activityLogView(processor),
      milestoneView(processor, [this](const juce::String& text) { setStatusText(text); })
{
    addAndMakeVisible(tabs);

    const auto tabColour = juce::Colours::transparentBlack;
    tabs.addTab("Instances", tabColour, &instanceView, false);
    tabs.addTab("Scenes", tabColour, &sceneListView, false);
    tabs.addTab("Mutations", tabColour, &debugPanel, false);
    tabs.addTab("Blueprint", tabColour, &blueprintView, false);
    tabs.addTab("Presets", tabColour, &presetLibraryView, false);
    tabs.addTab("Generate", tabColour, &generateView, false);
    tabs.addTab("Modulators", tabColour, &modulatorTargetView, false);
    tabs.addTab("Awareness", tabColour, &patternAwarenessView, false);
    tabs.addTab("Activity Log", tabColour, &activityLogView, false);
    tabs.addTab("Milestones", tabColour, &milestoneView, false);

    addAndMakeVisible(statusLabel);
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setFont(juce::Font(13.0f, juce::Font::italic));

    startTimer(kTimerIntervalMs);
}

EditorView::~EditorView()
{
    stopTimer();
}

void EditorView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void EditorView::resized()
{
    auto area = getLocalBounds();
    statusLabel.setBounds(area.removeFromBottom(kStatusBarHeight).reduced(10, 4));
    tabs.setBounds(area);
}

void EditorView::refreshAll()
{
    instanceView.refreshInstanceList();
    sceneListView.refreshAll();
    debugPanel.refreshTargets();
    blueprintView.refreshAll();
    presetLibraryView.refreshAll();
    generateView.refreshAll();
    modulatorTargetView.refreshAll();
    patternAwarenessView.refreshAll();
    activityLogView.refreshAll();
    milestoneView.refreshAll();
}

void EditorView::timerCallback()
{
    refreshAll();
}

void EditorView::setStatusText(const juce::String& text)
{
    statusLabel.setText(text, juce::dontSendNotification);
}
