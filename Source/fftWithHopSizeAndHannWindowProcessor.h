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


/*
 This processor takes care about buffering input and output samples for your FFT processing.
 Inherit from this class and override the processFrameInBuffer() function.

 Example Code:


 class MyProcessor : public FftWithHopSizeAndHannWindowProcessor<11>
 {
 public:
    MyProcessor() : FftWithHopSizeAndHannWindowProcessor ()
    {}

 private:
    void processFrameInBuffer() override
    {
        for (int ch = 0; ch < 2; ++ch)
            fft.performRealOnlyForwardTransform(fftInOutBuffer.getWritePointer(ch), true);

        // clear high frequency content
        for (int ch = 0; ch < 2; ++ch)
            FloatVectorOperations::clear(fftInOutBuffer.getWritePointer(ch, fftSize / 2), fftSize / 2);

        for (int ch = 0; ch < 2; ++ch)
            fft.performRealOnlyInverseTransform(fftInOutBuffer.getWritePointer(ch));
    }
 };


 */

#pragma once
#include "../JuceLibraryCode/JuceHeader.h"

using namespace dsp;
template <int fftPowerOf2>
class FftWithHopSizeAndHannWindowProcessor : public ProcessorBase
{
public:
    FftWithHopSizeAndHannWindowProcessor()
    : fft(fftPowerOf2)
    {
        // create Hann window
        hannWindow.resize(fftSize);
        WindowingFunction<float>::fillWindowingTables (hannWindow.getRawDataPointer(), fftSize, WindowingFunction<float>::WindowingMethod::hann, false);
        
        reset();
    }
    
    ~FftWithHopSizeAndHannWindowProcessor() {}

    void reset() override
    {
        notYetUsedAudioDataCount = 0;
    }

    void prepare (const ProcessSpec &spec) override
    {
        const int nCh = spec.numChannels;
        const int bufferSize = spec.maximumBlockSize;

        notYetUsedAudioData.setSize(nCh, fftSize - 1);
        fftInOutBuffer.setSize(nCh, 2 * fftSize);

        const int k = floor(1.0f + ((float) (bufferSize - 1)) / hopSize);
        const int M = k * hopSize + hopSize;

        outputBuffer.setSize(nCh, M + bufferSize - 1); // not sure if (bufferSize - 1) could be too much, but it's working like a charm... so.. whatever...
        outputBuffer.clear();

        int offset = 0;
        while (offset < fftSize)
            offset += bufferSize;

        outputOffset = offset - bufferSize;

        outputOffset = fftSize - 1;

        reset();
    }

    void process (const ProcessContextReplacing<float> &context) override
    {
        ProcessContextNonReplacing<float> replacingContext (context.getInputBlock(), context.getOutputBlock());
        process (replacingContext);
    }

