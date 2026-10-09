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
    A view of a Universal MIDI Packet stored in a contiguous buffer.

    Instances of this class do *not* own the packet data that they point to.
    Instead, they expect the packet data to live in a separate buffer that outlives
    the UMPPacketMetadata instance.

    @tags{Audio}
*/
struct UMPPacketMetadata
{
    UMPPacketMetadata() noexcept = default;

    UMPPacketMetadata (ump::View packetIn, int positionIn) noexcept
        : packet (packetIn), samplePosition (positionIn)
    {
    }

    /** A view of the packet's words. */
    ump::View packet;

    /** The packet's timestamp. */
    int samplePosition = 0;
};

//==============================================================================
/**
    An iterator to move over the packets in a UMPBuffer, which allows iterating
    over a UMPBuffer using C++11 range-for syntax.

    In the following example, we log the position of each MIDI 2.0 note-on in a buffer.
    @code
    for (const UMPPacketMetadata metadata : umpBuffer)
    {
        const auto firstWord = metadata.packet[0];

        if (ump::Utils::getMessageType (firstWord) == ump::Utils::MessageKind::channelVoice2
            && ump::Utils::getStatus (firstWord) == std::byte { 0x9 })
        {
            Logger::writeToLog (String (metadata.samplePosition));
        }
    }
    @endcode

    @tags{Audio}
*/
class JUCE_API UMPBufferIterator
{
    using Ptr = const uint32_t*;

public:
    UMPBufferIterator() = default;

    /** Constructs an iterator pointing at the packet starting at the word `dataIn`.
        `dataIn` must point to the start of a packet stored in a UMPBuffer. If it does
        not, calling other member functions on the iterator will result in undefined
        behaviour.
    */
    explicit UMPBufferIterator (const uint32_t* dataIn) noexcept
        : data (dataIn)
    {
    }

    using difference_type   = std::iterator_traits<Ptr>::difference_type;
    using value_type        = UMPPacketMetadata;
    using reference         = UMPPacketMetadata;
    using pointer           = void;
    using iterator_category = std::input_iterator_tag;

    /** Make this iterator point to the next packet in the buffer. */
    UMPBufferIterator& operator++() noexcept;

    /** Create a copy of this object, make this iterator point to the next packet in
        the buffer, then return the copy.
    */
    UMPBufferIterator operator++ (int) noexcept;

    /** Return true if this iterator points to the same packet as another
        iterator instance, otherwise return false.
    */
    bool operator== (const UMPBufferIterator& other) const noexcept { return data == other.data; }

    /** Return false if this iterator points to the same packet as another
        iterator instance, otherwise returns true.
    */
    bool operator!= (const UMPBufferIterator& other) const noexcept { return ! operator== (other); }

    /** Return an instance of UMPPacketMetadata which describes the packet to which
        the iterator is currently pointing.
    */
    reference operator*() const noexcept;

private:
    Ptr data = nullptr;
};

//==============================================================================
/**
    Holds a sequence of time-stamped Universal MIDI Packets.

    This is the Universal MIDI Packet equivalent of MidiBuffer. It holds a set of
    packets with integer time-stamps, and the buffer is kept sorted in order of the
    time-stamps.

    Packets of any type, using either the MIDI 1.0 or the MIDI 2.0 protocol, are
    stored exactly as they are added.

    @see MidiBuffer, UMPBufferIterator

    @tags{Audio}
*/
class JUCE_API UMPBuffer
{
public:
    //==============================================================================
    /** Creates an empty UMPBuffer. */
    UMPBuffer() noexcept = default;

    //==============================================================================
    /** Removes all packets from the buffer. */
    void clear() noexcept;

    /** Removes all packets between two times from the buffer.

        All packets for which (start <= packet position < start + numSamples) will
        be removed.
    */
    void clear (int start, int numSamples);

    /** Returns true if the buffer is empty.
        To actually retrieve the packets, use a UMPBufferIterator object
    */
    bool isEmpty() const noexcept;

    /** Counts the number of packets in the buffer.

        This is actually quite a slow operation, as it has to iterate through all
        the packets, so you might prefer to call isEmpty() if that's all you need
        to know.
    */
    int getNumPackets() const noexcept;

    /** Adds a packet to the buffer.

        The sample number will be used to determine the position of the packet in
        the buffer, which is always kept sorted.

        If a packet is added whose sample position is the same as one or more packets
        already in the buffer, the new packet will be placed after the existing ones.

        If the view is invalid, no packet will be added.

        To retrieve packets, use a UMPBufferIterator object.

        Returns true on success, or false on failure.
    */
    bool addPacket (ump::View packet, int sampleNumber);

