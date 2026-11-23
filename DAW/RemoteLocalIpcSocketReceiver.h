#pragma once
// Opt-in native localhost-only IPC receiver. It never sends DAW mutations.
// Explicit host startup plus a 256-bit pre-provisioned token are required.
// Packet: big-endian uint32 length, 64 lowercase hex token, LF, versioned command.
// Each TCP connection processes at most one packet. No external bind or TLS.
#include <JuceHeader.h>
#include "RemoteLocalIpcAuthenticator.h"
#include "RemoteVerifiedLocalCommandQueueAdapter.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

namespace idw::daw::remote {
class LocalIpcSocketReceiver {
public:
    LocalIpcSocketReceiver(TrustedTransportEventQueue& queue,
                           unsigned short port, std::string secret,
                           std::uint64_t epoch)
        : queue_(queue), port_(port), epoch_(epoch),
          auth_(std::move(secret),epoch) {}
    ~LocalIpcSocketReceiver() { stop(); }
    LocalIpcSocketReceiver(const LocalIpcSocketReceiver&)=delete;
    LocalIpcSocketReceiver& operator=(const LocalIpcSocketReceiver&)=delete;
    bool start() {
        if (!auth_.valid() || port_<1024 || worker_.joinable()) return false;
        stop_.store(false);
        worker_=std::thread([this]{ run(); });
        return true; // means worker started, NOT that bind or connection succeeded
    }
    void stop() {
        stop_.store(true);
        if (worker_.joinable()) worker_.join();
    }
    bool listening() const {return listening_.load();}
private:
    bool readExact(juce::StreamingSocket& socket, void* dest, int length) {
        auto* out=static_cast<char*>(dest);
        int offset=0;
        const auto started=juce::Time::getMillisecondCounter();
        while (!stop_.load() && offset<length &&
               juce::Time::getMillisecondCounter()-started<2500) {
            const int ready=socket.waitUntilReady(true,100);
            if (ready<0) return false;
            if (ready==0) continue;
            const int count=socket.read(out+offset,length-offset,false);
            if (count<=0) return false;
            offset+=count;
        }
        return offset==length && !stop_.load();
    }
    void run() {
        juce::StreamingSocket listener;
        // NEVER use a wildcard address, even as a fallback.
        if (!listener.createListener(static_cast<int>(port_),"127.0.0.1"))
            return;
        listening_.store(true);
        VerifiedLocalCommandQueueAdapter adapter(queue_);
        bool admitted=false;
        while (!stop_.load()) {
            if (listener.waitUntilReady(true,100)<=0) continue;
            std::unique_ptr<juce::StreamingSocket> client(listener.waitForNextConnection());
            if (!client) continue;
            unsigned char lengthBytes[4]={};
            if (!readExact(*client,lengthBytes,4)) continue;
            const std::uint32_t length=
                (static_cast<std::uint32_t>(lengthBytes[0])<<24) |
                (static_cast<std::uint32_t>(lengthBytes[1])<<16) |
                (static_cast<std::uint32_t>(lengthBytes[2])<<8) |
                static_cast<std::uint32_t>(lengthBytes[3]);
            if (length<66 || length>4096) {
                client->write("0",1);
                continue;
            }
            std::string payload(length,'\0'),wire;
            if (!readExact(*client,payload.data(),static_cast<int>(length)) ||
                !auth_.accept(payload,wire)) {
                client->write("0",1);
                continue;
            }
            // No external caller can set the trusted session flags.
            if (!admitted) {
                TrustedTransportEvent connected;
                connected.kind=TrustedTransportEvent::Kind::connected;
                connected.epoch=epoch_;
                connected.localTokenVerified=true;
                connected.hostConsented=true; // explicit local host START action
                connected.trackControlGranted=true; // host must still review each action
                if (!queue_.push(std::move(connected))) {
                    client->write("0",1);
                    continue;
                }
                admitted=true;
            }
            const bool queued=adapter.submitValidatedCommand(wire,epoch_);
            client->write(queued?"1":"0",1);
        }
        if (admitted) {
            TrustedTransportEvent disconnected;
            disconnected.kind=TrustedTransportEvent::Kind::disconnected;
            disconnected.epoch=epoch_;
            queue_.push(std::move(disconnected));
        }
        listening_.store(false);
    }
    TrustedTransportEventQueue& queue_;
    unsigned short port_;
    std::uint64_t epoch_;
    LocalIpcAuthenticator auth_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> listening_{false};
    std::thread worker_;
};
} // namespace idw::daw::remote
