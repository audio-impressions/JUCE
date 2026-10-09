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

/** Sets up `channels` so that it contains channel pointers suitable for passing to
    an AudioProcessor's processBlock.

    On return, `channels` will hold `max (processorIns, processorOuts)` entries.
    The first `processorIns` entries will point to buffers holding input data.
    Any entries after the first `processorIns` entries will point to zeroed buffers.

    In the case that the system only provides a single input channel, but the processor
    has been initialised with multiple input channels, the system input will be copied
    to all processor inputs.

    In the case that the system provides no input channels, but the processor has
    been initialised with multiple input channels, the processor's input channels will
    all be zeroed.

    @param ins            the system inputs.
    @param outs           the system outputs.
    @param numSamples     the number of samples in the system buffers.
    @param processorIns   the number of input channels requested by the processor.
    @param processorOuts  the number of output channels requested by the processor.
    @param tempBuffer     temporary storage for inputs that don't have a corresponding output.
    @param channels       holds pointers to each of the processor's audio channels.
*/
static void initialiseIoBuffers (Span<const float* const> ins,
                                 Span<float* const> outs,
                                 const int numSamples,
                                 size_t processorIns,
                                 size_t processorOuts,
                                 AudioBuffer<float>& tempBuffer,
                                 std::vector<float*>& channels)
{
    const auto totalNumChannels = jmax (processorIns, processorOuts);

    jassert (channels.capacity() >= totalNumChannels);
    jassert ((size_t) tempBuffer.getNumChannels() >= totalNumChannels);
    jassert (tempBuffer.getNumSamples() >= numSamples);

    channels.resize (totalNumChannels);

    const auto numBytes = (size_t) numSamples * sizeof (float);

    size_t tempBufferIndex = 0;

    for (size_t i = 0; i < totalNumChannels; ++i)
    {
        auto*& channelPtr = channels[i];
        channelPtr = i < outs.size()
                   ? outs[i]
                   : tempBuffer.getWritePointer ((int) tempBufferIndex++);

        // If there's a single input channel, route it to all inputs on the processor
        if (ins.size() == 1 && i < processorIns)
            memcpy (channelPtr, ins.front(), numBytes);

        // Otherwise, if there's a system input corresponding to this channel, use that
        else if (i < ins.size())
            memcpy (channelPtr, ins[i], numBytes);

        // Otherwise, silence the channel
        else
            zeromem (channelPtr, numBytes);
    }

    // Zero any output channels that won't be written by the processor
    for (size_t i = totalNumChannels; i < outs.size(); ++i)
        zeromem (outs[i], numBytes);
}

static ump::PacketProtocol getPacketProtocol (AudioProcessor::MidiFormat format)
{
    return format == AudioProcessor::MidiFormat::umpMidi2 ? ump::PacketProtocol::MIDI_2_0
                                                          : ump::PacketProtocol::MIDI_1_0;
}

template <typename FloatType>
static void processBlockWithMidi (AudioProcessor& processor, AudioBuffer<FloatType>& buffer, MidiBuffer& midi)
{
    processor.processBlock (buffer, midi);
}

template <typename FloatType>
static void processBlockWithMidi (AudioProcessor& processor, AudioBuffer<FloatType>& buffer, UMPBuffer& packets)
{
    processor.processBlockUMP (buffer, packets);
}

//==============================================================================
void AudioProcessorPlayer::DeviceInput::consume (ump::Iterator b, ump::Iterator e, double time)
{
    for (const auto& packet : makeRange (b, e))
    {
        const auto isActiveSense = ump::Utils::getMessageType (packet[0]) == ump::Utils::MessageKind::commonRealtime
                                   && ump::Utils::U8<1>::get (packet[0]) == 0xfe;

        if (! isActiveSense)
            collector.addPacketToQueue (packet, time);
    }
}

//==============================================================================
AudioProcessorPlayer::AudioProcessorPlayer (bool doDoublePrecisionProcessing)
    : isDoublePrecision (doDoublePrecisionProcessing)
{
}

AudioProcessorPlayer::~AudioProcessorPlayer()
{
    setMidiInputDeviceManager (nullptr);
    setProcessor (nullptr);
}

//==============================================================================
AudioProcessorPlayer::NumChannels AudioProcessorPlayer::findMostSuitableLayout (const AudioProcessor& proc) const
{
    if (proc.isMidiEffect())
        return {};

    std::vector<NumChannels> layouts { deviceChannels };

    if (deviceChannels.ins == 0 || deviceChannels.ins == 1)
    {
        layouts.emplace_back (defaultProcessorChannels.ins, deviceChannels.outs);
        layouts.emplace_back (deviceChannels.outs, deviceChannels.outs);
    }

    const auto it = std::find_if (layouts.begin(), layouts.end(), [&] (const NumChannels& chans)
    {
        return proc.checkBusesLayoutSupported (chans.toLayout());
    });

    if (it == layouts.end())
        return defaultProcessorChannels;

    return *it;
}

