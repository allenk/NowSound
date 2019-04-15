// NowSoundLib.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "NowSoundLib.h"
#include "NowSoundGraph.h"
#include "NowSoundTrack.h"

namespace NowSound
{
	__declspec(dllexport) NowSoundGraphInfo NowSoundGraph_GetStaticGraphInfo()
	{
		return CreateNowSoundGraphInfo(
			1,
			2,
			3,
			4,
			5
			// JUCETODO: , 6
		);
	}

	__declspec(dllexport) NowSoundTimeInfo NowSoundGraph_GetStaticTimeInfo()
	{
		return CreateNowSoundTimeInfo(
			// JUCETODO: 1,
			1,
			(float)2,
			(float)3,
			(float)4);
	}

	NowSoundGraphState NowSoundGraph_State()
	{
		if (NowSoundGraph::Instance() == nullptr)
		{
			return NowSoundGraphState::GraphUninitialized;
		}
		else
		{
			return NowSoundGraph::Instance()->State();
		}
	}

	void NowSoundGraph_InitializeInstance()
	{
		Check(NowSoundGraph_State() == NowSoundGraphState::GraphUninitialized);
		NowSoundGraph::InitializeInstance();
	}

	NowSoundGraphInfo NowSoundGraph_Info()
	{
		// externally, this can only be called once Initialize is complete; internally, NowSoundGraph::Info() is called *during* Initialize
		Check(NowSoundGraph::Instance()->State() > NowSoundGraphState::GraphUninitialized);
		return NowSoundGraph::Instance()->Info();
	}

	NowSoundLogInfo NowSoundGraph_LogInfo()
	{
		// externally, this can only be called once Initialize is complete
		Check(NowSoundGraph::Instance()->State() > NowSoundGraphState::GraphUninitialized);
		return NowSoundGraph::Instance()->LogInfo();
	}

	void NowSoundGraph_GetLogMessage(int32_t logMessageIndex, LPWSTR buffer, int32_t bufferCapacity)
	{
		// externally, this can only be called once Initialize is complete
		Check(NowSoundGraph::Instance()->State() > NowSoundGraphState::GraphUninitialized);
		NowSoundGraph::Instance()->GetLogMessage(logMessageIndex, buffer, bufferCapacity);
	}

	void NowSoundGraph_DropLogMessagesUpTo(int32_t logMessageIndex)
	{
		// externally, this can only be called once Initialize is complete
		Check(NowSoundGraph::Instance()->State() > NowSoundGraphState::GraphUninitialized);
		NowSoundGraph::Instance()->DropLogMessagesUpTo(logMessageIndex);
	}

	NowSoundSignalInfo NowSoundGraph_OutputSignalInfo()
    {
        Check(NowSoundGraph::Instance()->State() > NowSoundGraphState::GraphUninitialized);
        return NowSoundGraph::Instance()->OutputSignalInfo();
    }

#ifdef INPUT_DEVICE_SELECTION // JUCETODO
	void NowSoundGraph_InputDeviceId(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity)
	{
		NowSoundGraph::Instance()->InputDeviceId(deviceIndex, wcharBuffer, bufferCapacity);
	}

	void NowSoundGraph_InputDeviceName(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity)
	{
		NowSoundGraph::Instance()->InputDeviceName(deviceIndex, wcharBuffer, bufferCapacity);
	}

	void NowSoundGraph_InitializeDeviceInputs(int deviceIndex)
	{
		NowSoundGraph::Instance()->InitializeDeviceInputs(deviceIndex);
	}
#endif

	void NowSoundGraph_InitializeFFT(
		int outputBinCount,
		float centralFrequency,
		int octaveDivisions,
		int centralBinIndex,
		int fftSize)
	{
		NowSoundGraph::Instance()->InitializeFFT(outputBinCount, centralFrequency, octaveDivisions, centralBinIndex, fftSize);
	}

	NowSoundTimeInfo NowSoundGraph_TimeInfo()
	{
		return NowSoundGraph::Instance()->TimeInfo();
	}

#ifdef INPUT_DEVICE_SELECTION
	NowSoundInputInfo NowSoundGraph_InputInfo(AudioInputId audioInputId)
	{
		return NowSoundGraph::Instance()->InputInfo(audioInputId);
	}
#endif

	void NowSoundGraph_PlayUserSelectedSoundFileAsync()
	{
		NowSoundGraph::Instance()->PlayUserSelectedSoundFileAsync();
	}

	TrackId NowSoundGraph_CreateRecordingTrackAsync(AudioInputId audioInputId)
	{
		return NowSoundGraph::Instance()->CreateRecordingTrackAsync(audioInputId);
	}

	void NowSoundGraph_ShutdownInstance()
	{
		NowSoundGraph::ShutdownInstance();
	}

	__declspec(dllexport) NowSoundTrackInfo NowSoundTrack_GetStaticTrackInfo()
	{
		return CreateNowSoundTrackInfo(
			// don't pass 0 for zeroth (bool) field
            true,
			2,
			(float)3,
			4,
			5,
			(float)6,
			7,
			(float)8,
			9,
			(float)10);
	}

	__declspec(dllexport) NowSoundTrackState NowSoundTrack_State(TrackId trackId)
	{
		return NowSoundTrackAudioProcessor::Track(trackId)->State();
	}

	__declspec(dllexport) int64_t /*Duration<Beat>*/ NowSoundTrack_BeatDuration(TrackId trackId)
	{
		return NowSoundTrackAudioProcessor::Track(trackId)->BeatDuration().Value();
	}

	__declspec(dllexport) float /*ContinuousDuration<Beat>*/ NowSoundTrack_BeatPositionUnityNow(TrackId trackId)
	{
		return NowSoundTrackAudioProcessor::Track(trackId)->BeatPositionUnityNow().Value();
	}

	__declspec(dllexport) float /*ContinuousDuration<AudioSample>*/ NowSoundTrack_ExactDuration(TrackId trackId)
	{
		return NowSoundTrackAudioProcessor::Track(trackId)->ExactDuration().Value();
	}

	__declspec(dllexport) NowSoundTrackInfo NowSoundTrack_Info(TrackId trackId)
	{
		return NowSoundTrackAudioProcessor::Track(trackId)->Info();
	}

	__declspec(dllexport) NowSoundSignalInfo NowSoundTrack_SignalInfo(TrackId trackId)
	{
		return NowSoundTrackAudioProcessor::Track(trackId)->SignalInfo();
	}

	__declspec(dllexport) void NowSoundTrack_FinishRecording(TrackId trackId)
	{
		NowSoundTrackAudioProcessor::Track(trackId)->FinishRecording();
	}

	__declspec(dllexport) void NowSoundTrack_GetFrequencies(TrackId trackId, void* floatBuffer, int floatBufferCapacity)
	{
		NowSoundTrackAudioProcessor::Track(trackId)->GetFrequencies(floatBuffer, floatBufferCapacity);
	}

	__declspec(dllexport) bool NowSoundTrack_IsMuted(TrackId trackId)
	{
		return NowSoundTrackAudioProcessor::Track(trackId)->IsMuted();
	}

	__declspec(dllexport) void NowSoundTrack_SetIsMuted(TrackId trackId, bool isMuted)
	{
		NowSoundTrackAudioProcessor::Track(trackId)->IsMuted(isMuted);
	}

	__declspec(dllexport) void NowSoundTrack_Delete(TrackId trackId)
	{
		NowSoundTrackAudioProcessor::DeleteTrack(trackId);
	}
}