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

MidiKeyboardState::MidiKeyboardState()
{
    zerostruct (noteStates);
}

//==============================================================================
void MidiKeyboardState::reset()
{
    const ScopedLock sl (lock);
    zerostruct (noteStates);
    eventsToAdd.clear();
}

bool MidiKeyboardState::isNoteOn (const int midiChannel, const int n) const noexcept
{
    jassert (midiChannel > 0 && midiChannel <= 16);

    return isPositiveAndBelow (n, 128)
            && (noteStates[n] & (1 << (midiChannel - 1))) != 0;
}

bool MidiKeyboardState::isNoteOnForChannels (const int midiChannelMask, const int n) const noexcept
{
    return isPositiveAndBelow (n, 128)
            && (noteStates[n] & midiChannelMask) != 0;
}

void MidiKeyboardState::noteOn (const int midiChannel, const int midiNoteNumber, const float velocity)
{
    jassert (midiChannel > 0 && midiChannel <= 16);
    jassert (isPositiveAndBelow (midiNoteNumber, 128));

    const ScopedLock sl (lock);

    if (isPositiveAndBelow (midiNoteNumber, 128))
    {
        const int timeNow = (int) Time::getMillisecondCounter();
        eventsToAdd.addEvent (MidiMessage::noteOn (midiChannel, midiNoteNumber, velocity), timeNow);
        eventsToAdd.clear (0, timeNow - 500);

        noteOnInternal (midiChannel, midiNoteNumber, velocity);
    }
}

void MidiKeyboardState::noteOnInternal  (const int midiChannel, const int midiNoteNumber, const float velocity)
{
    if (isPositiveAndBelow (midiNoteNumber, 128))
    {
        noteStates[midiNoteNumber] = static_cast<uint16> (noteStates[midiNoteNumber] | (1 << (midiChannel - 1)));
        listeners.call ([&] (Listener& l) { l.handleNoteOn (this, midiChannel, midiNoteNumber, velocity); });
    }
}

void MidiKeyboardState::noteOff (const int midiChannel, const int midiNoteNumber, const float velocity)
{
    const ScopedLock sl (lock);

    if (isNoteOn (midiChannel, midiNoteNumber))
    {
        const int timeNow = (int) Time::getMillisecondCounter();
        eventsToAdd.addEvent (MidiMessage::noteOff (midiChannel, midiNoteNumber), timeNow);
        eventsToAdd.clear (0, timeNow - 500);

        noteOffInternal (midiChannel, midiNoteNumber, velocity);
    }
}

void MidiKeyboardState::noteOffInternal  (const int midiChannel, const int midiNoteNumber, const float velocity)
{
    if (isNoteOn (midiChannel, midiNoteNumber))
    {
        noteStates[midiNoteNumber] = static_cast<uint16> (noteStates[midiNoteNumber] & ~(1 << (midiChannel - 1)));
        listeners.call ([&] (Listener& l) { l.handleNoteOff (this, midiChannel, midiNoteNumber, velocity); });
    }
}

void MidiKeyboardState::allNotesOff (const int midiChannel)
{
    const ScopedLock sl (lock);

    if (midiChannel <= 0)
    {
        for (int i = 1; i <= 16; ++i)
            allNotesOff (i);
    }
    else
    {
        for (int i = 0; i < 128; ++i)
            noteOff (midiChannel, i, 0.0f);
    }
}

void MidiKeyboardState::processNextMidiEvent (const MidiMessage& message)
{
    if (message.isNoteOn())
    {
        noteOnInternal (message.getChannel(), message.getNoteNumber(), message.getFloatVelocity());
    }
    else if (message.isNoteOff())
    {
        noteOffInternal (message.getChannel(), message.getNoteNumber(), message.getFloatVelocity());
    }
    else if (message.isAllNotesOff())
    {
        for (int i = 0; i < 128; ++i)
            noteOffInternal (message.getChannel(), i, 0.0f);
    }
}

