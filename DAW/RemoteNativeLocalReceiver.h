#pragma once
// Include before platform GUI headers on Windows (winsock2 must precede windows.h).
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#include "RemoteNativeLocalAuthenticator.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace idw::daw::remote {
// Local IPC only. Does not establish TLS identity or grant host permission.
// Lifecycle calls belong to the owner thread; the worker only enqueues data.
class NativeLocalReceiver {
#ifdef _WIN32
    using Socket=SOCKET;
    static constexpr Socket invalid=INVALID_SOCKET;
#else
    using Socket=int;
    static constexpr Socket invalid=-1;
#endif
public:
    explicit NativeLocalReceiver(TrustedTransportEventQueue& queue) : queue_(queue) {}
    ~NativeLocalReceiver() { stop(); }
    NativeLocalReceiver(const NativeLocalReceiver&)=delete;
    NativeLocalReceiver& operator=(const NativeLocalReceiver&)=delete;

    bool start(const std::string& token, const std::string& peer, std::uint64_t epoch) {
        if (worker_.joinable()) return false;
        auto auth=std::make_unique<NativeLocalAuthenticator>(token,peer,epoch,queue_);
        if (!auth->configured()) return false;
#ifdef _WIN32
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2,2),&data)!=0) return false;
        winsockStarted_=true;
#endif
        listener_=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        if (listener_==invalid) { cleanup(); return false; }
        sockaddr_in address{};
        address.sin_family=AF_INET;
        address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        address.sin_port=0; // OS-assigned, loopback only; never a public listener.
        if (::bind(listener_,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0 ||
            ::listen(listener_,4)!=0 || !nonblocking(listener_)) { cleanup(); return false; }
#ifdef _WIN32
        int length=sizeof(address);
#else
        socklen_t length=sizeof(address);
#endif
        if (::getsockname(listener_,reinterpret_cast<sockaddr*>(&address),&length)!=0) {
            cleanup(); return false;
        }
        port_=ntohs(address.sin_port);
        epoch_=epoch;
        auth_=std::move(auth);
        stopping_.store(false);
        try { worker_=std::thread([this] { run(); }); }
        catch (...) { cleanup(); auth_.reset(); port_=0; epoch_=0; return false; }
        return true;
    }
    unsigned short port() const { return port_; }
    std::uint64_t epoch() const { return epoch_; }
    void stop() {
        stopping_.store(true);
        if (worker_.joinable()) worker_.join();
        // Revoke after the worker stops so no later command can race this event.
        if (epoch_!=0)
            queue_.push({TrustedTransportEvent::Kind::disconnected,epoch_,false,false,false,{}});
        epoch_=0;
        port_=0;
        auth_.reset();
        cleanup();
    }
private:
    static void closeSocket(Socket socket) {
#ifdef _WIN32
        closesocket(socket);
#else
        ::close(socket);
#endif
    }
    static bool nonblocking(Socket socket) {
#ifdef _WIN32
        u_long enabled=1;
        return ioctlsocket(socket,FIONBIO,&enabled)==0;
#else
        const auto flags=fcntl(socket,F_GETFL,0);
        return flags>=0 && fcntl(socket,F_SETFL,flags|O_NONBLOCK)==0;
#endif
    }
    static bool readable(Socket socket) {
#ifndef _WIN32
        if (socket>=FD_SETSIZE) return false;
#endif
        fd_set set;
        FD_ZERO(&set);
        FD_SET(socket,&set);
        timeval timeout{0,50000};
        return ::select(static_cast<int>(socket)+1,&set,nullptr,nullptr,&timeout)>0;
    }
    bool readExact(Socket socket, char* output, std::size_t size,
                   std::chrono::steady_clock::time_point deadline) {
        std::size_t count=0;
        while (count<size && !stopping_.load() && std::chrono::steady_clock::now()<deadline) {
            if (!readable(socket)) continue;
            const auto received=::recv(socket,output+count,static_cast<int>(size-count),0);
            if (received<=0) return false;
            count+=static_cast<std::size_t>(received);
        }
        return count==size && !stopping_.load();
    }
    void handle(Socket socket) {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
        unsigned char header[4]{};
        if (!readExact(socket,reinterpret_cast<char*>(header),4,deadline)) return;
        const std::uint32_t size=(std::uint32_t(header[0])<<24) |
            (std::uint32_t(header[1])<<16) | (std::uint32_t(header[2])<<8) | header[3];
        if (size==0 || size>4096) return;
        std::string packet(size,'\0');
        if (!readExact(socket,packet.data(),size,deadline)) return;
        const char result=auth_->accept(packet) ? '1' : '0';
#ifdef MSG_NOSIGNAL
        ::send(socket,&result,1,MSG_NOSIGNAL);
#else
        ::send(socket,&result,1,0);
#endif
    }
    void run() {
        while (!stopping_.load()) {
            if (!readable(listener_)) continue;
            const auto connection=::accept(listener_,nullptr,nullptr);
            if (connection==invalid) continue;
#ifdef SO_NOSIGPIPE
            int enabled=1;
            setsockopt(connection,SOL_SOCKET,SO_NOSIGPIPE,&enabled,sizeof(enabled));
#endif
            if (nonblocking(connection)) handle(connection);
            closeSocket(connection);
        }
    }
    void cleanup() {
        if (listener_!=invalid) { closeSocket(listener_); listener_=invalid; }
#ifdef _WIN32
        if (winsockStarted_) { WSACleanup(); winsockStarted_=false; }
#endif
    }
    TrustedTransportEventQueue& queue_;
    std::unique_ptr<NativeLocalAuthenticator> auth_;
    Socket listener_=invalid;
    std::thread worker_;
    std::atomic<bool> stopping_{true};
    unsigned short port_=0;
    std::uint64_t epoch_=0;
#ifdef _WIN32
    bool winsockStarted_=false;
#endif
};
}
