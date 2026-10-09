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

MidiDeviceListConnection MidiDeviceListConnection::make (std::function<void()> callback)
{
    auto& broadcaster = MidiDeviceListConnectionBroadcaster::get();

    const auto key = broadcaster.add (std::move (callback));

    MidiDeviceListConnection result;
    result.token = ErasedScopeGuard { [&broadcaster, key] { broadcaster.remove (key); } };
    return result;
}

//==============================================================================
static std::shared_ptr<ump::Session> getLegacySession()
{
    static std::weak_ptr<ump::Session> weak;

    if (auto strong = weak.lock())
        return strong;

    if (auto session = ump::Endpoints::getInstance()->makeSession (ump::Endpoints::Impl::getGlobalMidiClientName()))
    {
        auto strong = std::make_shared<ump::Session> (std::move (session));
        weak = strong;
        return strong;
    }

    return nullptr;
}

//==============================================================================
/*  The ump::Consumer attached to one of the connections of a MidiInput.

    While it is active, it passes the packets for the input's group on to the ump::Consumer
    objects that were added to it, with adjacent packets passed on together. The adapter for the
    MIDI 1.0 connection also converts the packets to bytestream messages for the
    MidiInputCallback listeners, in the same way that MidiInput always has.

    The adapter holds no device state, so that it can be tested without a device.
*/
class MidiInputConnectionAdapter final : public ump::Consumer
{
public:
    /*  Creates an adapter that only passes packets on to ump::Consumer objects. */
    explicit MidiInputConnectionAdapter (uint8_t groupIn)
        : group (groupIn)
    {
    }

    /*  Creates an adapter that also passes bytestream messages to `callbacksIn`, giving
        `sourceIn` as their source.
    */
    MidiInputConnectionAdapter (uint8_t groupIn,
                                MidiInput* sourceIn,
                                const WaitFreeListeners<MidiInputCallback>& callbacksIn)
        : group (groupIn),
          source (sourceIn),
          callbacks (&callbacksIn),
          converter (std::in_place, 4096)
    {
    }

    void setActive (bool shouldBeActive)
    {
        const SpinLock::ScopedLockType lock { spinLock };
        active = shouldBeActive;
    }

    /*  Must only be called on the message thread. Adding a consumer that is already present has
        no effect.
    */
    void addConsumer (ump::Consumer& c)
    {
        consumers.add (c);
    }

    /*  Must only be called on the message thread. If `c` is being called, this waits for that
        call to return.
    */
    void removeConsumer (ump::Consumer& c)
    {
        consumers.remove (c);
    }

    bool hasConsumers() const
    {
        return ! consumers.isEmpty();
    }

    void consume (ump::Iterator b, ump::Iterator e, double time) override
    {
        const SpinLock::ScopedTryLockType lock { spinLock };

        if (! lock.isLocked() || ! active)
            return;

        if (callbacks != nullptr)
        {
            for (const auto& view : makeRange (b, e))
            {
                if (ump::Utils::getGroup (view[0]) != group)
                    continue;

                converter->convert (view, time, [this] (ump::BytesOnGroup v, double t)
                {
                    const MidiMessage msg { v.bytes.data(), (int) v.bytes.size(), t };

                    callbacks->call ([&] (MidiInputCallback& l)
                    {
                        l.handleIncomingMidiMessage (source, msg);
                    });
                });
            }
        }

        consumers.call ([&] (ump::Consumer& c)
        {
            forEachRunOnGroup (b, e, [&] (ump::Iterator first, ump::Iterator last)
            {
                c.consume (first, last, time);
            });
        });
    }

private:
    /*  Calls `callback` with each run of adjacent packets in [b, e) that belong to this adapter's
        group. Utility and stream messages have no group, so they are never passed on, just as
        they never produce bytestream messages.
    */
    template <typename Callback>
    void forEachRunOnGroup (ump::Iterator b, ump::Iterator e, Callback&& callback) const
    {
        const auto isOnGroup = [this] (const ump::View& view)
        {
            const auto firstWord = view[0];

            return ump::Utils::hasGroup (ump::Utils::getMessageType (firstWord))
                   && ump::Utils::getGroup (firstWord) == group;
        };

        for (auto it = b; it != e;)
        {
            const auto runStart = std::find_if (it, e, isOnGroup);
            const auto runEnd = std::find_if_not (runStart, e, isOnGroup);

            if (runStart != runEnd)
                callback (runStart, runEnd);

            it = runEnd;
        }
    }

