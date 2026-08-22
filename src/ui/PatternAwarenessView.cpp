#include "PatternAwarenessView.h"
#include "../plugin/PluginProcessor.h"
#include "../model/Instance.h"

namespace
{
    constexpr int kRowHeight = 24;
    constexpr int kRowGap = 4;
    constexpr int kMargin = 10;
    constexpr int kPatternsPerInstance = 3; // matches MPL's fixed numPatterns
}

PatternAwarenessView::PatternAwarenessView(ComposerMastermindAudioProcessor& processor,
                                            std::function<void(const juce::String&)> status)
    : processorRef(processor), setStatus(std::move(status))
{
    addAndMakeVisible(instanceCombo);
    instanceCombo.setTextWhenNothingSelected("(pick instance)");
    addAndMakeVisible(instanceLabel);

    addAndMakeVisible(patternCombo);
    patternCombo.addItem("Pattern 1", 1);
    patternCombo.addItem("Pattern 2", 2);
    patternCombo.addItem("Pattern 3", 3);
    patternCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(patternLabel);

    addAndMakeVisible(resyncButton);
    resyncButton.onClick = [this] { resyncClicked(); };

    addAndMakeVisible(resyncAllButton);
    resyncAllButton.onClick = [this] { resyncAllClicked(); };

    addAndMakeVisible(connectionStatusLabel);
    connectionStatusLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Italic")));

    addAndMakeVisible(cacheDisplay);
    cacheDisplay.setMultiLine(true);
    cacheDisplay.setReadOnly(true);
    cacheDisplay.setScrollbarsShown(true);
    cacheDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    refreshAll();
}

void PatternAwarenessView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PatternAwarenessView::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto nextRow = [&](int height) -> juce::Rectangle<int>
    {
        auto row = area.removeFromTop(height);
        area.removeFromTop(kRowGap);
        return row;
    };

    auto controlsRow = nextRow(kRowHeight);
    instanceLabel.setBounds(controlsRow.removeFromLeft(60));
    instanceCombo.setBounds(controlsRow.removeFromLeft(160));
    controlsRow.removeFromLeft(kMargin);
    patternLabel.setBounds(controlsRow.removeFromLeft(55));
    patternCombo.setBounds(controlsRow.removeFromLeft(110));
    controlsRow.removeFromLeft(kMargin);
    resyncButton.setBounds(controlsRow.removeFromLeft(110));
    controlsRow.removeFromLeft(kMargin);
    resyncAllButton.setBounds(controlsRow.removeFromLeft(110));

    connectionStatusLabel.setBounds(nextRow(18));

    cacheDisplay.setBounds(area);
}

void PatternAwarenessView::resyncClicked()
{
    if (instanceCombo.getSelectedId() <= 0)
    {
        setStatus("Resync skipped: no instance selected");
        return;
    }

    const auto instanceId = instanceCombo.getText().toStdString();

    Instance instance;
    if (!processorRef.getComposerCore().getInstanceRegistry().getInstanceById(instanceId, instance))
    {
        setStatus("Resync failed: unknown instance '" + juce::String(instanceId) + "'");
        return;
    }

    const int patternIndex = patternCombo.getSelectedId() - 1;
    auto& server = processorRef.getComposerCore().getPatternSyncServer();
    const bool wasConnected = server.isChannelConnected(instance.midiChannel);

    server.requestSync(instance.midiChannel, patternIndex);

    if (!wasConnected)
    {
        setStatus("No live connection yet for channel " + juce::String(instance.midiChannel)
                       + " - check MPL's Composer Bridge is enabled and give the IPC connection a moment to establish");
        return;
    }

    setStatus("Requested pattern dump: '" + juce::String(instanceId) + "' pattern "
                   + juce::String(patternIndex + 1) + " (channel " + juce::String(instance.midiChannel) + ")");
}

void PatternAwarenessView::resyncAllClicked()
{
    const auto instances = processorRef.getComposerCore().getInstanceRegistry().getAllInstances();

    if (instances.empty())
    {
        setStatus("Resync All skipped: no instances registered");
        return;
    }

    auto& server = processorRef.getComposerCore().getPatternSyncServer();

    int requested = 0;
    int skippedNotConnected = 0;

    for (const auto& instance : instances)
    {
        const bool connected = server.isChannelConnected(instance.midiChannel);

        for (int patternIndex = 0; patternIndex < kPatternsPerInstance; ++patternIndex)
        {
            if (!connected)
            {
                ++skippedNotConnected;
                continue;
            }

            server.requestSync(instance.midiChannel, patternIndex);
            ++requested;
        }
    }

    juce::String message = "Resync All: requested " + juce::String(requested) + " pattern(s) across "
                            + juce::String((int) instances.size()) + " instance(s)";
    if (skippedNotConnected > 0)
        message << " (" << skippedNotConnected << " skipped - not connected yet)";

    setStatus(message);
}

void PatternAwarenessView::refreshAll()
{
    const auto instances = processorRef.getComposerCore().getInstanceRegistry().getAllInstances();

    const auto previousSelection = instanceCombo.getText();
    instanceCombo.clear(juce::dontSendNotification);
    int itemId = 1;
    int selectId = 0;
    for (const auto& instance : instances)
    {
        instanceCombo.addItem(instance.id, itemId);
        if (instance.id == previousSelection.toStdString())
            selectId = itemId;
        ++itemId;
    }
    if (selectId == 0 && !instances.empty())
        selectId = 1;
    instanceCombo.setSelectedId(selectId, juce::dontSendNotification);

    auto& server = processorRef.getComposerCore().getPatternSyncServer();
    const int currentBar = processorRef.getComposerCore().getCurrentBar();

    int connectedCount = 0;
    for (const auto& instance : instances)
        if (server.isChannelConnected(instance.midiChannel))
            ++connectedCount;

    connectionStatusLabel.setText(juce::String(connectedCount) + " of " + juce::String((int) instances.size())
                              + " registered instance(s) connected via IPC",
        juce::dontSendNotification);

    const auto cached = server.getCache().getAll();

    juce::String text;
    for (const auto& entry : cached)
    {
        const int bar = entry.capturedAtBar;
        const int age = currentBar - bar;

        text << entry.instanceId << "  pattern " << (entry.snapshot.patternIndex + 1)
             << "  captured at bar " << bar << " (" << age << " bar(s) ago)\n";

        int enabledCount = 0;
        for (size_t stepIndex = 0; stepIndex < entry.snapshot.steps.size(); ++stepIndex)
        {
            const auto& step = entry.snapshot.steps[stepIndex];
            if (!step.enabled)
                continue;

            ++enabledCount;
            text << "    step " << (int) stepIndex << ": note=" << step.note
                 << " vel=" << step.velocity << " dur=" << step.duration << "\n";
        }

        text << "    (" << enabledCount << "/" << (int) entry.snapshot.steps.size() << " steps enabled)\n\n";
    }

    if (cached.empty())
        text = "(no confirmed pattern content yet - pick an instance and pattern, then Resync Now)";

    cacheDisplay.setText(text, juce::dontSendNotification);
}
