// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include <string>
#include <sstream>

#include "stdint.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "Clock.h"
#include "GetBuffer.h"
#include "MagicConstants.h"
#include "NowSoundGraph.h"
#include "NowSoundLib.h"
#include "NowSoundTrack.h"
#include "Recorder.h"
#include "Slice.h"
#include "SliceStream.h"
#include "NowSoundTime.h"

using namespace concurrency;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace winrt::Windows::Foundation;

namespace NowSound
{
    std::map<TrackId, std::unique_ptr<NowSoundTrackAudioProcessor>> NowSoundTrackAudioProcessor::s_tracks{};

    // Windows::Media::AudioFrame NowSoundTrack::s_audioFrame{ nullptr };

    void NowSoundTrackAudioProcessor::DeleteTrack(TrackId trackId)
    {
        Check(trackId >= TrackId::TrackIdUndefined && trackId <= s_tracks.size());
        Track(trackId)->Delete();
        // emplace a null pointer
        s_tracks[trackId] = std::unique_ptr<NowSoundTrackAudioProcessor>{};
    }

    void NowSoundTrackAudioProcessor::AddTrack(TrackId id, std::unique_ptr<NowSoundTrackAudioProcessor>&& track)
    {
        s_tracks.emplace(id, std::move(track));
    }

    NowSoundTrackAudioProcessor* NowSoundTrackAudioProcessor::Track(TrackId id)
    {
        // NOTE THAT THIS PATTERN DOES NOT LOCK THE _tracks COLLECTION IN ANY WAY.
        // The only way this will be correct is if all modifications to _tracks happen only as a result of
        // non-concurrent, serialized external calls to NowSoundTrackAPI.
        Check(id > TrackId::TrackIdUndefined);
        NowSoundTrackAudioProcessor* value = s_tracks.at(id).get();
        Check(value != nullptr); // TODO: don't fail on invalid client values; instead return standard error code or something
        return value;
    }

    NowSoundTrackAudioProcessor::NowSoundTrackAudioProcessor(
		NowSoundGraph* graph,
        TrackId trackId,
        AudioInputId inputId,
        const BufferedSliceStream<AudioSample, float>& sourceStream,
		float initialPan)
		: _graph{ graph },
		_trackId{ trackId },
        _inputId{ inputId },
        _state{ NowSoundTrackState::TrackRecording },
        // latency compensation effectively means the track started before it was constructed ;-)
        _audioStream(
            Clock::Instance().Now() - Clock::Instance().TimeToSamples(MagicConstants::PreRecordingDuration),
            1, // mono streams only for now (and maybe indefinitely)
            NowSoundGraph::Instance()->AudioAllocator(),
            /*maxBufferedDuration:*/ 0,
            /*useContinuousLoopingMapper*/ false),
        // one beat is the shortest any track ever is (TODO: allow optionally relaxing quantization)
        _beatDuration{ 1 },
        // _audioFrameInputNode{ NowSoundGraph::Instance()->GetAudioGraph().CreateFrameInputNode() },
        _lastSampleTime{ Clock::Instance().Now() },
        _isMuted{ false },
        _debugLog{},
        _requiredSamplesHistogram { MagicConstants::AudioQuantumHistogramCapacity },
		_sinceLastSampleTimingHistogram{ MagicConstants::AudioQuantumHistogramCapacity },
		_volumeHistogram{ (int)Clock::Instance().TimeToSamples(MagicConstants::RecentVolumeDuration).Value() },
		_pan{ initialPan },
		_frequencyTracker{ _graph->FftSize() < 0
			? ((NowSoundFrequencyTracker*)nullptr)
			: new NowSoundFrequencyTracker(_graph->BinBounds(), _graph->FftSize()) }
	{
        Check(_lastSampleTime.Value() >= 0);

        // Tracks should only be created from the UI thread (or at least not from the audio thread).
        // TODO: thread contracts.

        // should only ever call this when graph is fully up and running
        Check(NowSoundGraph::Instance()->State() == NowSoundGraphState::GraphRunning);

		/* HACK: try NOT pre-recording any data... just push the start time back
        if (MagicConstants::PreRecordingDuration.Value() > 0)
        {
            // Prepend latencyCompensation's worth of previously buffered input audio, to prepopulate this track.
			Duration<AudioSample> latencyCompensationDuration = Clock::Instance().TimeToSamples(MagicConstants::PreRecordingDuration);
            Interval<AudioSample> lastIntervalOfSourceStream(
                sourceStream.InitialTime() + sourceStream.DiscreteDuration() - latencyCompensationDuration,
                latencyCompensationDuration);
            sourceStream.AppendTo(lastIntervalOfSourceStream, &_audioStream);
        }
		*/

        // Add the appropriate connections.

    }