    uint8_t group{};
    MidiInput* source = nullptr;
    const WaitFreeListeners<MidiInputCallback>* callbacks = nullptr;
    std::optional<ump::ToBytestreamConverter> converter;
    WaitFreeListeners<ump::Consumer> consumers;
    SpinLock spinLock;
    bool active = false;

    JUCE_DECLARE_NON_COPYABLE (MidiInputConnectionAdapter)
};

//==============================================================================
class MidiInput::Impl : private ump::DisconnectionListener
{
public:
    void start()
    {
        midi1Adapter.setActive (true);
        midi2Adapter.setActive (true);
    }

    void stop()
    {
        midi1Adapter.setActive (false);
        midi2Adapter.setActive (false);
    }

    MidiDeviceInfo getDeviceInfo() const noexcept
    {
        return customName.has_value() ? storedInfo.withName (*customName) : storedInfo;
    }

    void setName (String x)
    {
        customName = std::move (x);
    }

    void addCallback (MidiInputCallback& cb)
    {
        callbacks.add (cb);
    }

    void removeCallback (MidiInputCallback& cb)
    {
        callbacks.remove (cb);
    }

    bool addConsumer (ump::Consumer& c, ump::PacketProtocol wanted)
    {
        JUCE_ASSERT_MESSAGE_THREAD

        const auto useMidi2Connection = wanted == ump::PacketProtocol::MIDI_2_0 && openMidi2Connection();
        auto& adapter = useMidi2Connection ? midi2Adapter : midi1Adapter;
        auto& otherAdapter = useMidi2Connection ? midi1Adapter : midi2Adapter;

        // Adding a consumer that is already present has no effect, so a consumer that stays on the
        // same connection doesn't miss any packets
        otherAdapter.removeConsumer (c);
        adapter.addConsumer (c);

        if (! midi2Adapter.hasConsumers())
            closeMidi2Connection();

        return useMidi2Connection || wanted == ump::PacketProtocol::MIDI_1_0;
    }

    void removeConsumer (ump::Consumer& c)
    {
        JUCE_ASSERT_MESSAGE_THREAD

        midi1Adapter.removeConsumer (c);
        midi2Adapter.removeConsumer (c);

        if (! midi2Adapter.hasConsumers())
            closeMidi2Connection();
    }

    /*  session may be null, in which case it's up to the caller to ensure that the session lives
        long enough for the connection to be useful. Without a session, consumers that want
        MIDI 2.0 are served from the MIDI 1.0 connection.
    */
    static std::unique_ptr<MidiInput> make (std::shared_ptr<ump::Session> session,
                                            ump::Input connection,
                                            uint8_t group,
                                            const MidiDeviceInfo& info,
                                            MidiInputCallback* cb,
                                            ump::LegacyVirtualInput virtualEndpoint)
    {
        auto result = rawToUniquePtr (new MidiInput);
        result->pimpl = rawToUniquePtr (new Impl (session,
                                                  std::move (connection),
                                                  group,
                                                  result.get(),
                                                  info,
                                                  std::move (virtualEndpoint)));

        if (cb != nullptr)
            result->addCallback (*cb);

        return result;
    }

    uint8_t getGroup() const
    {
        return group;
    }

    ump::EndpointId getEndpointId() const
    {
        return connection.getEndpointId();
    }

    void addDisconnectionListener (DisconnectionListener& l)
    {
        JUCE_ASSERT_MESSAGE_THREAD
        disconnectionListeners.add (&l);
    }

