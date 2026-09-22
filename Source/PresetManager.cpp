#include "PresetManager.h"

juce::File PresetManager::dir() const
{
    auto d = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                 .getChildFile("IDW Voice MIDI Studio/Presets");
    d.createDirectory();
    return d;
}

bool PresetManager::save(const juce::String& name)
{
    auto xml = state.copyState().createXml();
    return xml && xml->writeTo(dir().getChildFile(name + ".xml"));
}

bool PresetManager::load(const juce::String& name)
{
    auto xml = juce::XmlDocument::parse(dir().getChildFile(name + ".xml"));
    if (! xml || ! xml->hasTagName(state.state.getType()))
        return false;

    state.replaceState(juce::ValueTree::fromXml(*xml));
    return true;
}

juce::StringArray PresetManager::list() const
{
    juce::StringArray names;
    for (auto& file : dir().findChildFiles(juce::File::findFiles, false, "*.xml"))
        names.add(file.getFileNameWithoutExtension());
    names.sort(true);
    return names;
}

juce::StringArray PresetManager::factoryPresetNames() const
{
    return {
        "Clean Vocal",
        "Tight Tracking",
        "Smooth Lead",
        "Scale Locked Lead",
        "Wide Bend Performance",
        "Beatbox Drums",
        "Expressive MPE",
        "Live Responsive"
    };
}

void PresetManager::setParameter(const juce::String& id, float value)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(state.getParameter(id)))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
}

bool PresetManager::applyFactoryPreset(const juce::String& name)
{
    // Start each factory preset from a predictable baseline.
    setParameter("gate", .008f);
    setParameter("confidence", .75f);
    setParameter("bend", 2.0f);
    setParameter("tuneCents", 0.0f);
    setParameter("scaleLock", 0.0f);
    setParameter("root", 0.0f);
    setParameter("scaleMask", 2741.0f);
    setParameter("beatbox", 0.0f);
    setParameter("beatThreshold", .035f);
    setParameter("kick", 36.0f);
    setParameter("snare", 38.0f);
    setParameter("hat", 42.0f);
    setParameter("gestureCC", 1.0f);
    setParameter("cc", 74.0f);
    setParameter("ccSense", 1.0f);
    setParameter("mpe", 0.0f);
    setParameter("mpeFirst", 2.0f);
    setParameter("mpeLast", 16.0f);
    setParameter("latencyMs", 0.0f);
    setParameter("scaleExpression", 0.0f);
    setParameter("melody", 1.0f);

    if (name == "Clean Vocal")
    {
        return true;
    }

    if (name == "Tight Tracking")
    {
        setParameter("gate", .015f);
        setParameter("confidence", .88f);
        setParameter("gestureCC", 0.0f);
        return true;
    }

    if (name == "Smooth Lead")
    {
        setParameter("gate", .005f);
        setParameter("confidence", .68f);
        setParameter("bend", 12.0f);
        setParameter("ccSense", .70f);
        return true;
    }

    if (name == "Scale Locked Lead")
    {
        setParameter("confidence", .78f);
        setParameter("scaleLock", 1.0f);
        setParameter("root", 0.0f);
        setParameter("scaleMask", 2741.0f); // Major scale pitch-class mask.
        return true;
    }

    if (name == "Wide Bend Performance")
    {
        setParameter("gate", .006f);
        setParameter("confidence", .70f);
        setParameter("bend", 12.0f);
        setParameter("ccSense", 1.25f);
        return true;
    }

    if (name == "Beatbox Drums")
    {
        // A high vocal gate suppresses most pitched-note output while the
        // independent beatbox detector remains sensitive.
        setParameter("melody", 0.0f);
        setParameter("gate", .080f);
        setParameter("confidence", .90f);
        setParameter("beatbox", 1.0f);
        setParameter("beatThreshold", .020f);
        setParameter("gestureCC", 0.0f);
        return true;
    }

    if (name == "Expressive MPE")
    {
        setParameter("gate", .005f);
        setParameter("confidence", .70f);
        setParameter("bend", 24.0f);
        setParameter("gestureCC", 1.0f);
        setParameter("ccSense", 1.50f);
        setParameter("mpe", 1.0f);
        return true;
    }

    if (name == "Live Responsive")
    {
        setParameter("gate", .010f);
        setParameter("confidence", .72f);
        setParameter("bend", 2.0f);
        setParameter("gestureCC", 0.0f);
        return true;
    }

    return false;
}
