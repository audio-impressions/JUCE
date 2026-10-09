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
void UMPMessageCollector::reset (const double newSampleRate, const ump::PacketProtocol newProtocol)
{
    const ScopedLock sl (midiCallbackLock);

    jassert (newSampleRate > 0);

   #if JUCE_DEBUG
    hasCalledReset = true;
   #endif
    sampleRate = newSampleRate;
    converter = ump::GenericUMPConverter { newProtocol };
    incomingPackets.clear();
    lastCallbackTime = Time::getMillisecondCounterHiRes();
}

ump::PacketProtocol UMPMessageCollector::getProtocol() const
{
    const ScopedLock sl (midiCallbackLock);
    return converter.getProtocol();
}

void UMPMessageCollector::addPacketToQueue (ump::View packet, double time)
{
    const ScopedLock sl (midiCallbackLock);

   #if JUCE_DEBUG
    jassert (hasCalledReset); // you need to call reset() to set the correct sample rate before using this object
   #endif

    // the packets that come in here need to be time-stamped correctly - see MidiInput
    // for details of what the number should be.
    jassert (! approximatelyEqual (time, 0.0));

    auto sampleNumber = (int) ((time - 0.001 * lastCallbackTime) * sampleRate);

    converter.convert (packet, [&] (const ump::View& converted)
    {
        incomingPackets.addPacket (converted, sampleNumber);
    });

    // if the packets don't get used for over a second, we'd better
    // get rid of any old ones to avoid the queue getting too big
    if (sampleNumber > sampleRate)
        incomingPackets.clear (0, sampleNumber - (int) sampleRate);
}

void UMPMessageCollector::removeNextBlockOfPackets (UMPBuffer& destBuffer,
                                                    const int numSamples)
{
    const ScopedLock sl (midiCallbackLock);

   #if JUCE_DEBUG
    jassert (hasCalledReset); // you need to call reset() to set the correct sample rate before using this object
   #endif

    jassert (numSamples > 0);

    auto timeNow = Time::getMillisecondCounterHiRes();
    auto msElapsed = timeNow - lastCallbackTime;

    lastCallbackTime = timeNow;

    if (! incomingPackets.isEmpty())
    {
        int numSourceSamples = jmax (1, roundToInt (msElapsed * 0.001 * sampleRate));
        int startSample = 0;
        int scale = 1 << 16;

        if (numSourceSamples > numSamples)
        {
            // if our list of events is longer than the buffer we're being
            // asked for, scale them down to squeeze them all in
            const int maxBlockLengthToUse = numSamples << 5;

            auto iter = incomingPackets.cbegin();

            if (numSourceSamples > maxBlockLengthToUse)
            {
                startSample = numSourceSamples - maxBlockLengthToUse;
                numSourceSamples = maxBlockLengthToUse;
                iter = incomingPackets.findNextSamplePosition (startSample);
            }

            scale = (numSamples << 10) / numSourceSamples;

            std::for_each (iter, incomingPackets.cend(), [&] (const UMPPacketMetadata& meta)
            {
                const auto pos = ((meta.samplePosition - startSample) * scale) >> 10;
                destBuffer.addPacket (meta.packet, jlimit (0, numSamples - 1, pos));
            });
        }
        else
        {
            // if our event list is shorter than the number we need, put them
            // towards the end of the buffer
            startSample = numSamples - numSourceSamples;

            for (const auto metadata : incomingPackets)
                destBuffer.addPacket (metadata.packet,
                                      jlimit (0, numSamples - 1, metadata.samplePosition + startSample));
        }

        incomingPackets.clear();
    }
}

void UMPMessageCollector::ensureStorageAllocated (size_t numWords)
{
    const ScopedLock sl (midiCallbackLock);
    incomingPackets.ensureSize (numWords);
}

//==============================================================================
void UMPMessageCollector::handleNoteOn (MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity)
{
    addNoteToQueue (true, midiChannel, midiNoteNumber, velocity);
}

void UMPMessageCollector::handleNoteOff (MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity)
{
    addNoteToQueue (false, midiChannel, midiNoteNumber, velocity);
}