    void removeDisconnectionListener (DisconnectionListener& l)
    {
        JUCE_ASSERT_MESSAGE_THREAD
        disconnectionListeners.remove (&l);
    }

    ~Impl() override
    {
        closeMidi2Connection();
        connection.removeDisconnectionListener (*this);
        connection.removeConsumer (midi1Adapter);
    }

private:
    Impl (std::shared_ptr<ump::Session> s,
          ump::Input x,
          uint8_t g,
          MidiInput* o,
          MidiDeviceInfo i,
          ump::LegacyVirtualInput v)
        : session (s),
          virtualEndpoint (std::move (v)),
          storedInfo (i),
          group (g),
          midi1Adapter (g, o, callbacks),
          midi2Adapter (g),
          connection (std::move (x))
    {
        // The MidiInputCallback listeners are served from this connection, so it must use MIDI 1.0
        jassert (connection.getProtocol() == ump::PacketProtocol::MIDI_1_0);

        connection.addConsumer (midi1Adapter);
        connection.addDisconnectionListener (*this);
    }

    /*  Opens the MIDI 2.0 connection to the endpoint if it isn't open already, and returns true
        if it is open afterwards.
    */
    bool openMidi2Connection()
    {
        if (midi2Connection.has_value())
            return true;

        // A port made by createNewDevice() is MIDI 1.0 only, and not every backend can connect to it in MIDI 2.0
        if (virtualEndpoint.isAlive())
            return false;

        if (session == nullptr)
        {
            // A second connection can't be opened without a session, so consumers that want
            // MIDI 2.0 will be served from the MIDI 1.0 connection instead
            jassertfalse;
            return false;
        }

        if (! connection.isAlive())
            return false;

        auto newConnection = session->connectInput (connection.getEndpointId(), ump::PacketProtocol::MIDI_2_0);

        if (! newConnection.isAlive())
            return false;

        newConnection.addConsumer (midi2Adapter);
        midi2Connection = std::move (newConnection);
        return true;
    }

    void closeMidi2Connection()
    {
        if (! midi2Connection.has_value())
            return;

        midi2Connection->removeConsumer (midi2Adapter);
        midi2Connection.reset();
    }

    void disconnected() override
    {
        disconnectionListeners.call ([&] (auto& l) { l.disconnected(); });
    }

    std::shared_ptr<ump::Session> session;
    ump::LegacyVirtualInput virtualEndpoint;
    std::optional<String> customName;
    MidiDeviceInfo storedInfo;
    ListenerList<DisconnectionListener> disconnectionListeners;
    WaitFreeListeners<MidiInputCallback> callbacks;
    uint8_t group{};
    MidiInputConnectionAdapter midi1Adapter, midi2Adapter;

    // The connections are declared after the adapters attached to them, so that they are
    // destroyed first
    ump::Input connection;
    std::optional<ump::Input> midi2Connection;
};

MidiInput::MidiInput() = default;
MidiInput::~MidiInput() = default;

Array<MidiDeviceInfo> MidiInput::getAvailableDevices()
{
    Array<MidiDeviceInfo> result;
    MidiDeviceListConnectionBroadcaster::get().getAllMidiDeviceInfo (ump::IOKind::src, result);
    return result;
}

MidiDeviceInfo MidiInput::getDefaultDevice()
{
    return getAvailableDevices().getFirst();
}

std::unique_ptr<MidiInput> MidiInput::openDevice (const String& deviceIdentifier, MidiInputCallback* callback)
{
    const auto address = MidiDeviceListConnectionBroadcaster::get().getEndpointGroupForId (ump::IOKind::src, deviceIdentifier);

    if (! address.has_value())
        return {};

    const auto info = MidiDeviceListConnectionBroadcaster::get().getInfoForId (ump::IOKind::src, deviceIdentifier);

    if (! info.has_value())
        return {};

    auto session = getLegacySession();

    if (session == nullptr)
        return {};

    auto connection = session->connectInput (address->endpointId, ump::PacketProtocol::MIDI_1_0);

    if (! connection.isAlive())
        return {};

    return Impl::make (session, std::move (connection), address->group, *info, callback, {});
}