    /** Adds a packet to the buffer from raw 32-bit words.

        The sample number will be used to determine the position of the packet in
        the buffer, which is always kept sorted.

        If a packet is added whose sample position is the same as one or more packets
        already in the buffer, the new packet will be placed after the existing ones.

        The words must hold exactly one packet. The first word will be inspected to
        calculate the number of words that the packet takes up, and if this doesn't
        match the number of words supplied, no packet will be added.

        To retrieve packets, use a UMPBufferIterator object.

        Returns true on success, or false on failure.
    */
    bool addPacket (Span<const uint32_t> packetWords, int sampleNumber);

    /** Adds some packets from another buffer to this one.

        @param otherBuffer          the buffer containing the packets you want to add
        @param startSample          the lowest sample number in the source buffer for which
                                    packets should be added. Any source packets whose timestamp is
                                    less than this will be ignored
        @param numSamples           the valid range of samples from the source buffer for which
                                    packets should be added - i.e. packets in the source buffer whose
                                    timestamp is greater than or equal to (startSample + numSamples)
                                    will be ignored. If this value is less than 0, all packets after
                                    startSample will be taken.
        @param sampleDeltaToAdd     a value which will be added to the source timestamps of the packets
                                    that are added to this buffer
    */
    void addPackets (const UMPBuffer& otherBuffer,
                     int startSample,
                     int numSamples,
                     int sampleDeltaToAdd);

    /** Returns the sample number of the first packet in the buffer.
        If the buffer's empty, this will just return 0.
    */
    int getFirstPacketTime() const noexcept;

    /** Returns the sample number of the last packet in the buffer.
        If the buffer's empty, this will just return 0.
    */
    int getLastPacketTime() const noexcept;

    //==============================================================================
    /** Converts the packets in this buffer to bytestream MIDI messages, and adds the
        messages to a MidiBuffer at the same sample positions.

        MIDI 2.0 Channel Voice messages are translated to MIDI 1.0, which may lose some
        detail, and packets that have no bytestream equivalent are skipped.

        The converter holds on to SysEx messages that span several packets until they
        are complete, so the same converter should be used for every block of a stream.
    */
    void addToMidiBuffer (MidiBuffer& destBuffer, ump::ToBytestreamConverter& converter) const;

    /** Converts the messages in a MidiBuffer to Universal MIDI Packets on group 0, and
        adds the packets to this buffer at the same sample positions.

        The converter determines the protocol of the packets and keeps its state between
        calls, so the same converter should be used for every block of a stream.
    */
    void addFromMidiBuffer (const MidiBuffer& sourceBuffer, ump::GenericUMPConverter& converter);

    //==============================================================================
    /** Exchanges the contents of this buffer with another one.

        This is a quick operation, because no memory allocating or copying is done, it
        just swaps the internal state of the two buffers.
    */
    void swapWith (UMPBuffer&) noexcept;

    /** Preallocates some memory for the buffer to use.
        This helps to avoid needing to reallocate space when the buffer has packets
        added to it.

        The size is given in 32-bit words. Each packet uses one word more than its own
        length, to hold its sample position.
    */
    void ensureSize (size_t minimumNumWords);

    /** Get a read-only iterator pointing to the beginning of this buffer. */
    UMPBufferIterator begin()  const noexcept { return cbegin(); }

    /** Get a read-only iterator pointing one past the end of this buffer. */
    UMPBufferIterator end()    const noexcept { return cend(); }

    /** Get a read-only iterator pointing to the beginning of this buffer. */
    UMPBufferIterator cbegin() const noexcept { return UMPBufferIterator (data.begin()); }

    /** Get a read-only iterator pointing one past the end of this buffer. */
    UMPBufferIterator cend()   const noexcept { return UMPBufferIterator (data.end()); }

    /** Get an iterator pointing to the first packet with a timestamp greater-than or
        equal-to `samplePosition`.
    */
    UMPBufferIterator findNextSamplePosition (int samplePosition) const noexcept;

    //==============================================================================
    /** The raw data holding this buffer.
        Obviously access to this data is provided at your own risk. Its internal format could
        change in future, so don't write code that relies on it!
    */
    Array<uint32_t> data;

private:
    JUCE_LEAK_DETECTOR (UMPBuffer)
};

} // namespace juce
