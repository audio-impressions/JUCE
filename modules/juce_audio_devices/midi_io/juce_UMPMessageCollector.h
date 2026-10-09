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
    Collects incoming realtime Universal MIDI Packets and turns them into blocks
    suitable for processing by a block-based audio callback.

    This is the Universal MIDI Packet equivalent of MidiMessageCollector. The channel
    voice packets that it returns always use the protocol that was passed to reset(),
    so any that arrive in the other protocol are translated. Other packets, such as
    system messages (including active sense), SysEx and Flex Data, are passed on unchanged.

    The class can also be used as either a MidiKeyboardState::Listener or a ump::Consumer
    so it can easily use a MIDI input or keyboard component as its source.

    @see UMPBuffer, MidiMessageCollector, AudioDeviceManager::addMidiInputDeviceConsumer

    @tags{Audio}
*/
class JUCE_API UMPMessageCollector : public MidiKeyboardState::Listener,
                                     public ump::Consumer
{
public:
    //==============================================================================
    /** Creates a UMPMessageCollector. */
    UMPMessageCollector() = default;

    /** Destructor. */
    ~UMPMessageCollector() override = default;

    //==============================================================================
    /** Clears any packets from the queue, and sets the protocol of the channel voice
        packets that the collector returns.

        You need to call this method before starting to use the collector, so that
        it knows the correct sample rate and protocol to use.
    */
    void reset (double sampleRate, ump::PacketProtocol protocol);

    /** Returns the protocol that was passed to reset(). */
    ump::PacketProtocol getProtocol() const;

    /** Takes an incoming real-time packet and adds it to the queue.

        The time is in seconds, on the same scale as Time::getMillisecondCounterHiRes() * 0.001,
        and the packet will be ready for retrieval as part of the block returned by the next
        call to removeNextBlockOfPackets().

        A channel voice packet in the other protocol is translated before it is added, which can
        produce several packets, or none if it has no equivalent or belongs to a bank select, RPN
        or NRPN that is still incomplete. Those are assembled per group and channel, whatever
        their source, so sources that interleave them on the same group and channel can mix them
        up, and reset() discards any that are incomplete.

        This method is fully thread-safe when overlapping calls are made with
        removeNextBlockOfPackets().
    */
    void addPacketToQueue (ump::View packet, double time);

    /** Removes all the pending packets from the queue as a buffer.

        This will also correct the packets' timestamps to make sure they're in
        the range 0 to numSamples - 1.

        This call should be made regularly by something like an audio processing
        callback, because the time that it happens is used in calculating the
        packet positions.

        This method is fully thread-safe when overlapping calls are made with
        addPacketToQueue().

        Precondition: numSamples must be greater than 0.
    */
    void removeNextBlockOfPackets (UMPBuffer& destBuffer, int numSamples);

    /** Preallocates storage for collected packets.

        This can be called before audio processing begins to ensure that there
        is sufficient space for the expected packets, in order to avoid
        allocations within the audio callback.

        The size is given in 32-bit words, as for UMPBuffer::ensureSize().
    */
    void ensureStorageAllocated (size_t numWords);

    //==============================================================================
    /** @internal */
    void handleNoteOn (MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    /** @internal */
    void handleNoteOff (MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    /** @internal */
    void consume (ump::Iterator, ump::Iterator, double) override;

private:
    //==============================================================================
    double lastCallbackTime = 0;
    CriticalSection midiCallbackLock;
    UMPBuffer incomingPackets;
    ump::GenericUMPConverter converter { ump::PacketProtocol::MIDI_1_0 };
    double sampleRate = 44100.0;
   #if JUCE_DEBUG
    bool hasCalledReset = false;
   #endif

    void addNoteToQueue (bool isNoteOn, int midiChannel, int midiNoteNumber, float velocity);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UMPMessageCollector)
};

} // namespace juce
