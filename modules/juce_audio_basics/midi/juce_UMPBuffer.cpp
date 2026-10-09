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

namespace UMPBufferHelpers
{
    inline int getPacketTime (const uint32_t* d) noexcept
    {
        return (int) d[0];
    }

    inline uint32_t getPacketTotalSize (const uint32_t* d) noexcept
    {
        return ump::Utils::getNumWordsForMessageType (d[1]) + 1;
    }

    static uint32_t* findPacketAfter (uint32_t* d, uint32_t* endData, int samplePosition) noexcept
    {
        while (d < endData && getPacketTime (d) <= samplePosition)
            d += getPacketTotalSize (d);

        return d;
    }
}

//==============================================================================
UMPBufferIterator& UMPBufferIterator::operator++() noexcept
{
    data += UMPBufferHelpers::getPacketTotalSize (data);
    return *this;
}

UMPBufferIterator UMPBufferIterator::operator++ (int) noexcept
{
    auto copy = *this;
    ++(*this);
    return copy;
}

UMPBufferIterator::reference UMPBufferIterator::operator*() const noexcept
{
    return { ump::View (data + 1), UMPBufferHelpers::getPacketTime (data) };
}

//==============================================================================
void UMPBuffer::swapWith (UMPBuffer& other) noexcept       { data.swapWith (other.data); }
void UMPBuffer::clear() noexcept                           { data.clearQuick(); }
void UMPBuffer::ensureSize (size_t minimumNumWords)        { data.ensureStorageAllocated ((int) minimumNumWords); }
bool UMPBuffer::isEmpty() const noexcept                   { return data.size() == 0; }

void UMPBuffer::clear (int start, int numSamples)
{
    auto startData = UMPBufferHelpers::findPacketAfter (data.begin(), data.end(), start - 1);
    auto endData   = UMPBufferHelpers::findPacketAfter (startData,    data.end(), start + numSamples - 1);

    data.removeRange ((int) (startData - data.begin()), (int) (endData - startData));
}

bool UMPBuffer::addPacket (ump::View packet, int sampleNumber)
{
    if (packet.data() == nullptr)
        return false;

    return addPacket (Span (packet.data(), packet.size()), sampleNumber);
}

bool UMPBuffer::addPacket (Span<const uint32_t> packetWords, int sampleNumber)
{
    if (packetWords.empty() || ump::Utils::getNumWordsForMessageType (packetWords.front()) != packetWords.size())
        return false;

    auto offset = (int) (UMPBufferHelpers::findPacketAfter (data.begin(), data.end(), sampleNumber) - data.begin());

    data.insertMultiple (offset, 0, (int) packetWords.size() + 1);

    auto* d = data.begin() + offset;
    *d++ = (uint32_t) sampleNumber;
    std::copy (packetWords.begin(), packetWords.end(), d);

    return true;
}

void UMPBuffer::addPackets (const UMPBuffer& otherBuffer,
                            int startSample, int numSamples, int sampleDeltaToAdd)
{
    for (auto i = otherBuffer.findNextSamplePosition (startSample); i != otherBuffer.cend(); ++i)
    {
        const auto metadata = *i;

        if (metadata.samplePosition >= startSample + numSamples && numSamples >= 0)
            break;

        addPacket (metadata.packet, metadata.samplePosition + sampleDeltaToAdd);
    }
}

int UMPBuffer::getNumPackets() const noexcept
{
    int n = 0;
    auto end = data.end();

    for (auto d = data.begin(); d < end; ++n)
        d += UMPBufferHelpers::getPacketTotalSize (d);

    return n;
}

int UMPBuffer::getFirstPacketTime() const noexcept
{
    return data.size() > 0 ? UMPBufferHelpers::getPacketTime (data.begin()) : 0;
}