void AudioProcessorPlayer::resizeChannels()
{
    const auto maxChannels = jmax (deviceChannels.ins,
                                   deviceChannels.outs,
                                   actualProcessorChannels.ins,
                                   actualProcessorChannels.outs);
    channels.resize ((size_t) maxChannels);
    tempBuffer.setSize (maxChannels, blockSize);
}

void AudioProcessorPlayer::setProcessor (AudioProcessor* const processorToPlay)
{
    const auto format = [&]
    {
        const ScopedLock sl (lock);
        prepareProcessor (processorToPlay);
        return midiFormat;
    }();

    // The registration is moved without the lock, as moving it can wait for the MIDI inputs
    moveMidiInputs (midiInputDeviceManager, format);
}

// Called with the lock held. Unlike setProcessor(), this never moves the registration with the
// MIDI input device manager, so audioDeviceAboutToStart() can call it on any thread
void AudioProcessorPlayer::prepareProcessor (AudioProcessor* const processorToPlay)
{
    if (processor == processorToPlay)
        return;

    sampleCount = 0;
    currentWorkgroup.reset();

    if (processorToPlay != nullptr && sampleRate > 0 && blockSize > 0)
    {
        defaultProcessorChannels = NumChannels { processorToPlay->getBusesLayout() };
        actualProcessorChannels  = findMostSuitableLayout (*processorToPlay);

        if (processorToPlay->isMidiEffect())
            processorToPlay->setRateAndBufferSizeDetails (sampleRate, blockSize);
        else
            processorToPlay->setPlayConfigDetails (actualProcessorChannels.ins,
                                                   actualProcessorChannels.outs,
                                                   sampleRate,
                                                   blockSize);

        auto supportsDouble = processorToPlay->supportsDoublePrecisionProcessing() && isDoublePrecision;

        processorToPlay->setProcessingPrecision (supportsDouble ? AudioProcessor::doublePrecision
                                                                : AudioProcessor::singlePrecision);

        processorToPlay->prepareToPlay (sampleRate, blockSize);
    }

    const auto newFormat = processorToPlay != nullptr ? processorToPlay->getMidiFormat() : midiFormat;

    if (newFormat != midiFormat)
    {
        midiFormat = newFormat;
        const auto protocol = getPacketProtocol (midiFormat);
        midiToPackets = ump::GenericUMPConverter { protocol };

        // Packets in either protocol can be converted for a processor that uses a MidiBuffer, so the
        // collector only starts again, discarding its packets, for a processor that needs the other one
        if (midiFormat != AudioProcessor::MidiFormat::midiBuffer
            && sampleRate > 0
            && packetCollector.getProtocol() != protocol)
        {
            packetCollector.reset (sampleRate, protocol);
        }
    }

    AudioProcessor* oldOne = nullptr;

    oldOne = isPrepared ? processor : nullptr;
    processor = processorToPlay;
    isPrepared = true;
    resizeChannels();

    if (oldOne != nullptr)
        oldOne->releaseResources();
}

void AudioProcessorPlayer::setDoublePrecisionProcessing (bool doublePrecision)
{
    if (doublePrecision != isDoublePrecision)
    {
        const ScopedLock sl (lock);

        currentWorkgroup.reset();

        if (processor != nullptr)
        {
            processor->releaseResources();

            auto supportsDouble = processor->supportsDoublePrecisionProcessing() && doublePrecision;

            processor->setProcessingPrecision (supportsDouble ? AudioProcessor::doublePrecision
                                                              : AudioProcessor::singlePrecision);

            processor->prepareToPlay (sampleRate, blockSize);
        }

        isDoublePrecision = doublePrecision;
    }
}

void AudioProcessorPlayer::setMidiOutput (MidiOutput* midiOutputToUse)
{
    if (midiOutput != midiOutputToUse)
    {
        const ScopedLock sl (lock);
        midiOutput = midiOutputToUse;
    }
}

void AudioProcessorPlayer::setMidiInputDeviceManager (AudioDeviceManager* managerToUse)
{
    const auto format = [&]
    {
        const ScopedLock sl (lock);
        return midiFormat;
    }();

    moveMidiInputs (managerToUse, format);
}

// Message thread only. The new registration is added before the old one is removed, so a message
// that arrives in between may reach the processor twice, but can't be missed
void AudioProcessorPlayer::moveMidiInputs (AudioDeviceManager* manager, AudioProcessor::MidiFormat format)
{
    if (manager == midiInputDeviceManager && (manager == nullptr || format == midiInputFormat))
        return;

    JUCE_ASSERT_MESSAGE_THREAD

    if (manager != nullptr)
        addMidiInputs (*manager, format);

    if (midiInputDeviceManager != nullptr)
        removeMidiInputs (*midiInputDeviceManager, midiInputFormat);

    midiInputDeviceManager = manager;
    midiInputFormat = format;
}

