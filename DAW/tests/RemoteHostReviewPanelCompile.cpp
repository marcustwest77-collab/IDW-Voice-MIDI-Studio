#include "../RemoteHostReviewPanel.h"
#include "../RemoteLocalTestSetup.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
// JUCE compilation and setup validation; no GUI/window or audio backend starts.
int main() {
    juce::TemporaryFile temporary(".pem");
    assert(temporary.getFile().replaceWithText("test-only certificate placeholder"));
    auto* object=new juce::DynamicObject();
    juce::var valid(object);
    object->setProperty("version",1);
    object->setProperty("peer","guest");
    object->setProperty("tls_port",49200);
    for (const auto* key : {"server_cert","server_key","client_ca"})
        object->setProperty(key,temporary.getFile().getFullPathName());
    object->setProperty("client_pin",juce::String::repeatedString("a",64));
    object->setProperty("approved_requests",juce::var(juce::Array<juce::var>{juce::var("local_arm_1")}));
    using idw::daw::remote::validateLocalTestSetup;
    assert(validateLocalTestSetup(valid).wasOk());
    auto rejects=[&](const char* key,const juce::var& value) {
        auto changed=juce::JSON::parse(juce::JSON::toString(valid));
        changed.getDynamicObject()->setProperty(key,value);
        assert(validateLocalTestSetup(changed).failed());
    };
    rejects("version",true);
    rejects("tls_port",true);
    rejects("tls_port",49200.5);
    rejects("tls_port",65536);
    rejects("peer","bad|peer");
    rejects("server_key","relative.pem");
    rejects("client_pin","invalid");
    rejects("token","never accept a stored secret");
    rejects("python_executable","never accept a stored executable");
    rejects("approved_requests",juce::var(juce::Array<juce::var>{}));
    rejects("approved_requests",juce::var(juce::Array<juce::var>{juce::var("same"),juce::var("same")}));
    auto missing=juce::JSON::parse(juce::JSON::toString(valid));
    missing.getDynamicObject()->removeProperty("peer");
    assert(validateLocalTestSetup(missing).failed());
    assert(validateLocalTestSetup(juce::var()).failed());
}
