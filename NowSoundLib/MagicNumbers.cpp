// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include "Clock.h"
#include "MagicNumbers.h"

using namespace NowSound;

// 100Hz input frame rate
// Also tried 0.1 (e.g. 10Hz frame rate) but still observed (with surface book realtek):
// good looping for a few repetitions, then crackling suddenly beginning, like some kind of internal timing/buffering problem.
// Why would it sound right and then spontaneously start sounding wrong???  Doesn't seem like it could be an algorithm bug
// in my code?  (because then it would NEVER sound right?)
const ContinuousDuration<Second> MagicNumbers::AudioFrameLengthSeconds{ (float)0.1 };

// exactly one beat per second for initial testing
const float MagicNumbers::InitialBeatsPerMinute{ 60 };

// 4/4 time
const int MagicNumbers::BeatsPerMeasure{ 4 };

// Stereo
const int MagicNumbers::AudioChannelCount{ 2 };

// 128 secs preallocated buffers
const int MagicNumbers::InitialAudioBufferCount{ 128 };

// 1/10 sec seems fine for NowSound with TASCAM US2x2 :-P  -- this should probably be user-tunable or even autotunable...
const Duration<AudioSample> MagicNumbers::TrackLatencyCompensation{ 0 /*Clock::SampleRateHz / 8*/ };

// This could easily be huge but 1000 is fine for getting at least a second's worth of per-track history at audio rate.
const int MagicNumbers::DebugLogCapacity{ 1000 };

// 200 histogram values at 100Hz = two seconds of history, enough to follow transient crackling/breakup
// (due to losing foreground execution status, for example)
const int MagicNumbers::AudioQuantumHistogramCapacity{ 200 };