void AudioProcessorPlayer::addMidiInputs (AudioDeviceManager& manager, AudioProcessor::MidiFormat format)
{
    switch (format)
    {
        case AudioProcessor::MidiFormat::midiBuffer:
            manager.addMidiInputDeviceCallback ({}, &messageCollector);
            return;

        case AudioProcessor::MidiFormat::umpMidi1:
            manager.addMidiInputDeviceConsumer ({}, midi1Input, ump::PacketProtocol::MIDI_1_0);
            return;

        case AudioProcessor::MidiFormat::umpMidi2:
            manager.addMidiInputDeviceConsumer ({}, midi2Input, ump::PacketProtocol::MIDI_2_0);
            return;
    }
}

void AudioProcessorPlayer::removeMidiInputs (AudioDeviceManager& manager, AudioProcessor::MidiFormat format)
{
    switch (format)
    {
        case AudioProcessor::MidiFormat::midiBuffer:
            manager.removeMidiInputDeviceCallback ({}, &messageCollector);
            return;

        case AudioProcessor::MidiFormat::umpMidi1:
            manager.removeMidiInputDeviceConsumer ({}, midi1Input);
            return;

        case AudioProcessor::MidiFormat::umpMidi2:
            manager.removeMidiInputDeviceConsumer ({}, midi2Input);
            return;
    }
}

//==============================================================================
void AudioProcessorPlayer::audioDeviceIOCallbackWithContext (const float* const* const inputChannelData,
                                                             const int numInputChannels,
                                                             float* const* const outputChannelData,
                                                             const int numOutputChannels,
                                                             const int numSamples,
                                                             const AudioIODeviceCallbackContext& context)
{
    const ScopedLock sl (lock);

    jassert (currentDevice != nullptr);

    // These should have been prepared by audioDeviceAboutToStart()...
    jassert (sampleRate > 0 && blockSize > 0);

    // Both collectors are emptied every block, as they time their contents from these calls
    incomingMidi.clear();
    messageCollector.removeNextBlockOfMessages (incomingMidi, numSamples);
    incomingPackets.clear();
    packetCollector.removeNextBlockOfPackets (incomingPackets, numSamples);

    // Each stream is converted only if it doesn't match the processor's format, and packets only
    // reach a processor that uses a MidiBuffer while the registration with the manager moves
    if (midiFormat == AudioProcessor::MidiFormat::midiBuffer)
        incomingPackets.addToMidiBuffer (incomingMidi, packetsToMidi);
    else
        incomingPackets.addFromMidiBuffer (incomingMidi, midiToPackets);

    initialiseIoBuffers ({ inputChannelData,  (size_t) numInputChannels },
                         { outputChannelData, (size_t) numOutputChannels },
                         numSamples,
                         (size_t) actualProcessorChannels.ins,
                         (size_t) actualProcessorChannels.outs,
                         tempBuffer,
                         channels);

    const auto totalNumChannels = jmax (actualProcessorChannels.ins, actualProcessorChannels.outs);
    AudioBuffer<float> buffer (channels.data(), (int) totalNumChannels, numSamples);

    if (processor != nullptr)
    {
        const ScopedLock sl2 (processor->getCallbackLock());

        if (std::exchange (currentWorkgroup, currentDevice->getWorkgroup()) != currentDevice->getWorkgroup())
            processor->audioWorkgroupContextChanged (currentWorkgroup);

        class PlayHead final : private AudioPlayHead
        {
        public:
            PlayHead (AudioProcessor& proc,
                      Optional<uint64_t> hostTimeIn,
                      uint64_t sampleCountIn,
                      double sampleRateIn)
                : processor (proc),
                  hostTimeNs (hostTimeIn),
                  sampleCount (sampleCountIn),
                  seconds ((double) sampleCountIn / sampleRateIn)
            {
                if (useThisPlayhead)
                    processor.setPlayHead (this);
            }

            ~PlayHead() override
            {
                if (useThisPlayhead)
                    processor.setPlayHead (nullptr);
            }

        private:
            Optional<PositionInfo> getPosition() const override
            {
                PositionInfo info;
                info.setHostTimeNs (hostTimeNs);
                info.setTimeInSamples ((int64_t) sampleCount);
                info.setTimeInSeconds (seconds);
                return info;
            }

            AudioProcessor& processor;
            Optional<uint64_t> hostTimeNs;
            uint64_t sampleCount;
            double seconds;
            bool useThisPlayhead = processor.getPlayHead() == nullptr;
        };

        PlayHead playHead { *processor,
                            context.hostTimeNs != nullptr ? makeOptional (*context.hostTimeNs) : nullopt,
                            sampleCount,
                            sampleRate };

        sampleCount += (uint64_t) numSamples;

        if (! processor->isSuspended())
        {
            const auto processAndSendMidi = [&] (auto& midi)
            {
                if (processor->isUsingDoublePrecision())
                {
                    conversionBuffer.makeCopyOf (buffer, true);
                    processBlockWithMidi (*processor, conversionBuffer, midi);
                    buffer.makeCopyOf (conversionBuffer, true);
                }
                else
                {
                    processBlockWithMidi (*processor, buffer, midi);
                }

                if (midiOutput != nullptr)
                {
                    if (midiOutput->isBackgroundThreadRunning())
                    {
                        midiOutput->sendBlockOfMessages (midi,
                                                         Time::getMillisecondCounter(),
                                                         sampleRate);
                    }
                    else
                    {
                        midiOutput->sendBlockOfMessagesNow (midi);
                    }
                }
            };

            if (midiFormat == AudioProcessor::MidiFormat::midiBuffer)
                processAndSendMidi (incomingMidi);
            else
                processAndSendMidi (incomingPackets);

            return;
        }
    }

    for (int i = 0; i < numOutputChannels; ++i)
        FloatVectorOperations::clear (outputChannelData[i], numSamples);
}

