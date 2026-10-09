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

class MidiInputConnectionAdapterTests final : public UnitTest
{
public:
    MidiInputConnectionAdapterTests()
        : UnitTest ("MidiInputConnectionAdapter", UnitTestCategories::midi)
    {
    }

    void runTest() override
    {
        using ump::PacketProtocol;

        // A MIDI 1.0 note-on (channel 1, note 60, velocity 100) and the MIDI 2.0 note-on that it
        // translates to
        const Words noteOnV1 { 0x20903c64 };
        const Words noteOnV2 { 0x40903c00, 0xc9240000 };

        testCase ("The MIDI 1.0 connection passes MIDI 1.0 packets on unchanged", [&]
        {
            expect (receiveOnConnection (PacketProtocol::MIDI_1_0, noteOnV1) == Calls { noteOnV1 });
        });

        testCase ("The MIDI 1.0 connection passes MIDI 2.0 packets on as MIDI 1.0", [&]
        {
            expect (receiveOnConnection (PacketProtocol::MIDI_1_0, noteOnV2) == Calls { noteOnV1 });
        });

        testCase ("The MIDI 2.0 connection passes MIDI 1.0 packets on as MIDI 2.0", [&]
        {
            expect (receiveOnConnection (PacketProtocol::MIDI_2_0, noteOnV1) == Calls { noteOnV2 });
        });

        testCase ("The MIDI 2.0 connection passes MIDI 2.0 packets on unchanged", [&]
        {
            expect (receiveOnConnection (PacketProtocol::MIDI_2_0, noteOnV2) == Calls { noteOnV2 });
        });

        testCase ("Adjacent packets for the input's group are passed on in a single call", [&]
        {
            expect (receiveAtOnce ({ 0x20903c64, 0x20803c40 })
                    == Calls { Words { 0x20903c64, 0x20803c40 } });

            expect (receiveAtOnce ({ 0x20903c64, 0x21903c64, 0x20803c40 })
                    == Calls { Words { 0x20903c64 }, Words { 0x20803c40 } });
        });

        testCase ("Packets for other groups are not passed on", [&]
        {
            expect (receiveAtOnce ({ 0x21903c64, 0x22903c64, 0x2f903c64 }).empty());
        });

        testCase ("Utility and stream messages are not passed on", [&]
        {
            // The bits that hold the group in other message types are all zero in these
            // messages, so a check of those bits alone would pass them on to group 0
            const Words words { 0x00000000,                                         // NOOP
                                0x20903c64,
                                0x00201234,                                         // JR timestamp
                                0x20803c40,
                                0xf0000101, 0x0000001f, 0x00000000, 0x00000000 };   // Endpoint discovery

            expect (receiveAtOnce (words) == Calls { Words { 0x20903c64 }, Words { 0x20803c40 } });
        });

        testCase ("Packets are only passed on while the adapter is active", [&]
        {
            ConsumerRecorder recorder;
            MidiInputConnectionAdapter adapter { 0 };
            adapter.addConsumer (recorder);

            consumeAtOnce (adapter, noteOnV1);
            expect (recorder.calls.empty());

            adapter.setActive (true);
            consumeAtOnce (adapter, noteOnV1);
            expect (recorder.calls == Calls { noteOnV1 });

            adapter.setActive (false);
            consumeAtOnce (adapter, noteOnV1);
            expect (recorder.calls == Calls { noteOnV1 });
        });

        testCase ("A consumer is added at most once, and receives nothing after it is removed", [&]
        {
            ConsumerRecorder recorder;
            MidiInputConnectionAdapter adapter { 0 };
            adapter.setActive (true);

            expect (! adapter.hasConsumers());

            adapter.addConsumer (recorder);
            adapter.addConsumer (recorder);
            expect (adapter.hasConsumers());

            consumeAtOnce (adapter, noteOnV1);
            expect (recorder.calls == Calls { noteOnV1 });

            adapter.removeConsumer (recorder);
            expect (! adapter.hasConsumers());

            consumeAtOnce (adapter, noteOnV1);
            expect (recorder.calls == Calls { noteOnV1 });
        });

        testCase ("The MIDI 1.0 connection leaves the bytestream messages unchanged", [&]
        {
            const Words words { 0x20903c00,                 // Note-on with zero velocity
                                0x20b00005,                 // Bank select MSB without an LSB...
                                0x20c00700,                 // ...then a program change
                                0x20b06500,                 // RPN MSB
                                0x20b06400,                 // RPN LSB
                                0x20b00602,                 // Data entry MSB without an LSB
                                0x30160102, 0x03040506,     // SysEx7 start
                                0x10f80000,                 // Timing clock within the SysEx
                                0x30320708, 0x00000000,     // SysEx7 end
                                0x40903c00, 0xc9240000 };   // MIDI 2.0 note-on

            const Messages expected { { 0x90, 0x3c, 0x00 },
                                      { 0xb0, 0x00, 0x05 },
                                      { 0xc0, 0x07 },
                                      { 0xb0, 0x65, 0x00 },
                                      { 0xb0, 0x64, 0x00 },
                                      { 0xb0, 0x06, 0x02 },
                                      { 0xf8 },
                                      { 0xf0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xf7 },
                                      { 0x90, 0x3c, 0x64 } };

            const auto direct = receiveBytes (words, std::nullopt);
            const auto throughConnection = receiveBytes (words, PacketProtocol::MIDI_1_0);

            expect (direct == expected);
            expect (throughConnection == direct);
        });
    }

private:
    using Words = std::vector<uint32_t>;
    using Calls = std::vector<Words>;
    using Messages = std::vector<std::vector<uint8_t>>;