void MidiKeyboardState::processNextMidiEvent (ump::View packet)
{
    const auto firstWord = packet[0];
    const auto messageType = ump::Utils::getMessageType (firstWord);

    if (messageType == ump::Utils::MessageKind::channelVoice1)
    {
        processNextMidiEvent (ump::SingleGroupMidi1ToBytestreamExtractor::fromUmp (ump::PacketX1 { firstWord }));
    }
    else if (messageType == ump::Utils::MessageKind::channelVoice2)
    {
        const auto status = ump::Utils::getStatus (firstWord);

        if (status == std::byte { 0x8 } || status == std::byte { 0x9 })
        {
            const auto channel = ump::Utils::getChannel (firstWord) + 1;
            const auto noteNumber = (int) ump::Utils::U8<2>::get (firstWord);
            const auto velocity = (float) ump::Utils::U16<0>::get (packet[1]) / (float) 0xffff;

            // Unlike in MIDI 1.0, a note-on with a velocity of 0 is still a note-on
            if (status == std::byte { 0x9 })
                noteOnInternal (channel, noteNumber, velocity);
            else
                noteOffInternal (channel, noteNumber, velocity);
        }
        else
        {
            // Other messages, such as all-notes-off, are handled in the same way as their MIDI 1.0 translations
            ump::Conversion::midi2ToMidi1DefaultTranslation (packet, [this] (const ump::View& translated)
            {
                processNextMidiEvent (translated);
            });
        }
    }
}

void MidiKeyboardState::processNextMidiBuffer (MidiBuffer& buffer,
                                               const int startSample,
                                               const int numSamples,
                                               const bool injectIndirectEvents)
{
    const ScopedLock sl (lock);

    for (const auto metadata : buffer)
        processNextMidiEvent (metadata.getMessage());

    if (injectIndirectEvents)
    {
        const int firstEventToAdd = eventsToAdd.getFirstEventTime();
        const double scaleFactor = numSamples / (double) (eventsToAdd.getLastEventTime() + 1 - firstEventToAdd);

        for (const auto metadata : eventsToAdd)
        {
            const auto pos = jlimit (0, numSamples - 1, roundToInt ((metadata.samplePosition - firstEventToAdd) * scaleFactor));
            buffer.addEvent (metadata.getMessage(), startSample + pos);
        }
    }

    eventsToAdd.clear();
}

void MidiKeyboardState::processNextMidiBuffer (UMPBuffer& buffer,
                                               const int startSample,
                                               const int numSamples,
                                               const bool injectIndirectEvents,
                                               const ump::PacketProtocol protocol)
{
    const ScopedLock sl (lock);

    for (const auto metadata : buffer)
        processNextMidiEvent (metadata.packet);

    if (injectIndirectEvents)
    {
        const int firstEventToAdd = eventsToAdd.getFirstEventTime();
        const double scaleFactor = numSamples / (double) (eventsToAdd.getLastEventTime() + 1 - firstEventToAdd);
        ump::GenericUMPConverter converter { protocol };

        for (const auto metadata : eventsToAdd)
        {
            const auto pos = jlimit (0, numSamples - 1,
                                     roundToInt ((metadata.samplePosition - firstEventToAdd) * scaleFactor));

            converter.convert (ump::BytesOnGroup { 0, metadata.asSpan() }, [&] (const ump::View& packet)
            {
                buffer.addPacket (packet, startSample + pos);
            });
        }
    }

    eventsToAdd.clear();
}

//==============================================================================
void MidiKeyboardState::addListener (Listener* listener)
{
    const ScopedLock sl (lock);
    listeners.add (listener);
}

void MidiKeyboardState::removeListener (Listener* listener)
{
    const ScopedLock sl (lock);
    listeners.remove (listener);
}

//==============================================================================
//==============================================================================
#if JUCE_UNIT_TESTS

struct MidiKeyboardStateTest final : public UnitTest
{
    MidiKeyboardStateTest()
        : UnitTest ("MidiKeyboardState", UnitTestCategories::midi)
    {}