void UMPMessageCollector::consume (ump::Iterator b, ump::Iterator e, double time)
{
    for (const auto& packet : makeRange (b, e))
        addPacketToQueue (packet, time);
}

void UMPMessageCollector::addNoteToQueue (bool isNoteOn, int midiChannel, int midiNoteNumber, float velocity)
{
    const auto channel = (uint8_t) (midiChannel - 1);
    const auto note = (uint8_t) midiNoteNumber;
    const auto time = Time::getMillisecondCounterHiRes() * 0.001;

    if (getProtocol() == ump::PacketProtocol::MIDI_2_0)
    {
        // a velocity that rounds to 0 makes a MIDI 1.0 note-on a note-off, so it does here too
        const auto sendNoteOn = isNoteOn && MidiMessage::floatValueToMidiByte (velocity) != 0;
        const auto attribute = ump::Factory::NoteAttributeKind::none;
        const auto value = (uint16_t) jlimit (0, 0xffff, roundToInt (velocity * 0xffff));
        const auto packet = sendNoteOn ? ump::Factory::makeNoteOnV2 (0, channel, note, attribute, value, 0)
                                       : ump::Factory::makeNoteOffV2 (0, channel, note, attribute, value, 0);
        addPacketToQueue (ump::View (packet.data()), time);
        return;
    }

    const auto value = MidiMessage::floatValueToMidiByte (velocity);
    const auto packet = isNoteOn ? ump::Factory::makeNoteOnV1 (0, channel, note, value)
                                 : ump::Factory::makeNoteOffV1 (0, channel, note, value);
    addPacketToQueue (ump::View (packet.data()), time);
}

//==============================================================================
//==============================================================================
#if JUCE_UNIT_TESTS

struct UMPMessageCollectorTest final : public UnitTest
{
    UMPMessageCollectorTest()
        : UnitTest ("UMPMessageCollector", UnitTestCategories::midi)
    {}

