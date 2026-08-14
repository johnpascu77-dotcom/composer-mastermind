#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../composer/ComposerCore.h"

class ComposerMastermindAudioProcessorEditor;

class ComposerMastermindAudioProcessor : public juce::AudioProcessor
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    ComposerMastermindAudioProcessor();
    ~ComposerMastermindAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    APVTS& getAPVTS() { return apvts; }

    // Exposed for the editor UI (add instances, push scenes/mutations).
    // These calls come from the message thread; ComposerCore's own
    // internals are mutex-guarded against the concurrent audio thread.
    ComposerCore& getComposerCore() { return composerCore; }

    static APVTS::ParameterLayout createParameterLayout();

private:
    APVTS apvts;
    ComposerCore composerCore;

    double lastBarStartPpq = -1.0;
    int currentBarIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ComposerMastermindAudioProcessor)
};