    void runTest() override
    {
        const auto none = ump::Factory::NoteAttributeKind::none;

        beginTest ("MIDI 1.0 note-on and note-off packets");
        {
            MidiKeyboardState state;
            NoteRecorder recorder { state };

            process (state, ump::Factory::makeNoteOnV1 (0, 2, 60, 100));
            expect (state.isNoteOn (3, 60));

            process (state, ump::Factory::makeNoteOffV1 (0, 2, 60, 64));
            expect (! state.isNoteOn (3, 60));

            const auto noteOnVelocity = MidiMessage::noteOn (3, 60, (uint8) 100).getFloatVelocity();
            const auto noteOffVelocity = MidiMessage::noteOff (3, 60, (uint8) 64).getFloatVelocity();
            expect (recorder.notes == std::vector<Note> { { true, 3, 60, noteOnVelocity },
                                                          { false, 3, 60, noteOffVelocity } });
        }

        beginTest ("MIDI 2.0 note-on and note-off packets");
        {
            const auto pitch = ump::Factory::NoteAttributeKind::pitch7_9;

            MidiKeyboardState state;
            NoteRecorder recorder { state };

            process (state, ump::Factory::makeNoteOnV2 (0, 2, 60, pitch, 0x8000, 0x1234));
            expect (state.isNoteOn (3, 60));

            process (state, ump::Factory::makeNoteOffV2 (0, 2, 60, pitch, 0x4000, 0x1234));
            expect (! state.isNoteOn (3, 60));

            expect (recorder.notes == std::vector<Note> { { true, 3, 60, (float) 0x8000 / (float) 0xffff },
                                                          { false, 3, 60, (float) 0x4000 / (float) 0xffff } });
        }

        beginTest ("A note-on with a velocity of 0 is a note-off in MIDI 1.0 only");
        {
            MidiKeyboardState state;
            NoteRecorder recorder { state };

            process (state, ump::Factory::makeNoteOnV1 (0, 0, 60, 100));
            process (state, ump::Factory::makeNoteOnV1 (0, 0, 60, 0));
            expect (! state.isNoteOn (1, 60));

            process (state, ump::Factory::makeNoteOnV2 (0, 0, 60, none, 0, 0));
            expect (state.isNoteOn (1, 60));

            const auto velocity = MidiMessage::noteOn (1, 60, (uint8) 100).getFloatVelocity();
            expect (recorder.notes == std::vector<Note> { { true, 1, 60, velocity },
                                                          { false, 1, 60, 0.0f },
                                                          { true, 1, 60, 0.0f } });
        }

        beginTest ("All-notes-off packets release every note on their channel");
        {
            const auto noteOn1 = ump::Factory::makeNoteOnV1 (0, 0, 60, 100);
            const auto noteOn2 = ump::Factory::makeNoteOnV2 (0, 0, 64, none, 0x8000, 0);
            const auto noteOnOtherChannel = ump::Factory::makeNoteOnV1 (0, 1, 60, 100);

            const auto checkAllNotesOff = [&] (auto allNotesOff)
            {
                MidiKeyboardState state;
                process (state, noteOn1);
                process (state, noteOn2);
                process (state, noteOnOtherChannel);

                process (state, allNotesOff);

                expect (! state.isNoteOn (1, 60));
                expect (! state.isNoteOn (1, 64));
                expect (state.isNoteOn (2, 60));
            };

            checkAllNotesOff (ump::Factory::makeControlChangeV1 (0, 0, 123, 0));
            checkAllNotesOff (ump::Factory::makeControlChangeV2 (0, 0, 123, 0));
        }

        beginTest ("Packets of other types are ignored");
        {
            MidiKeyboardState state;
            process (state, ump::Factory::makeNoteOnV1 (0, 0, 60, 100));

            NoteRecorder recorder { state };

            const std::array<std::byte, 3> sysExData { { std::byte { 0x7e }, std::byte { 0x7f }, std::byte { 0x06 } } };

            process (state, ump::Factory::makeNoop (0));
            process (state, ump::Factory::makeTimingClock (0));
            process (state, ump::Factory::makeSysExIn1Packet (0, sysExData));
            process (state, ump::Factory::makeEndpointDiscovery (1, 1, std::byte { 0x1f }));
            process (state, ump::Factory::makeControlChangeV1 (0, 0, 7, 100));
            process (state, ump::Factory::makePolyPressureV1 (0, 0, 60, 0));
            process (state, ump::Factory::makePitchBend (0, 0, 0x2000));
            process (state, ump::Factory::makeControlChangeV2 (0, 0, 7, 0x80000000));
            process (state, ump::Factory::makePolyPressureV2 (0, 0, 60, 0));
            process (state, ump::Factory::makeProgramChangeV2 (0, 0, 1, 5, 1, 2));
            process (state, ump::Factory::makePerNoteManagementV2 (0, 0, 60, std::byte { 0x3 }));

            // A mixed data set header, whose status matches a note-off
            process (state, ump::PacketX4 { 0x50803c00, 0, 0, 0 });

            expect (state.isNoteOn (1, 60));
            expect (recorder.notes.empty());
        }

        beginTest ("Processing a buffer handles packets of either protocol");
        {
            UMPBuffer buffer;
            buffer.addPacket (ump::Factory::makeNoteOnV1 (0, 0, 60, 100), 0);
            buffer.addPacket (ump::Factory::makeNoteOnV2 (0, 0, 64, none, 0, 0), 10);
            buffer.addPacket (ump::Factory::makeNoteOffV1 (0, 0, 60, 0), 20);

            MidiKeyboardState state;
            state.processNextMidiBuffer (buffer, 0, 64, false, ump::PacketProtocol::MIDI_2_0);

            expect (! state.isNoteOn (1, 60));
            expect (state.isNoteOn (1, 64));
            expectEquals (buffer.getNumPackets(), 3);
        }

        beginTest ("Keyboard notes are added in the given protocol");
        {
            const auto addKeyboardNotes = [] (ump::PacketProtocol protocol)
            {
                MidiKeyboardState state;
                state.noteOn (1, 60, 1.0f);
                state.noteOff (1, 60, 0.0f);
                state.noteOn (1, 62, 0.0f);

                UMPBuffer buffer;
                state.processNextMidiBuffer (buffer, 100, 50, true, protocol);
                return buffer;
            };

            const auto midi1 = addKeyboardNotes (ump::PacketProtocol::MIDI_1_0);
            expect (getWords (midi1) == makeWords (ump::Factory::makeNoteOnV1 (0, 0, 60, 127),
                                                   ump::Factory::makeNoteOffV1 (0, 0, 60, 0),
                                                   ump::Factory::makeNoteOnV1 (0, 0, 62, 0)));
            expectGreaterOrEqual (midi1.getFirstPacketTime(), 100);
            expectLessThan (midi1.getLastPacketTime(), 150);

            const auto midi2 = addKeyboardNotes (ump::PacketProtocol::MIDI_2_0);
            expect (getWords (midi2) == makeWords (ump::Factory::makeNoteOnV2 (0, 0, 60, none, 0xffff, 0),
                                                   ump::Factory::makeNoteOffV2 (0, 0, 60, none, 0, 0),
                                                   ump::Factory::makeNoteOffV2 (0, 0, 62, none, 0, 0)));
            expectGreaterOrEqual (midi2.getFirstPacketTime(), 100);
            expectLessThan (midi2.getLastPacketTime(), 150);
        }

        beginTest ("Keyboard notes are discarded if they aren't added");
        {
            MidiKeyboardState state;
            state.noteOn (1, 60, 1.0f);

            UMPBuffer buffer;
            state.processNextMidiBuffer (buffer, 0, 64, false, ump::PacketProtocol::MIDI_2_0);
            expect (buffer.isEmpty());

            state.processNextMidiBuffer (buffer, 0, 64, true, ump::PacketProtocol::MIDI_2_0);
            expect (buffer.isEmpty());
            expect (state.isNoteOn (1, 60));
        }
    }

    struct Note
    {
        bool isNoteOn = false;
        int channel = 0;
        int noteNumber = 0;
        float velocity = 0.0f;

        bool operator== (const Note& other) const
        {
            return isNoteOn == other.isNoteOn
                && channel == other.channel
                && noteNumber == other.noteNumber
                && exactlyEqual (velocity, other.velocity);
        }
    };

    struct NoteRecorder final : public MidiKeyboardState::Listener
    {
        explicit NoteRecorder (MidiKeyboardState& stateIn)
            : state (stateIn)
        {
            state.addListener (this);
        }

        ~NoteRecorder() override
        {
            state.removeListener (this);
        }

        void handleNoteOn (MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override
        {
            notes.push_back ({ true, midiChannel, midiNoteNumber, velocity });
        }

        void handleNoteOff (MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override
        {
            notes.push_back ({ false, midiChannel, midiNoteNumber, velocity });
        }

        MidiKeyboardState& state;
        std::vector<Note> notes;
    };

    template <size_t numWords>
    static void process (MidiKeyboardState& state, const ump::Packet<numWords>& packet)
    {
        state.processNextMidiEvent (ump::View (packet.data()));
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

static MidiKeyboardStateTest midiKeyboardStateTest;

#endif

} // namespace juce
