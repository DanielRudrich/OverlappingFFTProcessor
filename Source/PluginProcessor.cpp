/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
OverlappingFFTProcessorDemoAudioProcessor::OverlappingFFTProcessorDemoAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

OverlappingFFTProcessorDemoAudioProcessor::~OverlappingFFTProcessorDemoAudioProcessor()
{
}

//==============================================================================
const String OverlappingFFTProcessorDemoAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool OverlappingFFTProcessorDemoAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool OverlappingFFTProcessorDemoAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool OverlappingFFTProcessorDemoAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double OverlappingFFTProcessorDemoAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int OverlappingFFTProcessorDemoAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int OverlappingFFTProcessorDemoAudioProcessor::getCurrentProgram()
{
    return 0;
}

void OverlappingFFTProcessorDemoAudioProcessor::setCurrentProgram (int index)
{
}

const String OverlappingFFTProcessorDemoAudioProcessor::getProgramName (int index)
{
    return {};
}

void OverlappingFFTProcessorDemoAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void OverlappingFFTProcessorDemoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    myProcessor.prepare (sampleRate, samplesPerBlock, 2, 2);
}

void OverlappingFFTProcessorDemoAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool OverlappingFFTProcessorDemoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void OverlappingFFTProcessorDemoAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;

    dsp::AudioBlock<float> ab (buffer);
    dsp::ProcessContextReplacing<float> context (ab);
    myProcessor.process (context);
}

//==============================================================================
bool OverlappingFFTProcessorDemoAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* OverlappingFFTProcessorDemoAudioProcessor::createEditor()
{
    return new OverlappingFFTProcessorDemoAudioProcessorEditor (*this);
}

//==============================================================================
void OverlappingFFTProcessorDemoAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void OverlappingFFTProcessorDemoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OverlappingFFTProcessorDemoAudioProcessor();
}
