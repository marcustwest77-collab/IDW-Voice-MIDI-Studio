#pragma once
#include <juce_core/juce_core.h>
#include "RemoteCommandValidation.h"
#include <set>

namespace idw::daw::remote {
// Nonsecret local setup only. Never loads executable paths, tokens or grants.
inline juce::Result validateLocalTestSetup(const juce::var& value) {
    auto* object=value.getDynamicObject();
    const std::set<juce::String> fields{"version","peer","tls_port","server_cert",
        "server_key","client_ca","client_pin","approved_requests"};
    if (!object || object->getProperties().size()!=static_cast<int>(fields.size()))
        return juce::Result::fail("The setup has missing or unsupported fields.");
    for (int i=0;i<object->getProperties().size();++i)
        if (!fields.count(object->getProperties().getName(i).toString()))
            return juce::Result::fail("Only local connection settings are allowed.");
    if (!value["version"].isInt() || static_cast<int>(value["version"])!=1)
        return juce::Result::fail("The setup version must be 1.");
    if (!value["peer"].isString() || !validId(value["peer"].toString().toStdString()))
        return juce::Result::fail("Use a collaborator ID with 1-64 letters, numbers, underscores or hyphens.");
    const auto port=value["tls_port"];
    if (!(port.isInt() || port.isInt64()) || static_cast<juce::int64>(port)<1024 ||
        static_cast<juce::int64>(port)>65535)
        return juce::Result::fail("The local port must be an integer from 1024 to 65535.");
    const auto pin=value["client_pin"].toString();
    if (!value["client_pin"].isString() || pin.length()!=64 ||
        !pin.containsOnly("0123456789abcdef"))
        return juce::Result::fail("The client certificate fingerprint must contain 64 lowercase hex characters.");
    for (const auto* name : {"server_cert","server_key","client_ca"}) {
        const auto path=value[name].toString();
        if (!value[name].isString() || path.length()>4096 ||
            !juce::File::isAbsolutePath(path) || !juce::File(path).existsAsFile())
            return juce::Result::fail("Choose existing absolute paths for the server certificate, key and client CA.");
    }
    const auto* requests=value["approved_requests"].getArray();
    if (!requests || requests->isEmpty() || requests->size()>32)
        return juce::Result::fail("List 1-32 request IDs for this local test.");
    std::set<std::string> unique;
    for (const auto& request : *requests) {
        const auto id=request.toString().toStdString();
        if (!request.isString() || !validId(id) || !unique.insert(id).second)
            return juce::Result::fail("Request IDs must be valid and unique.");
    }
    return juce::Result::ok();
}
}