    void runTest() override
    {
        const auto none = ump::Factory::NoteAttributeKind::none;
        const auto noteOn1 = ump::Factory::makeNoteOnV1 (0, 0, 60, 100);
        const auto noteOn2 = ump::Factory::makeNoteOnV2 (0, 0, 60, none, 0xc924, 0);

        beginTest ("Packets are positioned by their time in seconds");
        {
            constexpr auto numSamples = (int) sampleRate;

            UMPMessageCollector collector;
            const auto startTime = getTimeInSeconds();
            collector.reset (sampleRate, ump::PacketProtocol::MIDI_1_0);

            const auto firstTime = startTime - 0.02;
            const auto secondTime = startTime - 0.01;
            collector.addPacketToQueue (ump::View (noteOn1.data()), firstTime);
            collector.addPacketToQueue (ump::View (noteOn1.data()), secondTime);

            UMPBuffer buffer;
            const auto timeBefore = getTimeInSeconds();
            collector.removeNextBlockOfPackets (buffer, numSamples);
            const auto timeAfter = getTimeInSeconds();

            expectEquals (buffer.getNumPackets(), 2);

            // The block ends during the call, and each position is truncated and the block's length
            // rounded to at least one sample, so a position can be up to 1.5 samples out
            const auto endOfBlock = (timeBefore + timeAfter) * 0.5;
            const auto tolerance = 1.5 + (timeAfter - timeBefore) * 0.5 * sampleRate;
            const auto firstPosition = (double) buffer.getFirstPacketTime();
            const auto secondPosition = (double) buffer.getLastPacketTime();

            expectWithinAbsoluteError (firstPosition, numSamples - (endOfBlock - firstTime) * sampleRate, tolerance);
            expectWithinAbsoluteError (secondPosition - firstPosition, (secondTime - firstTime) * sampleRate, 1.0);
        }

        beginTest ("Packets that span more than one block are scaled to fit it");
        {
            constexpr auto numSamples = 512;

            UMPMessageCollector collector;
            const auto startBefore = getTimeInSeconds();
            collector.reset (sampleRate, ump::PacketProtocol::MIDI_1_0);
            const auto startAfter = getTimeInSeconds();

            Thread::sleep (20);

            const auto lastTime = getTimeInSeconds();
            std::vector<double> times;

            for (auto i = 0; i < 5; ++i)
            {
                times.push_back (startAfter + (lastTime - startAfter) * i / 4);
                collector.addPacketToQueue (ump::View (noteOn1.data()), times.back());
            }

            UMPBuffer buffer;
            const auto endBefore = getTimeInSeconds();
            collector.removeNextBlockOfPackets (buffer, numSamples);
            const auto endAfter = getTimeInSeconds();

            expectEquals (buffer.getNumPackets(), 5);

            // The fixed-point scaling truncates three times, so a position can be early by 2.5
            // samples, plus one for every 1024 samples in the block's source
            const auto errorBelow = 2.5 + (endAfter - startBefore) * sampleRate / 1024;
            auto time = times.cbegin();

            for (const auto metadata : buffer)
            {
                const auto earliest = (*time - startAfter) / (endAfter - startAfter) * numSamples;
                const auto latest = (*time - startBefore) / (endBefore - startBefore) * numSamples;
                ++time;

                expectGreaterOrEqual ((double) metadata.samplePosition, earliest - errorBelow);
                expectLessOrEqual ((double) metadata.samplePosition, latest + 0.5);
            }
        }

        beginTest ("Keyboard notes use the collector's protocol");
        {
            const auto play = [] (MidiKeyboardState& state)
            {
                state.noteOn (1, 60, 1.0f);
                state.noteOff (1, 60, 0.0f);
            };

            expect (collectFromKeyboard (ump::PacketProtocol::MIDI_2_0, play)
                    == makeWords (ump::Factory::makeNoteOnV2 (0, 0, 60, none, 0xffff, 0),
                                  ump::Factory::makeNoteOffV2 (0, 0, 60, none, 0, 0)));

            expect (collectFromKeyboard (ump::PacketProtocol::MIDI_1_0, play)
                    == makeWords (ump::Factory::makeNoteOnV1 (0, 0, 60, 127),
                                  ump::Factory::makeNoteOffV1 (0, 0, 60, 0)));
        }

        beginTest ("A keyboard note-on with a velocity of 0 is a note-off in both protocols");
        {
            const auto play = [] (MidiKeyboardState& state) { state.noteOn (1, 60, 0.0f); };

            expect (collectFromKeyboard (ump::PacketProtocol::MIDI_2_0, play)
                    == makeWords (ump::Factory::makeNoteOffV2 (0, 0, 60, none, 0, 0)));

            expect (collectFromKeyboard (ump::PacketProtocol::MIDI_1_0, play)
                    == makeWords (ump::Factory::makeNoteOnV1 (0, 0, 60, 0)));
        }

        beginTest ("Channel voice packets in the other protocol are translated");
        {
            const auto controlChange1 = ump::Factory::makeControlChangeV1 (0, 0, 7, 100);
            const auto controlChange2 = ump::Factory::makeControlChangeV2 (0, 0, 7, 0xc9249249);
            const auto timingClock = ump::Factory::makeTimingClock (0);
            const auto words = makeWords (noteOn1, controlChange1, timingClock, noteOn2);

            expect (getWords (collect (ump::PacketProtocol::MIDI_2_0, words))
                    == makeWords (noteOn2, controlChange2, timingClock, noteOn2));

            expect (getWords (collect (ump::PacketProtocol::MIDI_1_0, words))
                    == makeWords (noteOn1, controlChange1, timingClock, noteOn1));
        }

        beginTest ("Translating a packet can add several packets, or none");
        {
            const auto bankMsb = ump::Factory::makeControlChangeV1 (0, 0, 0, 1);
            const auto bankLsb = ump::Factory::makeControlChangeV1 (0, 0, 32, 2);
            const auto programChange1 = ump::Factory::makeProgramChangeV1 (0, 0, 5);
            const auto programChange2 = ump::Factory::makeProgramChangeV2 (0, 0, 1, 5, 1, 2);

            const auto midi1 = collect (ump::PacketProtocol::MIDI_1_0, makeWords (programChange2));
            expect (getWords (midi1) == makeWords (bankMsb, bankLsb, programChange1));
            expectEquals (midi1.getFirstPacketTime(), midi1.getLastPacketTime());

            const auto midi2 = collect (ump::PacketProtocol::MIDI_2_0, makeWords (bankMsb, bankLsb, programChange1));
            expect (getWords (midi2) == makeWords (programChange2));

            const auto dataEntry = ump::Factory::makeControlChangeV1 (0, 0, 6, 10);
            expect (collect (ump::PacketProtocol::MIDI_2_0, makeWords (dataEntry)).isEmpty());
        }

        beginTest ("Resetting clears the queue and sets the protocol");
        {
            UMPMessageCollector collector;
            collector.reset (sampleRate, ump::PacketProtocol::MIDI_1_0);
            expect (collector.getProtocol() == ump::PacketProtocol::MIDI_1_0);

            collector.addPacketToQueue (ump::View (noteOn1.data()), getTimeInSeconds());
            collector.reset (sampleRate, ump::PacketProtocol::MIDI_2_0);
            expect (collector.getProtocol() == ump::PacketProtocol::MIDI_2_0);

            UMPBuffer buffer;
            collector.removeNextBlockOfPackets (buffer, (int) sampleRate);
            expect (buffer.isEmpty());
        }

        beginTest ("Removing packets reuses the destination's preallocated storage");
        {
            UMPMessageCollector collector;
            collector.ensureStorageAllocated (64);
            collector.reset (sampleRate, ump::PacketProtocol::MIDI_2_0);

            UMPBuffer buffer;
            buffer.ensureSize (64);

            const auto* storage = buffer.data.getRawDataPointer();

            for (auto block = 0; block < 3; ++block)
            {
                for (auto i = 0; i < 4; ++i)
                    collector.addPacketToQueue (ump::View (noteOn1.data()), getTimeInSeconds());

                buffer.clear();
                collector.removeNextBlockOfPackets (buffer, (int) sampleRate);
                expectEquals (buffer.getNumPackets(), 4);
            }

            expect (buffer.data.getRawDataPointer() == storage);
        }
    }