    void NowSoundTrackAudioProcessor::DebugLog(const std::wstring& entry)
    {
        _debugLog.push(entry);
        if (_debugLog.size() > MagicConstants::DebugLogCapacity)
        {
            _debugLog.pop();
        }
    }
    
    NowSoundTrackState NowSoundTrackAudioProcessor::State() const { return _state; }
    
    Duration<Beat> NowSoundTrackAudioProcessor::BeatDuration() const { return _beatDuration; }
    
    ContinuousDuration<Beat> NowSoundTrackAudioProcessor::BeatPositionUnityNow() const
    {
        // TODO: determine whether we really need a time that only moves forward between Unity frames.
        // For now, let time be determined solely by audio graph, and let Unity observe time increasing 
        // during a single Unity frame.
        Duration<AudioSample> sinceStart(Clock::Instance().Now() - _audioStream.InitialTime());
        Time<AudioSample> sinceStartTime(sinceStart.Value());

        ContinuousDuration<Beat> beats = Clock::Instance().TimeToBeats(sinceStartTime);
        int completeBeatsSinceStart = (int)beats.Value() % (int)BeatDuration().Value();
        return (ContinuousDuration<Beat>)(completeBeatsSinceStart + (beats.Value() - (int)beats.Value()));
    }

    ContinuousDuration<AudioSample> NowSoundTrackAudioProcessor::ExactDuration() const
    {
        return (int)BeatDuration().Value() * Clock::Instance().BeatDuration().Value();
    }

    Time<AudioSample> NowSoundTrackAudioProcessor::StartTime() const { return _audioStream.InitialTime(); }

    ContinuousDuration<Beat> TrackBeats(Duration<AudioSample> localTime, Duration<Beat> beatDuration)
    {
        ContinuousDuration<Beat> totalBeats = Clock::Instance().TimeToBeats(localTime.Value());
        Duration<Beat> nonFractionalBeats((int)totalBeats.Value());

        return (ContinuousDuration<Beat>)(
            // total (non-fractional) beats modulo the beat duration of the track
            (nonFractionalBeats.Value() % beatDuration.Value())
            // fractional beats of the track
            + (totalBeats.Value() - nonFractionalBeats.Value()));
    }

    NowSoundTrackInfo NowSoundTrackAudioProcessor::Info() 
    {
        Time<AudioSample> lastSampleTime = this->_lastSampleTime; // to prevent any drift from this being updated concurrently
        Time<AudioSample> startTime = this->_audioStream.InitialTime();
		Duration<AudioSample> localClockTime = Clock::Instance().Now() - startTime;
        return CreateNowSoundTrackInfo(
            startTime.Value(),
            Clock::Instance().TimeToBeats(startTime).Value(),
            this->_audioStream.DiscreteDuration().Value(),
            this->BeatDuration().Value(),
            this->_state == NowSoundTrackState::TrackLooping ? _audioStream.ExactDuration().Value() : 0,
			localClockTime.Value(),
			TrackBeats(localClockTime, this->_beatDuration).Value(),
			(lastSampleTime - startTime).Value(),
			_volumeHistogram.Average(),
			_pan,
            _requiredSamplesHistogram.Min(),
            _requiredSamplesHistogram.Max(),
            _requiredSamplesHistogram.Average(),
            _sinceLastSampleTimingHistogram.Min(),
            _sinceLastSampleTimingHistogram.Max(),
            _sinceLastSampleTimingHistogram.Average());
    }

    bool NowSoundTrackAudioProcessor::IsMuted() const { return _isMuted; }
    void NowSoundTrackAudioProcessor::SetIsMuted(bool isMuted) { _isMuted = isMuted; }

	void NowSoundTrackAudioProcessor::GetFrequencies(void* floatBuffer, int floatBufferCapacity)
	{
		if (_frequencyTracker == nullptr)
		{
			return;
		}

		_frequencyTracker->GetLatestHistogram((float*)floatBuffer, floatBufferCapacity);
	}
	
	void NowSoundTrackAudioProcessor::FinishRecording()
    {
        // TODO: ThreadContract.RequireUnity();

        // no need for any synchronization at all; the Record() logic will see this change.
        // We have no memory fence here but this write does reliably get seen sufficiently quickly in practice.
        _state = NowSoundTrackState::TrackFinishRecording;
    }