void AudioProcessorPlayer::audioDeviceAboutToStart (AudioIODevice* const device)
{
    currentDevice = device;
    auto newSampleRate = device->getCurrentSampleRate();
    auto newBlockSize  = device->getCurrentBufferSizeSamples();
    auto numChansIn    = device->getActiveInputChannels().countNumberOfSetBits();
    auto numChansOut   = device->getActiveOutputChannels().countNumberOfSetBits();

    const ScopedLock sl (lock);

    sampleRate = newSampleRate;
    blockSize  = newBlockSize;
    deviceChannels = { numChansIn, numChansOut };

    resizeChannels();

    messageCollector.reset (sampleRate);
    packetCollector.reset (sampleRate, getPacketProtocol (midiFormat));
    packetCollector.ensureStorageAllocated (2048);
    incomingMidi.ensureSize (2048);
    incomingPackets.ensureSize (2048);
    midiToPackets.reset();
    packetsToMidi.reset();

    currentWorkgroup.reset();

    if (processor != nullptr)
    {
        if (isPrepared)
            processor->releaseResources();

        auto* oldProcessor = processor;
        prepareProcessor (nullptr);
        prepareProcessor (oldProcessor);
    }
}

void AudioProcessorPlayer::audioDeviceStopped()
{
    const ScopedLock sl (lock);

    if (processor != nullptr && isPrepared)
        processor->releaseResources();

    sampleRate = 0.0;
    blockSize = 0;
    isPrepared = false;
    tempBuffer.setSize (1, 1);

    currentDevice = nullptr;
    currentWorkgroup.reset();
}

void AudioProcessorPlayer::handleIncomingMidiMessage (MidiInput*, const MidiMessage& message)
{
    messageCollector.addMessageToQueue (message);
}

//==============================================================================
//==============================================================================
#if JUCE_UNIT_TESTS

struct AudioProcessorPlayerTests final : public UnitTest
{
    struct Layout
    {
        int numIns, numOuts;
    };

    using MidiFormat = AudioProcessor::MidiFormat;

    // Stands in for an audio device, so that the tests can call the player's audio callback
    struct TestDevice final : public AudioIODevice
    {
        TestDevice() : AudioIODevice ("Test", "Test") {}

        StringArray getOutputChannelNames() override                                { return { "L", "R" }; }
        StringArray getInputChannelNames() override                                 { return {}; }
        Array<double> getAvailableSampleRates() override                            { return { sampleRate }; }
        Array<int> getAvailableBufferSizes() override                               { return { blockSize }; }
        int getDefaultBufferSize() override                                         { return blockSize; }
        String open (const BigInteger&, const BigInteger&, double, int) override    { return {}; }
        void close() override                                                       {}
        bool isOpen() override                                                      { return true; }
        void start (AudioIODeviceCallback*) override                                {}
        void stop() override                                                        {}
        bool isPlaying() override                                                   { return true; }
        String getLastError() override                                              { return {}; }
        int getCurrentBufferSizeSamples() override                                  { return blockSize; }
        double getCurrentSampleRate() override                                      { return sampleRate; }
        int getCurrentBitDepth() override                                           { return 32; }
        BigInteger getActiveOutputChannels() const override                         { return BigInteger (3); }
        BigInteger getActiveInputChannels() const override                          { return {}; }
        int getOutputLatencyInSamples() override                                    { return 0; }
        int getInputLatencyInSamples() override                                     { return 0; }

        // Long blocks let the collectors keep messages that are timestamped well in the past,
        // however slowly the test runs
        static constexpr double sampleRate = 48000.0;
        static constexpr int blockSize = 48000;
    };

