﻿// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

// P/Invoke wrapper types and classes for NowSoundLib.
namespace NowSoundLib
{
    // TODO: actually use this over P/Invoke via StringBuilder marshaling (avoiding all lifetime issues nicely)
    public struct DeviceInfo
    {
        public readonly string Id;
        public readonly string Name;
        // Would be nice if this was IReadOnlyDictionary but C# 4 don't do that
        public readonly Dictionary<string, object> Properties;

        /// <summary>
        /// Construct a DeviceInfo; it will directly reference the given dictionary (no copying).
        /// </summary>
        public DeviceInfo(string id, string name, Dictionary<string, object> properties)
        {
            Id = id;
            Name = name;
            Properties = properties;
        }
    }

    // Information about an audio graph.
    // This marshalable struct corresponds to the C++ P/Invokable type.
    // Since this has no fields that have particular units (e.g. no durations or times),
    // we just make the marshalable struct public in this case.
    public struct NowSoundGraphInfo
    {
        public Int32 SampleRate;
        public Int32 ChannelCount;
        public Int32 BitsPerSample;
        public Int32 LatencyInSamples;
        public Int32 SamplesPerQuantum;
    }

    // Information about timing in a created or running graph.
    // This marshalable struct maps to the C++ P/Invokable type.
    internal struct NowSoundTimeInfo
    {
        public Int64 TimeInSamples;
        public float ExactBeat;
        public float BeatsPerMinute;
        public Int32 BeatInMeasure;
    };

    // Information about an input in a created or running graph; all inputs are mono
    // (and can be panned at will).
    public struct NowSoundInputInfo
    {
        public float Volume;
        public float Pan;
    }

    // Information about a signal, in terms of the raw float signal values (no RMS or
    // decibel conversion is performed).
    public struct NowSoundSignalInfo
    {
        public float Min;
        public float Max;
        public float Avg;
    }

    // Information about the current graph time in NowSound terms.
    public struct TimeInfo
    {
        // The number of AudioInputs defined in the graph.
        //public readonly int AudioInputCount;
        // The number of samples elapsed since the audio graph started.
        public readonly Time<AudioSample> TimeInSamples;
        // The exact current beat (including fractional part; truncate to get integral beat count).
        public readonly ContinuousDuration<Beat> ExactBeat;
        // The current BPM of the graph.
        public readonly float BeatsPerMinute;
        // The current position in the measure. (e.g. 4/4 time = this ranges from 0 to 3)
        public readonly Int32 BeatInMeasure;

        internal TimeInfo(NowSoundTimeInfo pinvokeTimeInfo)
        {
            //AudioInputCount = pinvokeTimeInfo.AudioInputCount;
            TimeInSamples = pinvokeTimeInfo.TimeInSamples;
            ExactBeat = pinvokeTimeInfo.ExactBeat;
            BeatsPerMinute = pinvokeTimeInfo.BeatsPerMinute;
            BeatInMeasure = pinvokeTimeInfo.BeatInMeasure;
        }
    };

    // Information about a track's time in NowSound terms.
    // This marshalable struct maps to the C++ P/Invokable type.
    // TODO: maybe someday: look at custom marshalers to avoid the explicit (no-op) copy.
    internal struct NowSoundTrackInfo
    {
        // Only need one bit for this, but to avoid padding weirdness (observed in practice), we make this an int64.
        internal Int64 IsTrackLooping;
        internal Int64 StartTimeInSamples;
        internal float StartTimeInBeats;
        internal Int64 DurationInSamples;
        internal Int64 DurationInBeats;
        internal float ExactDuration;
        internal Int64 LocalClockTime;
        internal float LocalClockBeat;
        internal Int64 LastSampleTime;
        internal float Pan;
    };

