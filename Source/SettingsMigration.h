#pragma once
#include <JuceHeader.h>
#include <vector>

// The standalone's settings file name is part of its path: on Windows the app
// name is also the folder JUCE stores the file in. Renaming it therefore starts
// the updated build with no saved audio or MIDI device setup, so the previous
// file is copied to the new name once, before anything reads it.
namespace idw
{
inline constexpr const char* currentSettingsName = "IDW Voice MIDI Studio V9.2";
// The released V4 build.
inline constexpr const char* legacySettingsName = "IDW Voice MIDI Studio V4";
// Unreleased V9.2 development builds asked JUCE for a dotted suffix, which
// replaced everything after the last dot of the application name. The folder
// still came from the whole name, so that file sits beside the current one.
inline constexpr const char* devBuildSettingsFileName = "IDW Voice MIDI Studio V9.settings";

inline juce::PropertiesFile::Options settingsOptions(const juce::String& applicationName)
{
    juce::PropertiesFile::Options options;
    options.applicationName = applicationName;
    // No leading dot on purpose: JUCE treats a dotted suffix as replacing the
    // application name's extension, which would store "... V9.2" as
    // "... V9.settings". An undotted suffix is appended, keeping the whole name.
    options.filenameSuffix = "settings";
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

// Copies the first of the previous files that exists, in the order given.
inline bool copyFirstLegacySettings(const juce::File& currentFile,
                                    const std::vector<juce::File>& legacyFiles)
{
    for (const auto& legacyFile : legacyFiles)
        if (copyLegacySettings(currentFile, legacyFile))
            return true;

    return false;
}

// The previous files a first run tries, newest first: the unreleased dev
// build's file beside the current one, then the released V4 file, which keeps
// its own folder on Windows.
inline std::vector<juce::File> previousSettingsFiles(const juce::File& currentFile,
                                                     const juce::File& v4File)
{
    return {currentFile.getSiblingFile(devBuildSettingsFileName), v4File};
}

inline bool adoptLegacySettings()
{
    const auto current = defaultSettingsFile(currentSettingsName);
    return copyFirstLegacySettings(current, previousSettingsFiles(current, defaultSettingsFile(legacySettingsName)));
}
}
