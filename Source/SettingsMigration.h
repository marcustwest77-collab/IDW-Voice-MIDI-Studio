#pragma once
#include <JuceHeader.h>

// The standalone's settings file name is part of its path: on Windows the app
// name is also the folder JUCE stores the file in. Renaming it therefore starts
// the updated build with no saved audio or MIDI device setup, so the previous
// file is copied to the new name once, before anything reads it.
namespace idw
{
inline constexpr const char* currentSettingsName = "IDW Voice MIDI Studio V9.2";
inline constexpr const char* legacySettingsName = "IDW Voice MIDI Studio V4";

inline juce::PropertiesFile::Options settingsOptions(const juce::String& applicationName)
{
    juce::PropertiesFile::Options options;
    options.applicationName = applicationName;
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    return options;
}

inline juce::File defaultSettingsFile(const juce::String& applicationName)
{
    return settingsOptions(applicationName).getDefaultFile();
}

// Returns true only when the copy happened. An existing new file is the user's
// current setup and is never overwritten, and no old file means nothing to do.
inline bool copyLegacySettings(const juce::File& currentFile, const juce::File& legacyFile)
{
    if (currentFile.existsAsFile() || ! legacyFile.existsAsFile())
        return false;

    currentFile.getParentDirectory().createDirectory();
    return legacyFile.copyFileTo(currentFile);
}

inline bool adoptLegacySettings()
{
    return copyLegacySettings(defaultSettingsFile(currentSettingsName),
                              defaultSettingsFile(legacySettingsName));
}
}
