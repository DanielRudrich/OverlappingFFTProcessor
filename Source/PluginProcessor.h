/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "OverlappingFFTProcessor.h"


class MyProcessor : public OverlappingFFTProcessor
{
public:
    MyProcessor () : OverlappingFFTProcessor (11, 2)
    {}
    ~MyProcessor() {}

private:
    void processFrameInBuffer (const int maxNumChannels) override
    {
        for (int ch = 0; ch < maxNumChannels; ++ch)
            fft.performRealOnlyForwardTransform (fftInOutBuffer.getWritePointer (ch), true);

        // clear high frequency content
        for (int ch = 0; ch < maxNumChannels; ++ch)
            FloatVectorOperations::clear (fftInOutBuffer.getWritePointer (ch, fftSize / 2), fftSize / 2);

        for (int ch = 0; ch < maxNumChannels; ++ch)
            fft.performRealOnlyInverseTransform (fftInOutBuffer.getWritePointer (ch));
    }
};

//==============================================================================
/**
*/
class OverlappingFFTProcessorDemoAudioProcessor  : public AudioProcessor
{
public:
    //==============================================================================
    OverlappingFFTProcessorDemoAudioProcessor();
    ~OverlappingFFTProcessorDemoAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    MyProcessor myProcessor;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OverlappingFFTProcessorDemoAudioProcessor)
};
