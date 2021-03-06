// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"
#include "Clock.h"

using namespace NowSound;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace winrt::Windows::Foundation;

std::unique_ptr<NowSound::Clock> NowSound::Clock::s_instance;

void NowSound::Clock::Initialize(int sampleRateHz, int channelCount, float beatsPerMinute, int beatsPerMeasure)
{
    std::unique_ptr<Clock> clock(new Clock(sampleRateHz, channelCount, beatsPerMinute, beatsPerMeasure));
    Clock::s_instance = std::move(clock);
}

void NowSound::Clock::Shutdown()
{
    // this will drop the unique clock reference and deallocate the clock
    Clock::s_instance = nullptr;
}

NowSound::Clock::Clock(int sampleRateHz, int channelCount, float beatsPerMinute, int beatsPerMeasure)
    : _sampleRateHz(sampleRateHz),
    _channelCount(channelCount),
    _beatsPerMinute(beatsPerMinute),
    _beatsPerMeasure(beatsPerMeasure),
    _now(0),
    _beatDuration(0)
{
    Check(!IsInitialized()); // No Clock yet
    CalculateBeatDuration();
}

void NowSound::Clock::CalculateBeatDuration()
{
    _beatDuration = (ContinuousDuration<AudioSample>)(((float)SampleRateHz() * 60) / _beatsPerMinute);
}

void NowSound::Clock::BeatsPerMinute(float value)
{
    _beatsPerMinute = value;
    CalculateBeatDuration();
}

const long NowSound::Clock::TicksPerSecond = 10 * 1000 * 1000;

void NowSound::Clock::AdvanceFromAudioGraph(Duration<AudioSample> duration)
{
    // TODO: += operator
    _now = _now + duration;
}
