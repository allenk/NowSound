# NowSound
Low latency audio library for Windows 10, targeted at Unity UWP and desktop apps.

NowSound is a wrapper library around the [JUCE audio library](https://juce.com).  It exposes a P/Invoke C-style API, such that it can be invoked by Unity apps (on either
the .NET or Mono runtimes).

**Status as of August 2019: the library works and has VST2 plugin support, and the demo .NET looper app
supports multiple tracks, input sound effects, track muting, and track deleting.**

## APIs

The P/Invokable C API is defined in [NowSoundLib.h](https://github.com/RobJellinghaus/NowSound/blob/master/NowSoundLib/NowSoundLib.h).  The C# wrapper around that API is in [NowSoundLib.cs](https://github.com/RobJellinghaus/NowSound/blob/master/NowSoundPInvokeLib/NowSoundLib.cs).

## Project structure

NowSound consists of the following subprojects:

- NowSoundLibShared: the core C++ classes for streaming and buffering
- NowSoundLib: a UWP C++ library sharing NowSoundLibShared and invoking AudioGraph,
  exposing a P/Invoke interface
- NowSoundPInvokeLib: a .NET C# library that wraps the P/Invoke NowSoundLib interface
- NowSoundWinFormsApp: a C# WinForms app (old school!) that uses the NowSoundPInvokeLib to
  demonstrate multitrack looping
- UnitTestsDesktop: a C++ TAEF testing library for the NowSoundLibShared code

Note that any pull requests must ensure that all tests are passing.

[VST](https://en.wikipedia.org/wiki/Virtual_Studio_Technology) support is planned, in the desktop
version of the library.  (UWP security restrictions are not friendly to most current VST plugins.) 

## Dependencies and Building

The JUCE library is required for compiling this project; the existing build expects it to 

## Rationale

I implemented NowSound because I needed lower-latency audio than is available in Unity's audio
subsystems, and because I needed to ensure all audio processing was happening natively.  On tight
low-latency audio deadlines, driving audio from C# (or any garbage-collected language) is sure to
cause audible trouble at some point.

JUCE is just a great audio system.  In particular the JUCE [AudioProcessorGraph](https://docs.juce.com/master/tutorial_audio_processor_graph.html) abstraction is
critical to how NowSoundLib handles effects processing; each track has its own internal pipeline
of AudioProcessor nodes, which it can manage purely internally without the rest of the system
needing to be involved.

If you are interested in NowSound, check out the project which motivated me to write it:
[my gestural Kinect-and-mixed-reality live looper, Holofunk](http://holofunk.com).  You might also
enjoy [my blog](http://robjsoftware.info) and in particular [this post about why I need
this sound library for my Holofunk project](https://robjsoftware.info/2017/12/02/big-ol-2017-mega-update-progress-almost-entirely-invisible/).
(Though it turns out that I was wrong about one thing there: AudioGraph is *not* managed
only, and when I realized this, I realized AudioGraph was the path of least resistance
for this library.)

## Code Patterns

It's worth mentioning some aspects of the code that are pretty important to developing and
maintaining it:

### Modern C++ Ownership

There is not a single explicit C++ `delete` statement in this repository.  Using std::unique_ptr,
std::move, and rvalue `&&` references means that the code is able to handle all buffer management
efficiently and correctly, without reference counting or GC overhead and without explicit deletion.

### Generic Types for Units

In my early audio programming experience with C, I learned a deep fear of the "int" data type.
Integers (either short, normal, or long) were used for sample counts, byte counts, numbers of
ticks, timestamps, milliseconds, and every other quantity.  Getting confused was easy to do
and hard to debug.

This code uses a common pattern, defining generic types `Time<T>` and `Duration<T>`.  The "T"
generic parameter serves as a placeholder for the type of time/duration involved.  For instance,
`Time<Sample>` defines a timestamp in terms of a count of audio samples since the start of the
program.  `Duration<Sample>` defines a time interval in terms of a number of audio samples.
Arithmetic operators exist to add Time + Duration, subtract Duration from Time, subtract Time
from Time giving a Duration, etc., provided that the T parameters match; you can't subtract
a `Time<Sample>` from a `Time<Seconds>` as the compiler won't let you.

This may look verbose, but it precludes so many kinds of bugs that it has been well worth
doing.
