/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-9-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

namespace juce
{

//==============================================================================
/**
    An AudioIODeviceCallback object which streams audio through an AudioProcessor.

    To use one of these, just make it the callback used by your AudioIODevice, and
    give it a processor to use by calling setProcessor().

    It's also a MidiInputCallback, so you can connect it to both an audio and midi
    input to send both streams through the processor. To receive the midi inputs of an
    AudioDeviceManager in the format that the processor uses, pass the manager to
    setMidiInputDeviceManager() rather than registering this object with it. To set a
    MidiOutput for the processor, use the setMidiOutput() method.

    @see AudioProcessor, AudioProcessorGraph

    @tags{Audio}
*/
class JUCE_API  AudioProcessorPlayer    : public AudioIODeviceCallback,
                                          public MidiInputCallback
{
public:
    //==============================================================================
    AudioProcessorPlayer (bool doDoublePrecisionProcessing = false);

    /** Destructor. */
    ~AudioProcessorPlayer() override;

    //==============================================================================
    /** Sets the processor that should be played.

        The processor that is passed in will not be deleted or owned by this object.
        To stop anything playing, pass a nullptr to this method.

        If an AudioDeviceManager has been set with setMidiInputDeviceManager(), this must be
        called on the message thread, because a processor that uses a different MIDI format (see
        AudioProcessor::getMidiFormat()) moves the player's registration with the manager. A
        message that arrives during the move may reach the processor twice, and a change between
        the umpMidi1 and umpMidi2 formats discards any messages from the manager that are still
        waiting for the next block.
    */
    void setProcessor (AudioProcessor* processorToPlay);

    /** Returns the current audio processor that is being played. */
    AudioProcessor* getCurrentProcessor() const noexcept            { return processor; }

    /** Returns a midi message collector that you can pass midi messages to if you
        want them to be injected into the midi stream that is being sent to the
        processor.
    */
    MidiMessageCollector& getMidiMessageCollector() noexcept        { return messageCollector; }

    /** Sets the AudioDeviceManager whose MIDI inputs should be passed to the processor, if required.

        The messages from all of the manager's enabled MIDI inputs, apart from active sense, reach
        the processor in the format that it uses (see AudioProcessor::getMidiFormat()).

        Use this instead of registering this object, or the collector returned by
        getMidiMessageCollector(), with AudioDeviceManager::addMidiInputDeviceCallback(). Otherwise
        each message may reach the processor twice, or the player may remove that registration
        along with its own.

        The manager will not be deleted or owned by this object. Before the manager is deleted,
        pass a nullptr to this method. This must be called on the message thread, and while a
        manager is set, the destructor removes the player's registration, so it must run on the
        message thread too.
    */
    void setMidiInputDeviceManager (AudioDeviceManager* managerToUse);

    /** Sets the MIDI output that should be used, if required.

        The MIDI output will not be deleted or owned by this object. If the MIDI output is
        deleted, pass a nullptr to this method.
    */
    void setMidiOutput (MidiOutput* midiOutputToUse);

    /** Switch between double and single floating point precisions processing.

        The audio IO callbacks will still operate in single floating point precision,
        however, all internal processing including the AudioProcessor will be processed in
        double floating point precision if the AudioProcessor supports it (see
        AudioProcessor::supportsDoublePrecisionProcessing()). Otherwise, the processing will
        remain single precision irrespective of the parameter doublePrecision.
    */
    void setDoublePrecisionProcessing (bool doublePrecision);

    /** Returns true if this player processes internally processes the samples with
        double floating point precision.
    */
    inline bool getDoublePrecisionProcessing() { return isDoublePrecision; }

    //==============================================================================
    /** @internal */
    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const*, int, int, const AudioIODeviceCallbackContext&) override;
    /** @internal */
    void audioDeviceAboutToStart (AudioIODevice*) override;
    /** @internal */
    void audioDeviceStopped() override;
    /** @internal */
    void handleIncomingMidiMessage (MidiInput*, const MidiMessage&) override;

private:
   #if JUCE_UNIT_TESTS
    friend struct AudioProcessorPlayerTests;
   #endif

    struct NumChannels
    {
        NumChannels() = default;
        NumChannels (int numIns, int numOuts) : ins (numIns), outs (numOuts) {}

        explicit NumChannels (const AudioProcessor::BusesLayout& layout)
            : ins (layout.getNumChannels (true, 0)), outs (layout.getNumChannels (false, 0)) {}

        AudioProcessor::BusesLayout toLayout() const
        {
            return { { AudioChannelSet::canonicalChannelSet (ins) },
                     { AudioChannelSet::canonicalChannelSet (outs) } };
        }

        int ins = 0, outs = 0;
    };

    /*  Passes the packets from the MIDI inputs of an AudioDeviceManager to the packet
        collector, apart from active sense, which the manager also holds back from its
        MidiInputCallback objects. There's one for each protocol, so that a registration can
        be moved by adding the new one before removing the old one.
    */
    struct DeviceInput final : public ump::Consumer
    {
        explicit DeviceInput (UMPMessageCollector& c) : collector (c) {}

        void consume (ump::Iterator, ump::Iterator, double) override;

        UMPMessageCollector& collector;
    };

    //==============================================================================
    NumChannels findMostSuitableLayout (const AudioProcessor&) const;
    void resizeChannels();
    void prepareProcessor (AudioProcessor*);
    void moveMidiInputs (AudioDeviceManager*, AudioProcessor::MidiFormat);
    void addMidiInputs (AudioDeviceManager&, AudioProcessor::MidiFormat);
    void removeMidiInputs (AudioDeviceManager&, AudioProcessor::MidiFormat);

    //==============================================================================
    AudioProcessor* processor = nullptr;
    CriticalSection lock;
    double sampleRate = 0;
    int blockSize = 0;
    bool isPrepared = false, isDoublePrecision = false;

    NumChannels deviceChannels, defaultProcessorChannels, actualProcessorChannels;
    std::vector<float*> channels;
    AudioBuffer<float> tempBuffer;
    AudioBuffer<double> conversionBuffer;

    MidiBuffer incomingMidi;
    MidiMessageCollector messageCollector;
    UMPBuffer incomingPackets;
    UMPMessageCollector packetCollector;
    ump::GenericUMPConverter midiToPackets { ump::PacketProtocol::MIDI_1_0 };
    ump::ToBytestreamConverter packetsToMidi { 2048 };
    AudioProcessor::MidiFormat midiFormat = AudioProcessor::MidiFormat::midiBuffer;
    MidiOutput* midiOutput = nullptr;
    uint64_t sampleCount = 0;

    AudioDeviceManager* midiInputDeviceManager = nullptr;
    AudioProcessor::MidiFormat midiInputFormat = AudioProcessor::MidiFormat::midiBuffer;
    DeviceInput midi1Input { packetCollector }, midi2Input { packetCollector };

    AudioIODevice* currentDevice = nullptr;
    AudioWorkgroup currentWorkgroup;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioProcessorPlayer)
};

} // namespace juce
