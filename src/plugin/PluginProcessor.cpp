#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../state/StateSnapshotStore.h"
#include <cmath>

ComposerMastermindAudioProcessor::ComposerMastermindAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
#ifndef JucePlugin_PreferredChannelConfigurations
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      ),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

ComposerMastermindAudioProcessor::~ComposerMastermindAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ComposerMastermindAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    return { params.begin(), params.end() };
}

void ComposerMastermindAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);

    lastBarStartPpq = -1.0;
    currentBarIndex = 0;
}

void ComposerMastermindAudioProcessor::releaseResources()
{
}

bool ComposerMastermindAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    juce::ignoreUnused(layouts);
    return true;
}

void ComposerMastermindAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    // Advance the composer engine once per host bar boundary. Using the
    // host-reported last-bar-start ppq (rather than doing our own ppq/time
    // signature math) tracks tempo and time-signature changes for free.
    if (auto* currentPlayHead = getPlayHead())
    {
        if (const auto position = currentPlayHead->getPosition())
        {
            if (position->getIsPlaying())
            {
                if (const auto barStart = position->getPpqPositionOfLastBarStart())
                {
                    if (lastBarStartPpq < 0.0 || std::abs(*barStart - lastBarStartPpq) > 1.0e-6)
                    {
                        lastBarStartPpq = *barStart;
                        composerCore.processBar(currentBarIndex);
                        ++currentBarIndex;
                    }
                }
            }
            else
            {
                // Only fire on the transition into stopped, not every block
                // while stopped: re-anchors the active scene's elapsed-bars
                // clock so a restart doesn't miscount against stale bar
                // numbers from before the stop.
                if (lastBarStartPpq >= 0.0)
                    composerCore.notifyTransportReset();

                lastBarStartPpq = -1.0;
                currentBarIndex = 0;
            }
        }
    }

    midiMessages.clear();

    for (const auto& cc : composerCore.getCCDispatcher().takePending())
    {
        midiMessages.addEvent(juce::MidiMessage::controllerEvent(
                                   juce::jlimit(1, 16, cc.channel),
                                   juce::jlimit(0, 127, cc.controller),
                                   juce::jlimit(0, 127, cc.value)),
                               0);
    }
}

juce::AudioProcessorEditor* ComposerMastermindAudioProcessor::createEditor()
{
    return new ComposerMastermindAudioProcessorEditor(*this);
}

bool ComposerMastermindAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String ComposerMastermindAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ComposerMastermindAudioProcessor::acceptsMidi() const
{
    return true;
}

bool ComposerMastermindAudioProcessor::producesMidi() const
{
    return true;
}

bool ComposerMastermindAudioProcessor::isMidiEffect() const
{
    return false;
}

double ComposerMastermindAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ComposerMastermindAudioProcessor::getNumPrograms()
{
    return 1;
}

int ComposerMastermindAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ComposerMastermindAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String ComposerMastermindAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void ComposerMastermindAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void ComposerMastermindAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    if (xml != nullptr)
    {
        xml->setAttribute("composerMastermindSnapshot", StateSnapshotStore::createSnapshot(composerCore));
        copyXmlToBinary(*xml, destData);
    }
}

void ComposerMastermindAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState == nullptr)
        return;

    if (xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

    if (xmlState->hasAttribute("composerMastermindSnapshot"))
    {
        std::string errorMessage;
        StateSnapshotStore::restoreSnapshot(xmlState->getStringAttribute("composerMastermindSnapshot"),
                                             composerCore, errorMessage);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ComposerMastermindAudioProcessor();
}