static inline bool isValidMidi1VirtualEndpoint (const std::optional<ump::Endpoint>& ep,
                                                ump::BlockDirection dir)
{
    if (! ep.has_value())
        return false;

    if (! ep->hasMidi1Support())
        return false;

    if (ep->getProtocol() != ump::PacketProtocol::MIDI_1_0)
        return false;

    if (! ep->hasStaticBlocks())
        return false;

    auto blocks = ep->getBlocks();
    const auto iter = std::find_if (blocks.begin(), blocks.end(), [&] (const ump::Block& b)
    {
        return b.getDirection() == dir
               && b.getNumGroups() == 1
               && b.isEnabled()
               && b.getMIDI1ProxyKind() != ump::BlockMIDI1ProxyKind::inapplicable;
    });

    if (iter == blocks.end())
        return false;

    return true;
}

std::unique_ptr<MidiInput> MidiInput::createNewDevice (const String& name, MidiInputCallback* callback)
{
    auto session = getLegacySession();

    if (! session)
        return {};

    auto port = session->createLegacyVirtualInput (name);

    if (! port)
        return {};

    jassert (isValidMidi1VirtualEndpoint (ump::Endpoints::getInstance()->getEndpoint (port.getId()),
                                          ump::BlockDirection::receiver));

    auto connection = session->connectInput (port.getId(), ump::PacketProtocol::MIDI_1_0);

    if (! connection)
        return {};

    const auto portId = port.getId().dst;
    return Impl::make (session, std::move (connection), 0, { name, portId }, callback, std::move (port));
}

void MidiInput::start()
{
    pimpl->start();
}

void MidiInput::stop()
{
    pimpl->stop();
}

MidiDeviceInfo MidiInput::getDeviceInfo() const noexcept
{
    return pimpl->getDeviceInfo();
}

void MidiInput::setName (const String& newName) noexcept
{
    pimpl->setName (newName);
}

uint8_t MidiInput::getGroup() const
{
    return pimpl->getGroup();
}

ump::EndpointId MidiInput::getEndpointId() const
{
    return pimpl->getEndpointId();
}

void MidiInput::addCallback (MidiInputCallback& callback)
{
    pimpl->addCallback (callback);
}

void MidiInput::removeCallback (MidiInputCallback& callback)
{
    pimpl->removeCallback (callback);
}

bool MidiInput::addConsumer (ump::Consumer& consumer, ump::PacketProtocol wanted)
{
    return pimpl->addConsumer (consumer, wanted);
}

void MidiInput::removeConsumer (ump::Consumer& consumer)
{
    pimpl->removeConsumer (consumer);
}

void MidiInput::addDisconnectionListener (ump::DisconnectionListener& l)
{
    pimpl->addDisconnectionListener (l);
}

void MidiInput::removeDisconnectionListener (ump::DisconnectionListener& l)
{
    pimpl->removeDisconnectionListener (l);
}

//==============================================================================
MidiOutput::MidiOutput (std::shared_ptr<ump::Session> s,
                        ump::Output x,
                        uint8_t g,
                        const MidiDeviceInfo& i,
                        ump::LegacyVirtualOutput v)
    : session (s),
      virtualEndpoint (std::move (v)),
      connection (std::move (x)),
      storedInfo (i),
      group (g)
{
    mainPackets.reserve (2048);
    connection.addDisconnectionListener (*this);
}

MidiOutput::~MidiOutput()
{
    connection.removeDisconnectionListener (*this);
}

Array<MidiDeviceInfo> MidiOutput::getAvailableDevices()
{
    Array<MidiDeviceInfo> result;
    MidiDeviceListConnectionBroadcaster::get().getAllMidiDeviceInfo (ump::IOKind::dst, result);
    return result;
}