    // Records the MIDI that it receives, in the format that it's created with
    struct MidiRecorder final : public AudioProcessor
    {
        explicit MidiRecorder (MidiFormat formatToUse)
            : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo())),
              recordedFormat (formatToUse)
        {
        }

        using AudioProcessor::processBlock;
        using AudioProcessor::processBlockUMP;

        void processBlock (AudioBuffer<float>&, MidiBuffer& midiMessages) override
        {
            recordedMidi.addEvents (midiMessages, 0, -1, 0);
            midiStorage.push_back (midiMessages.data.begin());
        }

        void processBlockUMP (AudioBuffer<float>&, UMPBuffer& umpMessages) override
        {
            recordedPackets.addPackets (umpMessages, 0, -1, 0);
            packetStorage.push_back (umpMessages.data.begin());
        }

        MidiFormat getMidiFormat() const override                     { return recordedFormat; }
        const String getName() const override                         { return "MidiRecorder"; }
        void prepareToPlay (double, int) override                     {}
        void releaseResources() override                              {}
        double getTailLengthSeconds() const override                  { return 0.0; }
        bool acceptsMidi() const override                             { return true; }
        bool producesMidi() const override                            { return false; }
        AudioProcessorEditor* createEditor() override                 { return nullptr; }
        bool hasEditor() const override                               { return false; }
        int getNumPrograms() override                                 { return 1; }
        int getCurrentProgram() override                              { return 0; }
        void setCurrentProgram (int) override                         {}
        const String getProgramName (int) override                    { return {}; }
        void changeProgramName (int, const String&) override          {}
        void getStateInformation (MemoryBlock&) override              {}
        void setStateInformation (const void*, int) override          {}

        const MidiFormat recordedFormat;
        MidiBuffer recordedMidi;
        UMPBuffer recordedPackets;
        std::vector<const uint8*> midiStorage;
        std::vector<const uint32_t*> packetStorage;
    };

    AudioProcessorPlayerTests()
        : UnitTest ("AudioProcessorPlayer", UnitTestCategories::audio) {}

    void runTest() override
    {
        beginTest ("Buffers are prepared correctly for a variety of channel layouts");
        {
            const Layout processorLayouts[] { Layout { 0, 0 },
                                              Layout { 1, 1 },
                                              Layout { 4, 4 },
                                              Layout { 4, 8 },
                                              Layout { 8, 4 } };

            const Layout systemLayouts[] { Layout { 0, 1 },
                                           Layout { 0, 2 },
                                           Layout { 1, 1 },
                                           Layout { 1, 2 },
                                           Layout { 1, 0 },
                                           Layout { 2, 2 },
                                           Layout { 2, 0 } };

            for (const auto& processorLayout : processorLayouts)
            {
                for (const auto& systemLayout : systemLayouts)
                    runTest (systemLayout, processorLayout);
            }
        }

        beginTest ("A processor that uses a MidiBuffer receives injected messages as before");
        {
            MidiRecorder recorder { MidiFormat::midiBuffer };
            TestDevice device;
            AudioProcessorPlayer player;
            player.setProcessor (&recorder);
            player.audioDeviceAboutToStart (&device);

            const auto messages = getTestMessages (60);
            addMessages (player.getMidiMessageCollector(), messages);
            processNextBlock (player);

            expectEquals (recorder.recordedMidi.getNumEvents(), (int) messages.size());
            expect (recorder.recordedMidi.data == getExpectedMidi (messages).data);
            expect (recorder.recordedPackets.isEmpty());

            player.audioDeviceStopped();
            player.setProcessor (nullptr);
        }

        for (const auto format : { MidiFormat::umpMidi1, MidiFormat::umpMidi2 })
        {
            const auto isMidi2 = format == MidiFormat::umpMidi2;

            beginTest (String ("A processor that uses MIDI ") + (isMidi2 ? "2.0" : "1.0")
                       + " packets receives injected messages in that protocol");
            {
                MidiRecorder recorder { format };
                TestDevice device;
                AudioProcessorPlayer player;
                player.setProcessor (&recorder);
                player.audioDeviceAboutToStart (&device);

                const auto messages = getTestMessages (60);
                addMessages (player.getMidiMessageCollector(), messages);
                processNextBlock (player);

                expectEquals (recorder.recordedPackets.getNumPackets(), (int) messages.size());
                expect (recorder.recordedPackets.data == getExpectedPackets (messages, format).data);
                expect (recorder.recordedMidi.isEmpty());

                if (! recorder.recordedPackets.isEmpty())
                {
                    const auto noteOn = (*recorder.recordedPackets.begin()).packet;
                    const auto expectedKind = isMidi2 ? ump::Utils::MessageKind::channelVoice2
                                                      : ump::Utils::MessageKind::channelVoice1;
                    expect (ump::Utils::getMessageType (noteOn[0]) == expectedKind);
                }

                player.audioDeviceStopped();
                player.setProcessor (nullptr);
            }
        }

        beginTest ("A processor that replaces one of another format keeps receiving messages");
        {
            ScopedJuceInitialiser_GUI libraryInitialiser;
            AudioDeviceManager manager;

            MidiRecorder first { MidiFormat::midiBuffer },
                         second { MidiFormat::umpMidi2 },
                         third { MidiFormat::umpMidi1 },
                         fourth { MidiFormat::midiBuffer };

            TestDevice device;
            AudioProcessorPlayer player;
            player.setMidiInputDeviceManager (&manager);
            player.audioDeviceAboutToStart (&device);

            const auto play = [&] (MidiRecorder& recorder, int noteNumber)
            {
                player.setProcessor (&recorder);
                const auto messages = getTestMessages (noteNumber);
                addMessages (player.getMidiMessageCollector(), messages);
                processNextBlock (player);
                return messages;
            };

            const auto firstMessages  = play (first, 60);
            const auto secondMessages = play (second, 61);
            const auto thirdMessages  = play (third, 62);
            const auto fourthMessages = play (fourth, 63);

            expectEquals (first.recordedMidi.getNumEvents(), (int) firstMessages.size());
            expectEquals (second.recordedPackets.getNumPackets(), (int) secondMessages.size());
            expectEquals (third.recordedPackets.getNumPackets(), (int) thirdMessages.size());
            expectEquals (fourth.recordedMidi.getNumEvents(), (int) fourthMessages.size());

            expect (first.recordedMidi.data == getExpectedMidi (firstMessages).data);
            expect (second.recordedPackets.data == getExpectedPackets (secondMessages, MidiFormat::umpMidi2).data);
            expect (third.recordedPackets.data == getExpectedPackets (thirdMessages, MidiFormat::umpMidi1).data);
            expect (fourth.recordedMidi.data == getExpectedMidi (fourthMessages).data);

            player.audioDeviceStopped();
            player.setProcessor (nullptr);
            player.setMidiInputDeviceManager (nullptr);
        }

        beginTest ("The registration with the manager follows the processor's format");
        {
            ScopedJuceInitialiser_GUI libraryInitialiser;
            AudioDeviceManager manager;

            MidiRecorder midi2 { MidiFormat::umpMidi2 },
                         midi1 { MidiFormat::umpMidi1 },
                         legacy { MidiFormat::midiBuffer };

            TestDevice device;
            AudioProcessorPlayer player;
            player.audioDeviceAboutToStart (&device);
            player.setMidiInputDeviceManager (&manager);

            expect (player.midiInputDeviceManager == &manager);
            expect (player.midiInputFormat == MidiFormat::midiBuffer);

            for (auto* recorder : { &midi2, &midi1, &legacy, &midi1 })
            {
                player.setProcessor (recorder);
                expect (player.midiInputFormat == recorder->getMidiFormat());

                player.audioDeviceAboutToStart (&device);
                expect (player.midiInputFormat == recorder->getMidiFormat());
            }

            player.setProcessor (nullptr);
            expect (player.midiInputFormat == MidiFormat::umpMidi1);

            player.audioDeviceStopped();
            player.setMidiInputDeviceManager (nullptr);
            expect (player.midiInputDeviceManager == nullptr);
        }

        const auto none = ump::Factory::NoteAttributeKind::none;
        const auto pitch = ump::Factory::NoteAttributeKind::pitch7_9;

        const auto midi1Words = makeWords (ump::Factory::makeNoteOnV1 (0, 1, 60, 100),
                                           ump::Factory::makeControlChangeV1 (0, 1, 74, 90),
                                           ump::Factory::makeNoteOffV1 (0, 1, 60, 64));

        const auto midi2Words = makeWords (ump::Factory::makeNoteOnV2 (0, 1, 60, pitch, 0x1234, 0x5678),
                                           ump::Factory::makeControlChangeV2 (0, 1, 74, 0x89abcdef),
                                           ump::Factory::makeNoteOffV2 (0, 1, 60, none, 0x4321, 0));

        beginTest ("Packets from the manager in the processor's protocol reach it unchanged");
        {
            expect (getWords (receiveFromManager (MidiFormat::umpMidi1, midi1Words)) == midi1Words);
            expect (getWords (receiveFromManager (MidiFormat::umpMidi2, midi2Words)) == midi2Words);
        }

        beginTest ("Active sense from the manager isn't passed to the processor");
        {
            const auto activeSense = ump::Factory::makeActiveSensing (0);
            const auto noteOn1 = ump::Factory::makeNoteOnV1 (0, 0, 60, 100);
            const auto noteOn2 = ump::Factory::makeNoteOnV2 (0, 0, 60, none, 0x8000, 0);

            expect (getWords (receiveFromManager (MidiFormat::umpMidi1, makeWords (activeSense, noteOn1, activeSense)))
                    == makeWords (noteOn1));
            expect (getWords (receiveFromManager (MidiFormat::umpMidi2, makeWords (activeSense, noteOn2, activeSense)))
                    == makeWords (noteOn2));
        }

        beginTest ("MIDI 1.0 packets from a device that can't provide MIDI 2.0 are translated once");
        {
            std::vector<uint32_t> expected;
            ump::GenericUMPConverter converter { ump::PacketProtocol::MIDI_2_0 };
            converter.convert (ump::Iterator (midi1Words.data(), midi1Words.size()),
                               ump::Iterator (midi1Words.data() + midi1Words.size(), 0),
                               [&] (const ump::View& packet)
                               {
                                   expected.insert (expected.end(), packet.begin(), packet.end());
                               });

            const auto received = receiveFromManager (MidiFormat::umpMidi2, midi1Words);
            expectEquals (received.getNumPackets(), 3);
            expect (getWords (received) == expected);
        }

        beginTest ("Messages waiting when a processor is replaced reach the next one");
        {
            MidiRecorder legacy { MidiFormat::midiBuffer },
                         midi2 { MidiFormat::umpMidi2 },
                         legacyAgain { MidiFormat::midiBuffer };

            TestDevice device;
            AudioProcessorPlayer player;
            player.setProcessor (&legacy);
            player.audioDeviceAboutToStart (&device);

            const auto messages = getTestMessages (60);
            addMessages (player.getMidiMessageCollector(), messages);
            player.setProcessor (&midi2);
            processNextBlock (player);

            expect (legacy.recordedMidi.isEmpty());
            expectEquals (midi2.recordedPackets.getNumPackets(), (int) messages.size());
            expect (midi2.recordedPackets.data == getExpectedPackets (messages, MidiFormat::umpMidi2).data);

            consume (player.midi2Input, midi2Words);
            player.setProcessor (&legacyAgain);
            processNextBlock (player);

            MidiBuffer expected;
            ump::ToBytestreamConverter converter { 64 };
            makeBuffer (midi2Words).addToMidiBuffer (expected, converter);

            expectEquals (legacyAgain.recordedMidi.getNumEvents(), 3);
            expect (legacyAgain.recordedMidi.data == expected.data);

            player.audioDeviceStopped();
            player.setProcessor (nullptr);
        }

        beginTest ("Packets waiting when a processor changes between MIDI 1.0 and MIDI 2.0 packets are discarded");
        {
            MidiRecorder midi2 { MidiFormat::umpMidi2 },
                         midi1 { MidiFormat::umpMidi1 },
                         midi2Again { MidiFormat::umpMidi2 };

            TestDevice device;
            AudioProcessorPlayer player;
            player.setProcessor (&midi2);
            player.audioDeviceAboutToStart (&device);

            consume (player.midi2Input, midi2Words);
            player.setProcessor (&midi1);
            processNextBlock (player);
            expect (midi1.recordedPackets.isEmpty());

            consume (player.midi1Input, midi1Words);
            player.setProcessor (&midi2Again);
            processNextBlock (player);
            expect (midi2Again.recordedPackets.isEmpty());

            consume (player.midi2Input, midi2Words);
            processNextBlock (player);
            expect (getWords (midi2Again.recordedPackets) == midi2Words);

            player.audioDeviceStopped();
            player.setProcessor (nullptr);
        }

        beginTest ("The buffers passed to a processor keep the storage that they had when the device started");
        {
            for (const auto format : { MidiFormat::midiBuffer, MidiFormat::umpMidi2 })
            {
                MidiRecorder recorder { format };
                TestDevice device;
                AudioProcessorPlayer player;
                player.setProcessor (&recorder);
                player.audioDeviceAboutToStart (&device);

                const auto* midiStorage = player.incomingMidi.data.begin();
                const auto* packetStorage = player.incomingPackets.data.begin();
                expect (midiStorage != nullptr && packetStorage != nullptr);

                auto numSent = 0;

                // Each block holds more than the one before, so a buffer that grows as needed would move
                for (const auto numCopies : { 1, 4, 20 })
                {
                    for (auto i = 0; i < numCopies; ++i)
                    {
                        const auto messages = getTestMessages (60 + i);
                        addMessages (player.getMidiMessageCollector(), messages);
                        numSent += (int) messages.size();
                    }

                    processNextBlock (player);
                }

                const auto isStorage = [] (const auto& pointers, const auto* storage)
                {
                    return pointers.size() == 3
                           && std::all_of (pointers.begin(), pointers.end(), [&] (auto p) { return p == storage; });
                };

                if (format == MidiFormat::midiBuffer)
                {
                    expectEquals (recorder.recordedMidi.getNumEvents(), numSent);
                    expect (isStorage (recorder.midiStorage, midiStorage));
                }
                else
                {
                    expectEquals (recorder.recordedPackets.getNumPackets(), numSent);
                    expect (isStorage (recorder.packetStorage, packetStorage));
                }

                expect (player.incomingMidi.data.begin() == midiStorage);
                expect (player.incomingPackets.data.begin() == packetStorage);

                player.audioDeviceStopped();
                player.setProcessor (nullptr);
            }
        }
    }

    void runTest (Layout systemLayout, Layout processorLayout)
    {
        const int numSamples = 256;
        const auto systemIns = getTestBuffer (systemLayout.numIns, numSamples);
        auto systemOuts = getTestBuffer (systemLayout.numOuts, numSamples);
        AudioBuffer<float> tempBuffer (jmax (processorLayout.numIns, processorLayout.numOuts), numSamples);
        std::vector<float*> channels ((size_t) tempBuffer.getNumChannels());

        initialiseIoBuffers ({ systemIns.getArrayOfReadPointers(),   (size_t) systemIns.getNumChannels() },
                             { systemOuts.getArrayOfWritePointers(), (size_t) systemOuts.getNumChannels() },
                             numSamples,
                             (size_t) processorLayout.numIns,
                             (size_t) processorLayout.numOuts,
                             tempBuffer,
                             channels);

        for (const auto [index, channel] : enumerate (channels, int{}))
        {
            const auto value = [&, channelIndex = index]
            {
                // Any channels past the number of processor inputs should be silent.
                if (processorLayout.numIns <= channelIndex)
                    return 0.0f;

                // If there's one input, all input channels should copy from that input.
                if (systemLayout.numIns == 1)
                    return 1.0f;

                // If there's not exactly one input, any channels past the number of system inputs should be silent.
                if (systemLayout.numIns <= channelIndex)
                    return 0.0f;

                // Otherwise, each processor input should match the corresponding system input.
                return (float) (channelIndex + 1);
            }();

            expect (FloatVectorOperations::findMinAndMax (channel, numSamples) == Range<float> (value, value));
        }
    }

    static AudioBuffer<float> getTestBuffer (int numChannels, int numSamples)
    {
        AudioBuffer<float> result (numChannels, numSamples);

        for (int i = 0; i < result.getNumChannels(); ++i)
            FloatVectorOperations::fill (result.getWritePointer (i), (float) i + 1, result.getNumSamples());

        return result;
    }

    // The messages are timestamped well before the next block, so that the collectors put them
    // all at its start, whatever the timing of the test
    static std::vector<MidiMessage> getTestMessages (int noteNumber)
    {
        const uint8 sysEx[] { 0x7e, 0x7f, 0x09, 0x01 };

        std::vector<MidiMessage> result { MidiMessage::noteOn (1, noteNumber, (uint8) 100),
                                          MidiMessage::controllerEvent (2, 7, 90),
                                          MidiMessage::pitchWheel (3, 10000),
                                          MidiMessage::createSysExMessage (sysEx, (int) std::size (sysEx)),
                                          MidiMessage::noteOff (1, noteNumber, (uint8) 64) };

        const auto time = Time::getMillisecondCounterHiRes() * 0.001 - 10.0;

        for (auto& message : result)
            message.setTimeStamp (time);

        return result;
    }

    static void addMessages (MidiMessageCollector& collector, const std::vector<MidiMessage>& messages)
    {
        for (const auto& message : messages)
            collector.addMessageToQueue (message);
    }

    // Returns the block that a MidiMessageCollector makes from the messages on its own
    static MidiBuffer getExpectedMidi (const std::vector<MidiMessage>& messages)
    {
        MidiMessageCollector collector;
        collector.reset (TestDevice::sampleRate);
        addMessages (collector, messages);

        MidiBuffer result;
        collector.removeNextBlockOfMessages (result, TestDevice::blockSize);
        return result;
    }

    static UMPBuffer getExpectedPackets (const std::vector<MidiMessage>& messages, MidiFormat format)
    {
        ump::GenericUMPConverter converter { format == MidiFormat::umpMidi2 ? ump::PacketProtocol::MIDI_2_0
                                                                           : ump::PacketProtocol::MIDI_1_0 };
        UMPBuffer result;
        result.addFromMidiBuffer (getExpectedMidi (messages), converter);
        return result;
    }

    static void consume (ump::Consumer& consumer, const std::vector<uint32_t>& words)
    {
        consumer.consume (ump::Iterator (words.data(), words.size()),
                          ump::Iterator (words.data() + words.size(), 0),
                          Time::getMillisecondCounterHiRes() * 0.001 - 10.0);
    }

    // Passes the words to the player in the way that the manager would for a processor of the
    // given format, and returns the packets that reach the processor in the next block
    static UMPBuffer receiveFromManager (MidiFormat format, const std::vector<uint32_t>& words)
    {
        MidiRecorder recorder { format };
        TestDevice device;
        AudioProcessorPlayer player;
        player.setProcessor (&recorder);
        player.audioDeviceAboutToStart (&device);

        consume (format == MidiFormat::umpMidi2 ? player.midi2Input : player.midi1Input, words);
        processNextBlock (player);

        player.audioDeviceStopped();
        player.setProcessor (nullptr);
        return recorder.recordedPackets;
    }

    static UMPBuffer makeBuffer (const std::vector<uint32_t>& words)
    {
        UMPBuffer result;

        for (const auto& packet : makeRange (ump::Iterator (words.data(), words.size()),
                                             ump::Iterator (words.data() + words.size(), 0)))
            result.addPacket (packet, 0);

        return result;
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

    static void processNextBlock (AudioProcessorPlayer& player)
    {
        AudioBuffer<float> outputs (2, TestDevice::blockSize);
        player.audioDeviceIOCallbackWithContext (nullptr, 0, outputs.getArrayOfWritePointers(), 2,
                                                 TestDevice::blockSize, {});
    }
};

static AudioProcessorPlayerTests audioProcessorPlayerTests;

#endif

} // namespace juce