    void NowSoundTrackAudioProcessor::Delete()
    {
        // TODO: ThreadContract.RequireUnity();

        // _audioFrameInputNode.Stop();

		/*
        while (_audioFrameInputNode.OutgoingConnections().Size() > 0)
        {
            _audioFrameInputNode.RemoveOutgoingConnection(_audioFrameInputNode.OutgoingConnections().GetAt(0).Destination());
        }
        // TODO: does destruction properly clean this up? _audioFrameInputNode.Dispose();
		*/
    }

	const double Pi = std::atan(1) * 4;

    void NowSoundTrackAudioProcessor::processBlock(
        AudioBuffer<float>& buffer,
        MidiBuffer& midiMessages)
    {
        // Depending on the current state of this track, we either record, or we finish recording
        // and switch modes to looping, or we're straight looping.
        Duration<AudioSample> duration{ buffer.getNumSamples() };
        switch (_state)
        {
        case NowSoundTrackState::TrackRecording:
        {
            // How many complete beats after we record this data?
            Time<AudioSample> durationAsTime((_audioStream.DiscreteDuration() + duration).Value());
            Duration<Beat> completeBeats = (Duration<Beat>)((int)Clock::Instance().TimeToBeats(durationAsTime).Value());

            // If it's more than our _beatDuration, bump our _beatDuration
            // TODO: implement other quantization policies here
            if (completeBeats >= _beatDuration)
            {
                // 1/2/4* quantization, like old times. TODO: make this selectable
                if (_beatDuration == 1)
                {
                    _beatDuration = 2;
                }
                else if (_beatDuration == 2)
                {
                    _beatDuration = 4;
                }
                else
                {
                    _beatDuration = _beatDuration + Duration<Beat>(4);
                }
                // blow up if we happen somehow to be recording more than one beat's worth (should never happen given low latency expectation)
                Check(completeBeats < BeatDuration());
            }

            // and actually record the full amount of available data
            _audioStream.Append(duration, buffer.getReadPointer(0));
            // and volume track
            _volumeHistogram.AddAll(buffer.getReadPointer(0), duration.Value(), true);
            // and provide it to frequency histogram as well
            if (_frequencyTracker != nullptr)
            {
                _frequencyTracker->Record(buffer.getReadPointer(0), duration.Value());
            }
            break;
        }

        case NowSoundTrackState::TrackFinishRecording:
        {
            // we now need to be sample-accurate.  If we get too many samples, here is where we truncate.
            Duration<AudioSample> roundedUpDuration((long)std::ceil(ExactDuration().Value()));

            // we should not have advanced beyond roundedUpDuration yet, or something went wrong at end of recording
            Check(_audioStream.DiscreteDuration() <= roundedUpDuration);

            if (_audioStream.DiscreteDuration() + duration >= roundedUpDuration)
            {
                // reduce duration so we only capture the exact right number of samples
                duration = roundedUpDuration - _audioStream.DiscreteDuration();

                // we are done recording altogether
                _state = NowSoundTrackState::TrackLooping;
                continueRecording = false;

                _audioStream.Append(duration, data);

                // now that we have done our final append, shut the stream at the current duration
                _audioStream.Shut(ExactDuration());
            }
            else
            {
                // capture the full duration
                _audioStream.Append(duration, data);
            }

            break;
        }
        }
#if NOPE
        Check(sender == _audioFrameInputNode);

        Check(args.RequiredSamples() >= 0);
        uint32_t requiredSamples = (uint32_t)args.RequiredSamples();

        if (requiredSamples == 0)
        {
            s_zeroByteOutgoingFrameCount++;
            return;
        }

        // sender.DiscardQueuedFrames();

        DateTime dateTimeNow = DateTime::clock::now();
        TimeSpan sinceLast = dateTimeNow - _lastQuantumTime;
        _lastQuantumTime = dateTimeNow;

        if (_state != NowSoundTrackState::TrackLooping)
        {
            // copy nothing to anywhere
            return;
        }

        float samplesSinceLastQuantum = ((float)sinceLast.count() * Clock::Instance().SampleRateHz() / Clock::TicksPerSecond);

        _requiredSamplesHistogram.Add((float)requiredSamples);
        _sinceLastSampleTimingHistogram.Add(samplesSinceLastQuantum);

#if STATIC_AUDIO_FRAME
        if (s_audioFrame == nullptr)
        {
            // The AudioFrame.Duration property is a TimeSpan, despite the fact that this seems an inherently
            // inaccurate way to precisely express an audio sample count.  So we just have a short frame and
            // we fill it completely and often.
            s_audioFrame = Windows::Media::AudioFrame(
                (uint32_t)(MagicConstants::AudioFrameDuration.Value()
                    * sizeof(float)
                    * Clock::Instance().ChannelCount()));
        }
        Windows::Media::AudioFrame audioFrame = s_audioFrame;
#else
        Windows::Media::AudioFrame audioFrame((uint32_t)(Clock::Instance().SampleRateHz()
            * MagicConstants::AudioFrameLengthSeconds.Value()
            * sizeof(float)
            * Clock::Instance().ChannelCount()));
#endif

        {
            // This nested scope sets the extent of the LockBuffer call below, which must close before the AddFrame call.
            // Otherwise the AddFrame will throw E_ACCESSDENIED when it tries to take a read lock on the frame.
            uint8_t* audioGraphInputDataInBytes{};
            uint32_t capacityInBytes{};

            // OMG KENNY KERR WINS AGAIN:
            // https://gist.github.com/kennykerr/f1d941c2d26227abbf762481bcbd84d3
            Windows::Media::AudioBuffer buffer(audioFrame.LockBuffer(Windows::Media::AudioBufferAccessMode::Write));
            IMemoryBufferReference reference(buffer.CreateReference());
            winrt::impl::com_ref<IMemoryBufferByteAccess> interop = reference.as<IMemoryBufferByteAccess>();
            check_hresult(interop->GetBuffer(&audioGraphInputDataInBytes, &capacityInBytes));

            int sampleSizeInBytes = Clock::Instance().ChannelCount() * sizeof(float);

            /*
            uint32_t requiredBytes = requiredSamples * sampleSizeInBytes;
            uint32_t maxInputBytes = MagicConstants::MaxInputSamples * sampleSizeInBytes;
#undef max // never never should have been a macro
            uint32_t bytesRemaining = std::max(requiredBytes, maxInputBytes);
            */
            uint32_t bytesRemaining = capacityInBytes;
            Check((bytesRemaining % sampleSizeInBytes) == 0);

            int samplesRemaining = (int)bytesRemaining / sampleSizeInBytes;

            // TODO: contact Microsoft about this: why is this API taking a timespan rather than an exact sample count?
            // And how does one ensure the timespan conversion is exactly right?
            // For now, avoid this property altogether and tune things with the size of the audio frame;
            // note that *creating* an audio frame takes a sample count and not a timespan duration....
            //audioFrame.Duration(TimeSpan(samplesRemaining * Clock::TicksPerSecond / Clock::Instance().SampleRateHz()));

			// Coefficients for panning the mono data into the audio buffer.
			// Use cosine panner for volume preservation.
			double angularPosition = _pan * Pi / 2;
			double leftCoefficient = std::cos(angularPosition);
			double rightCoefficient = std::sin(angularPosition);

			int channelCount = Clock::Instance().ChannelCount();
			// only stereo supported
			// TODO: support more channels, fuller spatialization
			Check(channelCount == 2); 

			while (samplesRemaining > 0)
            {
                // get a slice up to samplesRemaining samples in length
                Slice<AudioSample, float> slice(
                    _audioStream.GetSliceContaining(Interval<AudioSample>(_lastSampleTime, samplesRemaining)));
				Duration<AudioSample> sliceDuration = slice.SliceDuration();

				// longest is a mono stream.
				// Now is when we must stereo-pan it.
				float* audioGraphInputDataInFloats = (float*)audioGraphInputDataInBytes;

				// Record all this data in the frequency tracker.
				_frequencyTracker->Record(slice.OffsetPointer(), sliceDuration.Value());

				// Pan each mono sample (and track its volume), if we're not muted.
				if (!IsMuted())
				{
					for (int i = 0; i < sliceDuration.Value(); i++)
					{
						float value = slice.Get(i, 0);
						_volumeHistogram.Add(std::abs(value));
						audioGraphInputDataInFloats[i * channelCount] = (float)(leftCoefficient * value);
						audioGraphInputDataInFloats[i * channelCount + 1] = (float)(rightCoefficient);
					}
				}

                audioGraphInputDataInBytes += slice.SliceDuration().Value() * channelCount * sizeof(float);
                _lastSampleTime = _lastSampleTime + slice.SliceDuration();
                Check(_lastSampleTime.Value() >= 0);
                samplesRemaining -= (int)slice.SliceDuration().Value();
            }
        }

        sender.AddFrame(audioFrame);
#endif
    }

    int NowSoundTrackAudioProcessor::s_zeroByteOutgoingFrameCount{};
}