std::unique_ptr<MidiOutput> MidiOutput::openDevice (const String& deviceIdentifier)
{
    const auto address = MidiDeviceListConnectionBroadcaster::get().getEndpointGroupForId (ump::IOKind::dst, deviceIdentifier);

    if (! address.has_value())
        return {};

    const auto info = MidiDeviceListConnectionBroadcaster::get().getInfoForId (ump::IOKind::dst, deviceIdentifier);

    if (! info.has_value())
        return {};

    auto session = getLegacySession();

    if (session == nullptr)
        return {};

    auto connection = session->connectOutput (address->endpointId);

    if (! connection.isAlive())
        return {};

    return rawToUniquePtr (new MidiOutput (session, std::move (connection), address->group, *info, {}));
}

std::unique_ptr<MidiOutput> MidiOutput::createNewDevice (const String& name)
{
    auto session = getLegacySession();

    if (! session)
        return {};

    auto port = session->createLegacyVirtualOutput (name);

    if (! port)
        return {};

    jassert (isValidMidi1VirtualEndpoint (ump::Endpoints::getInstance()->getEndpoint (port.getId()),
                                          ump::BlockDirection::sender));

    auto connection = session->connectOutput (port.getId());

    if (! connection)
        return {};

    const auto portId = port.getId().src;
    return rawToUniquePtr (new MidiOutput (session, std::move (connection), 0, { name, portId }, std::move (port)));
}

MidiDeviceInfo MidiOutput::getDeviceInfo() const noexcept
{
    return customName.has_value() ? storedInfo.withName (*customName) : storedInfo;
}

/*  Returns a copy of a packet that is moved to the given group, unless it's a message that has no
    group.
*/
static std::array<uint32_t, 4> copyPacketToGroup (ump::View packet, uint8_t group)
{
    std::array<uint32_t, 4> result {};
    std::copy (packet.begin(), packet.end(), result.begin());

    if (ump::Utils::hasGroup (ump::Utils::getMessageType (result[0])))
        result[0] = ump::Utils::U4<1>::set (result[0], group);

    return result;
}

bool MidiOutput::sendMessageNow (ump::View packet)
{
    const auto copy = copyPacketToGroup (packet, group);
    const ump::Iterator iterator { copy.data(), copy.size() };
    return connection.send (iterator, std::next (iterator));
}

bool MidiOutput::sendBlockOfMessagesNow (const UMPBuffer& buffer)
{
    mainPackets.clear();

    for (const auto metadata : buffer)
    {
        const auto copy = copyPacketToGroup (metadata.packet, group);
        mainPackets.add (ump::View (copy.data()));
    }

    return connection.send (mainPackets.begin(), mainPackets.end());
}

void MidiOutput::sendBlockOfMessages (const UMPBuffer& buffer,
                                      double millisecondCounterToStartAt,
                                      double samplesPerSecondForBuffer)
{
    // This needs to be a value in the future - check the documentation for this function!
    jassert (millisecondCounterToStartAt > 0);

    addScheduledMessages (outputThread, buffer, group, millisecondCounterToStartAt, samplesPerSecondForBuffer);
}

MidiOutput::ScheduledMessage MidiOutput::makeScheduledMessage (const MidiMessageMetadata& message,
                                                               uint8_t groupToUse,
                                                               uint32_t time)
{
    ScheduledMessage result { time, {}, {} };
    auto isFirstPacket = true;

    ump::ToUMP1Converter{}.convert ({ groupToUse, message.asSpan() }, [&] (const ump::View& view)
    {
        if (std::exchange (isFirstPacket, false))
            std::copy (view.begin(), view.end(), result.firstPacket.begin());
        else
            result.otherPackets.add (view);
    });

    return result;
}

MidiOutput::ScheduledMessage MidiOutput::makeScheduledMessage (const UMPPacketMetadata& metadata,
                                                               uint8_t groupToUse,
                                                               uint32_t time)
{
    return { time, copyPacketToGroup (metadata.packet, groupToUse), {} };
}

bool MidiDeviceInfo::operator== (const MidiDeviceInfo& other) const noexcept
{
    const auto tie = [] (auto& x) { return std::tuple (x.name, x.identifier); };
    return tie (*this) == tie (other);
}

} // namespace juce
