// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include <algorithm>

#include "Clock.h"
#include "GetBuffer.h"
#include "Histogram.h"
#include "MagicConstants.h"
#include "NowSoundLib.h"
#include "NowSoundGraph.h"
#include "NowSoundTrack.h"
#include "NowSoundInput.h"

using namespace std;
using namespace winrt;

using namespace winrt::Windows::Foundation;

namespace NowSound
{
	NowSoundInputAudioProcessor::NowSoundInputAudioProcessor(
		NowSoundGraph* nowSoundGraph,
		AudioInputId inputId,
		BufferAllocator<float>* audioAllocator,
		int channel)
		: SpatialAudioProcessor(nowSoundGraph, MakeName(L"Input ", (int)inputId), 0.5),
		_audioInputId{ inputId },
		_channel{ channel },
		_incomingAudioStream{ 0, Clock::Instance().ChannelCount(), audioAllocator, Clock::Instance().SampleRateHz(), /*useExactLoopingMapper:*/false }		
	{
	}

	NowSoundSpatialParameters NowSoundInputAudioProcessor::Info()
	{
		NowSoundSpatialParameters ret;
		ret.Volume = 0; // TODO: fix this by going to output node
		ret.Pan = Pan();
		return ret;
	}

    NowSoundTrackAudioProcessor* NowSoundInputAudioProcessor::CreateRecordingTrack(TrackId id)
	{
		NowSoundTrackAudioProcessor* track = new NowSoundTrackAudioProcessor(Graph(), id, _incomingAudioStream, Pan());

		// Add the new track to the collection of tracks in NowSoundTrackAPI.
		Graph()->AddTrack(id, track);

        return track;
	}

	void NowSoundInputAudioProcessor::processBlock(juce::AudioBuffer<float>& audioBuffer, juce::MidiBuffer& midiBuffer)
	{
		// temporary debugging code: see if processBlock is ever being called under Holofunk
		if (CheckLogThrottle()) {
			std::wstringstream wstr{};
			wstr << getName() << L"::processBlock: count " << NextCounter();
			NowSoundGraph::Instance()->Log(wstr.str());
				
			if (_audioInputId == AudioInput1)
			{
				Graph()->LogConnections();
			}
		}

		// HACK!!!  If this is the zeroth input, then update the audio graph time.
        // We don't really have a great graph-level place to receive notifications from the JUCE graph,
        // so this is really a reasonable spot if you squint hard enough.  (At least it is always
        // connected and always receiving data.)
        if (_audioInputId == AudioInput1)
        {
            Clock::Instance().AdvanceFromAudioGraph(audioBuffer.getNumSamples());
        }

        // TODO: actually record into the bounded input stream!  if we decide that lookback is actually needed again.

        // now process the input audio spatially so we hear it panned in the output
        SpatialAudioProcessor::processBlock(audioBuffer, midiBuffer);
	}
}