    static constexpr auto sampleRate = 48000.0;

    static double getTimeInSeconds()
    {
        return Time::getMillisecondCounterHiRes() * 0.001;
    }

    /*  Passes `words` to a new collector in a single call, and returns the next block. */
    static UMPBuffer collect (ump::PacketProtocol protocol, const std::vector<uint32_t>& words)
    {
        UMPMessageCollector collector;
        collector.reset (sampleRate, protocol);
        collector.consume (ump::Iterator (words.data(), words.size()),
                           ump::Iterator (words.data() + words.size(), 0),
                           getTimeInSeconds());

        UMPBuffer buffer;
        collector.removeNextBlockOfPackets (buffer, (int) sampleRate);
        return buffer;
    }

    /*  Plays `notes` on a keyboard that a new collector listens to, and returns the words in
        the next block.
    */
    template <typename Notes>
    static std::vector<uint32_t> collectFromKeyboard (ump::PacketProtocol protocol, Notes&& notes)
    {
        UMPMessageCollector collector;
        collector.reset (sampleRate, protocol);

        MidiKeyboardState state;
        state.addListener (&collector);
        notes (state);
        state.removeListener (&collector);

        UMPBuffer buffer;
        collector.removeNextBlockOfPackets (buffer, (int) sampleRate);
        return getWords (buffer);
    }

    static std::vector<uint32_t> getWords (const UMPBuffer& buffer)
    {
        std::vector<uint32_t> words;

        for (const auto metadata : buffer)
            words.insert (words.end(), metadata.packet.begin(), metadata.packet.end());

        return words;
    }

    template <typename... Packets>
    static std::vector<uint32_t> makeWords (const Packets&... packets)
    {
        std::vector<uint32_t> words;
        (words.insert (words.end(), packets.begin(), packets.end()), ...);
        return words;
    }
};

static UMPMessageCollectorTest umpMessageCollectorTest;

#endif

} // namespace juce
