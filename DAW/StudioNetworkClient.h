#pragma once
#include <juce_events/juce_events.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <regex>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#endif

// Windows-native WSS client. No certificate bypass, redirects, auto-reconnect,
// stored tickets or command retries. Call connect/send/disconnect on message thread.
class StudioNetworkClient {
public:
    std::function<void(const juce::var&)> onMessage;
    std::function<void(const juce::String&)> onStatus;
    ~StudioNetworkClient(){disconnect();}
    static bool validOrigin(const std::string& origin){
        return std::regex_match(origin,std::regex(R"(https://[A-Za-z0-9.-]+(:[0-9]{1,5})?)")) ||
            std::regex_match(origin,std::regex(R"(http://(127\.0\.0\.1|localhost)(:[0-9]{1,5})?)"));
    }
    bool connect(const juce::String& origin,const juce::String& room,const juce::String& ticket){
        disconnect();
        if(!validOrigin(origin.toStdString()) || !std::regex_match(room.toStdString(),std::regex("[A-Za-z0-9_-]{24}")) ||
           !std::regex_match(ticket.toStdString(),std::regex("[A-Za-z0-9_-]{43}"))){
            if(onStatus)onStatus("Invalid server, room or ticket.");return false;
        }
#if JUCE_WINDOWS
        state=std::make_shared<State>();auto s=state;
        s->message=onMessage;s->status=onStatus;
        auto* auth=new juce::DynamicObject();auth->setProperty("type","join");
        auth->setProperty("room",room);auth->setProperty("ticket",ticket);
        auto json=juce::JSON::toString(juce::var(auth),true).toStdString();
        worker=std::thread([s,origin,json]{run(s,origin,json);});
        if(onStatus)onStatus("Connecting…");return true;
#else
        if(onStatus)onStatus("Studio networking is currently available on Windows only.");return false;
#endif
    }
    void disconnect(){
#if JUCE_WINDOWS
        if(state){state->alive=false;std::lock_guard<std::mutex> lock(state->mutex);
            if(state->socket){WinHttpCloseHandle(state->socket);state->socket=nullptr;}}
        if(worker.joinable())worker.join();state.reset();
#endif
    }
    bool send(const juce::var& data){
#if JUCE_WINDOWS
        auto s=state;if(!s || !s->alive)return false;
        auto text=juce::JSON::toString(data,true).toStdString();
        if(text.size()>65536)return false;
        std::lock_guard<std::mutex> lock(s->mutex);
        return s->socket && WinHttpWebSocketSend(s->socket,WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
            const_cast<char*>(text.data()),static_cast<DWORD>(text.size()))==NO_ERROR;
#else
        juce::ignoreUnused(data);return false;
#endif
    }
private:
#if JUCE_WINDOWS
    struct State {
        std::atomic<bool> alive{true};std::atomic<int> pending{0};std::mutex mutex;HINTERNET socket=nullptr;
        std::function<void(const juce::var&)> message;
        std::function<void(const juce::String&)> status;
    };
    std::shared_ptr<State> state;std::thread worker;
    struct Handle {HINTERNET value=nullptr;~Handle(){if(value)WinHttpCloseHandle(value);}};
    static void status(const std::shared_ptr<State>& s,const char* text){
        juce::MessageManager::callAsync([s,text=juce::String(text)]{if(s->alive && s->status)s->status(text);});
    }
    static void run(const std::shared_ptr<State>& s,const juce::String& origin,const std::string& auth){
        Handle session{WinHttpOpen(L"IDW-Studio/1",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
        if(!session.value){status(s,"Connection failed.");return;}
        WinHttpSetTimeouts(session.value,3000,3000,3000,3000);
        const std::wstring url=origin.toWideCharPointer();URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);
        parts.dwHostNameLength=DWORD(-1);
        if(!WinHttpCrackUrl(url.c_str(),static_cast<DWORD>(url.size()),0,&parts)){status(s,"Invalid server.");return;}
        const std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
        Handle connection{WinHttpConnect(session.value,host.c_str(),parts.nPort,0)};
        Handle request{connection.value?WinHttpOpenRequest(connection.value,L"GET",L"/v1/connect",nullptr,
            WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,parts.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0):nullptr};
        DWORD disabled=WINHTTP_DISABLE_REDIRECTS;
        if(!request.value || !WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled)) ||
           !WinHttpSetOption(request.value,WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,nullptr,0)){
            status(s,"Connection failed.");return;}
        const std::wstring headers=L"Origin: "+url+L"\r\n";
        if(!s->alive || !WinHttpSendRequest(request.value,headers.c_str(),static_cast<DWORD>(headers.size()),
            WINHTTP_NO_REQUEST_DATA,0,0,0) || !WinHttpReceiveResponse(request.value,nullptr)){
            status(s,"Connection failed. Check server and certificate.");return;}
        HINTERNET socket=WinHttpWebSocketCompleteUpgrade(request.value,0);
        if(!socket){status(s,"Server refused the studio connection.");return;}
        {
            std::lock_guard<std::mutex> lock(s->mutex);
            if(!s->alive){WinHttpCloseHandle(socket);return;}
            s->socket=socket;
            if(WinHttpWebSocketSend(socket,WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                const_cast<char*>(auth.data()),static_cast<DWORD>(auth.size()))!=NO_ERROR){
                WinHttpCloseHandle(s->socket);s->socket=nullptr;status(s,"Room authentication failed.");return;}
        }
        std::string message;char buffer[4096];
        while(s->alive){
            DWORD bytes=0;WINHTTP_WEB_SOCKET_BUFFER_TYPE kind{};
            const auto result=WinHttpWebSocketReceive(socket,buffer,sizeof(buffer),&bytes,&kind);
            if(result!=NO_ERROR || kind==WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)break;
            if(kind!=WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE && kind!=WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE)break;
            message.append(buffer,bytes);if(message.size()>65536)break;
            if(kind==WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE){
                auto value=juce::JSON::parse(juce::String::fromUTF8(message.data(),static_cast<int>(message.size())));
                if(!value.isObject())break;
                if(s->pending.fetch_add(1)>=128){--s->pending;break;}
                juce::MessageManager::callAsync([s,value]{--s->pending;if(s->alive && s->message)s->message(value);});
                message.clear();
            }
        }
        {std::lock_guard<std::mutex> lock(s->mutex);if(s->socket){WinHttpCloseHandle(s->socket);s->socket=nullptr;}}
        status(s,"Disconnected. Join again; fresh permission is required.");
    }
#endif
};
