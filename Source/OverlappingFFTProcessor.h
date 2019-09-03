/*
 ==============================================================================
 Author: Daniel Rudrich
 https://github.com/DanielRudrich

 This code is free: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code.  If not, see <https://www.gnu.org/licenses/>.
 ==============================================================================
 */

/* Changelog
 2019-09-03
    - compatibility to JUCE 5.4.4
 */

#pragma once
#include "../JuceLibraryCode/JuceHeader.h"

/**
 This processor takes care of buffering input and output samples for your FFT processing.
 With fttSizeAsPowerOf2 and hopSizeDividerAsPowerOf2 the fftSize and hopSize can be specifiec.
 Inherit from this class and override the processFrameInBuffer() function in order to
 implement your processing. You can also override the `createWindow()` method to use
 another window (default: Hann window).

 @code
 class MyProcessor : public OverlappingFFTProcessor
 {
 public:
     MyProcessor () : OverlappingFFTProcessor (11, 2) {}
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

 */
class OverlappingFFTProcessor
{
public:
    /** Constructor
     @param fftSizeAsPowerOf2 defines the fftSize as a power of 2: fftSize = 2^fftSizeAsPowerOf2
     @param hopSizeDividerAsPowerOf2 defines the hopSize as a fraction of fftSize: hopSize = fftSize / (2^hopSizeDivider)
     */
    OverlappingFFTProcessor (const int fftSizeAsPowerOf2, const int hopSizeDividerAsPowerOf2 = 1)
    : fft (fftSizeAsPowerOf2), fftSize (1 << fftSizeAsPowerOf2), hopSize (fftSize >> hopSizeDividerAsPowerOf2)
    {
        // make sure you have at least an overlap of 50%
        jassert (hopSizeDividerAsPowerOf2 > 0);

        // make sure you don't want to hop smaller than 1 sample
        jassert (hopSizeDividerAsPowerOf2 <= fftSizeAsPowerOf2);

        DBG ("Overlapping FFT Processor created with fftSize: " << fftSize << " and hopSize: " << hopSize);

        // create window
        window.resize (fftSize);
        createWindow();

        // don`t change the size of the window during createWindow()!
        jassert (window.size() == fftSize);
    }
    
    virtual ~OverlappingFFTProcessor() {}


    void reset() {}

    void prepare (const double sampleRate, const int maximumBlockSize, const int numInputChannels, const int numOutputChannels)
    {
        nChIn = numInputChannels;
        nChOut = numOutputChannels;
        const auto maxCh = jmax (nChIn, nChOut);

        const int bufferSize = maximumBlockSize;

        notYetUsedAudioData.setSize (nChIn, fftSize - 1);
        fftInOutBuffer.setSize (maxCh, 2 * fftSize);

        const int k = floor (1.0f + ((float) (bufferSize - 1)) / hopSize);
        const int M = k * hopSize + (fftSize - hopSize);

        outputBuffer.setSize (nChIn, M + bufferSize - 1); // not sure if (bufferSize - 1) could be too much, but it's working like a charm... so.. whatever...
        outputBuffer.clear();

        int offset = 0;
        while (offset < fftSize)
            offset += bufferSize;

        outputOffset = offset - bufferSize;

        outputOffset = fftSize - 1;

        notYetUsedAudioDataCount = 0;
    }


    void process (const dsp::ProcessContextReplacing<float>& context)
    {
        process (context.getInputBlock(), context.getOutputBlock());
    }

    void process (const dsp::ProcessContextNonReplacing<float>& context)
    {
        process (context.getInputBlock(), context.getOutputBlock());
    }