    void process (ProcessContextNonReplacing<float> context)
    {
        AudioBlock<float> inputBlock = context.getInputBlock();
        AudioBlock<float> outputBlock = context.getOutputBlock();
        const int L = (int) inputBlock.getNumSamples();
        const int nChannels = (int) inputBlock.getNumChannels();
        const int initialNotYetUsedAudioDataCount = notYetUsedAudioDataCount;
        int notYetUsedAudioDataOffset = 0;
        int usedSamples = 0;

        while (notYetUsedAudioDataCount > 0 && notYetUsedAudioDataCount + L >= fftSize)
        {
            // copy not yet used data into fftInOut buffer (with hann windowing)
            for (int ch = 0; ch < nChannels; ++ch)
            {
                FloatVectorOperations::multiply(fftInOutBuffer.getWritePointer(ch), // destination
                                                notYetUsedAudioData.getReadPointer(ch, notYetUsedAudioDataOffset), // source 1 (audio data)
                                                hannWindow.getRawDataPointer(), // source 2 (window)
                                                notYetUsedAudioDataCount // number of samples
                                                );

                // fill up fftInOut buffer with new data (with hann windowing)
                FloatVectorOperations::multiply(fftInOutBuffer.getWritePointer(ch, notYetUsedAudioDataCount), // destination
                                                inputBlock.getChannelPointer(ch), // source 1 (audio data)
                                                hannWindow.getRawDataPointer() + notYetUsedAudioDataCount, // source 2 (window)
                                                fftSize - notYetUsedAudioDataCount // number of samples
                                                );
            }

            // process frame and buffer output
            processAndBufferOutput();

            notYetUsedAudioDataOffset += hopSize;
            notYetUsedAudioDataCount -= hopSize;
        }
        
        if (notYetUsedAudioDataCount > 0) // not enough new input samples to use all of the previous data
        {
            for (int ch = 0; ch < nChannels; ++ch)
            {
                FloatVectorOperations::copy(notYetUsedAudioData.getWritePointer(ch),
                                            notYetUsedAudioData.getReadPointer(ch, initialNotYetUsedAudioDataCount - notYetUsedAudioDataCount),
                                            notYetUsedAudioDataCount);
                FloatVectorOperations::copy(notYetUsedAudioData.getWritePointer(ch, notYetUsedAudioDataCount),
                                            inputBlock.getChannelPointer(ch) + usedSamples,
                                            L);
            }
            notYetUsedAudioDataCount += L;
        }
        else  // all of the previous data used
        {
            int dataOffset = - notYetUsedAudioDataCount;
            
            while (L - dataOffset >= fftSize)
            {
                for (int ch = 0; ch < nChannels; ++ch)
                {
                    FloatVectorOperations::multiply(fftInOutBuffer.getWritePointer(ch),
                                                    inputBlock.getChannelPointer(ch) + dataOffset,
                                                    hannWindow.getRawDataPointer(), fftSize);
                }
                processAndBufferOutput();

                dataOffset += hopSize;
            }
            
            int remainingSamples = L - dataOffset;
            if (remainingSamples > 0)
            {
                for (int ch = 0; ch < nChannels; ++ch)
                {
                    FloatVectorOperations::copy(notYetUsedAudioData.getWritePointer(ch),
                                                inputBlock.getChannelPointer(ch) + dataOffset,
                                                L - dataOffset);
                }
            }
            notYetUsedAudioDataCount = remainingSamples;
        }


        // return processed samples from outputBuffer
        const int nChOut = (int) outputBlock.getNumChannels();
        const int shiftStart = L;
        int shiftL = outputOffset + hopSize - L;

        const int tooMuch = shiftStart + shiftL - outputBuffer.getNumSamples();
        if (tooMuch > 0)
        {
            shiftL -= tooMuch;
        }

        for (int ch = 0; ch < nChOut; ++ch)
        {
            FloatVectorOperations::copy(outputBlock.getChannelPointer(ch), outputBuffer.getReadPointer(ch), L);
            FloatVectorOperations::copy(outputBuffer.getWritePointer(ch), outputBuffer.getReadPointer(ch, shiftStart), shiftL);
        }
        outputOffset -= L;
    }
    
private:
    void processAndBufferOutput()
    {
        processFrameInBuffer();
        writeBackFrame();
    }

    // you should override this
    virtual void processFrameInBuffer()
    {
    }

    void writeBackFrame()
    {
        const int nCh = outputBuffer.getNumChannels();
        for (int ch = 0; ch < nCh; ++ch)
        {
            outputBuffer.addFrom(ch, outputOffset, fftInOutBuffer, ch, 0, hopSize);
            outputBuffer.copyFrom(ch, outputOffset + hopSize, fftInOutBuffer, ch, hopSize, hopSize);
        }
        outputOffset += hopSize;
    }

public:
    dsp::FFT fft;
    AudioBuffer<float> fftInOutBuffer;
    const int fftSize = 1 << fftPowerOf2;
    const int hopSize = fftSize / 2;

private:
    Array<float> hannWindow;

    AudioBuffer<float> notYetUsedAudioData;
    AudioBuffer<float> outputBuffer;
    int outputOffset;

    int notYetUsedAudioDataCount;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FftWithHopSizeAndHannWindowProcessor)
};