int UMPBuffer::getLastPacketTime() const noexcept
{
    if (data.size() == 0)
        return 0;

    auto endData = data.end();

    for (auto d = data.begin();;)
    {
        auto nextOne = d + UMPBufferHelpers::getPacketTotalSize (d);

        if (nextOne >= endData)
            return UMPBufferHelpers::getPacketTime (d);

        d = nextOne;
    }
}

UMPBufferIterator UMPBuffer::findNextSamplePosition (int samplePosition) const noexcept
{
    return std::find_if (cbegin(), cend(), [&] (const UMPPacketMetadata& metadata) noexcept
    {
        return metadata.samplePosition >= samplePosition;
    });
}

void UMPBuffer::addToMidiBuffer (MidiBuffer& destBuffer, ump::ToBytestreamConverter& converter) const
{
    for (const auto metadata : *this)
    {
        const auto addEvent = [&] (const ump::BytesOnGroup& message, double)
        {
            destBuffer.addEvent (message.bytes.data(), (int) message.bytes.size(), metadata.samplePosition);
        };

        converter.convert (metadata.packet, (double) metadata.samplePosition, addEvent);
    }
}

void UMPBuffer::addFromMidiBuffer (const MidiBuffer& sourceBuffer, ump::GenericUMPConverter& converter)
{
    for (const auto metadata : sourceBuffer)
    {
        converter.convert (ump::BytesOnGroup { 0, metadata.asSpan() }, [&] (const ump::View& packet)
        {
            addPacket (packet, metadata.samplePosition);
        });
    }
}

//==============================================================================
//==============================================================================
#if JUCE_UNIT_TESTS

struct UMPBufferTest final : public UnitTest
{
    UMPBufferTest()
        : UnitTest ("UMPBuffer", UnitTestCategories::midi)
    {}

