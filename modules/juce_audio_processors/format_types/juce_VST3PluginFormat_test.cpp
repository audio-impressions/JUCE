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

#include <juce_audio_processors_headless/format_types/juce_VST3Headers.h>
#include <juce_audio_processors_headless/format_types/juce_VST3Utilities.h>
#include <juce_audio_processors_headless/format_types/juce_VST3Common.h>

namespace juce
{

class VST3PluginFormatTests final : public UnitTest
{
public:
    VST3PluginFormatTests()
        : UnitTest ("VST3 Hosting", UnitTestCategories::audioProcessors)
    {
    }

    void runTest() override
    {
        beginTest ("ChannelMapping for a stereo bus performs no remapping");
        {
            ChannelMapping map (AudioChannelSet::stereo());
            expect (map.size() == 2);

            expect (map.getJuceChannelForVst3Channel (0) == 0); // L -> left
            expect (map.getJuceChannelForVst3Channel (1) == 1); // R -> right
        }

        beginTest ("ChannelMapping for a k91_6 bus remaps the channels appropriately");
        {
            ChannelMapping map (AudioChannelSet::create9point1point6ITU());
            expect (map.size() == 16);

            // VST3 order is:
            //      L
            //      R
            //      C
            //      Lfe
            //      Ls
            //      Rs
            //      Lc
            //      Rc
            //      Sl
            //      Sr
            //      Tfl
            //      Tfr
            //      Trl
            //      Trr
            //      Tsl
            //      Tsr
            // JUCE order is:
            //      left
            //      right
            //      centre
            //      LFE
            //      leftSurround
            //      rightSurround
            //      leftCentre
            //      rightCentre
            //      leftSurroundSide
            //      rightSurroundSide
            //      topFrontLeft
            //      topRearRight
            //      topRearLeft
            //      topRearRight
            //      topSideLeft
            //      topSideRight

            expect (map.getJuceChannelForVst3Channel (0)  == 0);  // L   -> left
            expect (map.getJuceChannelForVst3Channel (1)  == 1);  // R   -> right
            expect (map.getJuceChannelForVst3Channel (2)  == 2);  // C   -> centre
            expect (map.getJuceChannelForVst3Channel (3)  == 3);  // Lfe -> LFE
            expect (map.getJuceChannelForVst3Channel (4)  == 4);  // Ls  -> leftSurround
            expect (map.getJuceChannelForVst3Channel (5)  == 5);  // Rs  -> rightSurround
            expect (map.getJuceChannelForVst3Channel (6)  == 6);  // Lc  -> leftCentre
            expect (map.getJuceChannelForVst3Channel (7)  == 7);  // Rc  -> rightCentre
            expect (map.getJuceChannelForVst3Channel (8)  == 8);  // Sl  -> leftSurroundSide
            expect (map.getJuceChannelForVst3Channel (9)  == 9);  // Sr  -> rightSurroundSide
            expect (map.getJuceChannelForVst3Channel (10) == 10); // Tfl -> topFrontLeft
            expect (map.getJuceChannelForVst3Channel (11) == 11); // Tfr -> topFrontRight
            expect (map.getJuceChannelForVst3Channel (12) == 12); // Trl -> topRearLeft
            expect (map.getJuceChannelForVst3Channel (13) == 13); // Trr -> topRearRight
            expect (map.getJuceChannelForVst3Channel (14) == 14); // Tsl -> topSideLeft
            expect (map.getJuceChannelForVst3Channel (15) == 15); // Tsr -> topSideRight
        }

        beginTest ("ChannelMapping for a k91_6_W bus remaps the channels appropriately");
        {
            ChannelMapping map (AudioChannelSet::create9point1point6());
            expect (map.size() == 16);

            // VST3 order is:
            //      L
            //      R
            //      C
            //      Lfe
            //      Ls
            //      Rs
            //      Sl
            //      Sr
            //      Tfl
            //      Tfr
            //      Trl
            //      Trr
            //      Tsl
            //      Tsr
            //      Lw
            //      Rw
            // JUCE order is:
            //      Left
            //      Right
            //      Centre
            //      LFE
            //      Left Surround Side
            //      Right Surround Side
            //      Top Front Left
            //      Top Front Right
            //      Top Rear Left
            //      Top Rear Right
            //      Left Surround Rear
            //      Right Surround Rear
            //      Wide Left
            //      Wide Right
            //      Top Side Left
            //      Top Side Right

            expect (map.getJuceChannelForVst3Channel (0)  == 0);  // L   -> left
            expect (map.getJuceChannelForVst3Channel (1)  == 1);  // R   -> right
            expect (map.getJuceChannelForVst3Channel (2)  == 2);  // C   -> centre
            expect (map.getJuceChannelForVst3Channel (3)  == 3);  // Lfe -> LFE
            expect (map.getJuceChannelForVst3Channel (4)  == 10); // Ls  -> leftSurroundRear
            expect (map.getJuceChannelForVst3Channel (5)  == 11); // Rs  -> rightSurroundRear
            expect (map.getJuceChannelForVst3Channel (6)  == 4);  // Sl  -> leftSurroundSide
            expect (map.getJuceChannelForVst3Channel (7)  == 5);  // Sr  -> rightSurroundSide
            expect (map.getJuceChannelForVst3Channel (8)  == 6);  // Tfl -> topFrontLeft
            expect (map.getJuceChannelForVst3Channel (9)  == 7);  // Tfr -> topFrontRight
            expect (map.getJuceChannelForVst3Channel (10) == 8);  // Trl -> topRearLeft
            expect (map.getJuceChannelForVst3Channel (11) == 9);  // Trr -> topRearRight
            expect (map.getJuceChannelForVst3Channel (12) == 14); // Tsl -> topSideLeft
            expect (map.getJuceChannelForVst3Channel (13) == 15); // Tsr -> topSideRight
            expect (map.getJuceChannelForVst3Channel (14) == 12); // Lw  -> wideLeft
            expect (map.getJuceChannelForVst3Channel (15) == 13); // Lw  -> wideRight
        }

        const auto blockSize = 128;

        beginTest ("If the host provides more buses than the plugin knows about, the remapped buffer is silent and uses only internal channels");
        {
            ClientBufferMapperData<float> remapper;
            remapper.prepare (2, blockSize * 2);

            const std::vector<DynamicChannelMapping> emptyBuses;
            const std::vector<DynamicChannelMapping> stereoBus { DynamicChannelMapping { AudioChannelSet::stereo() } };

            TestBuffers testBuffers { blockSize };

            auto ins  = MultiBusBuffers{}.withBus (testBuffers, 2).withBus (testBuffers, 1);
            auto outs = MultiBusBuffers{}.withBus (testBuffers, 2).withBus (testBuffers, 1);
            auto data = makeProcessData (blockSize, ins, outs);

            for (const auto& config : { Config { stereoBus, stereoBus }, Config { emptyBuses, stereoBus }, Config { stereoBus, emptyBuses } })
            {
                testBuffers.init();

                {
                    const ClientRemappedBuffer<float> scopedBuffer { remapper, &config.ins, &config.outs, data };
                    auto& remapped = scopedBuffer.buffer;

                    expect (remapped.getNumChannels() == config.getNumChannels());
                    expect (remapped.getNumSamples() == blockSize);

                    for (auto i = 0; i < remapped.getNumChannels(); ++i)
                        expect (allMatch (remapped, i, 0.0f));
                }

                expect (! testBuffers.isClear (0));
                expect (! testBuffers.isClear (1));
                expect (! testBuffers.isClear (2));
                expect (testBuffers.isClear (3));
                expect (testBuffers.isClear (4));
                expect (testBuffers.isClear (5));
            }
        }

        beginTest ("If the host provides fewer buses than the plugin knows about, the remapped buffer is silent and uses only internal channels");
        {
            ClientBufferMapperData<float> remapper;
            remapper.prepare (3, blockSize * 2);

            const std::vector<DynamicChannelMapping> noBus;
            const std::vector<DynamicChannelMapping> oneBus { DynamicChannelMapping { AudioChannelSet::mono() } };
            const std::vector<DynamicChannelMapping> twoBuses { DynamicChannelMapping { AudioChannelSet::mono() },
                                                                DynamicChannelMapping { AudioChannelSet::stereo() } };

            TestBuffers testBuffers { blockSize };

            auto ins  = MultiBusBuffers{}.withBus (testBuffers, 1);
            auto outs = MultiBusBuffers{}.withBus (testBuffers, 1);
            auto data = makeProcessData (blockSize, ins, outs);

            for (const auto& config : { Config { noBus, twoBuses },
                                        Config { twoBuses, noBus },
                                        Config { oneBus, twoBuses },
                                        Config { twoBuses, oneBus },
                                        Config { twoBuses, twoBuses } })
            {
                testBuffers.init();

                {
                    const ClientRemappedBuffer<float> scopedBuffer { remapper, &config.ins, &config.outs, data };
                    auto& remapped = scopedBuffer.buffer;

                    expect (remapped.getNumChannels() == config.getNumChannels());
                    expect (remapped.getNumSamples() == blockSize);

                    // The remapped buffer will only be cleared if the host's input layout does not
                    // match the client's input layout.
                    if (config.ins.size() != 1)
                        for (auto i = 0; i < remapped.getNumChannels(); ++i)
                            expect (allMatch (remapped, i, 0.0f));
                }

                expect (! testBuffers.isClear (0));
                expect (testBuffers.isClear (1));
            }
        }

        beginTest ("If the host channel count on any bus is incorrect, the remapped buffer is silent and uses only internal channels");
        {
            ClientBufferMapperData<float> remapper;
            remapper.prepare (3, blockSize * 2);

            const std::vector<DynamicChannelMapping> monoBus { DynamicChannelMapping { AudioChannelSet::mono() } };
            const std::vector<DynamicChannelMapping> stereoBus { DynamicChannelMapping { AudioChannelSet::stereo() } };

            TestBuffers testBuffers { blockSize };

            auto ins  = MultiBusBuffers{}.withBus (testBuffers, 1);
            auto outs = MultiBusBuffers{}.withBus (testBuffers, 2);
            auto data = makeProcessData (blockSize, ins, outs);

            for (const auto& config : { Config { stereoBus, monoBus },
                                        Config { stereoBus, stereoBus },
                                        Config { monoBus, monoBus } })
            {
                testBuffers.init();

                {
                    const ClientRemappedBuffer<float> scopedBuffer { remapper, &config.ins, &config.outs, data };
                    auto& remapped = scopedBuffer.buffer;

                    expect (remapped.getNumChannels() == config.getNumChannels());
                    expect (remapped.getNumSamples() == blockSize);

                    // The remapped buffer will only be cleared if the host's input layout does not
                    // match the client's input layout.
                    if (config.ins.front().size() != 1)
                        for (auto i = 0; i < remapped.getNumChannels(); ++i)
                            expect (allMatch (remapped, i, 0.0f));
                }

                expect (! testBuffers.isClear (0));
                expect (testBuffers.isClear (1));
                expect (testBuffers.isClear (2));
            }
        }

        beginTest ("A layout with more output channels than input channels leaves unused inputs untouched");
        {
            ClientBufferMapperData<float> remapper;
            remapper.prepare (20, blockSize * 2);

            const Config config { { DynamicChannelMapping { AudioChannelSet::mono() },
                                    DynamicChannelMapping { AudioChannelSet::create5point1() } },
                                  { DynamicChannelMapping { AudioChannelSet::stereo() },
                                    DynamicChannelMapping { AudioChannelSet::create7point1() } } };

            TestBuffers testBuffers { blockSize };

            auto ins  = MultiBusBuffers{}.withBus (testBuffers, 1).withBus (testBuffers, 6);
            auto outs = MultiBusBuffers{}.withBus (testBuffers, 2).withBus (testBuffers, 8);

            auto data = makeProcessData (blockSize, ins, outs);

            testBuffers.init();

            {
                ClientRemappedBuffer<float> scopedBuffer { remapper, &config.ins, &config.outs, data };
                auto& remapped = scopedBuffer.buffer;

                expect (remapped.getNumChannels() == 10);

                // Data from the input channels is copied to the correct channels of the remapped buffer
                expect (allMatch (remapped, 0, 1.0f));
                expect (allMatch (remapped, 1, 2.0f));
                expect (allMatch (remapped, 2, 3.0f));
                expect (allMatch (remapped, 3, 4.0f));
                expect (allMatch (remapped, 4, 5.0f));
                expect (allMatch (remapped, 5, 6.0f));
                expect (allMatch (remapped, 6, 7.0f));
                // The remaining channels are output-only, so they may contain any data

                // Write some data to the buffer in JUCE layout
                for (auto i = 0; i < remapped.getNumChannels(); ++i)
                {
                    auto* ptr = remapped.getWritePointer (i);
                    std::fill (ptr, ptr + remapped.getNumSamples(), (float) i);
                }
            }

            // Channels are copied back to the correct output buffer
            expect (channelStartsWithValue (data.outputs[0], 0, 0.0f));
            expect (channelStartsWithValue (data.outputs[0], 1, 1.0f));

            expect (channelStartsWithValue (data.outputs[1], 0, 2.0f));
            expect (channelStartsWithValue (data.outputs[1], 1, 3.0f));
            expect (channelStartsWithValue (data.outputs[1], 2, 4.0f));
            expect (channelStartsWithValue (data.outputs[1], 3, 5.0f));
            expect (channelStartsWithValue (data.outputs[1], 4, 8.0f));  // JUCE surround side -> VST3 surround side
            expect (channelStartsWithValue (data.outputs[1], 5, 9.0f));
            expect (channelStartsWithValue (data.outputs[1], 6, 6.0f));  // JUCE surround rear -> VST3 surround rear
            expect (channelStartsWithValue (data.outputs[1], 7, 7.0f));
        }

        beginTest ("A layout with more input channels than output channels doesn't attempt to output any input channels");
        {
            ClientBufferMapperData<float> remapper;
            remapper.prepare (15, blockSize * 2);

            const Config config { { DynamicChannelMapping { AudioChannelSet::create7point1point6() },
                                    DynamicChannelMapping { AudioChannelSet::mono() } },
                                  { DynamicChannelMapping { AudioChannelSet::createLCRS() },
                                    DynamicChannelMapping { AudioChannelSet::stereo() } } };

            TestBuffers testBuffers { blockSize };

            auto ins  = MultiBusBuffers{}.withBus (testBuffers, 14).withBus (testBuffers, 1);
            auto outs = MultiBusBuffers{}.withBus (testBuffers, 4) .withBus (testBuffers, 2);

            auto data = makeProcessData (blockSize, ins, outs);

            testBuffers.init();

            {
                ClientRemappedBuffer<float> scopedBuffer { remapper, &config.ins, &config.outs, data };
                auto& remapped = scopedBuffer.buffer;

                expect (remapped.getNumChannels() == 15);

                // Data from the input channels is copied to the correct channels of the remapped buffer
                expect (allMatch (remapped, 0,   1.0f));
                expect (allMatch (remapped, 1,   2.0f));
                expect (allMatch (remapped, 2,   3.0f));
                expect (allMatch (remapped, 3,   4.0f));
                expect (allMatch (remapped, 4,   7.0f));
                expect (allMatch (remapped, 5,   8.0f));
                expect (allMatch (remapped, 6,   9.0f));
                expect (allMatch (remapped, 7,  10.0f));
                expect (allMatch (remapped, 8,  11.0f));
                expect (allMatch (remapped, 9,  12.0f));
                expect (allMatch (remapped, 10,  5.0f));
                expect (allMatch (remapped, 11,  6.0f));
                expect (allMatch (remapped, 12, 13.0f));
                expect (allMatch (remapped, 13, 14.0f));
                expect (allMatch (remapped, 14, 15.0f));

                // Write some data to the buffer in JUCE layout
                for (auto i = 0; i < remapped.getNumChannels(); ++i)
                {
                    auto* ptr = remapped.getWritePointer (i);
                    std::fill (ptr, ptr + remapped.getNumSamples(), (float) i);
                }
            }

            // Channels are copied back to the correct output buffer
            expect (channelStartsWithValue (data.outputs[0], 0, 0.0f));
            expect (channelStartsWithValue (data.outputs[0], 1, 1.0f));
            expect (channelStartsWithValue (data.outputs[0], 2, 2.0f));
            expect (channelStartsWithValue (data.outputs[0], 3, 3.0f));

            expect (channelStartsWithValue (data.outputs[1], 0, 4.0f));
            expect (channelStartsWithValue (data.outputs[1], 1, 5.0f));
        }

        beginTest ("Inactive buses are ignored");
        {
            ClientBufferMapperData<float> remapper;
            remapper.prepare (18, blockSize * 2);

            Config config { { DynamicChannelMapping { AudioChannelSet::create7point1point6() },
                              DynamicChannelMapping { AudioChannelSet::mono(), false },
                              DynamicChannelMapping { AudioChannelSet::quadraphonic() },
                              DynamicChannelMapping { AudioChannelSet::mono(), false } },
                            { DynamicChannelMapping { AudioChannelSet::create5point0(), false },
                              DynamicChannelMapping { AudioChannelSet::createLCRS() },
                              DynamicChannelMapping { AudioChannelSet::stereo() } } };

            config.ins[1].setHostActive (false);
            config.ins[3].setHostActive (false);

            TestBuffers testBuffers { blockSize };

            // The host doesn't need to provide trailing buses that are inactive, as long as the
            // client knows those buses are inactive.
            auto ins  = MultiBusBuffers{}.withBus (testBuffers, 14).withBus (testBuffers, 1).withBus (testBuffers, 4);
            auto outs = MultiBusBuffers{}.withBus (testBuffers, 5) .withBus (testBuffers, 4).withBus (testBuffers, 2);

            auto data = makeProcessData (blockSize, ins, outs);

            testBuffers.init();

            {
                ClientRemappedBuffer<float> scopedBuffer { remapper, &config.ins, &config.outs, data };
                auto& remapped = scopedBuffer.buffer;

                expect (remapped.getNumChannels() == 18);

                // Data from the input channels is copied to the correct channels of the remapped buffer
                expect (allMatch (remapped, 0,   1.0f));
                expect (allMatch (remapped, 1,   2.0f));
                expect (allMatch (remapped, 2,   3.0f));
                expect (allMatch (remapped, 3,   4.0f));
                expect (allMatch (remapped, 4,   7.0f));
                expect (allMatch (remapped, 5,   8.0f));
                expect (allMatch (remapped, 6,   9.0f));
                expect (allMatch (remapped, 7,  10.0f));
                expect (allMatch (remapped, 8,  11.0f));
                expect (allMatch (remapped, 9,  12.0f));
                expect (allMatch (remapped, 10,  5.0f));
                expect (allMatch (remapped, 11,  6.0f));
                expect (allMatch (remapped, 12, 13.0f));
                expect (allMatch (remapped, 13, 14.0f));

                expect (allMatch (remapped, 14, 16.0f));
                expect (allMatch (remapped, 15, 17.0f));
                expect (allMatch (remapped, 16, 18.0f));
                expect (allMatch (remapped, 17, 19.0f));

                // Write some data to the buffer in JUCE layout
                for (auto i = 0; i < remapped.getNumChannels(); ++i)
                {
                    auto* ptr = remapped.getWritePointer (i);
                    std::fill (ptr, ptr + remapped.getNumSamples(), (float) i);
                }
            }

            // All channels on the first output bus should be cleared, because the plugin
            // thinks that this bus is inactive.
            expect (channelStartsWithValue (data.outputs[0], 0, 0.0f));
            expect (channelStartsWithValue (data.outputs[0], 1, 0.0f));
            expect (channelStartsWithValue (data.outputs[0], 2, 0.0f));
            expect (channelStartsWithValue (data.outputs[0], 3, 0.0f));
            expect (channelStartsWithValue (data.outputs[0], 4, 0.0f));

            // Remaining channels should be copied back as normal
            expect (channelStartsWithValue (data.outputs[1], 0, 0.0f));
            expect (channelStartsWithValue (data.outputs[1], 1, 1.0f));
            expect (channelStartsWithValue (data.outputs[1], 2, 2.0f));
            expect (channelStartsWithValue (data.outputs[1], 3, 3.0f));

            expect (channelStartsWithValue (data.outputs[2], 0, 4.0f));
            expect (channelStartsWithValue (data.outputs[2], 1, 5.0f));
        }

        beginTest ("Null pointers are allowed on inactive buses provided to clients");
        {
            ClientBufferMapperData<float> remapper;
            remapper.prepare (8, blockSize * 2);

            const std::vector<ChannelMapping> emptyBuses;
            const std::vector<ChannelMapping> stereoBus { ChannelMapping { AudioChannelSet::stereo() } };

            Config config { { DynamicChannelMapping { AudioChannelSet::stereo() },
                              DynamicChannelMapping { AudioChannelSet::quadraphonic(), false },
                              DynamicChannelMapping { AudioChannelSet::stereo() } },
                            { DynamicChannelMapping { AudioChannelSet::quadraphonic() },
                              DynamicChannelMapping { AudioChannelSet::stereo(), false },
                              DynamicChannelMapping { AudioChannelSet::quadraphonic() } } };

            config.ins[1].setHostActive (false);
            config.outs[1].setHostActive (false);

            TestBuffers testBuffers { blockSize };

            auto ins  = MultiBusBuffers{}.withBus (testBuffers, 2).withBus (testBuffers, 4).withBus (testBuffers, 2);
            auto outs = MultiBusBuffers{}.withBus (testBuffers, 4).withBus (testBuffers, 2).withBus (testBuffers, 4);

            auto data = makeProcessData (blockSize, ins, outs);

            for (auto i = 0; i < 4; ++i)
                data.inputs [1].channelBuffers32[i] = nullptr;

            for (auto i = 0; i < 2; ++i)
                data.outputs[1].channelBuffers32[i] = nullptr;

            testBuffers.init();

            {
                ClientRemappedBuffer<float> scopedBuffer { remapper, &config.ins, &config.outs, data };
                auto& remapped = scopedBuffer.buffer;

                expect (remapped.getNumChannels() == 8);

                expect (allMatch (remapped, 0,   1.0f));
                expect (allMatch (remapped, 1,   2.0f));
                // skip 4 inactive channels
                expect (allMatch (remapped, 2,   7.0f));
                expect (allMatch (remapped, 3,   8.0f));

                // Write some data to the buffer in JUCE layout
                for (auto i = 0; i < remapped.getNumChannels(); ++i)
                {
                    auto* ptr = remapped.getWritePointer (i);
                    std::fill (ptr, ptr + remapped.getNumSamples(), (float) i);
                }
            }

            expect (channelStartsWithValue (data.outputs[0], 0, 0.0f));
            expect (channelStartsWithValue (data.outputs[0], 1, 1.0f));
            expect (channelStartsWithValue (data.outputs[0], 2, 2.0f));
            expect (channelStartsWithValue (data.outputs[0], 3, 3.0f));

            expect (channelStartsWithValue (data.outputs[2], 0, 4.0f));
            expect (channelStartsWithValue (data.outputs[2], 1, 5.0f));
            expect (channelStartsWithValue (data.outputs[2], 2, 6.0f));
            expect (channelStartsWithValue (data.outputs[2], 3, 7.0f));
        }

        beginTest ("HostBufferMapper reorders channels correctly");
        {
            HostBufferMapper mapper;

            {
                mapper.prepare ({ ChannelMapping { AudioChannelSet::stereo() },
                                  ChannelMapping { AudioChannelSet::create7point1point2() },
                                  ChannelMapping { AudioChannelSet::create9point1point6(), false },
                                  ChannelMapping { AudioChannelSet::createLCRS() } });
                AudioBuffer<float> hostBuffer (16, blockSize);
                const auto* clientBuffers = mapper.getVst3LayoutForJuceBuffer (hostBuffer);

                expect (clientBuffers[0].numChannels == 2);
                expect (clientBuffers[1].numChannels == 10);
                // Even though it's disabled, this bus should still have the correct channel count
                expect (clientBuffers[2].numChannels == 16);
                expect (clientBuffers[3].numChannels == 4);

                expect (clientBuffers[0].channelBuffers32[0]  == hostBuffer.getReadPointer (0));
                expect (clientBuffers[0].channelBuffers32[1]  == hostBuffer.getReadPointer (1));

                expect (clientBuffers[1].channelBuffers32[0]  == hostBuffer.getReadPointer (2));
                expect (clientBuffers[1].channelBuffers32[1]  == hostBuffer.getReadPointer (3));
                expect (clientBuffers[1].channelBuffers32[2]  == hostBuffer.getReadPointer (4));
                expect (clientBuffers[1].channelBuffers32[3]  == hostBuffer.getReadPointer (5));
                expect (clientBuffers[1].channelBuffers32[4]  == hostBuffer.getReadPointer (8));
                expect (clientBuffers[1].channelBuffers32[5]  == hostBuffer.getReadPointer (9));
                expect (clientBuffers[1].channelBuffers32[6]  == hostBuffer.getReadPointer (6));
                expect (clientBuffers[1].channelBuffers32[7]  == hostBuffer.getReadPointer (7));
                expect (clientBuffers[1].channelBuffers32[8]  == hostBuffer.getReadPointer (10));
                expect (clientBuffers[1].channelBuffers32[9]  == hostBuffer.getReadPointer (11));

                for (auto i = 0; i < clientBuffers[2].numChannels; ++i)
                    expect (clientBuffers[2].channelBuffers32[i] == nullptr);

                expect (clientBuffers[3].channelBuffers32[0]  == hostBuffer.getReadPointer (12));
                expect (clientBuffers[3].channelBuffers32[1]  == hostBuffer.getReadPointer (13));
                expect (clientBuffers[3].channelBuffers32[2]  == hostBuffer.getReadPointer (14));
                expect (clientBuffers[3].channelBuffers32[3]  == hostBuffer.getReadPointer (15));
            }

            {
                mapper.prepare ({ ChannelMapping { AudioChannelSet::mono() },
                                  ChannelMapping { AudioChannelSet::mono(), false },
                                  ChannelMapping { AudioChannelSet::mono() },
                                  ChannelMapping { AudioChannelSet::mono(), false } });
                AudioBuffer<double> hostBuffer (2, blockSize);
                const auto* clientBuffers = mapper.getVst3LayoutForJuceBuffer (hostBuffer);

                expect (clientBuffers[0].numChannels == 1);
                expect (clientBuffers[1].numChannels == 1);
                expect (clientBuffers[2].numChannels == 1);
                expect (clientBuffers[3].numChannels == 1);

                expect (clientBuffers[0].channelBuffers64[0] == hostBuffer.getReadPointer (0));
                expect (clientBuffers[1].channelBuffers64[0] == nullptr);
                expect (clientBuffers[2].channelBuffers64[0] == hostBuffer.getReadPointer (1));
                expect (clientBuffers[3].channelBuffers64[0] == nullptr);
            }
        }

        beginTest ("Speaker layout conversions");
        {
            using namespace Steinberg::Vst::SpeakerArr;

            for (const auto& [channelSet, arr] : { std::tuple (AudioChannelSet::ambisonic (1), kAmbi1stOrderACN),
                                                   std::tuple (AudioChannelSet::ambisonic (2), kAmbi2cdOrderACN),
                                                   std::tuple (AudioChannelSet::ambisonic (3), kAmbi3rdOrderACN),
                                                   std::tuple (AudioChannelSet::ambisonic (4), kAmbi4thOrderACN),
                                                   std::tuple (AudioChannelSet::ambisonic (5), kAmbi5thOrderACN),
                                                   std::tuple (AudioChannelSet::ambisonic (6), kAmbi6thOrderACN),
                                                   std::tuple (AudioChannelSet::ambisonic (7), kAmbi7thOrderACN), })
            {
                expect (getVst3SpeakerArrangement (channelSet) == arr);
                expect (channelSet == getChannelSetForSpeakerArrangement (arr));
            }
        }

        beginTest ("HostToClientParamQueue::append uses a node from storage");
        {
            HostToClientParamQueue::NodeStorage storage;
            storage.push_back (HostToClientParamQueue::makeNode());

            HostToClientParamQueue queue { {}, {}, storage };
            queue.append ({ 100, 0.5f });

            expect (queue.getPointCount() == 1);
            expect (storage.empty());

            queue.clear();

            expect (queue.getPointCount() == 0);
            expect (storage.size() == 1);
        }

        beginTest ("If there are no nodes in storage, HostToClientParamQueue::append creates a new node");
        {
            HostToClientParamQueue::NodeStorage storage;

            HostToClientParamQueue queue { {}, {}, storage };
            queue.append ({ 100, 0.5f });

            expect (queue.getPointCount() == 1);
            expect (storage.empty());

            queue.clear();

            expect (queue.getPointCount() == 0);
            expect (storage.size() == 1);
        }

        beginTest ("UMPConverter builds MIDI 1.0 packets with the values that a MidiBuffer receives");
        {
            using namespace Steinberg::Vst;

            const std::vector<uint8> sysEx { 0xf0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xf7 };
            const std::vector<Event> events { makeNoteOn (2, 60, 0.5f, 7, 10),
                                              makePolyPressure (2, 60, 0.25f, 15),
                                              makeNoteOff (2, 60, 1.0f, 7, 20),
                                              makeLegacyController (3, 74, 100, 0, 25),
                                              makeLegacyController (4, kPitchBend, 0x10, 0x40, 26),
                                              makeSysEx (sysEx, 30) };

            expect (toPackets (ump::PacketProtocol::MIDI_1_0, events).data
                    == convertMidiBuffer (events, ump::PacketProtocol::MIDI_1_0).data);
        }

        beginTest ("UMPConverter keeps a MIDI 1.0 note-on with a very low velocity as a note-on");
        {
            UMPBuffer expected;
            addPacket (expected, ump::Factory::makeNoteOnV1 (0, 0, 60, 1), 0);

            const auto packets = toPackets (ump::PacketProtocol::MIDI_1_0, { makeNoteOn (0, 60, 0.001f, -1, 0) });
            expect (packets.data == expected.data);
        }

        beginTest ("UMPConverter builds MIDI 2.0 notes and poly pressure with their full resolution");
        {
            constexpr auto none = ump::Factory::NoteAttributeKind::none;

            const auto packets = toPackets (ump::PacketProtocol::MIDI_2_0, { makeNoteOn (0, 60, 0.5f, -1, 0),
                                                                             makeNoteOn (15, 127, 1.0f, -1, 1),
                                                                             makeNoteOn (1, 0, 0.0f, -1, 2),
                                                                             makePolyPressure (0, 60, 0.5f, 3),
                                                                             makePolyPressure (0, 60, 1.0f, 4),
                                                                             makeNoteOff (0, 60, 0.25f, -1, 5) });

            UMPBuffer expected;
            addPacket (expected, ump::Factory::makeNoteOnV2 (0, 0, 60, none, 0x8000, 0), 0);
            addPacket (expected, ump::Factory::makeNoteOnV2 (0, 15, 127, none, 0xffff, 0), 1);
            addPacket (expected, ump::Factory::makeNoteOnV2 (0, 1, 0, none, 0, 0), 2);
            addPacket (expected, ump::Factory::makePolyPressureV2 (0, 0, 60, 0x80000000), 3);
            addPacket (expected, ump::Factory::makePolyPressureV2 (0, 0, 60, 0xffffffff), 4);
            addPacket (expected, ump::Factory::makeNoteOffV2 (0, 0, 60, none, 0x4000, 0), 5);

            expect (packets.data == expected.data);
        }

        beginTest ("UMPConverter clamps values that are out of range");
        {
            constexpr auto none = ump::Factory::NoteAttributeKind::none;

            const std::vector<Steinberg::Vst::Event> events { makeNoteOn (-1, -5, -0.5f, -1, 0),
                                                              makeNoteOn (16, 200, 1.5f, -1, 1),
                                                              makePolyPressure (3, 64, 2.0f, 2),
                                                              makePolyPressure (3, 64, -1.0f, 3) };

            UMPBuffer expected1;
            addPacket (expected1, ump::Factory::makeNoteOnV1 (0, 0, 0, 1), 0);
            addPacket (expected1, ump::Factory::makeNoteOnV1 (0, 15, 127, 127), 1);
            addPacket (expected1, ump::Factory::makePolyPressureV1 (0, 3, 64, 127), 2);
            addPacket (expected1, ump::Factory::makePolyPressureV1 (0, 3, 64, 0), 3);
            addPacket (expected1, ump::Factory::makeControlChangeV1 (0, 0, 7, 127), 4);
            addPacket (expected1, ump::Factory::makeControlChangeV1 (0, 15, 7, 0), 5);

            UMPBuffer expected2;
            addPacket (expected2, ump::Factory::makeNoteOnV2 (0, 0, 0, none, 0, 0), 0);
            addPacket (expected2, ump::Factory::makeNoteOnV2 (0, 15, 127, none, 0xffff, 0), 1);
            addPacket (expected2, ump::Factory::makePolyPressureV2 (0, 3, 64, 0xffffffff), 2);
            addPacket (expected2, ump::Factory::makePolyPressureV2 (0, 3, 64, 0), 3);
            addPacket (expected2, ump::Factory::makeControlChangeV2 (0, 0, 7, 0xffffffff), 4);
            addPacket (expected2, ump::Factory::makeControlChangeV2 (0, 15, 7, 0), 5);

            for (const auto& [protocol, expected] : { std::tuple (ump::PacketProtocol::MIDI_1_0, &expected1),
                                                      std::tuple (ump::PacketProtocol::MIDI_2_0, &expected2) })
            {
                MidiEventList::UMPConverter converter;
                converter.reset (protocol);

                auto packets = toPackets (converter, events);
                converter.addController (packets, 0, 7, 1.5, 4);
                converter.addController (packets, 17, 7, -0.5, 5);

                expect (packets.data == expected->data);
            }
        }

        beginTest ("UMPConverter sends note expression to a MIDI 2.0 processor as per-note controllers");
        {
            using namespace Steinberg::Vst;

            constexpr auto none = ump::Factory::NoteAttributeKind::none;
            const auto oneSemitoneUp = 0.5 + 1.0 / 240.0;

            const auto packets = toPackets (ump::PacketProtocol::MIDI_2_0,
                                            { makeNoteOn (1, 64, 1.0f, 5, 0),
                                              makeNoteExpression (kVolumeTypeID, 5, 0.25, 1),
                                              makeNoteExpression (kPanTypeID, 5, 0.5, 2),
                                              makeNoteExpression (kExpressionTypeID, 5, 1.0, 3),
                                              makeNoteExpression (kBrightnessTypeID, 5, 0.0, 4),
                                              makeNoteExpression (kVibratoTypeID, 5, 0.75, 5),
                                              makeNoteExpression (kTuningTypeID, 5, oneSemitoneUp, 6),
                                              makeNoteExpression (kTuningTypeID, 5, 0.0, 7),
                                              makeNoteExpression (kTuningTypeID, 5, 1.0, 8),
                                              makeNoteExpression (kCustomStart, 5, 0.5, 9),
                                              makeNoteExpression (kVolumeTypeID, 6, 0.5, 9),
                                              makeNoteOff (1, 64, 0.0f, 5, 10),
                                              makeNoteExpression (kVolumeTypeID, 5, 0.5, 11) });

            const auto makeController = [] (uint8_t index, uint32_t data)
            {
                return ump::Factory::makeRegisteredPerNoteControllerV2 (0, 1, 64, index, data);
            };

            UMPBuffer expected;
            addPacket (expected, ump::Factory::makeNoteOnV2 (0, 1, 64, none, 0xffff, 0), 0);
            addPacket (expected, makeController (7, 0x40000000), 1);
            addPacket (expected, makeController (10, 0x80000000), 2);
            addPacket (expected, makeController (11, 0xffffffff), 3);
            addPacket (expected, makeController (74, 0), 4);
            addPacket (expected, makeController (77, 0xbfffffff), 5);
            addPacket (expected, makeController (3, 65u << 25), 6);
            addPacket (expected, makeController (3, 0), 7);
            addPacket (expected, makeController (3, 0xffffffff), 8);
            addPacket (expected, ump::Factory::makeNoteOffV2 (0, 1, 64, none, 0, 0), 10);

            expect (packets.data == expected.data);
        }

        beginTest ("UMPConverter doesn't send note expression for notes that have ended, or to a MIDI 1.0 processor");
        {
            using namespace Steinberg::Vst;

            const std::vector<Event> endedNote { makeNoteOn (2, 50, 1.0f, 8, 0),
                                                 makeNoteOff (2, 50, 1.0f, -1, 1),
                                                 makeNoteExpression (kVolumeTypeID, 8, 0.5, 2) };

            expect (countPerNoteControllers (toPackets (ump::PacketProtocol::MIDI_2_0, endedNote)) == 0);

            const std::vector<Event> playingNote { makeNoteOn (2, 50, 1.0f, 8, 0),
                                                   makeNoteExpression (kVolumeTypeID, 8, 0.5, 1) };

            expect (countPerNoteControllers (toPackets (ump::PacketProtocol::MIDI_2_0, playingNote)) == 1);
            expect (toPackets (ump::PacketProtocol::MIDI_1_0, playingNote).getNumPackets() == 1);
        }

        beginTest ("UMPConverter detaches and resets the per-note controllers of a key before its next note-on");
        {
            using namespace Steinberg::Vst;

            constexpr auto none = ump::Factory::NoteAttributeKind::none;

            MidiEventList::UMPConverter converter;
            converter.reset (ump::PacketProtocol::MIDI_2_0);

            const auto packets = toPackets (converter, { makeNoteOn (0, 64, 1.0f, 1, 0),
                                                         makeNoteExpression (kVolumeTypeID, 1, 0.5, 1),
                                                         makeNoteOff (0, 64, 1.0f, 1, 2),
                                                         makeNoteOn (0, 64, 1.0f, 2, 3),
                                                         makeNoteOff (0, 64, 1.0f, 2, 4),
                                                         makeNoteOn (0, 64, 1.0f, 3, 5),
                                                         makeNoteOn (1, 64, 1.0f, 4, 6) });

            UMPBuffer expected;
            addPacket (expected, ump::Factory::makeNoteOnV2 (0, 0, 64, none, 0xffff, 0), 0);
            addPacket (expected, ump::Factory::makeRegisteredPerNoteControllerV2 (0, 0, 64, 7, 0x80000000), 1);
            addPacket (expected, ump::Factory::makeNoteOffV2 (0, 0, 64, none, 0xffff, 0), 2);
            addPacket (expected, ump::Factory::makePerNoteManagementV2 (0, 0, 64, std::byte { 0x3 }), 3);
            addPacket (expected, ump::Factory::makeNoteOnV2 (0, 0, 64, none, 0xffff, 0), 3);
            addPacket (expected, ump::Factory::makeNoteOffV2 (0, 0, 64, none, 0xffff, 0), 4);
            addPacket (expected, ump::Factory::makeNoteOnV2 (0, 0, 64, none, 0xffff, 0), 5);
            addPacket (expected, ump::Factory::makeNoteOnV2 (0, 1, 64, none, 0xffff, 0), 6);

            expect (packets.data == expected.data);

            toPackets (converter, { makeNoteOn (0, 60, 1.0f, 5, 0), makeNoteExpression (kPanTypeID, 5, 0.5, 1) });
            converter.reset (ump::PacketProtocol::MIDI_2_0);

            expect (toPackets (converter, { makeNoteOn (0, 60, 1.0f, 6, 0) }).getNumPackets() == 1);
        }

        beginTest ("UMPConverter keeps track of the notes that are playing from one block to the next");
        {
            using namespace Steinberg::Vst;

            MidiEventList::UMPConverter converter;
            converter.reset (ump::PacketProtocol::MIDI_2_0);

            std::vector<Event> noteOns;

            for (auto i = 0; i < 257; ++i)
                noteOns.push_back (makeNoteOn (0, i % 128, 1.0f, 1000 + i, 0));

            toPackets (converter, noteOns);

            // Only the 256 notes that started last can receive note expression
            const auto packets = toPackets (converter, { makeNoteExpression (kVolumeTypeID, 1000, 0.5, 0),
                                                         makeNoteExpression (kVolumeTypeID, 1001, 0.5, 1),
                                                         makeNoteExpression (kVolumeTypeID, 1256, 0.5, 2) });

            expect (countPerNoteControllers (packets) == 2);

            converter.reset (ump::PacketProtocol::MIDI_2_0);

            const auto afterReset = toPackets (converter, { makeNoteExpression (kVolumeTypeID, 1256, 0.5, 0) });
            expect (countPerNoteControllers (afterReset) == 0);
        }

        beginTest ("UMPConverter builds the controllers that IMidiMapping assigns to parameters");
        {
            using namespace Steinberg::Vst;

            for (const auto value : { 0.0, 0.1, 0.25, 0.5, 64.0 / 127.0, 0.99, 1.0 })
            {
                for (const auto controller : { 7, (int) kAfterTouch, (int) kPitchBend })
                {
                    MidiEventList::UMPConverter converter1, converter2;
                    converter1.reset (ump::PacketProtocol::MIDI_1_0);
                    converter2.reset (ump::PacketProtocol::MIDI_2_0);

                    UMPBuffer packets1, packets2;
                    converter1.addController (packets1, 3, controller, value, 0);
                    converter2.addController (packets2, 3, controller, value, 0);

                    // The MIDI 1.0 packet has the values that the MidiBuffer version would have
                    const auto expectedMidi1 = [&]
                    {
                        if (controller == kAfterTouch)
                            return MidiMessage::channelPressureChange (3, jlimit (0, 127, (int) (value * 128.0)));

                        if (controller == kPitchBend)
                            return MidiMessage::pitchWheel (3, jlimit (0, 0x3fff, (int) (value * 0x4000)));

                        return MidiMessage::controllerEvent (3, controller, jlimit (0, 127, (int) (value * 128.0)));
                    }();

                    MidiBuffer midi;
                    midi.addEvent (expectedMidi1, 0);
                    ump::GenericUMPConverter toMidi1 { ump::PacketProtocol::MIDI_1_0 };
                    UMPBuffer expected;
                    expected.addFromMidiBuffer (midi, toMidi1);

                    expect (packets1.data == expected.data);

                    // Narrowing the MIDI 2.0 value gives the MIDI 1.0 value
                    const auto word1 = (*packets1.begin()).packet[0];
                    const auto packet2 = (*packets2.begin()).packet;
                    const auto byte2 = (int) ump::Utils::U8<2>::get (word1);
                    const auto byte3 = (int) ump::Utils::U8<3>::get (word1);

                    expect (ump::Utils::getMessageType (packet2[0]) == ump::Utils::MessageKind::channelVoice2);
                    expect (ump::Utils::getStatus (packet2[0]) == ump::Utils::getStatus (word1));
                    expect (ump::Utils::getChannel (packet2[0]) == 2);

                    if (controller == kPitchBend)
                        expect ((int) ump::Conversion::scaleTo14 (packet2[1]) == (byte2 | (byte3 << 7)));
                    else if (controller == kAfterTouch)
                        expect ((int) ump::Conversion::scaleTo7 (packet2[1]) == byte2);
                    else
                        expect ((int) ump::Conversion::scaleTo7 (packet2[1]) == byte3);
                }
            }
        }

        beginTest ("UMPConverter sends a MIDI 2.0 processor the RPNs from IMidiMapping as registered controllers");
        {
            const auto addControllers = [] (MidiEventList::UMPConverter& converter, UMPBuffer& packets)
            {
                converter.addController (packets, 1, 101, 0.0, 0);
                converter.addController (packets, 1, 100, 0.0, 0);
                converter.addController (packets, 1, 6, 2.0 / 128.0, 0);
                converter.addController (packets, 1, 38, 0.0, 0);
                converter.addController (packets, 1, 0, 0.5, 1);
                converter.addPendingPackets (packets);
            };

            MidiEventList::UMPConverter converter2;
            converter2.reset (ump::PacketProtocol::MIDI_2_0);
            UMPBuffer packets2;
            addControllers (converter2, packets2);

            const auto data = ump::Conversion::scaleTo32 ((uint16_t) (2 << 7));
            UMPBuffer expected2;
            addPacket (expected2, ump::Factory::makeRegisteredControllerV2 (0, 0, 0, 0, data), 0);

            expect (packets2.data == expected2.data);

            MidiEventList::UMPConverter converter1;
            converter1.reset (ump::PacketProtocol::MIDI_1_0);
            UMPBuffer packets1;
            addControllers (converter1, packets1);

            UMPBuffer expected1;
            addPacket (expected1, ump::Factory::makeControlChangeV1 (0, 0, 101, 0), 0);
            addPacket (expected1, ump::Factory::makeControlChangeV1 (0, 0, 100, 0), 0);
            addPacket (expected1, ump::Factory::makeControlChangeV1 (0, 0, 6, 2), 0);
            addPacket (expected1, ump::Factory::makeControlChangeV1 (0, 0, 38, 0), 0);
            addPacket (expected1, ump::Factory::makeControlChangeV1 (0, 0, 0, 64), 1);

            expect (packets1.data == expected1.data);
        }

        beginTest ("UMPConverter translates the RPNs for a MIDI 2.0 processor in time order");
        {
            MidiEventList::UMPConverter converter;
            converter.reset (ump::PacketProtocol::MIDI_2_0);

            // RPN 0 is set to 12 before the null RPN is selected, but the parameter changes arrive one
            // queue at a time, and the LSB of the value arrives as an event
            UMPBuffer packets;
            converter.addController (packets, 1, 101, 0.0, 0);
            converter.addController (packets, 1, 101, 127.0 / 128.0, 10);
            converter.addController (packets, 1, 100, 0.0, 0);
            converter.addController (packets, 1, 100, 127.0 / 128.0, 10);
            converter.addController (packets, 1, 6, 12.0 / 128.0, 1);

            MidiEventList events;
            addEvents (events, { makeLegacyController (0, 38, 0, 0, 2) });
            converter.toUMPBuffer (packets, events);
            converter.addPendingPackets (packets);

            const auto data = ump::Conversion::scaleTo32 ((uint16_t) (12 << 7));
            UMPBuffer expected;
            addPacket (expected, ump::Factory::makeRegisteredControllerV2 (0, 0, 0, 0, data), 2);

            expect (packets.data == expected.data);
        }

        beginTest ("UMPConverter translates MIDI 1.0 events for a MIDI 2.0 processor as it would a MidiBuffer");
        {
            using namespace Steinberg::Vst;

            const std::vector<uint8> sysEx { 0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7 };
            const std::vector<Event> events { makeLegacyController (0, 74, 100, 0, 0),
                                              makeLegacyController (1, kPitchBend, 0x7f, 0x7f, 1),
                                              makeLegacyController (2, kAfterTouch, 0x40, 0, 2),
                                              makeLegacyController (3, kCtrlProgramChange, 5, 0, 3),
                                              makeSysEx (sysEx, 4) };

            expect (toPackets (ump::PacketProtocol::MIDI_2_0, events).data
                    == convertMidiBuffer (events, ump::PacketProtocol::MIDI_2_0).data);
        }

        beginTest ("UMPConverter sends the host MIDI 1.0 packets as the MidiBuffer version sends the same messages");
        {
            const uint8 sysEx[] { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

            MidiBuffer midi;
            midi.addEvent (MidiMessage::noteOn (1, 60, (uint8) 100), 0);
            midi.addEvent (MidiMessage::noteOff (1, 60, (uint8) 20), 5);
            midi.addEvent (MidiMessage::controllerEvent (2, 7, 64), 6);
            midi.addEvent (MidiMessage::pitchWheel (3, 1000), 7);
            midi.addEvent (MidiMessage::aftertouchChange (4, 60, 50), 8);
            midi.addEvent (MidiMessage::channelPressureChange (5, 30), 9);
            midi.addEvent (MidiMessage::programChange (6, 3), 10);
            midi.addEvent (MidiMessage::createSysExMessage (sysEx, (int) std::size (sysEx)), 11);
            midi.addEvent (MidiMessage::quarterFrame (2, 5), 12);
            midi.addEvent (MidiMessage::midiClock(), 13);

            MidiEventList expected;
            MidiEventList::pluginToHostEventList (expected, midi);

            ump::GenericUMPConverter toMidi1 { ump::PacketProtocol::MIDI_1_0 };
            UMPBuffer packets;
            packets.addFromMidiBuffer (midi, toMidi1);

            MidiEventList::UMPConverter converter;
            converter.reset (ump::PacketProtocol::MIDI_1_0);
            MidiEventList result;
            converter.pluginToHostEventList (result, packets);

            expect (eventListsMatch (result, expected));
        }

        beginTest ("UMPConverter sends the host MIDI 2.0 notes at full velocity, and other messages as MIDI 1.0");
        {
            using namespace Steinberg::Vst;

            constexpr auto none = ump::Factory::NoteAttributeKind::none;

            UMPBuffer packets;
            addPacket (packets, ump::Factory::makeNoteOnV2 (0, 1, 60, none, 0x8000, 0), 0);
            addPacket (packets, ump::Factory::makeNoteOffV2 (0, 1, 60, none, 0xffff, 0), 1);
            addPacket (packets, ump::Factory::makeControlChangeV2 (0, 2, 7, 0x80000000), 2);
            addPacket (packets, ump::Factory::makeRegisteredPerNoteControllerV2 (0, 1, 60, 7, 0x80000000), 3);
            addPacket (packets, ump::Factory::makePitchBendV2 (0, 3, 0xffffffff), 4);
            addPacket (packets, ump::Factory::makePolyPressureV2 (0, 4, 60, 0x80000000), 5);
            addPacket (packets, ump::Factory::makePerNotePitchBendV2 (0, 1, 60, 0), 6);

            MidiEventList::UMPConverter converter;
            converter.reset (ump::PacketProtocol::MIDI_2_0);
            MidiEventList result;
            converter.pluginToHostEventList (result, packets);

            expect (result.getEventCount() == 5);

            const auto noteOn = getEvent (result, 0);
            expect (noteOn.type == Event::kNoteOnEvent);
            expect (noteOn.sampleOffset == 0);
            expect (noteOn.noteOn.channel == 1 && noteOn.noteOn.pitch == 60 && noteOn.noteOn.noteId == -1);
            expect (exactlyEqual (noteOn.noteOn.velocity, (float) 0x8000 / 65535.0f));

            const auto noteOff = getEvent (result, 1);
            expect (noteOff.type == Event::kNoteOffEvent);
            expect (noteOff.sampleOffset == 1);
            expect (noteOff.noteOff.channel == 1 && noteOff.noteOff.pitch == 60);
            expect (exactlyEqual (noteOff.noteOff.velocity, 1.0f));

            const auto expectController = [&] (Steinberg::int32 index, int sampleOffset, LegacyMIDICCOutEvent cc)
            {
                const auto e = getEvent (result, index);
                expect (e.type == Event::kLegacyMIDICCOutEvent);
                expect (e.sampleOffset == sampleOffset);
                expect (e.midiCCOut.channel == cc.channel);
                expect (e.midiCCOut.controlNumber == cc.controlNumber);
                expect (e.midiCCOut.value == cc.value);
                expect (e.midiCCOut.value2 == cc.value2);
            };

            expectController (2, 2, { 7, 2, 64, 0 });
            expectController (3, 4, { kPitchBend, 3, 0x7f, 0x7f });
            expectController (4, 5, { kCtrlPolyPressure, 4, 60, 64 });
        }

        beginTest ("UMPConverter sends the host each SysEx message whose packets are all in the buffer");
        {
            const std::array<std::byte, 6> first { std::byte { 0x01 }, std::byte { 0x02 }, std::byte { 0x03 },
                                                   std::byte { 0x04 }, std::byte { 0x05 }, std::byte { 0x06 } };
            const std::array<std::byte, 1> last { std::byte { 0x07 } };
            const std::array<std::byte, 2> complete { std::byte { 0x08 }, std::byte { 0x09 } };

            UMPBuffer packets;
            addPacket (packets, ump::Factory::makeSysExStart (0, first), 0);
            addPacket (packets, ump::Factory::makeSysExEnd (0, last), 0);
            addPacket (packets, ump::Factory::makeSysExIn1Packet (0, complete), 1);
            addPacket (packets, ump::Factory::makeSysExEnd (0, last), 2);
            addPacket (packets, ump::Factory::makeSysExStart (0, first), 3);

            MidiEventList::UMPConverter converter;
            converter.reset (ump::PacketProtocol::MIDI_2_0);
            MidiEventList result;
            converter.pluginToHostEventList (result, packets);

            expect (result.getEventCount() == 2);

            const auto expectSysEx = [&] (Steinberg::int32 index, int sampleOffset, std::vector<uint8> bytes)
            {
                const auto e = getEvent (result, index);
                expect (e.type == Steinberg::Vst::Event::kDataEvent);
                expect (e.sampleOffset == sampleOffset);
                expect (e.data.type == Steinberg::Vst::DataEvent::kMidiSysEx);
                expect (std::vector<uint8> (e.data.bytes, e.data.bytes + e.data.size) == bytes);
            };

            expectSysEx (0, 0, { 0xf0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xf7 });
            expectSysEx (1, 1, { 0xf0, 0x08, 0x09, 0xf7 });
        }

        beginTest ("UMPConverter has room for the SysEx in a full buffer");
        {
            const std::array<std::byte, 6> bytes { std::byte { 0x01 }, std::byte { 0x02 }, std::byte { 0x03 },
                                                   std::byte { 0x04 }, std::byte { 0x05 }, std::byte { 0x06 } };

            // Each packet takes 3 of the 2048 words that the VST3 wrapper reserves
            UMPBuffer packets;
            constexpr auto numPackets = 2048 / 3;

            for (auto i = 0; i < numPackets; ++i)
                addPacket (packets, ump::Factory::makeSysExIn1Packet (0, bytes), i);

            MidiEventList::UMPConverter converter;
            converter.reset (ump::PacketProtocol::MIDI_2_0);
            MidiEventList result;
            converter.pluginToHostEventList (result, packets);

            expect (result.getEventCount() == numPackets);

            const std::vector<uint8> expected { 0xf0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0xf7 };

            for (const auto index : { 0, numPackets - 1 })
            {
                const auto e = getEvent (result, index);
                expect (std::vector<uint8> (e.data.bytes, e.data.bytes + e.data.size) == expected);
            }
        }

       #if JUCE_VST3_MIDI2_NOTE_ON_ATTRIBUTES
        beginTest ("UMPConverter gives a MIDI 2.0 note-on the attribute that the host sends as note expression");
        {
            constexpr auto none = ump::Factory::NoteAttributeKind::none;
            constexpr auto pitch7_9 = ump::Factory::NoteAttributeKind::pitch7_9;
            constexpr auto profile = ump::Factory::NoteAttributeKind::profile;

            const std::vector<Steinberg::Vst::Event> events { makeNoteOn (0, 60, 1.0f, 1, 0),
                                                              makeNoteOnAttribute (203003, 1, 0x1234abcd, 0),
                                                              makeNoteOnAttribute (203002, 2, 0x00000042, 1),
                                                              makeNoteOn (0, 61, 1.0f, 2, 1),
                                                              makeNoteOn (0, 62, 1.0f, 3, 2),
                                                              makeNoteOnAttribute (203001, 3, 0x0001, 2),
                                                              makeNoteOnAttribute (203003, 3, 0x0002, 2),
                                                              makeNoteOn (0, 63, 1.0f, 4, 3),
                                                              makeNoteOnAttribute (203000, 4, 0x0003, 3),
                                                              makeNoteOnAttribute (204003, 4, 0x0004, 3),
                                                              makeNoteOn (0, 64, 1.0f, -1, 4),
                                                              makeNoteOnAttribute (203003, -1, 0x0005, 4) };

            UMPBuffer expected2;
            addPacket (expected2, ump::Factory::makeNoteOnV2 (0, 0, 60, pitch7_9, 0xffff, 0xabcd), 0);
            addPacket (expected2, ump::Factory::makeNoteOnV2 (0, 0, 61, profile, 0xffff, 0x0042), 1);
            addPacket (expected2, ump::Factory::makeNoteOnV2 (0, 0, 62, pitch7_9, 0xffff, 0x0002), 2);
            addPacket (expected2, ump::Factory::makeNoteOnV2 (0, 0, 63, none, 0xffff, 0), 3);
            addPacket (expected2, ump::Factory::makeNoteOnV2 (0, 0, 64, none, 0xffff, 0), 4);

            expect (toPackets (ump::PacketProtocol::MIDI_2_0, events).data == expected2.data);

            UMPBuffer expected1;

            for (auto i = 0; i < 5; ++i)
                addPacket (expected1, ump::Factory::makeNoteOnV1 (0, 0, (uint8_t) (60 + i), 127), i);

            expect (toPackets (ump::PacketProtocol::MIDI_1_0, events).data == expected1.data);
        }
       #endif

        beginTest ("UMPConverter sends no more than 2048 events to the host at once");
        {
            UMPBuffer packets;

            for (auto i = 0; i < 3000; ++i)
                addPacket (packets, ump::Factory::makeNoteOnV1 (0, 0, (uint8_t) (i % 128), 100), i);

            MidiEventList::UMPConverter converter;
            converter.reset (ump::PacketProtocol::MIDI_1_0);
            MidiEventList result;
            converter.pluginToHostEventList (result, packets);

            expect (result.getEventCount() == 2048);
        }
    }

private:
    //==============================================================================
    struct Config
    {
        Config (std::vector<DynamicChannelMapping> i, std::vector<DynamicChannelMapping> o)
            : ins (std::move (i)), outs (std::move (o))
        {
            for (auto container : { &ins, &outs })
                for (auto& x : *container)
                    x.setHostActive (true);
        }

        std::vector<DynamicChannelMapping> ins, outs;

        int getNumChannels() const { return countUsedClientChannels (ins, outs); }
    };

    struct TestBuffers
    {
        explicit TestBuffers (int samples) : numSamples (samples) {}

        void init()
        {
            for (const auto [index, channel] : enumerate (buffers, 1))
                std::fill (channel.begin(), channel.end(), (float) index);
        }

        bool allMatch (int channel, float value) const
        {
            const auto& buf = buffers[(size_t) channel];
            return std::all_of (buf.begin(), buf.end(), [&] (auto x) { return exactlyEqual (x, value); });
        }

        bool isClear (int channel) const
        {
            return allMatch (channel, 0.0f);
        }

        float* addChannel()
        {
            buffers.emplace_back (numSamples);
            return buffers.back().data();
        }

              float* get (int channel)       { return buffers[(size_t) channel].data(); }
        const float* get (int channel) const { return buffers[(size_t) channel].data(); }

        std::vector<std::vector<float>> buffers;
        int numSamples = 0;
    };

    static bool channelStartsWithValue (Steinberg::Vst::AudioBusBuffers& bus, size_t index, float value)
    {
        return exactlyEqual (bus.channelBuffers32[index][0], value);
    }

    static bool allMatch (const AudioBuffer<float>& buf, int index, float value)
    {
        const auto* ptr = buf.getReadPointer (index);
        return std::all_of (ptr, ptr + buf.getNumSamples(), [&] (auto x) { return exactlyEqual (x, value); });
    }

    struct MultiBusBuffers
    {
        std::vector<Steinberg::Vst::AudioBusBuffers> buffers;
        std::vector<std::vector<float*>> pointerStorage;

        MultiBusBuffers withBus (TestBuffers& storage, int numChannels) &&
        {
            MultiBusBuffers result { std::move (buffers), std::move (pointerStorage) };

            std::vector<float*> pointers;

            for (auto i = 0; i < numChannels; ++i)
                pointers.push_back (storage.addChannel());

            Steinberg::Vst::AudioBusBuffers buffer;
            buffer.numChannels = (Steinberg::int32) pointers.size();
            buffer.channelBuffers32 = pointers.data();

            result.buffers.push_back (buffer);
            result.pointerStorage.push_back (std::move (pointers));

            return result;
        }
    };

    static Steinberg::Vst::ProcessData makeProcessData (int blockSize, MultiBusBuffers& ins, MultiBusBuffers& outs)
    {
        Steinberg::Vst::ProcessData result;
        result.numSamples = blockSize;
        result.inputs = ins.buffers.data();
        result.numInputs = (Steinberg::int32) ins.buffers.size();
        result.outputs = outs.buffers.data();
        result.numOutputs = (Steinberg::int32) outs.buffers.size();
        return result;
    }

    //==============================================================================
    template <typename Packet>
    static void addPacket (UMPBuffer& buffer, const Packet& packet, int samplePosition)
    {
        buffer.addPacket (ump::View (packet.data()), samplePosition);
    }

    static Steinberg::Vst::Event makeEvent (Steinberg::uint16 type, int sampleOffset)
    {
        Steinberg::Vst::Event e{};
        e.type = type;
        e.sampleOffset = sampleOffset;
        return e;
    }

    static Steinberg::Vst::Event makeNoteOn (int channel, int pitch, float velocity, int noteId, int sampleOffset)
    {
        auto e = makeEvent (Steinberg::Vst::Event::kNoteOnEvent, sampleOffset);
        e.noteOn.channel = (Steinberg::int16) channel;
        e.noteOn.pitch = (Steinberg::int16) pitch;
        e.noteOn.velocity = velocity;
        e.noteOn.noteId = noteId;
        return e;
    }

    static Steinberg::Vst::Event makeNoteOff (int channel, int pitch, float velocity, int noteId, int sampleOffset)
    {
        auto e = makeEvent (Steinberg::Vst::Event::kNoteOffEvent, sampleOffset);
        e.noteOff.channel = (Steinberg::int16) channel;
        e.noteOff.pitch = (Steinberg::int16) pitch;
        e.noteOff.velocity = velocity;
        e.noteOff.noteId = noteId;
        return e;
    }

    static Steinberg::Vst::Event makePolyPressure (int channel, int pitch, float pressure, int sampleOffset)
    {
        auto e = makeEvent (Steinberg::Vst::Event::kPolyPressureEvent, sampleOffset);
        e.polyPressure.channel = (Steinberg::int16) channel;
        e.polyPressure.pitch = (Steinberg::int16) pitch;
        e.polyPressure.pressure = pressure;
        e.polyPressure.noteId = -1;
        return e;
    }

    static Steinberg::Vst::Event makeNoteExpression (Steinberg::Vst::NoteExpressionTypeID typeId,
                                                     int noteId,
                                                     double value,
                                                     int sampleOffset)
    {
        auto e = makeEvent (Steinberg::Vst::Event::kNoteExpressionValueEvent, sampleOffset);
        e.noteExpressionValue.typeId = typeId;
        e.noteExpressionValue.noteId = noteId;
        e.noteExpressionValue.value = value;
        return e;
    }

   #if JUCE_VST3_MIDI2_NOTE_ON_ATTRIBUTES
    static Steinberg::Vst::Event makeNoteOnAttribute (Steinberg::Vst::NoteExpressionTypeID typeId,
                                                      int noteId,
                                                      Steinberg::uint64 value,
                                                      int sampleOffset)
    {
        auto e = makeEvent (Steinberg::Vst::Event::kNoteExpressionIntValueEvent, sampleOffset);
        e.noteExpressionIntValue.typeId = typeId;
        e.noteExpressionIntValue.noteId = noteId;
        e.noteExpressionIntValue.value = value;
        return e;
    }
   #endif

    static Steinberg::Vst::Event makeLegacyController (int channel,
                                                       int number,
                                                       int value,
                                                       int value2,
                                                       int sampleOffset)
    {
        auto e = makeEvent (Steinberg::Vst::Event::kLegacyMIDICCOutEvent, sampleOffset);
        e.midiCCOut.channel = (Steinberg::int8) channel;
        e.midiCCOut.controlNumber = (Steinberg::uint8) number;
        e.midiCCOut.value = (Steinberg::int8) value;
        e.midiCCOut.value2 = (Steinberg::int8) value2;
        return e;
    }

    static Steinberg::Vst::Event makeSysEx (const std::vector<uint8>& bytes, int sampleOffset)
    {
        auto e = makeEvent (Steinberg::Vst::Event::kDataEvent, sampleOffset);
        e.data.type = Steinberg::Vst::DataEvent::kMidiSysEx;
        e.data.bytes = bytes.data();
        e.data.size = (Steinberg::uint32) bytes.size();
        return e;
    }

    static void addEvents (MidiEventList& list, const std::vector<Steinberg::Vst::Event>& events)
    {
        for (auto e : events)
            list.addEvent (e);
    }

    static UMPBuffer toPackets (MidiEventList::UMPConverter& converter,
                                const std::vector<Steinberg::Vst::Event>& events)
    {
        MidiEventList list;
        addEvents (list, events);

        UMPBuffer result;
        converter.toUMPBuffer (result, list);
        converter.addPendingPackets (result);
        return result;
    }

    static UMPBuffer toPackets (ump::PacketProtocol protocol, const std::vector<Steinberg::Vst::Event>& events)
    {
        MidiEventList::UMPConverter converter;
        converter.reset (protocol);
        return toPackets (converter, events);
    }

    // Converts the events to a MidiBuffer, which is then converted in the same way as an AudioProcessorPlayer
    // converts the messages that it receives in a MidiBuffer
    static UMPBuffer convertMidiBuffer (const std::vector<Steinberg::Vst::Event>& events, ump::PacketProtocol protocol)
    {
        MidiEventList list;
        addEvents (list, events);

        MidiBuffer midi;
        MidiEventList::toMidiBuffer (midi, list);

        ump::GenericUMPConverter converter { protocol };
        UMPBuffer result;
        result.addFromMidiBuffer (midi, converter);
        return result;
    }

    static int countPerNoteControllers (const UMPBuffer& packets)
    {
        return (int) std::count_if (packets.begin(), packets.end(), [] (const UMPPacketMetadata& metadata)
        {
            return ump::Utils::getMessageType (metadata.packet[0]) == ump::Utils::MessageKind::channelVoice2
                && ump::Utils::getStatus (metadata.packet[0]) == std::byte { 0x0 };
        });
    }

    static Steinberg::Vst::Event getEvent (MidiEventList& list, Steinberg::int32 index)
    {
        Steinberg::Vst::Event e{};
        list.getEvent (index, e);
        return e;
    }

    static bool eventsMatch (const Steinberg::Vst::Event& a, const Steinberg::Vst::Event& b)
    {
        if (a.type != b.type || a.busIndex != b.busIndex || a.sampleOffset != b.sampleOffset)
            return false;

        switch (a.type)
        {
            case Steinberg::Vst::Event::kNoteOnEvent:
                return a.noteOn.channel == b.noteOn.channel
                    && a.noteOn.pitch == b.noteOn.pitch
                    && exactlyEqual (a.noteOn.velocity, b.noteOn.velocity)
                    && exactlyEqual (a.noteOn.tuning, b.noteOn.tuning)
                    && a.noteOn.length == b.noteOn.length
                    && a.noteOn.noteId == b.noteOn.noteId;

            case Steinberg::Vst::Event::kNoteOffEvent:
                return a.noteOff.channel == b.noteOff.channel
                    && a.noteOff.pitch == b.noteOff.pitch
                    && exactlyEqual (a.noteOff.velocity, b.noteOff.velocity)
                    && exactlyEqual (a.noteOff.tuning, b.noteOff.tuning)
                    && a.noteOff.noteId == b.noteOff.noteId;

            case Steinberg::Vst::Event::kDataEvent:
                return a.data.type == b.data.type
                    && a.data.size == b.data.size
                    && std::equal (a.data.bytes, a.data.bytes + a.data.size, b.data.bytes);

            case Steinberg::Vst::Event::kLegacyMIDICCOutEvent:
                return a.midiCCOut.channel == b.midiCCOut.channel
                    && a.midiCCOut.controlNumber == b.midiCCOut.controlNumber
                    && a.midiCCOut.value == b.midiCCOut.value
                    && a.midiCCOut.value2 == b.midiCCOut.value2;

            default:
                return false;
        }
    }

    static bool eventListsMatch (MidiEventList& a, MidiEventList& b)
    {
        if (a.getEventCount() != b.getEventCount())
            return false;

        for (Steinberg::int32 i = 0; i < a.getEventCount(); ++i)
            if (! eventsMatch (getEvent (a, i), getEvent (b, i)))
                return false;

        return true;
    }
};

static VST3PluginFormatTests vst3PluginFormatTests;

} // namespace juce
