// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include "Clock.h"
#include "MagicConstants.h"
#include "MeasurementAudioProcessor.h"

using namespace NowSound;

MeasurementAudioProcessor::MeasurementAudioProcessor(NowSoundGraph* graph)
    : BaseAudioProcessor(),
    _graph{ graph },
    // hardcoded to the clock's channel count, e.g. the overall output bus width.
    _volumeHistogram{ (int)Clock::Instance().TimeToSamples(MagicConstants::RecentVolumeDuration).Value() },
    _frequencyTracker{ graph->FftSize() < 0
    ? ((NowSoundFrequencyTracker*)nullptr)
    : new NowSoundFrequencyTracker(graph->BinBounds(), graph->FftSize()) }
{}

const Histogram& MeasurementAudioProcessor::VolumeHistogram() const { return _volumeHistogram; }

void MeasurementAudioProcessor::GetFrequencies(void* floatBuffer, int floatBufferCapacity)
{
    if (_frequencyTracker == nullptr)
    {
        return;
    }

    _frequencyTracker->GetLatestHistogram((float*)floatBuffer, floatBufferCapacity);
}

const double Pi = std::atan(1) * 4;

void MeasurementAudioProcessor::processBlock(AudioBuffer<float>& audioBuffer, MidiBuffer& midiBuffer)
{
    Check(audioBuffer.getNumChannels() == 2);

    int numSamples = audioBuffer.getNumSamples();

    const float* outputBufferChannel0 = audioBuffer.getReadPointer(0);
    // TODO: track right channel at all
    // const float* outputBufferChannel1 = audioBuffer.getReadPointer(1);

    // Pan each mono sample (and track its volume), if we're not muted.
    for (int i = 0; i < numSamples; i++)
    {
        float value = outputBufferChannel0[i];
        _volumeHistogram.Add(std::abs(value));
    }

    // and provide it to frequency histogram as well
    if (_frequencyTracker != nullptr)
    {
        // TODO: average both channels? or handle tracking stereo frequencies?
        // for now only frequency track channel 0
        _frequencyTracker->Record(outputBufferChannel0, numSamples);
    }
}