    void runTest() override
    {
        const auto noteOn1 = ump::Factory::makeNoteOnV1 (0, 0, 60, 100);
        const auto noteOn2 = ump::Factory::makeNoteOnV2 (0, 0, 60, ump::Factory::NoteAttributeKind::none, 0x8000, 0);
        const auto threeWords = ump::PacketX3 { 0xb0000000, 0x00000001, 0x00000002 };
        const auto endpointDiscovery = ump::Factory::makeEndpointDiscovery (1, 1, std::byte { 0x1f });

        beginTest ("Add packets of every size");
        {
            UMPBuffer buffer;
            expect (buffer.addPacket (noteOn1, 0));
            expect (buffer.addPacket (noteOn2, 1));
            expect (buffer.addPacket (threeWords, 2));
            expect (buffer.addPacket (endpointDiscovery, 3));

            expectEquals (buffer.getNumPackets(), 4);
            expectEquals (buffer.data.size(), 14);
            expectEquals (buffer.getFirstPacketTime(), 0);
            expectEquals (buffer.getLastPacketTime(), 3);

            auto iter = buffer.cbegin();
            expect (packetMatches (*iter++, noteOn1, 0));
            expect (packetMatches (*iter++, noteOn2, 1));
            expect (packetMatches (*iter++, threeWords, 2));
            expect (packetMatches (*iter++, endpointDiscovery, 3));
            expect (iter == buffer.cend());
        }

        beginTest ("Malformed packets are rejected");
        {
            UMPBuffer buffer;
            const std::array<uint32_t, 3> words { { noteOn2[0], noteOn2[1], 0 } };

            expect (! buffer.addPacket (Span (words.data(), 1), 0));
            expect (! buffer.addPacket (Span (words.data(), 3), 0));
            expect (! buffer.addPacket (Span<const uint32_t>(), 0));
            expect (! buffer.addPacket (ump::View(), 0));
            expect (buffer.isEmpty());

            expect (buffer.addPacket (Span (words.data(), 2), 0));
            expect (packetMatches (*buffer.cbegin(), noteOn2, 0));
        }

        beginTest ("Packets are kept in sample order");
        {
            UMPBuffer buffer;
            buffer.addPacket (endpointDiscovery, 30);
            buffer.addPacket (noteOn1, 10);
            buffer.addPacket (noteOn2, 20);
            buffer.addPacket (threeWords, 10);
            buffer.addPacket (noteOn2, 0);

            const std::vector<std::pair<int, uint32_t>> expected { { 0,  noteOn2[0] },
                                                                   { 10, noteOn1[0] },
                                                                   { 10, threeWords[0] },
                                                                   { 20, noteOn2[0] },
                                                                   { 30, endpointDiscovery[0] } };
            std::vector<std::pair<int, uint32_t>> actual;

            for (const auto metadata : buffer)
                actual.emplace_back (metadata.samplePosition, metadata.packet[0]);

            expect (actual == expected);

            expect (packetMatches (*buffer.findNextSamplePosition (10), noteOn1, 10));
            expect (packetMatches (*buffer.findNextSamplePosition (11), noteOn2, 20));
            expect (buffer.findNextSamplePosition (31) == buffer.cend());
        }

        const auto testBuffer = [&]
        {
            UMPBuffer buffer;
            buffer.addPacket (noteOn1, 0);
            buffer.addPacket (noteOn2, 10);
            buffer.addPacket (threeWords, 20);
            buffer.addPacket (endpointDiscovery, 30);
            return buffer;
        }();

        beginTest ("Clear packets");
        {
            {
                auto buffer = testBuffer;
                buffer.clear();
                expect (buffer.isEmpty());
                expectEquals (buffer.getNumPackets(), 0);
                expectEquals (buffer.getFirstPacketTime(), 0);
                expectEquals (buffer.getLastPacketTime(), 0);
            }

            {
                auto buffer = testBuffer;
                buffer.clear (10, 0);
                expectEquals (buffer.getNumPackets(), 4);
            }

            {
                auto buffer = testBuffer;
                buffer.clear (10, 1);
                expectEquals (buffer.getNumPackets(), 3);
                expect (packetMatches (*buffer.findNextSamplePosition (1), threeWords, 20));
            }

            {
                auto buffer = testBuffer;
                buffer.clear (10, 20);
                expectEquals (buffer.getNumPackets(), 2);
                expectEquals (buffer.getFirstPacketTime(), 0);
                expectEquals (buffer.getLastPacketTime(), 30);
            }

            {
                auto buffer = testBuffer;
                buffer.clear (10, 300);
                expectEquals (buffer.getNumPackets(), 1);
            }
        }

        beginTest ("Add packets from another buffer");
        {
            {
                UMPBuffer buffer;
                buffer.addPackets (testBuffer, 10, 20, 5);
                expectEquals (buffer.getNumPackets(), 2);
                expect (packetMatches (*buffer.cbegin(), noteOn2, 15));
                expectEquals (buffer.getLastPacketTime(), 25);
            }

            {
                UMPBuffer buffer;
                buffer.addPackets (testBuffer, 10, -1, -10);
                expectEquals (buffer.getNumPackets(), 3);
                expectEquals (buffer.getFirstPacketTime(), 0);
                expectEquals (buffer.getLastPacketTime(), 20);
            }
        }

        beginTest ("Adding packets after ensureSize doesn't allocate");
        {
            UMPBuffer buffer;
            buffer.ensureSize (5 * (1 + 4));

            const auto* storage = buffer.data.getRawDataPointer();

            for (auto i = 0; i < 5; ++i)
                expect (buffer.addPacket (endpointDiscovery, 4 - i));

            const auto copy = buffer;
            buffer.clear();
            buffer.addPackets (copy, 0, -1, 0);

            expectEquals (buffer.getNumPackets(), 5);
            expect (buffer.data.getRawDataPointer() == storage);
        }

        beginTest ("Convert to and from MidiBuffer");
        {
            const std::array<uint8, 9> sysExData { { 0x7e, 0x7f, 0x06, 0x01, 0x10, 0x11, 0x12, 0x13, 0x14 } };

            MidiBuffer midi;
            midi.addEvent (MidiMessage::noteOn (1, 60, (uint8) 100), 5);
            midi.addEvent (MidiMessage::createSysExMessage (sysExData.data(), (int) sysExData.size()), 10);

            ump::GenericUMPConverter midi1Converter { ump::PacketProtocol::MIDI_1_0 };
            UMPBuffer buffer;
            buffer.addFromMidiBuffer (midi, midi1Converter);

            const std::vector<UMPPacketMetadata> packets (buffer.begin(), buffer.end());
            expectEquals ((int) packets.size(), 3);
            expect (packetMatches (packets[0], noteOn1, 5));

            {
                ump::ToBytestreamConverter converter { 64 };
                MidiBuffer roundTripped;
                buffer.addToMidiBuffer (roundTripped, converter);
                expect (roundTripped.data == midi.data);
            }

            {
                UMPBuffer firstBlock, secondBlock;
                firstBlock.addPacket (packets[0].packet, 5);
                firstBlock.addPacket (packets[1].packet, 10);
                secondBlock.addPacket (packets[2].packet, 3);

                ump::ToBytestreamConverter converter { 64 };
                MidiBuffer firstMidi, secondMidi;
                firstBlock.addToMidiBuffer (firstMidi, converter);
                secondBlock.addToMidiBuffer (secondMidi, converter);

                MidiBuffer expectedFirst, expectedSecond;
                expectedFirst.addEvents (midi, 0, 10, 0);
                expectedSecond.addEvents (midi, 10, 1, -7);

                expect (firstMidi.data == expectedFirst.data);
                expect (secondMidi.data == expectedSecond.data);
            }
        }

        beginTest ("Convert from MidiBuffer to MIDI 2.0 packets");
        {
            ump::GenericUMPConverter midi2Converter { ump::PacketProtocol::MIDI_2_0 };

            {
                MidiBuffer midi;
                midi.addEvent (MidiMessage::noteOn (1, 60, (uint8) 100), 5);

                UMPBuffer buffer;
                buffer.addFromMidiBuffer (midi, midi2Converter);

                const uint16_t scaledVelocity = 0xc924;
                const auto expected = ump::Factory::makeNoteOnV2 (0, 0, 60, ump::Factory::NoteAttributeKind::none,
                                                                  scaledVelocity, 0);
                expectEquals (buffer.getNumPackets(), 1);
                expect (packetMatches (*buffer.cbegin(), expected, 5));
            }

            {
                MidiBuffer bankSelect, programChange;
                bankSelect.addEvent (MidiMessage::controllerEvent (1, 0, 1), 0);
                bankSelect.addEvent (MidiMessage::controllerEvent (1, 32, 2), 0);
                programChange.addEvent (MidiMessage::programChange (1, 5), 3);

                UMPBuffer buffer;
                buffer.addFromMidiBuffer (bankSelect, midi2Converter);
                expect (buffer.isEmpty());

                buffer.addFromMidiBuffer (programChange, midi2Converter);
                expectEquals (buffer.getNumPackets(), 1);
                expect (packetMatches (*buffer.cbegin(), ump::Factory::makeProgramChangeV2 (0, 0, 1, 5, 1, 2), 3));

                ump::GenericUMPConverter newConverter { ump::PacketProtocol::MIDI_2_0 };
                UMPBuffer withoutBank;
                withoutBank.addFromMidiBuffer (programChange, newConverter);
                expect (packetMatches (*withoutBank.cbegin(), ump::Factory::makeProgramChangeV2 (0, 0, 0, 5, 0, 0), 3));
            }
        }
    }

    template <size_t numWords>
    static bool packetMatches (const UMPPacketMetadata& metadata,
                               const ump::Packet<numWords>& packet,
                               int samplePosition)
    {
        return metadata.samplePosition == samplePosition
            && metadata.packet.size() == numWords
            && std::equal (packet.begin(), packet.end(), metadata.packet.begin());
    }
};

static UMPBufferTest umpBufferTest;

#endif

} // namespace juce
