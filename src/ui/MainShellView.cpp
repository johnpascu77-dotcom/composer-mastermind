#include "MainShellView.h"

MainShellView::MainShellView(ComposerMastermindAudioProcessor& processor)
    : primaryView(processor),
      composeView(processor, [this](const juce::String& text) { setShellStatus(text); }),
      expertView(processor),
      proposeView(processor, [this](const juce::String& text) { setShellStatus(text); })
{
    addAndMakeVisible(primaryView);
    addChildComponent(composeView); // starts hidden - PrimaryView is the default view
    addChildComponent(expertView);
    addChildComponent(proposeView);

    addAndMakeVisible(composeToggleButton);
    composeToggleButton.onClick = [this] { toggleComposeClicked(); };

    addAndMakeVisible(expertToggleButton);
    expertToggleButton.onClick = [this] { toggleExpertClicked(); };

    addAndMakeVisible(proposeToggleButton);
    proposeToggleButton.onClick = [this] { toggleProposeClicked(); };

    addAndMakeVisible(shellStatusLabel);
    shellStatusLabel.setFont(juce::Font(12.0f));
    shellStatusLabel.setJustificationType(juce::Justification::centredLeft);

    updateVisibilityAndButtonText();
}

void MainShellView::resized()
{
    auto area = getLocalBounds();
    auto toggleRow = area.removeFromTop(kToggleButtonHeight + kMargin * 2);

    expertToggleButton.setBounds(
        toggleRow.removeFromRight(kToggleButtonWidth + kMargin).reduced(kMargin).withHeight(kToggleButtonHeight));
    proposeToggleButton.setBounds(
        toggleRow.removeFromRight(kToggleButtonWidth + kMargin).reduced(kMargin).withHeight(kToggleButtonHeight));
    composeToggleButton.setBounds(
        toggleRow.removeFromRight(kToggleButtonWidth + kMargin).reduced(kMargin).withHeight(kToggleButtonHeight));
    shellStatusLabel.setBounds(toggleRow.reduced(kMargin, 0));

    primaryView.setBounds(area);
    composeView.setBounds(area);
    expertView.setBounds(area);
    proposeView.setBounds(area);
}

void MainShellView::refreshAll()
{
    expertView.refreshAll();
    composeView.refreshAll();
    proposeView.refreshAll();
}

void MainShellView::toggleComposeClicked()
{
    mode = (mode == ShellMode::Compose) ? ShellMode::Primary : ShellMode::Compose;
    updateVisibilityAndButtonText();
}

void MainShellView::toggleExpertClicked()
{
    mode = (mode == ShellMode::Expert) ? ShellMode::Primary : ShellMode::Expert;
    updateVisibilityAndButtonText();
}

void MainShellView::toggleProposeClicked()
{
    mode = (mode == ShellMode::Propose) ? ShellMode::Primary : ShellMode::Propose;
    if (mode == ShellMode::Propose)
        proposeView.refreshAll(); // pick up any proposals saved from elsewhere since last shown
    updateVisibilityAndButtonText();
}

void MainShellView::updateVisibilityAndButtonText()
{
    primaryView.setVisible(mode == ShellMode::Primary);
    composeView.setVisible(mode == ShellMode::Compose);
    expertView.setVisible(mode == ShellMode::Expert);
    proposeView.setVisible(mode == ShellMode::Propose);

    composeToggleButton.setButtonText(mode == ShellMode::Compose ? "Score View" : "Compose");
    expertToggleButton.setButtonText(mode == ShellMode::Expert ? "Score View" : "Expert");
    proposeToggleButton.setButtonText(mode == ShellMode::Propose ? "Score View" : "Propose");
}

void MainShellView::setShellStatus(const juce::String& text)
{
    shellStatusLabel.setText(text, juce::dontSendNotification);
}