    void process (const dsp::AudioBlock<const float>& inputBlock, dsp::AudioBlock<float>& outputBlock)
    {
        const auto L = (int) inputBlock.getNumSamples();
        const auto numChIn = jmin (static_cast<int> (inputBlock.getNumChannels()), nChIn);
        const auto numChOut = jmin (static_cast<int> (outputBlock.getNumChannels()), nChOut);
        const auto maxNumChannels = jmax (numChIn, numChOut);

        const int initialNotYetUsedAudioDataCount = notYetUsedAudioDataCount;
        int notYetUsedAudioDataOffset = 0;
        int usedSamples = 0;


        // we've got some left overs, so let's use them together with the new samples
        while (notYetUsedAudioDataCount > 0 && notYetUsedAudioDataCount + L >= fftSize)
        {
            // copy not yet used data into fftInOut buffer (with hann windowing)
            for (int ch = 0; ch < numChIn; ++ch)
            {
                FloatVectorOperations::multiply (fftInOutBuffer.getWritePointer (ch), // destination
                                                 notYetUsedAudioData.getReadPointer (ch, notYetUsedAudioDataOffset), // source 1 (audio data)
                                                 window.data(), // source 2 (window)
                                                 notYetUsedAudioDataCount // number of samples
                                                );

                // fill up fftInOut buffer with new data (with hann windowing)
                FloatVectorOperations::multiply (fftInOutBuffer.getWritePointer (ch, notYetUsedAudioDataCount), // destination
                                                 inputBlock.getChannelPointer (ch), // source 1 (audio data)
                                                 window.data() + notYetUsedAudioDataCount, // source 2 (window)
                                                 fftSize - notYetUsedAudioDataCount // number of samples
                                                );
            }

            // process frame and buffer output
            processFrameInBuffer (maxNumChannels);
            writeBackFrame();

            notYetUsedAudioDataOffset += hopSize;
            notYetUsedAudioDataCount -= hopSize;
        }
        
        if (notYetUsedAudioDataCount > 0) // not enough new input samples to use all of the previous data
        {
            for (int ch = 0; ch < numChIn; ++ch)
            {
                FloatVectorOperations::copy (notYetUsedAudioData.getWritePointer (ch),
                                             notYetUsedAudioData.getReadPointer (ch, initialNotYetUsedAudioDataCount - notYetUsedAudioDataCount),
                                             notYetUsedAudioDataCount);
                FloatVectorOperations::copy (notYetUsedAudioData.getWritePointer (ch, notYetUsedAudioDataCount),
                                             inputBlock.getChannelPointer (ch) + usedSamples,
                                             L);
            }
            notYetUsedAudioDataCount += L;
        }
        else  // all of the previous data used
        {
            int dataOffset = - notYetUsedAudioDataCount;
            
            while (L - dataOffset >= fftSize)
            {
                for (int ch = 0; ch < numChIn; ++ch)
                {
                    FloatVectorOperations::multiply (fftInOutBuffer.getWritePointer (ch),
                                                     inputBlock.getChannelPointer (ch) + dataOffset,
                                                     window.data(), fftSize);
                }
                processFrameInBuffer (maxNumChannels);
                writeBackFrame();

                dataOffset += hopSize;
            }
            
            int remainingSamples = L - dataOffset;
            if (remainingSamples > 0)
            {
                for (int ch = 0; ch < numChIn; ++ch)
                {
                    FloatVectorOperations::copy (notYetUsedAudioData.getWritePointer (ch),
                                                 inputBlock.getChannelPointer (ch) + dataOffset,
                                                 L - dataOffset);
                }
            }
            notYetUsedAudioDataCount = remainingSamples;
        }


        // return processed samples from outputBuffer
        const int shiftStart = L;
        int shiftL = outputOffset + fftSize - hopSize - L;

        const int tooMuch = shiftStart + shiftL - outputBuffer.getNumSamples();
        if (tooMuch > 0)
            shiftL -= tooMuch;

        for (int ch = 0; ch < numChOut; ++ch)
        {
            FloatVectorOperations::copy (outputBlock.getChannelPointer (ch), outputBuffer.getReadPointer (ch), L);
            FloatVectorOperations::copy (outputBuffer.getWritePointer (ch), outputBuffer.getReadPointer (ch, shiftStart), shiftL);
        }

        for (int ch = numChOut; ch < outputBlock.getNumChannels(); ++ch)
            FloatVectorOperations::clear (outputBlock.getChannelPointer (ch), L);

        outputOffset -= L;
    }

    const int getNumInputChannels() const { return nChIn; }
    const int getNumOutputChannels() const { return nChOut; }
    
private:
    virtual void createWindow()
    {
        dsp::WindowingFunction<float>::fillWindowingTables (window.data(), fftSize, dsp::WindowingFunction<float>::WindowingMethod::hann, false);

        const float hopSizeCompensateFactor = 1.0f / (fftSize / hopSize / 2);
        for (auto& elem : window)
            elem *= hopSizeCompensateFactor;
    }

    /**
     This method get's called each time the processor has gathered enough samples for a transformation.
     The data in the `fftInOutBuffer` is still in time domain. Use the `fft` member to transform it into
     frequency domain, do your calculations, and transform it back to time domain.
     @param maxNumChannels the max number of channels of `fftInOutBuffer` you should use
     */
    virtual void processFrameInBuffer (const int maxNumChannels) {}

    void writeBackFrame()
    {
        for (int ch = 0; ch < nChOut; ++ch)
        {
            outputBuffer.addFrom (ch, outputOffset, fftInOutBuffer, ch, 0, fftSize - hopSize);
            outputBuffer.copyFrom (ch, outputOffset + fftSize - hopSize, fftInOutBuffer, ch, fftSize - hopSize, hopSize);
        }
        outputOffset += hopSize;
    }

protected:
    dsp::FFT fft;
    std::vector<float> window;
    AudioBuffer<float> fftInOutBuffer;
    const int fftSize;
    const int hopSize;

private:
    int nChIn;
    int nChOut;

    AudioBuffer<float> notYetUsedAudioData;
    AudioBuffer<float> outputBuffer;
    int outputOffset;

    int notYetUsedAudioDataCount = 0;

    JUCE_DECLARE_NON_COPYABLE (OverlappingFFTProcessor)
};
