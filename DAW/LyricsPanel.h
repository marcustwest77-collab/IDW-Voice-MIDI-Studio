#pragma once
#include <JuceHeader.h>
#include <functional>

// Project-owned lyric notebook. Editing never touches the audio graph.
class IDWLyricsPanel final : public juce::Component {
public:
    std::function<void(const juce::String&)> onEdited;
    IDWLyricsPanel() {
        heading.setText("LYRICS / SONG NOTES",juce::dontSendNotification);
        notice.setText("Use SAVE PROJECT to keep your words with this session.",juce::dontSendNotification);
        close.setButtonText("CLOSE");
        close.onClick=[this]{setVisible(false);};
        editor.setMultiLine(true,true);
        editor.setReturnKeyStartsNewLine(true);
        editor.setScrollbarsShown(true);
        editor.setInputRestrictions(32768);
        editor.setFont(juce::Font(juce::FontOptions(18.0f)));
        editor.setTextToShowWhenEmpty("Write or paste your hook, verses and session notes here...",juce::Colours::grey);
        editor.onTextChange=[this]{if(onEdited)onEdited(editor.getText());};
        addAndMakeVisible(heading);addAndMakeVisible(notice);
        addAndMakeVisible(close);addAndMakeVisible(editor);
        setOpaque(true);
    }
    void setLyrics(const juce::String& value) { editor.setText(value,false); }
    void focusEditor() { editor.grabKeyboardFocus(); }
    void resized() override {
        auto area=getLocalBounds().reduced(12);
        auto row=area.removeFromTop(32);
        close.setBounds(row.removeFromRight(75));heading.setBounds(row);
        notice.setBounds(area.removeFromTop(36));
        editor.setBounds(area);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
        g.setColour(juce::Colours::grey);g.drawRect(getLocalBounds(),1);
    }
private:
    juce::Label heading,notice;
    juce::TextButton close;
    juce::TextEditor editor;
};