    struct ConsumerRecorder final : public ump::Consumer
    {
        void consume (ump::Iterator b, ump::Iterator e, double) override
        {
            Words words;

            for (const auto& view : makeRange (b, e))
                words.insert (words.end(), view.cbegin(), view.cend());

            calls.push_back (std::move (words));
        }

        Calls calls;
    };

    struct CallbackRecorder final : public MidiInputCallback
    {
        void handleIncomingMidiMessage (MidiInput*, const MidiMessage& message) override
        {
            const auto* data = message.getRawData();
            messages.emplace_back (data, data + message.getRawDataSize());
        }

        Messages messages;
    };

    /*  Passes all of `words` to `consumer` in a single call. */
    static void consumeAtOnce (ump::Consumer& consumer, const Words& words)
    {
        consumer.consume (ump::Iterator { words.data(), words.size() },
                          ump::Iterator { words.data() + words.size(), 0 },
                          0.0);
    }

    /*  Converts each packet of `words` to `protocol`, as a connection using that protocol would,
        and passes the results to `consumer` one packet per call.
    */
    static void consumeThroughConnection (ump::Consumer& consumer, ump::PacketProtocol protocol, const Words& words)
    {
        ump::GenericUMPConverter converter { protocol };

        converter.convert (ump::Iterator { words.data(), words.size() },
                           ump::Iterator { words.data() + words.size(), 0 },
                           [&] (const ump::View& view)
        {
            const ump::Iterator packet { view.data(), view.size() };
            consumer.consume (packet, std::next (packet), 0.0);
        });
    }

    /*  Returns the calls that a consumer of an active adapter for group 0 receives when `words`
        arrive on a connection that uses `protocol`.
    */
    static Calls receiveOnConnection (ump::PacketProtocol protocol, const Words& words)
    {
        ConsumerRecorder recorder;
        MidiInputConnectionAdapter adapter { 0 };
        adapter.addConsumer (recorder);
        adapter.setActive (true);

        consumeThroughConnection (adapter, protocol, words);
        return recorder.calls;
    }

    /*  Returns the calls that a consumer of an active adapter for group 0 receives when `words`
        are passed to the adapter in a single call.
    */
    static Calls receiveAtOnce (const Words& words)
    {
        ConsumerRecorder recorder;
        MidiInputConnectionAdapter adapter { 0 };
        adapter.addConsumer (recorder);
        adapter.setActive (true);

        consumeAtOnce (adapter, words);
        return recorder.calls;
    }

    /*  Returns the bytestream messages that a MidiInputCallback listener of an active adapter for
        group 0 receives when `words` arrive. If `protocol` is set, the words first pass through
        the conversion of a connection that uses that protocol. Otherwise, they are passed to the
        adapter unchanged.
    */
    static Messages receiveBytes (const Words& words, std::optional<ump::PacketProtocol> protocol)
    {
        CallbackRecorder recorder;
        WaitFreeListeners<MidiInputCallback> callbacks;
        callbacks.add (recorder);

        MidiInputConnectionAdapter adapter { 0, nullptr, callbacks };
        adapter.setActive (true);

        if (protocol.has_value())
            consumeThroughConnection (adapter, *protocol, words);
        else
            consumeAtOnce (adapter, words);

        return recorder.messages;
    }
};

static MidiInputConnectionAdapterTests midiInputConnectionAdapterTests;

} // namespace juce