    // Information about a track's time in NowSound terms.
    public struct TrackInfo
    {
        // Is the track currently looping? If not, it's still recording.
        public readonly bool IsTrackLooping;
        // The start time of the track in audio samples from the beginning of the session.
        public readonly Time<AudioSample> StartTime;
        // The start time of the track in (fractional) beats. (TODO?: add ContinuousTime<TTime>)
        public readonly ContinuousDuration<Beat> StartTimeInBeats;
        // The duration of the track in audio samples.
        public readonly Duration<AudioSample> Duration;
        // The duration of the track in (discrete) beats. (TODO?: allow non-beat-alignment)
        public readonly Duration<Beat> DurationInBeats;
        // The duration of the track in exact seconds; DurationInSamples is this, rounded up to the nearest sample.
        public readonly ContinuousDuration<Second> ExactDuration;
        // The local clock time (relative to the start of the current track); expressed as a duration.
        public readonly Duration<AudioSample> LocalClockTime;
        // The local clock time, in terms of beats.
        public readonly ContinuousDuration<Beat> LocalClockBeat;
        // The time at which the track last delivered samples.
        public readonly Time<AudioSample> LastSampleTime;
        // The current panning (0 = left, 1 = right).
        public readonly float Pan;

        internal TrackInfo(NowSoundTrackInfo pinvokeTrackInfo)
        {
            IsTrackLooping = pinvokeTrackInfo.IsTrackLooping > 0;
            StartTime = pinvokeTrackInfo.StartTimeInSamples;
            StartTimeInBeats = pinvokeTrackInfo.StartTimeInBeats;
            Duration = pinvokeTrackInfo.DurationInSamples;
            DurationInBeats = pinvokeTrackInfo.DurationInBeats;
            ExactDuration = pinvokeTrackInfo.ExactDuration;
            LocalClockTime = pinvokeTrackInfo.LocalClockTime;
            LocalClockBeat = pinvokeTrackInfo.LocalClockBeat;
            LastSampleTime = pinvokeTrackInfo.LastSampleTime;
            Pan = pinvokeTrackInfo.Pan;
        }
    };

    // The states of a NowSound graph.
    // Note that since this is extern "C", this is not an enum class, so these identifiers have to begin with Track
    // to disambiguate them from the TrackState identifiers.
    public enum NowSoundGraphState
    {
        // InitializeAsync() has not yet been called.
        GraphUninitialized,

        // Some error has occurred; GetLastError() will have details.
        GraphInError,

        // The audio graph has been started and is running.
        GraphRunning,
    };

    // The state of a particular IHolofunkAudioTrack.
    // Note that since this is extern "C", this is not an enum class, so these identifiers have to begin with Track
    // to disambiguate them from the GraphState identifiers.
    public enum NowSoundTrackState
    {
        // This track is not initialized -- important for some state machine cases and for catching bugs
        // (also important that this be the default value)
        TrackUninitialized,

        // The track is being recorded and it is not known when it will finish.
        TrackRecording,

        // The track is finishing off its now-known recording time.
        TrackFinishRecording,

        // The track is playing back, looping.
        TrackLooping,
    };

    // The audio inputs known to the app.
    /// Prevents confusing an audio input with some other int value.
    // 
    // The predefined values are really irrelevant; it can be cast to and from int as necessary.
    // But, used in parameters, the type helps with making the code self-documenting.
    public enum AudioInputId
    {
        AudioInputUndefined,
        AudioInput1,
        AudioInput2,
        AudioInput3,
        AudioInput4,
        AudioInput5,
        AudioInput6,
        AudioInput7,
    };

    // "Typedef" (C# style) for track IDs.
    // Default value (0) is not used.
    public enum TrackId
    {
        Undefined = 0
    }

    // Operations on the audio graph as a whole.
    // There is a single "static" audio graph defined here; multiple audio graphs are not (yet?) supported.
    // All async methods document the state the graph must be in when called, and the state the graph
    // transitions to on completion.
    //
    // Note that this class has private static extern methods for P/Invoking to NowSoundLib,
    // and public static methods which wrap the P/Invoke methods with more accurate C# types.
    //
    // TODO: make this support multiple (non-static) graphs.
    public class NowSoundGraphAPI
    {
        [DllImport("NowSoundLib")]
        static extern NowSoundGraphInfo NowSoundTrack_GetStaticGraphInfo();

        // Get static graph info for validating correct P/Invoke binding.
        internal static NowSoundGraphInfo GetStaticGraphInfo()
        {
            return NowSoundTrack_GetStaticGraphInfo();

        }

        [DllImport("NowSoundLib")]
        static extern NowSoundTimeInfo NowSoundGraph_GetStaticTimeInfo();

        // Get static time info for validating correct P/Invoke binding.
        internal static TimeInfo GetStaticTimeInfo()
        {
            return new TimeInfo(NowSoundGraph_GetStaticTimeInfo());
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundGraphState NowSoundGraph_State();

        // Get the current state of the audio graph; intended to be efficiently pollable by the client.
        // This is the only method that may be called in any state whatoever.
        public static NowSoundGraphState State()
        {
            return NowSoundGraph_State();
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_InitializeInstance();

        // Initialize the audio graph subsystem such that device information can be queried.
        // Graph must be Uninitialized.  On completion, graph becomes Initialized.
        // Must be called from message/UI thread. May have a momentary delay as JUCE doesn't support
        // async initialization.
        public static void InitializeInstance()
        {
            NowSoundGraph_InitializeInstance();
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundGraphInfo NowSoundGraph_Info();

        // Get the graph info for the created graph.
        // Graph must be Created or Running.
        public static NowSoundGraphInfo Info()
        {
            return NowSoundGraph_Info();
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundSignalInfo NowSoundGraph_OutputSignalInfo();

        // Get information about the current (final mix) output signal.
        // Graph must be Running.
        public static NowSoundSignalInfo OutputSignalInfo()
        {
            return NowSoundGraph_OutputSignalInfo();
        }

        // TODO: add query methods for device ID & name taking StringBuilder

        // Initialize the given device, given its index (as passed to InputDeviceInfo); returns the AudioInputId of the
        // input device.  If the input device has multiple channels, multiple consecutive AudioInputIds will be allocated,
        // but only the first will be returned.
        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_InitializeDeviceInputs(int deviceIndex);

        // Initialize the given device, given its index (as passed to InputDeviceInfo); returns the AudioInputId of the
        // input device.  If the input device has multiple channels, multiple consecutive AudioInputIds will be allocated,
        // but only the first will be returned.
        public static void InitializeDeviceInputs(int deviceIndex)
        {
            NowSoundGraph_InitializeDeviceInputs(deviceIndex);
        }

        // Initialize the FFT subsystem, which for now must be done before graph creation.
        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_InitializeFFT(
            int outputBinCount,
            float centralFrequency,
            int octaveDivisions,
            int centralBinIndex,
            int fftSize);

        public static void InitializeFFT(
            // How many output bins in the (logarithmic) frequency histogram?
            int outputBinCount,
            // What central frequency to use for the histogram?
            float centralFrequency,
            // How many divisions to make in each octave?
            int octaveDivisions,
            // Which bin index should be centered on centralFrequency?
            int centralBinIndex,
            // How many samples as input to and output from the FFT?
            int fftSize)
        {
            NowSoundGraph_InitializeFFT(
                outputBinCount,
                centralFrequency,
                octaveDivisions,
                centralBinIndex,
                fftSize);
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundTimeInfo NowSoundGraph_TimeInfo();

        // Get the current audio graph time.
        // Graph must be Running.
        public static TimeInfo TimeInfo()
        {
            return new TimeInfo(NowSoundGraph_TimeInfo());
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_PlayUserSelectedSoundFileAsync();

        // Play a user-selected sound file.
        // Graph must be Started.
        public static void PlayUserSelectedSoundFileAsync()
        {
            NowSoundGraph_PlayUserSelectedSoundFileAsync();
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_DestroyAudioGraphAsync();

        // Tear down the whole graph.
        // Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
        public static void DestroyAudioGraphAsync()
        {
            NowSoundGraph_DestroyAudioGraphAsync();
        }

        [DllImport("NowSoundLib")]
        static extern TrackId NowSoundGraph_CreateRecordingTrackAsync(AudioInputId id);

        // Create a new track and begin recording.
        // Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
        // TrackId returned from native code is incremented for TrackId on this end, to ensure any default
        // TrackId values are treated as uninitialized.
        public static TrackId CreateRecordingTrackAsync(AudioInputId id)
        {
            return NowSoundGraph_CreateRecordingTrackAsync(id);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_ShutdownInstance();

        // Shut down the graph and dispose its singleton instance.  On completion, state becomes Uninitialized.
        // Must be called from message/UI thread. May have a momentary delay as JUCE doesn't support
        // async initialization.
        public static void ShutdownInstance()
        {
            NowSoundGraph_ShutdownInstance();
        }

    }

    // Interface used to invoke operations on a particular audio track.
    // TODO: update native code to never return a zero/default TrackId, and then remove the "trackId - 1" code everywhere here.
    public class NowSoundTrackAPI
    {
        [DllImport("NowSoundLib")]
        static extern NowSoundTrackInfo NowSoundTrack_GetStaticTrackInfo();

        // In what state is this track?
        public static TrackInfo GetStaticTrackInfo()
        {
            return new TrackInfo(NowSoundTrack_GetStaticTrackInfo());
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundTrackState NowSoundTrack_State(TrackId trackId);

        // In what state is this track?
        public static NowSoundTrackState State(TrackId trackId)
        {
            return NowSoundTrack_State(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundTrackInfo NowSoundTrack_Info(TrackId trackId);

        // The current timing information for this Track.
        public static TrackInfo Info(TrackId trackId)
        {
            return new TrackInfo(NowSoundTrack_Info(trackId));
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundSignalInfo NowSoundTrack_SignalInfo(TrackId trackId);

        // The current output signal information for this Track.
        public static NowSoundSignalInfo SignalInfo(TrackId trackId)
        {
            return NowSoundTrack_SignalInfo(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundTrack_FinishRecording(TrackId trackId);

        // The user wishes the track to finish recording now.
        // Contractually requires State == NowSoundTrack_State.Recording.
        public static void FinishRecording(TrackId trackId)
        {
            NowSoundTrack_FinishRecording(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern bool NowSoundTrack_GetFrequencies(TrackId trackId, float[] floatBuffer, int floatBufferCapacity);

        // Get the current track frequency histogram; LPWSTR must actually reference a float buffer of the
        // same length as the outputBinCount argument passed to InitializeFFT, but must be typed as LPWSTR
        // and must have a capacity represented in two-byte wide characters (to match the P/Invoke style of
        // "pass in StringBuilder", known to work well).
        // Returns true if there was enough data to update the buffer, or false if there was not.
        public static bool GetFrequencies(TrackId trackId, float[] floatBuffer, int floatBufferCapacity)
        {
            return NowSoundTrack_GetFrequencies(trackId, floatBuffer, floatBufferCapacity);
        }

        [DllImport("NowSoundLib")]
        static extern bool NowSoundTrack_IsMuted(TrackId trackId);

        // True if this is muted.
        // 
        // Note that something can be in FinishRecording state but still be muted, if the user is fast!
        // Hence this is a separate flag, not represented as a NowSoundTrack_State.
        public static bool IsMuted(TrackId trackId)
        {
            return NowSoundTrack_IsMuted(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundTrack_SetIsMuted(TrackId trackId, bool isMuted);

        public static void SetIsMuted(TrackId trackId, bool isMuted)
        {
            NowSoundTrack_SetIsMuted(trackId, isMuted);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundTrack_Delete(TrackId trackId);

        // Delete this Track; after this, all methods become invalid to call (contract failure).
        public static void Delete(TrackId trackId)
        {
            NowSoundTrack_Delete(trackId);
        }
    }
}
