#include "MidiSineSynth.h"
#include "PlaybackTiming.h"
#include "LoopbackWavAnalysis.h"
#include "LoopbackReference.h"
#include "LoopbackCapture.h" // experimental live capture engine; not wired to UI yet
#include "PianoRoll.h"
#include <JuceHeader.h>
#include "SessionModel.h"
#include "ClipEnvelope.h"
#include "ClipPlayback.h"
#include "WaveformView.h"
#include "TimelineZoom.h"
#include "SessionHistory.h"
#include <optional>
#include "TimelineMath.h"
#include "TakeRecorder.h"
#include "MultiTakeRecorder.h" // compile-check experimental multitrack engine
#include "SessionPersistence.h"
#include "ProjectPackaging.h"
#include "OfflineBounce.h"
#include "IDWConnectPanel.h"
#include "AIApprovalPanel.h"
#include "AIStatusSnapshot.h"
#include <atomic>
#include <memory>
#include <map>
#include <thread>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sapi.h>
#include <sphelper.h>
#endif
// Neo Studio: visual-only styling; recording and audio callbacks remain unchanged.
class IDWNeoLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    IDWNeoLookAndFeel() { applySkin(0); }
    void applySkin(int skin) {
        const auto accent=juce::Colour(skin==1?0xffffc857:skin==2?0xff73f9ae:skin==3?0xffd4a6ff:0xff15d4ff);
        const auto base=juce::Colour(skin==1?0xff292014:skin==2?0xff102b25:skin==3?0xff20182f:0xff152638);
        setColour(juce::TextButton::buttonColourId,base);
        setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff007fa9));
        setColour(juce::TextButton::textColourOffId,juce::Colour(0xffd6ebf8));
        setColour(juce::TextButton::textColourOnId,juce::Colours::white);
        setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff111e2e));
        setColour(juce::ComboBox::outlineColourId,juce::Colour(0xff28506a));
        setColour(juce::ComboBox::textColourId,juce::Colour(0xffd6ebf8));
        setColour(juce::Slider::thumbColourId,accent);
        setColour(juce::Slider::trackColourId,juce::Colour(0xff127a9e));
        setColour(juce::Slider::backgroundColourId,juce::Colour(0xff182739));
        setColour(juce::Label::textColourId,juce::Colour(0xffd6ebf8));
        setColour(juce::ToggleButton::textColourId,juce::Colour(0xffd6ebf8));
        setColour(juce::ToggleButton::tickColourId,accent);
        setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff101b2b));
        setColour(juce::TextEditor::textColourId,juce::Colours::white);
        setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff101b2b));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xff176b94));
    }
    juce::Colour accentColour() const {return findColour(juce::Slider::thumbColourId);}
    void setCustomAccent(juce::Colour accent){
        setColour(juce::Slider::thumbColourId,accent);
        setColour(juce::ToggleButton::tickColourId,accent);
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& button,
                              const juce::Colour& background,
                              bool highlighted,bool down) override {
        const auto area=button.getLocalBounds().toFloat().reduced(1.5f);
        const bool recording=button.getButtonText().containsIgnoreCase("RECORD") ||
            button.getButtonText().containsIgnoreCase("STOP RECORD");
        const auto accent=recording ? juce::Colour(0xffe94565) : accentColour();
        const auto fill=down ? accent.withAlpha(0.36f) :
            (highlighted ? juce::Colour(0xff214463) : background);
        g.setColour(fill);g.fillRoundedRectangle(area,7.0f);
        g.setColour(accent.withAlpha(highlighted || down ? 0.95f : 0.44f));
        g.drawRoundedRectangle(area,7.0f,highlighted ? 1.6f : 1.0f);
    }
};
class Workstation final : public juce::AudioAppComponent, private juce::Timer, private juce::ChangeListener {
public:
    Workstation() {
        setSize(1450,850);
        setLookAndFeel(&neoTheme);
        formats.registerBasicFormats();
        setAudioChannels(8,2); // Request up to eight available hardware inputs.
        deviceManager.addChangeListener(this);
        deviceManager.addAudioCallback(&recorder);
        deviceManager.addAudioCallback(&multiRecorder);
        deviceManager.addAudioCallback(&loopbackCapture);
        title.setText("IDW  |  VOICE MIDI STUDIO  |  NEO STUDIO BETA",juce::dontSendNotification);
        title.setColour(juce::Label::textColourId,juce::Colour(0xff4adfff));
        title.setFont(juce::Font(juce::FontOptions(22.0f,juce::Font::bold)));
        addAndMakeVisible(title);
        addAndMakeVisible(skinSelector);
        addAndMakeVisible(recordReadiness);
        recordReadiness.setJustificationType(juce::Justification::centredLeft);
        recordReadiness.setFont(juce::Font(juce::FontOptions(12.0f,juce::Font::bold)));
        addAndMakeVisible(customAccentButton);
        addAndMakeVisible(customBackgroundButton);
        addAndMakeVisible(customPanelButton);
        addAndMakeVisible(saveSkinButton);
        addAndMakeVisible(loadSkinButton);
        addAndMakeVisible(deleteSkinButton);
        addAndMakeVisible(exportSkinButton);
        addAndMakeVisible(importSkinButton);
        addAndMakeVisible(skinToolsButton);
        skinToolsButton.setButtonText("SKIN TOOLS");
        skinToolsButton.setTooltip("Customize, save, load, import and export Neo Studio skins.");
        skinToolsButton.onClick=[this]{
            juce::PopupMenu menu;
            menu.addItem(1,"Accent color");
            menu.addItem(2,"Background color");
            menu.addItem(3,"Panel color");
            menu.addSeparator();
            menu.addItem(4,"Save skin");
            menu.addItem(5,"Load skin");
            menu.addItem(6,"Delete skin");
            menu.addItem(7,"Export skin");
            menu.addItem(8,"Import skin");
            juce::Component::SafePointer<Workstation> safeThis(this);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&skinToolsButton),
                [safeThis](int selection){
                    if(safeThis==nullptr)return;
                    switch(selection){
                        case 1:safeThis->customAccentButton.triggerClick();break;
                        case 2:safeThis->customBackgroundButton.triggerClick();break;
                        case 3:safeThis->customPanelButton.triggerClick();break;
                        case 4:safeThis->saveSkinButton.triggerClick();break;
                        case 5:safeThis->loadSkinButton.triggerClick();break;
                        case 6:safeThis->deleteSkinButton.triggerClick();break;
                        case 7:safeThis->exportSkinButton.triggerClick();break;
                        case 8:safeThis->importSkinButton.triggerClick();break;
                        default:break;
                    }
                });
        };
        for(auto* action:{&customAccentButton,&customBackgroundButton,&customPanelButton,
                          &saveSkinButton,&loadSkinButton,&deleteSkinButton,
                          &exportSkinButton,&importSkinButton})
            action->setVisible(false);
        importSkinButton.setButtonText("IMPORT SKIN");
        importSkinButton.setTooltip("Import a validated IDW skin JSON file; existing presets are not overwritten.");
        importSkinButton.onClick=[this]{
            auto chooser=std::make_shared<juce::FileChooser>("Import IDW Skin",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.json");
            juce::Component::SafePointer<Workstation> safeThis(this);
            chooser->launchAsync(juce::FileBrowserComponent::openMode |
                juce::FileBrowserComponent::canSelectFiles,
                [safeThis,chooser](const juce::FileChooser& result){
                    if(safeThis==nullptr)return;
                    const auto source=result.getResult();
                    if(source==juce::File())return;
                    const auto parsed=juce::JSON::parse(source);
                    auto* object=parsed.getDynamicObject();
                    auto invalid=[&](){
                        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                            "Invalid Skin","The selected file is not a valid IDW skin preset.");
                    };
                    if(object==nullptr){invalid();return;}
                    const auto name=object->getProperty("name").toString().trim();
                    if(name.isEmpty() || name.length()>40 ||
                       !name.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-")){
                        invalid();return;
                    }
                    const auto skinProperty=object->getProperty("skin");
                    const auto skinText=skinProperty.toString();
                    if(skinText.length()!=1 || !skinText.containsOnly("0123")){invalid();return;}
                    for(const char* key:{"background","panel","accent"}){
                        const auto rgb=object->getProperty(key).toString();
                        if(rgb.length()!=6 || !rgb.containsOnly("0123456789abcdefABCDEF")){
                            invalid();return;
                        }
                    }
                    const auto root=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("IDW Neo Studio").getChildFile("skins");
                    const auto target=root.getChildFile(name+".json");
                    if(!root.createDirectory().wasOk() || target.exists()){
                        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                            "Import Skin","Unable to import: destination unavailable or a preset with this name already exists.");
                        return;
                    }
                    if(!target.replaceWithText(juce::JSON::toString(parsed)))
                        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                            "Import Skin","Unable to write the selected skin.");
                    else
                        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                            "Skin Imported","Open LOAD SKIN to apply '"+name+"'.");
                });
        };
        exportSkinButton.setButtonText("EXPORT SKIN");
        exportSkinButton.setTooltip("Export a saved custom skin JSON file to a chosen folder.");
        exportSkinButton.onClick=[this]{
            const auto root=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("IDW Neo Studio").getChildFile("skins");
            const auto files=root.findChildFiles(juce::File::findFiles,false,"*.json");
            if(files.isEmpty()){
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                    "Export Skin","Save a named skin first.");
                return;
            }
            juce::PopupMenu menu;
            for(int i=0;i<files.size();++i)menu.addItem(i+1,files[i].getFileNameWithoutExtension());
            juce::Component::SafePointer<Workstation> safeThis(this);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&exportSkinButton),
                [safeThis,files](int choice){
                    if(safeThis==nullptr || choice<1 || choice>files.size())return;
                    const auto source=files[choice-1];
                    auto chooser=std::make_shared<juce::FileChooser>("Export IDW Skin",
                        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                            .getChildFile(source.getFileName()),"*.json");
                    chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                        juce::FileBrowserComponent::canSelectFiles |
                        juce::FileBrowserComponent::warnAboutOverwriting,
                        [safeThis,source,chooser](const juce::FileChooser& result){
                            if(safeThis==nullptr)return;
                            const auto target=result.getResult();
                            if(target==juce::File())return;
                            if(!source.copyFileTo(target))
                                juce::AlertWindow::showMessageBoxAsync(
                                    juce::MessageBoxIconType::WarningIcon,
                                    "Export failed","Could not export skin to "+target.getFullPathName());
                        });
                });
        };
        deleteSkinButton.setButtonText("DELETE SKIN");
        deleteSkinButton.setTooltip("Delete a named skin preset after confirmation. The active colors remain unchanged.");
        deleteSkinButton.onClick=[this]{
            const auto root=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("IDW Neo Studio").getChildFile("skins");
            const auto files=root.findChildFiles(juce::File::findFiles,false,"*.json");
            if(files.isEmpty()){
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                    "Delete Skin","No saved skins to delete.");
                return;
            }
            juce::PopupMenu menu;
            for(int i=0;i<files.size();++i)
                menu.addItem(i+1,files[i].getFileNameWithoutExtension());
            juce::Component::SafePointer<Workstation> safeThis(this);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&deleteSkinButton),
                [safeThis,files](int choice){
                    if(safeThis==nullptr || choice<1 || choice>files.size())return;
                    const auto file=files[choice-1];
                    auto* confirmation=new juce::AlertWindow("Delete Skin",
                        "Permanently delete preset '"+file.getFileNameWithoutExtension()+
                        "'? Your current DAW appearance will remain unchanged.",
                        juce::MessageBoxIconType::WarningIcon);
                    confirmation->addButton("Delete",1);
                    confirmation->addButton("Cancel",0);
                    confirmation->enterModalState(true,
                        juce::ModalCallbackFunction::create([safeThis,confirmation,file](int accepted){
                            if(accepted==1 && safeThis!=nullptr && !file.deleteFile())
                                juce::AlertWindow::showMessageBoxAsync(
                                    juce::MessageBoxIconType::WarningIcon,
                                    "Unable to Delete Skin",file.getFullPathName());
                            delete confirmation;
                        }));
                });
        };
        loadSkinButton.setButtonText("LOAD SKIN");
        loadSkinButton.onClick=[this]{
            auto root=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("IDW Neo Studio").getChildFile("skins");
            auto files=root.findChildFiles(juce::File::findFiles,false,"*.json");
            if(files.isEmpty()){
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                    "Load Skin","No saved skins yet. Use SAVE SKIN first.");
                return;
            }
            juce::PopupMenu menu;
            for(int i=0;i<files.size();++i)menu.addItem(i+1,files[i].getFileNameWithoutExtension());
            juce::Component::SafePointer<Workstation> safeThis(this);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&loadSkinButton),
                [safeThis,files](int selection){
                    if(safeThis==nullptr || selection<=0 || selection>files.size())return;
                    const auto parsed=juce::JSON::parse(files[selection-1]);
                    auto* object=parsed.getDynamicObject();
                    if(!object)return;
                    const auto skinValue=object->getProperty("skin");
                    const int skin=(int)skinValue;
                    if(skin<0 || skin>3)return;
                    auto validRGB=[object](const juce::Identifier& key){
                        const auto rgb=object->getProperty(key).toString();
                        return rgb.length()==6 && rgb.containsOnly("0123456789abcdefABCDEF");
                    };
                    if(!validRGB("background") || !validRGB("panel") || !validRGB("accent"))return;
                    safeThis->activeSkin=skin;
                    safeThis->skinSelector.setSelectedId(skin+1,juce::dontSendNotification);
                    safeThis->neoTheme.applySkin(skin);
                    safeThis->customBackground=juce::Colour::fromString("ff"+object->getProperty("background").toString());
                    safeThis->customPanel=juce::Colour::fromString("ff"+object->getProperty("panel").toString());
                    safeThis->customAccent=juce::Colour::fromString("ff"+object->getProperty("accent").toString());
                    safeThis->useCustomBackground=true;safeThis->useCustomPanel=true;
                    safeThis->neoTheme.setCustomAccent(safeThis->customAccent);
                    safeThis->title.setColour(juce::Label::textColourId,safeThis->customAccent);
                    const auto preferences=juce::File::getSpecialLocation(
                        juce::File::userApplicationDataDirectory).getChildFile("IDW Neo Studio");
                    if(preferences.createDirectory().wasOk()){
                        preferences.getChildFile("preferences.txt").replaceWithText(juce::String(skin));
                        preferences.getChildFile("background.txt").replaceWithText(
                            safeThis->customBackground.toDisplayString(false));
                        preferences.getChildFile("panel.txt").replaceWithText(
                            safeThis->customPanel.toDisplayString(false));
                        preferences.getChildFile("accent.txt").replaceWithText(
                            safeThis->customAccent.toDisplayString(false));
                    }
                    safeThis->repaint();
                    for(auto* child:safeThis->getChildren())child->repaint();
                });
        };
        saveSkinButton.setButtonText("SAVE SKIN");
        saveSkinButton.onClick=[this]{
            auto* dialog=new juce::AlertWindow("Save Custom Skin","Enter a preset name (letters, numbers, spaces)",juce::MessageBoxIconType::InfoIcon);
            dialog->addTextEditor("name","My Skin","Name");
            dialog->addButton("Save",1);dialog->addButton("Cancel",0);
            juce::Component::SafePointer<Workstation> safeThis(this);
            dialog->enterModalState(true,juce::ModalCallbackFunction::create([safeThis,dialog](int result){
                if(result==1 && safeThis!=nullptr){
                    const auto name=dialog->getTextEditorContents("name").trim();
                    if(name.isNotEmpty() && name.length()<=40 &&
                       name.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-")){
                        auto root=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("IDW Neo Studio").getChildFile("skins");
                        if(root.createDirectory().wasOk()){
                            auto payload=new juce::DynamicObject();
                            payload->setProperty("name",name);
                            payload->setProperty("skin",safeThis->activeSkin);
                            payload->setProperty("background",(safeThis->useCustomBackground ? safeThis->customBackground :
                                juce::Colour(safeThis->activeSkin==1?0xff17100b:safeThis->activeSkin==2?0xff091c18:safeThis->activeSkin==3?0xff160d22:0xff080f1c)).toDisplayString(false));
                            payload->setProperty("panel",(safeThis->useCustomPanel ? safeThis->customPanel :
                                juce::Colour(safeThis->activeSkin==1?0xff2b2118:safeThis->activeSkin==2?0xff112d26:safeThis->activeSkin==3?0xff261d39:0xff101f32)).toDisplayString(false));
                            payload->setProperty("accent",safeThis->neoTheme.accentColour().toDisplayString(false));
                            root.getChildFile(name+".json").replaceWithText(juce::JSON::toString(juce::var(payload)));
                        }
                    }
                }
                delete dialog;
            }));
        };
        customPanelButton.setButtonText("PANEL");
        customPanelButton.onClick=[this]{
            auto* dialog=new juce::AlertWindow("Custom Panel Color",
                "Enter six-digit RGB hex (example 202A40)",juce::MessageBoxIconType::InfoIcon);
            dialog->addTextEditor("rgb",customPanel.toDisplayString(false),"RGB hex");
            dialog->addButton("Apply",1);dialog->addButton("Cancel",0);
            juce::Component::SafePointer<Workstation> safeThis(this);
            dialog->enterModalState(true,juce::ModalCallbackFunction::create([safeThis,dialog](int result){
                if(result==1 && safeThis!=nullptr){
                    const auto rgb=dialog->getTextEditorContents("rgb").trim().trimCharactersAtStart("#");
                    if(rgb.length()==6 && rgb.containsOnly("0123456789abcdefABCDEF")){
                        safeThis->customPanel=juce::Colour::fromString("ff"+rgb);
                        safeThis->useCustomPanel=true;
                        const auto file=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("IDW Neo Studio").getChildFile("panel.txt");
                        if(file.getParentDirectory().createDirectory().wasOk())
                            file.replaceWithText(rgb.toUpperCase());
                        safeThis->repaint();
                    }
                }
                delete dialog;
            }));
        };
        customBackgroundButton.setButtonText("BG COLOR");
        customBackgroundButton.onClick=[this]{
            auto* dialog=new juce::AlertWindow("Custom Background","Enter 6-digit RGB hex (example 101820)",juce::MessageBoxIconType::InfoIcon);
            dialog->addTextEditor("rgb",customBackground.toDisplayString(false),"RGB hex");
            dialog->addButton("Apply",1);dialog->addButton("Cancel",0);
            dialog->enterModalState(true,juce::ModalCallbackFunction::create([this,dialog](int result){
                if(result==1){
                    const auto rgb=dialog->getTextEditorContents("rgb").trim().trimCharactersAtStart("#");
                    if(rgb.length()==6 && rgb.containsOnly("0123456789abcdefABCDEF")){
                        customBackground=juce::Colour::fromString("ff"+rgb);
                        useCustomBackground=true;
                        const auto file=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("IDW Neo Studio").getChildFile("background.txt");
                        if(file.getParentDirectory().createDirectory().wasOk())file.replaceWithText(rgb.toUpperCase());
                        repaint();
                    }
                }
                delete dialog;
            }));
        };
        customAccentButton.setButtonText("ACCENT");
        customAccentButton.onClick=[this]{
            auto* dialog=new juce::AlertWindow("Custom Accent",
                "Enter 6-digit RGB hex (example FF6600)",juce::MessageBoxIconType::InfoIcon);
            dialog->addTextEditor("rgb",neoTheme.accentColour().toDisplayString(false),"RGB hex");
            dialog->addButton("Apply",1);
            dialog->addButton("Cancel",0);
            dialog->enterModalState(true,juce::ModalCallbackFunction::create([this,dialog](int result){
                if(result==1){
                    const auto rgb=dialog->getTextEditorContents("rgb").trim().trimCharactersAtStart("#");
                    if(rgb.length()==6 && rgb.containsOnly("0123456789abcdefABCDEF")){
                        customAccent=juce::Colour::fromString("ff"+rgb);
                        neoTheme.setCustomAccent(customAccent);
                        const auto customFile=juce::File::getSpecialLocation(
                            juce::File::userApplicationDataDirectory)
                            .getChildFile("IDW Neo Studio").getChildFile("accent.txt");
                        if(customFile.getParentDirectory().createDirectory().wasOk())
                            customFile.replaceWithText(rgb.toUpperCase());
                        title.setColour(juce::Label::textColourId,customAccent);
                        repaint();
                        for(auto* child:getChildren())child->repaint();
                    }
                }
                delete dialog;
            }));
        };
        skinSelector.addItem("Neo Cyan",1);
        skinSelector.addItem("Royal Gold",2);
        skinSelector.addItem("Emerald Circuit",3);
        skinSelector.addItem("Purple Night",4);
        const auto preferences=juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory)
            .getChildFile("IDW Neo Studio").getChildFile("preferences.txt");
        const int storedSkin=preferences.existsAsFile() ?
            preferences.loadFileAsString().getIntValue() : 0;
        activeSkin=juce::jlimit(0,3,storedSkin);
        skinSelector.setSelectedId(activeSkin+1,juce::dontSendNotification);
        neoTheme.applySkin(activeSkin);
        const auto panelFile=preferences.getSiblingFile("panel.txt");
        if(panelFile.existsAsFile()){
            const auto rgb=panelFile.loadFileAsString().trim();
            if(rgb.length()==6 && rgb.containsOnly("0123456789abcdefABCDEF")){
                customPanel=juce::Colour::fromString("ff"+rgb);
                useCustomPanel=true;
            }
        }
        const auto backgroundFile=preferences.getSiblingFile("background.txt");
        if(backgroundFile.existsAsFile()){
            const auto rgb=backgroundFile.loadFileAsString().trim();
            if(rgb.length()==6 && rgb.containsOnly("0123456789abcdefABCDEF")){
                customBackground=juce::Colour::fromString("ff"+rgb);
                useCustomBackground=true;
            }
        }
        const auto customFile=preferences.getSiblingFile("accent.txt");
        if(customFile.existsAsFile()){
            const auto rgb=customFile.loadFileAsString().trim();
            if(rgb.length()==6 && rgb.containsOnly("0123456789abcdefABCDEF")){
                customAccent=juce::Colour::fromString("ff"+rgb);
                neoTheme.setCustomAccent(customAccent);
            }
        }
        title.setColour(juce::Label::textColourId,neoTheme.accentColour());
        skinSelector.setTooltip("Change the visual skin without affecting audio or session content.");
        skinSelector.onChange=[this]{
            // Choosing a factory skin must clear overrides from a previously
            // loaded custom preset, otherwise the visible palette never changes.
            activeSkin=juce::jlimit(0,3,skinSelector.getSelectedId()-1);
            useCustomBackground=false;
            useCustomPanel=false;
            neoTheme.applySkin(activeSkin);
            customAccent=neoTheme.accentColour();
            const auto preferences=juce::File::getSpecialLocation(
                juce::File::userApplicationDataDirectory)
                .getChildFile("IDW Neo Studio").getChildFile("preferences.txt");
            if(preferences.getParentDirectory().createDirectory().wasOk()){
                preferences.replaceWithText(juce::String(activeSkin));
                preferences.getSiblingFile("background.txt").deleteFile();
                preferences.getSiblingFile("panel.txt").deleteFile();
                preferences.getSiblingFile("accent.txt").deleteFile();
            }
            title.setColour(juce::Label::textColourId,neoTheme.accentColour());
            repaint();
            for(auto* child:getChildren())child->repaint();
        };
        for(auto* b:{&play,&stop,&audio,&midi,&open,&record,&saveProject,&openProject,&exportProject,&bounce,&cancelBounce,&connectButton,&aiButton}) addAndMakeVisible(*b);
        play.setButtonText("PLAY"); stop.setButtonText("STOP"); open.setButtonText("OPEN WAV/AIFF");
        // Text command console is a microphone-free test harness. Speech recognition
        // can feed the same dispatcher later; no always-listening mic is implied.
        addAndMakeVisible(voiceCommandInput);
        voiceCommandInput.setTextToShowWhenEmpty("type command",juce::Colours::grey);
        voiceCommandInput.setTooltip("Say: IDW play, IDW stop, IDW arm track, IDW record vocals, IDW next track, IDW voice help. Windows: press Win+H to dictate.");
        addAndMakeVisible(runVoiceCommand);
        addAndMakeVisible(voiceHistoryButton);
        addAndMakeVisible(pushToTalkButton);
        addAndMakeVisible(commandStatus);
        pushToTalkButton.setButtonText("TALK");
        pushToTalkButton.setTooltip("Windows native speech recognition: TALK listens for one command; click again to cancel.");
        pushToTalkButton.onClick=[this]{startDictation();};
        voiceHistoryButton.setButtonText("HISTORY");
        voiceHistoryButton.setTooltip("Show recent voice and typed commands with their results.");
        voiceHistoryButton.onClick=[this]{executeStudioCommand("command history");};
        commandStatus.setText("VOICE READY: click field, Win+H, speak, Enter",juce::dontSendNotification);
        commandStatus.setTooltip("Windows: focus the command field, use Win+H for speech-to-text, then Enter to run. No always-listening microphone.");
        runVoiceCommand.setButtonText("COMMAND");
        runVoiceCommand.onClick=[this]{
            voiceCommandInput.grabKeyboardFocus();
            executeStudioCommand(voiceCommandInput.getText());
        };
        runVoiceCommand.setTooltip("Voice-ready command input: on Windows press Win+H and dictate a command, then press Enter. Native speech recognition is not yet integrated.");
        voiceCommandInput.onReturnKey=[this]{executeStudioCommand(voiceCommandInput.getText());};
        saveProject.setButtonText("SAVE PROJECT");
        openProject.setButtonText("OPEN PROJECT");
        exportProject.setButtonText("EXPORT PORTABLE");
        bounce.setButtonText("BOUNCE WAV");
        cancelBounce.setButtonText("CANCEL EXPORT");cancelBounce.setEnabled(false);
        connectButton.setButtonText("IDW CONNECT");
        addChildComponent(connectPanel);
        connectButton.onClick=[this]{
            connectPanel.setVisible(!connectPanel.isVisible());
            connectPanel.toFront(false);
        };
        synthPreset.addItem("Keys",1);
        synthPreset.addItem("Bass",2);
        synthPreset.addItem("Lead",3);
        synthPreset.addItem("Organ",4);
        synthPreset.addItem("Pad",5);
        synthPreset.setSelectedId(1,juce::dontSendNotification);
        addAndMakeVisible(synthPreset);
        synthPreset.onChange=[this]{
            if(playing.load() || selectedTrack>=session.tracks.size())return;
            session.setTrackSynthPreset(selectedTrack,synthPreset.getSelectedId()-1);
        };
        attackLabel.setText("ATTACK ms",juce::dontSendNotification);
        releaseLabel.setText("RELEASE ms",juce::dontSendNotification);
        addAndMakeVisible(attackLabel);addAndMakeVisible(releaseLabel);
        for(auto* slider:{&attackControl,&releaseControl}){
            slider->setRange(0.0,2000.0,1.0);
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,58,20);
            addAndMakeVisible(*slider);
        }
        const auto updateEnvelope=[this]{
            if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load() ||
               selectedTrack>=session.tracks.size())return;
            session.setTrackSynthEnvelope(selectedTrack,
                (float)attackControl.getValue(),(float)releaseControl.getValue());
        };
        attackControl.onValueChange=updateEnvelope;
        releaseControl.onValueChange=updateEnvelope;
        pianoButton.setButtonText("PIANO ROLL");
        addAndMakeVisible(pianoButton);
        addChildComponent(pianoRoll);
        pianoRoll.session=&session;
        pianoButton.onClick=[this]{
            if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
            if(selectedTrack>=session.tracks.size() ||
               session.tracks[selectedTrack].type!=idw::daw::TrackType::midi)return;
            pianoRoll.trackIndex=selectedTrack;
            pianoRoll.setVisible(!pianoRoll.isVisible());
            if(pianoRoll.isVisible()){
                aiPanel.setVisible(false);connectPanel.setVisible(false);
                pianoRoll.toFront(false);pianoRoll.repaint();
            }
        };
        aiButton.setButtonText("IDW AI STUDIO");
        addChildComponent(aiPanel);
        aiButton.onClick=[this]{
            aiPanel.setVisible(!aiPanel.isVisible());
            if(aiPanel.isVisible()){connectPanel.setVisible(false);aiPanel.toFront(false);}
        };
        aiPanel.getSanitizedStatus=[this]{
            return juce::JSON::toString(idw::daw::aiSessionStatus(
                session,playing.load(),(recorder.isRecording() || multiRecorder.isRecording())),true);
        };
        aiPanel.onApproved=[this](const idw::daw::AIProposal& proposal){
            if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return false;
            if(proposal.kind==idw::daw::AIProposal::Kind::addAudioTrack){
                const auto count=session.tracks.size();
                addAudioTrack();
                if(session.tracks.size()==count)return false;
            }else if(!idw::daw::applyApprovedAIProposal(session,proposal))return false;
            if(proposal.kind==idw::daw::AIProposal::Kind::tempo)
                tempo.setValue(session.bpm,juce::sendNotificationSync);
            refreshMixer();repaint();
            return true;
        };
        record.setButtonText("RECORD VOCAL");
        latencyLabel.setText("TAKE TRIM ms",juce::dontSendNotification);
        addAndMakeVisible(latencyLabel);
        latencyAdjust.setRange(-100.0,250.0,1.0);
        latencyAdjust.setValue(0.0);
        latencyAdjust.setTextBoxStyle(juce::Slider::TextBoxRight,false,65,24);
        latencyAdjust.setTooltip("Manual adjustment added to driver-reported input latency (milliseconds). Negative values reduce trim. Loopback measures round-trip delay and must NOT be copied directly into this input-trim control.");
        addAndMakeVisible(latencyAdjust);
        analyzeCalibration.setButtonText("ANALYZE LOOPBACK WAV");
        analyzeCalibration.setTooltip("Measure known test-pulse positions in an existing WAV. Does not change take trim.");
        addAndMakeVisible(analyzeCalibration);
        analyzeCalibration.onClick=[this]{analyzeCalibrationWav();};
        generateCalibration.setButtonText("CREATE PULSE WAV");
        generateCalibration.setTooltip("Generate a safe-level 48 kHz test WAV with pulses at 100, 200, 300, 400, 500 ms.");
        addAndMakeVisible(generateCalibration);
        generateCalibration.onClick=[this]{createCalibrationReference();};
        liveCalibration.setButtonText("LIVE LOOPBACK TEST");
        liveCalibration.setTooltip("Experimental: route audio interface line output 1 to selected line input. Low-volume pulses; headphones disconnected from loopback path.");
        addAndMakeVisible(liveCalibration);
        liveCalibration.onClick=[this]{startLiveCalibration();};
        overdubControl.setButtonText("PLAY BEAT WHILE RECORDING");
        overdubControl.setTooltip("Play existing tracks while capturing new takes on empty armed tracks.");
        overdubControl.setToggleState(false,juce::dontSendNotification);
        addAndMakeVisible(overdubControl);
        audio.setButtonText("+ AUDIO TRACK"); midi.setButtonText("+ MIDI TRACK");
        play.onClick=[this]{if(playing.load() || loopbackCapture.isActive() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;if(loadedCount()>0 || std::any_of(session.tracks.begin(),session.tracks.end(),[](const auto& t){return !t.notes.empty();})){pianoRoll.setVisible(false);
            std::atomic_store_explicit(&playbackSession,
                std::make_shared<const idw::daw::Session>(session),std::memory_order_release);
            sessionFrames.store(0);for(auto& slot:sources){slot->transport.stop();slot->transport.setPosition(slot->sourceOffsetSeconds);slot->nextSourceFrame=-1;slot->transport.start();}playing=true;startTimerHz(30);repaint();}};
        stop.onClick=[this]{if(exportRunning.load())return;if(loopbackCapture.isActive() || loopbackCapture.isComplete()){loopbackCapture.cancel();liveCalibration.setEnabled(true);}else if(loopbackCapture.consumeFailed()){loopbackCapture.cancel();liveCalibration.setEnabled(true);}if((recorder.isRecording() || multiRecorder.isRecording()))toggleRecording();for(auto& slot:sources){slot->transport.stop();slot->transport.setPosition(slot->sourceOffsetSeconds);}sessionFrames.store(0);playing=false;stopTimer();playheadBeats=0;refreshMixer();repaint();};
        open.onClick=[this]{loadAudio();};
        record.onClick=[this]{toggleRecording();};
        saveProject.onClick=[this]{saveProjectFile();};
        openProject.onClick=[this]{openProjectFile();};
        exportProject.onClick=[this]{exportProjectFile();};
        bounce.onClick=[this]{bounceMix();};
        cancelBounce.onClick=[this]{exportCancel.store(true);};
        audio.onClick=[this]{if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;addAudioTrack();refreshMixer();repaint();};
        midi.onClick=[this]{if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;session.addTrack(idw::daw::TrackType::midi);refreshMixer();repaint();};
        tempo.setRange(20,300,1);tempo.setValue(120);tempo.setTextBoxStyle(juce::Slider::TextBoxRight,false,70,28);
        tempo.onValueChange=[this]{if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;session.setTempo(tempo.getValue());};addAndMakeVisible(tempo);
        tempoLabel.setText("BPM",juce::dontSendNotification);addAndMakeVisible(tempoLabel);
        addAudioTrack();
        session.addTrack(idw::daw::TrackType::midi);
        gainLabel.setText("TRACK 1 VOL",juce::dontSendNotification);
        panLabel.setText("PAN",juce::dontSendNotification);
        for(auto* label:{&gainLabel,&panLabel})addAndMakeVisible(*label);
        gainControl.setRange(0.0,2.0,0.01);gainControl.setValue(1.0);
        panControl.setRange(-1.0,1.0,0.01);panControl.setValue(0.0);
        gainControl.setTextBoxStyle(juce::Slider::TextBoxRight,false,56,26);
        panControl.setTextBoxStyle(juce::Slider::TextBoxRight,false,56,26);
        for(auto* slider:{&gainControl,&panControl})addAndMakeVisible(*slider);
        muteControl.setButtonText("MUTE");addAndMakeVisible(muteControl);
        soloControl.setButtonText("SOLO");addAndMakeVisible(soloControl);
        gainControl.onValueChange=[this]{updateFirstTrackMix();};
        panControl.onValueChange=[this]{updateFirstTrackMix();};
        muteControl.onClick=[this]{
            if(playing.load() || session.tracks.empty())return;
            session.tracks[selectedTrack].mute=muteControl.getToggleState();repaint();
        };
        soloControl.onClick=[this]{
            if(playing.load() || session.tracks.empty())return;
            session.tracks[selectedTrack].solo=soloControl.getToggleState();repaint();
        };
        previousTrack.setButtonText("< TRACK");
        nextTrack.setButtonText("TRACK >");
        addAndMakeVisible(previousTrack);addAndMakeVisible(nextTrack);
        inputChannel.addItemList({"INPUT 1","INPUT 2","INPUT 3","INPUT 4",
            "INPUT 5","INPUT 6","INPUT 7","INPUT 8"},1);
        inputChannel.setSelectedId(1,juce::dontSendNotification);
        inputLevel.setText("INPUT LEVEL: idle",juce::dontSendNotification);
        addAndMakeVisible(inputLevel);
        addAndMakeVisible(inputChannel);
        inputChannel.onChange=[this]{
            if(selectedTrack>=session.tracks.size() || playing.load() ||
               recorder.isRecording() || multiRecorder.isRecording() || exportRunning.load())return;
            const int selectedId=inputChannel.getSelectedId();
            if(selectedId<=0)return; // Device change may clear the selection.
            if(auto* device=deviceManager.getCurrentAudioDevice()){
                const int physicalInput=selectedId-1;
                if(physicalInput>=device->getInputChannelNames().size() ||
                   !device->getActiveInputChannels()[physicalInput])return;
            }else return;
            session.tracks[selectedTrack].inputChannel=selectedId-1;
        };
        monitorControl.setButtonText("HEADPHONES ONLY");
        monitorControl.setTooltip("Dry monitoring of the first armed input; headphones required. OFF by default.");
        monitorControl.setToggleState(false,juce::dontSendNotification);
        monitorControl.setEnabled(false);
        addAndMakeVisible(monitorControl);
        preflightButton.setButtonText("PREFLIGHT");
        preflightButton.setTooltip("Check audio input, track routing, output, and recording folder before a session.");
        addAndMakeVisible(preflightButton);
        preflightButton.onClick=[this]{showRecordingPreflight();};
        audioDevices.setButtonText("AUDIO DEVICES");
        audioDevices.setTooltip("Select a Windows ASIO driver and enable your PreSonus AudioBox microphone input.");
        addAndMakeVisible(audioDevices);
        audioDevices.onClick=[this]{
            if(playing.load() || recorder.isRecording() || multiRecorder.isRecording() || exportRunning.load()){
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Stop audio first","Stop recording and playback before changing the audio interface.");
                return;
            }
            auto* selector=new juce::AudioDeviceSelectorComponent(
                deviceManager,0,8,1,2,false,false,false,false);
            selector->setSize(540,440);
            juce::DialogWindow::LaunchOptions options;
            options.dialogTitle="IDW Audio Devices - Select PreSonus AudioBox";
            options.content.setOwned(selector);
            options.dialogBackgroundColour=juce::Colours::darkgrey;
            options.escapeKeyTriggersCloseButton=true;
            options.useNativeTitleBar=true;
            options.resizable=false;
            options.launchAsync();
        };
        monitorControl.onClick=[this]{
            const bool enabled=monitorControl.getToggleState() && multiRecorder.isRecording();
            multiRecorder.setHeadphoneMonitor(enabled,0.1f);
            if(!multiRecorder.isRecording())
                monitorControl.setToggleState(false,juce::dontSendNotification);
        };
        armControl.setButtonText("ARM INPUT");addAndMakeVisible(armControl);
        // Tracks start disarmed; the user explicitly arms the intended destination.
        armedTracks.fill(false);
        armControl.onClick=[this]{
            if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load() ||
               selectedTrack>=session.tracks.size() ||
               session.tracks[selectedTrack].type!=idw::daw::TrackType::audio)return;
            // Treat our per-track arm state as authoritative; do not depend on the
            // button toggle state, which can be refreshed when selection changes.
            armedTracks[selectedTrack]=!armedTracks[selectedTrack];
            refreshMixer();
        };
        previousTrack.onClick=[this]{selectTrack(-1);};
        nextTrack.onClick=[this]{selectTrack(1);};
        masterLabel.setText("MASTER VOL",juce::dontSendNotification);
        addAndMakeVisible(masterLabel);
        masterGain.setRange(0.0,1.5,0.01);
        masterGain.setValue(1.0);
        masterGain.setTextBoxStyle(juce::Slider::TextBoxRight,false,55,25);
        addAndMakeVisible(masterGain);
        masterGain.onValueChange=[this]{masterLevel.store((float)masterGain.getValue(),std::memory_order_relaxed);};
        clearClips.setButtonText("RESET CLIP");addAndMakeVisible(clearClips);
        clearClips.onClick=[this]{peakClipLeft.store(false);peakClipRight.store(false);repaint();};
        eqLabel.setText("MASTER TONE",juce::dontSendNotification);addAndMakeVisible(eqLabel);
        lowEq.setRange(-12,12,0.5);lowEq.setValue(0);
        highEq.setRange(-12,12,0.5);highEq.setValue(0);
        for(auto* slider:{&lowEq,&highEq}){
            slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,56,24);
            addAndMakeVisible(*slider);
        }
        lowEq.onValueChange=[this]{lowDb.store((float)lowEq.getValue());};
        highEq.onValueChange=[this]{highDb.store((float)highEq.getValue());};
        limitLabel.setText("OUTPUT CEILING",juce::dontSendNotification);addAndMakeVisible(limitLabel);
        ceilingControl.setRange(-12.0,-0.1,0.1);ceilingControl.setValue(-1.0);
        ceilingControl.setTextBoxStyle(juce::Slider::TextBoxRight,false,60,24);
        addAndMakeVisible(ceilingControl);
        ceilingControl.onValueChange=[this]{ceilingDb.store((float)ceilingControl.getValue(),std::memory_order_relaxed);};
        refreshMixer();
    }
    void resized() override {
        auto area=getLocalBounds().reduced(20);
        setWantsKeyboardFocus(true);
        auto header=area.removeFromTop(42);
        recordReadiness.setBounds(26,249,juce::jmax(100,getWidth()-52),18);
        skinToolsButton.setBounds(header.removeFromRight(112).reduced(3));
        skinSelector.setBounds(header.removeFromRight(165).reduced(3));
        title.setBounds(header);
        auto controls=area.removeFromTop(44);
        play.setBounds(controls.removeFromLeft(110).reduced(4));
        stop.setBounds(controls.removeFromLeft(100).reduced(4));
        tempoLabel.setBounds(controls.removeFromLeft(45));
        tempo.setBounds(controls.removeFromLeft(195));
        audio.setBounds(controls.removeFromLeft(160).reduced(3));
        midi.setBounds(controls.removeFromLeft(160).reduced(3));
        open.setBounds(controls.removeFromLeft(180).reduced(3));
        record.setBounds(controls.removeFromLeft(165).reduced(3));
        auto projectRow=area.removeFromTop(40);
        saveProject.setBounds(projectRow.removeFromLeft(145).reduced(3));
        openProject.setBounds(projectRow.removeFromLeft(145).reduced(3));
        exportProject.setBounds(projectRow.removeFromLeft(160).reduced(3));
        bounce.setBounds(projectRow.removeFromLeft(125).reduced(3));
        cancelBounce.setBounds(projectRow.removeFromLeft(135).reduced(3));
        connectButton.setBounds(projectRow.removeFromLeft(125).reduced(3));
        aiButton.setBounds(projectRow.removeFromLeft(125).reduced(3));
        pianoButton.setBounds(projectRow.removeFromLeft(125).reduced(3));
        synthPreset.setBounds(projectRow.removeFromLeft(100).reduced(3));
        voiceCommandInput.setBounds(projectRow.removeFromLeft(145).reduced(3));
        runVoiceCommand.setBounds(projectRow.removeFromLeft(80).reduced(3));
        voiceHistoryButton.setBounds(projectRow.removeFromLeft(80).reduced(3));
        pushToTalkButton.setBounds(projectRow.removeFromLeft(70).reduced(3));
        commandStatus.setBounds(projectRow.removeFromLeft(95).reduced(3));
        auto mixRow=area.removeFromTop(42);
        gainLabel.setBounds(mixRow.removeFromLeft(110));
        gainControl.setBounds(mixRow.removeFromLeft(165));
        panLabel.setBounds(mixRow.removeFromLeft(45));
        panControl.setBounds(mixRow.removeFromLeft(165));
        muteControl.setBounds(mixRow.removeFromLeft(80));
        soloControl.setBounds(mixRow.removeFromLeft(75));
        previousTrack.setBounds(mixRow.removeFromLeft(100));
        nextTrack.setBounds(mixRow.removeFromLeft(100));
        armControl.setBounds(mixRow.removeFromLeft(105));
        inputChannel.setBounds(mixRow.removeFromLeft(90));
        auto monitorRow=area.removeFromTop(28);
        monitorControl.setBounds(monitorRow.removeFromLeft(190));
        audioDevices.setBounds(monitorRow.removeFromLeft(155).reduced(2));
        preflightButton.setBounds(monitorRow.removeFromLeft(130).reduced(2));
        inputLevel.setBounds(area.removeFromTop(26));
        auto overdubRow=area.removeFromTop(36);
        overdubControl.setBounds(overdubRow.removeFromLeft(285));
        latencyLabel.setBounds(overdubRow.removeFromLeft(120));
        latencyAdjust.setBounds(overdubRow.removeFromLeft(200));
        analyzeCalibration.setBounds(overdubRow.removeFromLeft(200).reduced(3));
        generateCalibration.setBounds(overdubRow.removeFromLeft(170).reduced(3));
        liveCalibration.setBounds(overdubRow.removeFromLeft(175).reduced(3));
        auto masterRow=area.removeFromTop(38);
        masterLabel.setBounds(masterRow.removeFromLeft(115));
        masterGain.setBounds(masterRow.removeFromLeft(260));
        clearClips.setBounds(masterRow.removeFromLeft(120).reduced(3));
        auto synthRow=area.removeFromTop(35);
        attackLabel.setBounds(synthRow.removeFromLeft(100));
        attackControl.setBounds(synthRow.removeFromLeft(175));
        releaseLabel.setBounds(synthRow.removeFromLeft(115));
        releaseControl.setBounds(synthRow.removeFromLeft(175));
        auto eqRow=area.removeFromTop(35);
        eqLabel.setBounds(eqRow.removeFromLeft(115));
        lowEq.setBounds(eqRow.removeFromLeft(180));
        highEq.setBounds(eqRow.removeFromLeft(180));
        auto ceilingRow=area.removeFromTop(35);
        limitLabel.setBounds(ceilingRow.removeFromLeft(145));
        ceilingControl.setBounds(ceilingRow.removeFromLeft(220));
        connectPanel.setBounds(juce::Rectangle<int>(20,335,juce::jmin(getWidth()-40,565),205));
        aiPanel.setBounds(juce::Rectangle<int>(20,335,juce::jmin(getWidth()-40,575),225));
        pianoRoll.setBounds(juce::Rectangle<int>(20,335,juce::jmax(100,getWidth()-40),480));
    }
    void startDictation(){
#if JUCE_WINDOWS
        if(voiceListening.exchange(false)){
            pushToTalkButton.setButtonText("TALK");
            commandStatus.setText("VOICE: listening cancelled",juce::dontSendNotification);
            return;
        }
        if(voiceListenThread.joinable())voiceListenThread.join();
        voiceListening.store(true);
        pushToTalkButton.setButtonText("STOP TALK");
        commandStatus.setText("LISTENING: say one command (8 seconds max)",juce::dontSendNotification);
        juce::Component::SafePointer<Workstation> safeThis(this);
        voiceListenThread=std::thread([this,safeThis]{
            const HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            ISpRecognizer* recognizer=nullptr;
            ISpRecoContext* context=nullptr;
            ISpRecoGrammar* grammar=nullptr;
            bool recognized=false;
            bool recognizerReady=false;
            juce::String transcript;
            if(SUCCEEDED(initialized) &&
               SUCCEEDED(CoCreateInstance(CLSID_SpSharedRecognizer,nullptr,CLSCTX_ALL,
                                         IID_ISpRecognizer,(void**)&recognizer)) &&
               SUCCEEDED(recognizer->CreateRecoContext(&context)) &&
               SUCCEEDED(context->SetNotifyWin32Event()) &&
               SUCCEEDED(context->SetInterest(SPFEI(SPEI_RECOGNITION),SPFEI(SPEI_RECOGNITION))) &&
               SUCCEEDED(context->CreateGrammar(1,&grammar)) &&
               SUCCEEDED(grammar->LoadDictation(nullptr,SPLO_STATIC)) &&
               SUCCEEDED(grammar->SetDictationState(SPRS_ACTIVE))){
                recognizerReady=true;
                const auto endAt=juce::Time::getMillisecondCounter()+8000u;
                while(voiceListening.load() &&
                      static_cast<juce::int32>(endAt-juce::Time::getMillisecondCounter())>0){
                    if(WaitForSingleObject(context->GetNotifyEventHandle(),150)!=WAIT_OBJECT_0)continue;
                    SPEVENT event{};
                    ULONG fetched=0;
                    while(SUCCEEDED(context->GetEvents(1,&event,&fetched)) && fetched){
                        if(event.eEventId==SPEI_RECOGNITION && event.lParam){
                            auto* result=reinterpret_cast<ISpRecoResult*>(event.lParam);
                            WCHAR* words=nullptr;
                            if(SUCCEEDED(result->GetText(SP_GETWHOLEPHRASE,SP_GETWHOLEPHRASE,
                                                         FALSE,&words,nullptr)) && words){
                                transcript=juce::String(words).trim();
                                CoTaskMemFree(words);
                                recognized=transcript.isNotEmpty();
                            }
                        }
                        SpClearEvent(&event);
                        if(recognized)break;
                    }
                    if(recognized)break;
                }
            }
            if(grammar){grammar->SetDictationState(SPRS_INACTIVE);grammar->Release();}
            if(context)context->Release();
            if(recognizer)recognizer->Release();
            if(SUCCEEDED(initialized))CoUninitialize();
            const bool active=voiceListening.exchange(false);
            juce::MessageManager::callAsync([safeThis,transcript,recognized,recognizerReady,active]{
                if(safeThis==nullptr)return;
                safeThis->pushToTalkButton.setButtonText("TALK");
                if(!active)return; // User cancelled: discard any buffered words.
                if(recognized){
                    safeThis->voiceCommandInput.setText(transcript,juce::dontSendNotification);
                    safeThis->executeStudioCommand(transcript);
                }else if(!recognizerReady)
                    safeThis->commandStatus.setText("VOICE: Windows Speech recognizer unavailable; check speech settings",
                                                    juce::dontSendNotification);
                else
                    safeThis->commandStatus.setText("VOICE: no speech recognized; try TALK again",
                                                    juce::dontSendNotification);
            });
        });
#else
        voiceCommandInput.grabKeyboardFocus();
        commandStatus.setText("Speech recognition currently available on Windows only",
                              juce::dontSendNotification);
#endif
    }
    bool clipEditAvailable() const {
        return !playing.load() && !recorder.isRecording() && !multiRecorder.isRecording() &&
            !exportRunning.load() && !aiPanel.isVisible() && !connectPanel.isVisible();
    }
    void checkpointClips(){
        if(clipUndo.size()>=64)clipUndo.erase(clipUndo.begin());
        clipUndo.push_back(session);
        clipRedo.clear();
    }
    void restoreClipHistory(bool undo){
        if(!clipEditAvailable())return;
        auto& from=undo?clipUndo:clipRedo;
        auto& to=undo?clipRedo:clipUndo;
        if(!idw::daw::restoreSessionHistory(session,from,to))return;
        selectedClipValid=false;
        repaint();
    }
    void splitSelectedClip(){
        if(!clipEditAvailable() || !selectedClipValid ||
           selectedClipTrack>=session.tracks.size() ||
           selectedClipIndex>=session.tracks[selectedClipTrack].clips.size())return;
        const auto before=session;
        if(session.splitClip(selectedClipTrack,selectedClipIndex,playheadBeats)){
            if(clipUndo.size()>=64)clipUndo.erase(clipUndo.begin());
            clipUndo.push_back(before);clipRedo.clear();repaint();
        }
    }
    void showClipFadeMenu(size_t track,size_t index) {
        if(!clipEditAvailable())return;
        juce::PopupMenu menu;
        menu.addSectionHeader("Clip fades (beats)");
        menu.addItem(1,"Clear fades");
        menu.addItem(2,"Fade in: 1/4 beat");
        menu.addItem(3,"Fade out: 1/4 beat");
        menu.addItem(4,"Both edges: 1/4 beat");
        menu.addItem(5,"Linear curves");
        menu.addItem(6,"Equal-power curves");
        std::vector<size_t> partners;
        for(size_t i=0;i<session.tracks[track].clips.size();++i){
            auto trial=session;
            if(trial.crossfadeClips(track,index,i,idw::daw::FadeCurve::linear)){
                partners.push_back(i);
                juce::PopupMenu curves;
                curves.addItem(100+(int)partners.size()*2,"Linear (correlated audio)");
                curves.addItem(101+(int)partners.size()*2,"Equal power (different audio)");
                menu.addSubMenu("Crossfade with clip "+juce::String((int)i+1),curves);
            }
        }
        const auto beforeJson=juce::JSON::toString(idw::daw::sessionToJson(session));
        menu.showMenuAsync(juce::PopupMenu::Options(),
            [safe=juce::Component::SafePointer<Workstation>(this),track,index,partners,beforeJson](int result){
                if(safe==nullptr || result==0 || !safe->clipEditAvailable() ||
                   juce::JSON::toString(idw::daw::sessionToJson(safe->session))!=beforeJson)return;
                const auto before=safe->session;
                const auto c=before.tracks[track].clips[index];
                bool changed=false;
                using Curve=idw::daw::FadeCurve;
                if(result>=102){
                    const size_t partner=(size_t)((result-102)/2);
                    if(partner<partners.size())changed=safe->session.crossfadeClips(track,index,partners[partner],
                        result%2 ? Curve::equalPower : Curve::linear);
                }else{
                    const double duration=std::min(0.25,c.lengthBeats);
                    changed=safe->session.setClipFades(track,index,
                        result==1?0:(result==2 || result==4?duration:c.fadeInBeats),
                        result==1?0:(result==3 || result==4?duration:c.fadeOutBeats),
                        result==5?Curve::linear:result==6?Curve::equalPower:c.fadeInCurve,
                        result==5?Curve::linear:result==6?Curve::equalPower:c.fadeOutCurve);
                }
                if(changed && juce::JSON::toString(idw::daw::sessionToJson(safe->session))!=beforeJson){
                    if(safe->clipUndo.size()>=64)safe->clipUndo.erase(safe->clipUndo.begin());
                    safe->clipUndo.push_back(before);safe->clipRedo.clear();safe->repaint();
                }
            });
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if(key.getModifiers().isCommandDown() && !key.getModifiers().isAltDown()){
            const int code=key.getKeyCode();
            if(code=='=' || code=='+' || code=='-' || code=='0'){
                const int next=code=='0' ? idw::daw::defaultTimelineZoom :
                    juce::jlimit(0,idw::daw::maxTimelineZoom,
                        timelineZoomLevel+(code=='-' ? -1 : 1));
                if(next!=timelineZoomLevel){timelineZoomLevel=next;clampTimelineScroll();repaint();}
                return true;
            }
        }
        if(key.getKeyCode()==juce::KeyPress::F9Key && !key.getModifiers().isAltDown() &&
           !key.getModifiers().isCommandDown()){
            startDictation();
            return true;
        }
        if(key.getModifiers().isCommandDown() && key.getKeyCode()=='Z'){
            restoreClipHistory(!key.getModifiers().isShiftDown());return true;
        }
        if(key.getModifiers().isCommandDown() && key.getKeyCode()=='Y'){
            restoreClipHistory(false);return true;
        }
        if(key.getKeyCode()=='S' && !key.getModifiers().isCommandDown()){
            splitSelectedClip();return true;
        }
        if(!selectedClipValid || playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) ||
           exportRunning.load() || aiPanel.isVisible() || connectPanel.isVisible())
            return false;
        if(selectedClipTrack>=session.tracks.size() ||
           selectedClipIndex>=session.tracks[selectedClipTrack].clips.size()){
            selectedClipValid=false;
            return false;
        }
        const auto& clip=session.tracks[selectedClipTrack].clips[selectedClipIndex];
        // Alt gives keyboard edits sixteenth-beat precision without changing
        // the existing quarter-beat shortcuts or audio source files.
        const double editStep=key.getModifiers().isAltDown()?0.0625:0.25;
        if(key.getKeyCode()=='[' || key.getKeyCode()==']'){
            const auto before=session;
            const bool changed=key.getKeyCode()=='['
                ? session.trimClipLeft(selectedClipTrack,selectedClipIndex,clip.startBeat+editStep)
                : session.trimClipRight(selectedClipTrack,selectedClipIndex,
                    clip.startBeat+clip.lengthBeats-editStep);
            if(changed){if(clipUndo.size()>=64)clipUndo.erase(clipUndo.begin());
                clipUndo.push_back(before);clipRedo.clear();repaint();}
            return changed;
        }
        const bool shift=key.getModifiers().isShiftDown();
        const auto before=session;
        bool changed=false;
        if(key.getKeyCode()==juce::KeyPress::leftKey)
            changed=shift ? session.resizeClip(selectedClipTrack,selectedClipIndex,
                         std::max(editStep,clip.lengthBeats-editStep))
                          : session.moveClip(selectedClipTrack,selectedClipIndex,
                         std::max(0.0,clip.startBeat-editStep));
        else if(key.getKeyCode()==juce::KeyPress::rightKey)
            changed=shift ? session.resizeClip(selectedClipTrack,selectedClipIndex,
                         clip.lengthBeats+editStep)
                          : session.moveClip(selectedClipTrack,selectedClipIndex,
                         clip.startBeat+editStep);
        if(changed){
            const auto& previous=before.tracks[selectedClipTrack].clips[selectedClipIndex];
            const auto& current=session.tracks[selectedClipTrack].clips[selectedClipIndex];
            if(previous.startBeat!=current.startBeat ||
               previous.lengthBeats!=current.lengthBeats){
                if(clipUndo.size()>=64)clipUndo.erase(clipUndo.begin());
                clipUndo.push_back(before);
                clipRedo.clear();
                repaint();
            }
        }
        return changed;
    }
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override {
        const int mixerHeight=neoMixerExpanded ? 190 : 112;
        const int mixerTop=getHeight()-mixerHeight-48;
        const int arrangementBottom=mixerTop>420 ? mixerTop-8 : getHeight()-48;
        if(event.x>=205 && event.x<getWidth()-20 && event.y>=330 &&
           event.y<arrangementBottom && !aiPanel.isVisible() && !connectPanel.isVisible() &&
           (event.mods.isShiftDown() || std::abs(wheel.deltaX)>std::abs(wheel.deltaY))){
            const double motion=std::abs(wheel.deltaX)>std::abs(wheel.deltaY)
                ? -wheel.deltaX : -wheel.deltaY;
            timelineScrollBeats=idw::daw::timelineClampScroll(
                timelineScrollBeats+motion*16.0,timelineContentEndBeat(),
                (double)juce::jmax(1,getWidth()-230),timelineZoomLevel);
            repaint();
            return;
        }
        if(event.x>=20 && event.x<getWidth()-20 && event.y>=366 && event.y<arrangementBottom && !session.tracks.empty() && !aiPanel.isVisible() && !connectPanel.isVisible()){
            const int visible=juce::jmax(1,(arrangementBottom-366)/70);
            const size_t maximum=session.tracks.size()>(size_t)visible ?
                session.tracks.size()-(size_t)visible : 0;
            if(wheel.deltaY<0 && arrangementScrollTrack<maximum)++arrangementScrollTrack;
            else if(wheel.deltaY>0 && arrangementScrollTrack>0)--arrangementScrollTrack;
            repaint();
            return;
        }
        juce::AudioAppComponent::mouseWheelMove(event,wheel);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        draggingClip=false;
        dragHistorySnapshot.reset();
        // Keep the selected clip when clicking the ruler to position a split.
        if(!(e.y>=330 && e.y<366 && e.x>=205))selectedClipValid=false;
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load() ||
           aiPanel.isVisible() || connectPanel.isVisible())return;
        // The Neo Mixer overview doubles as a channel selector when stopped.
        // Handle this before hit-testing clips beneath the overview.
        const int mixerHeight=neoMixerExpanded ? 190 : 112;
        const int mixerTop=getHeight()-mixerHeight-48;
        if(mixerTop>420 && e.y>=mixerTop && e.y<mixerTop+mixerHeight && e.x>=30 && !session.tracks.empty()){ 
            if(e.y<mixerTop+22 && e.x<360){neoMixerExpanded=!neoMixerExpanded;ensureSelectedTrackVisible();repaint();return;}
            const int count=juce::jmin(10,(int)session.tracks.size());
            const int width=juce::jmax(65,juce::jmin(120,(getWidth()-180)/juce::jmax(1,count+1)));
            const int index=(e.x-30)/width;
            if(index>=0 && index<count && e.x<30+(index+1)*width){
                selectedTrack=(size_t)index;
                const int faderX=30+index*width+width-19;
                if(e.x>=faderX-9 && e.x<=faderX+9 && e.y>=mixerTop+47 && e.y<mixerTop+mixerHeight-8){
                    draggingMixerFader=true;
                    updateMixerFader(e.y);
                }else if(e.y>=mixerTop+42 && e.y<=mixerTop+58 && e.x>=30+index*width+5 && e.x<faderX-12){
                    draggingMixerPan=true;
                    updateMixerPan(e.x,30+index*width,width);
                }
                refreshMixer();
                repaint();
            }
            return;
        }
        constexpr int top=366, height=70, left=205;
        if(e.y>=330 && e.y<366 && e.x>=left){
            playheadBeats=std::round(idw::daw::timelineBeatAtPixel(e.x,left,timelineZoomLevel,timelineScrollBeats)*4.0)/4.0;
            repaint();return;
        }
        if(e.y<top || e.x<left || session.tracks.empty())return;
        const int visibleMixerHeight=neoMixerExpanded ? 190 : 112;
        const int visibleMixerTop=getHeight()-visibleMixerHeight-48;
        const int arrangementLimit=visibleMixerTop>420 ? visibleMixerTop-8 : getHeight()-48;
        if(e.y>=arrangementLimit)return;
        const auto ti=(size_t)((e.y-top)/height)+arrangementScrollTrack;
        if(ti>=session.tracks.size())return;
        const auto& clips=session.tracks[ti].clips;
        for(size_t i=clips.size();i>0;--i){
            const auto& c=clips[i-1];
            const int x=juce::roundToInt(idw::daw::timelinePixelAtBeat(c.startBeat,left,timelineZoomLevel,timelineScrollBeats));
            const int w=juce::jmax(2,juce::roundToInt(c.lengthBeats*idw::daw::timelinePixelsPerBeat(timelineZoomLevel)));
            if(e.x>=left && e.x>=x && e.x<x+w && e.y>=top+((int)ti-(int)arrangementScrollTrack)*height+8 &&
               e.y<top+((int)ti-(int)arrangementScrollTrack+1)*height-10){
                dragTrack=ti;dragIndex=i-1;dragOriginX=e.x;
                selectedClipTrack=ti;selectedClipIndex=i-1;selectedClipValid=true;
                if(e.mods.isPopupMenu()){
                    showClipFadeMenu(ti,i-1);repaint();return;
                }
                dragOriginalStart=c.startBeat;dragOriginalLength=c.lengthBeats;
                dragFade=0;
                const int rowTop=top+((int)ti-(int)arrangementScrollTrack)*height;
                if(e.y<rowTop+22){
                    const int inX=x+juce::roundToInt(c.fadeInBeats*idw::daw::timelinePixelsPerBeat(timelineZoomLevel));
                    const int outX=x+w-juce::roundToInt(c.fadeOutBeats*idw::daw::timelinePixelsPerBeat(timelineZoomLevel));
                    if(std::abs(e.x-inX)<=7 || std::abs(e.x-outX)<=7)
                        dragFade=std::abs(e.x-inX)<=std::abs(e.x-outX)?1:2;
                }
                dragResize=e.x>=x+w-10;
                draggingClip=true;
                dragHistorySnapshot=session;
                grabKeyboardFocus();
                repaint();
                break;
            }
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if(draggingMixerPan){
            if(!playing.load() && !recorder.isRecording() && !multiRecorder.isRecording() &&
               !exportRunning.load()){
                const int count=juce::jmin(10,(int)session.tracks.size());
                const int width=juce::jmax(65,juce::jmin(120,(getWidth()-180)/juce::jmax(1,count+1)));
                updateMixerPan(e.x,30+(int)selectedTrack*width,width);
            }
            return;
        }
        if(draggingMixerFader){
            if(!playing.load() && !recorder.isRecording() && !multiRecorder.isRecording() &&
               !exportRunning.load())updateMixerFader(e.y);
            return;
        }
        if(!draggingClip || playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) ||
           exportRunning.load())return;
        const auto delta=(double)(e.x-dragOriginX)/idw::daw::timelinePixelsPerBeat(timelineZoomLevel);
        // Quarter-beat snapping on the visible timeline.
        const auto snap=[](double beats){return std::max(0.0,std::round(beats*4.0)/4.0);};
        if(dragFade && dragHistorySnapshot){
            const auto& original=dragHistorySnapshot->tracks[dragTrack].clips[dragIndex];
            const double duration=std::clamp((dragFade==1?original.fadeInBeats:original.fadeOutBeats)+
                (dragFade==1?delta:-delta),0.0,original.lengthBeats);
            session.setClipFades(dragTrack,dragIndex,
                dragFade==1?duration:original.fadeInBeats,dragFade==2?duration:original.fadeOutBeats,
                original.fadeInCurve,original.fadeOutCurve);
        }else if(dragResize){
            // Re-evaluate from the gesture's original clip, so shortening then
            // dragging back does not permanently clamp its fade durations.
            if(dragHistorySnapshot)
                session.tracks[dragTrack].clips[dragIndex]=dragHistorySnapshot->tracks[dragTrack].clips[dragIndex];
            session.resizeClip(dragTrack,dragIndex,std::max(0.25,snap(dragOriginalLength+delta)));
        }else
            session.moveClip(dragTrack,dragIndex,snap(dragOriginalStart+delta));
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {
        if(draggingClip && dragHistorySnapshot &&
           dragTrack<session.tracks.size() &&
           dragIndex<session.tracks[dragTrack].clips.size() &&
           dragTrack<dragHistorySnapshot->tracks.size() &&
           dragIndex<dragHistorySnapshot->tracks[dragTrack].clips.size()){
            const auto& oldClip=dragHistorySnapshot->tracks[dragTrack].clips[dragIndex];
            const auto& newClip=session.tracks[dragTrack].clips[dragIndex];
            if(oldClip.startBeat!=newClip.startBeat || oldClip.lengthBeats!=newClip.lengthBeats ||
               oldClip.fadeInBeats!=newClip.fadeInBeats || oldClip.fadeOutBeats!=newClip.fadeOutBeats){
                if(clipUndo.size()>=64)clipUndo.erase(clipUndo.begin());
                clipUndo.push_back(std::move(*dragHistorySnapshot));
                clipRedo.clear();
            }
        }
        dragHistorySnapshot.reset();
        draggingClip=false;draggingMixerFader=false;draggingMixerPan=false;
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(useCustomBackground?customBackground:juce::Colour(activeSkin==1?0xff17100b:activeSkin==2?0xff091c18:activeSkin==3?0xff160d22:0xff080f1c));
        // Neo Studio control decks: visual grouping stays outside the audio callback.
        const auto deck=[&g,this](juce::Rectangle<float> bounds,const juce::String& label){
            g.setColour(useCustomPanel ? customPanel : juce::Colour(activeSkin==1?0xff2b2118:activeSkin==2?0xff112d26:activeSkin==3?0xff261d39:0xff101f32));
            g.fillRoundedRectangle(bounds,10.0f);
            g.setColour(neoTheme.accentColour().withAlpha(0.32f));
            g.drawRoundedRectangle(bounds,10.0f,1.0f);
            g.setColour(neoTheme.accentColour());
            g.setFont(juce::Font(juce::FontOptions(11.0f,juce::Font::bold)));
            g.drawText(label,bounds.removeFromTop(16.0f).reduced(10.0f,0.0f),juce::Justification::centredLeft);
        };
        const auto fullWidth=(float)juce::jmax(100,getWidth()-40);
        deck({20.0f,68.0f,fullWidth,93.0f},"TRANSPORT  /  PROJECT");
        deck({20.0f,164.0f,fullWidth,84.0f},"TRACK CONTROL  /  INPUT ROUTING");
        deck({20.0f,251.0f,fullWidth,74.0f},"MASTER  /  SYNTH  /  PROCESSING");
        const auto rect=getLocalBounds().reduced(20);
        const int top=330, trackHeight=70, left=205;
        g.setColour(juce::Colour(activeSkin==1?0xff352a1e:activeSkin==2?0xff173e32:activeSkin==3?0xff302340:0xff122338));g.fillRect(20,top,getWidth()-40,36);
        g.setColour(juce::Colours::lightgrey);
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText("ZOOM "+juce::String(juce::roundToInt(100.0*idw::daw::timelinePixelsPerBeat(timelineZoomLevel)/12.0))+"%  Shift+wheel",
                   24,top+5,177,26,juce::Justification::centredLeft);
        const int firstBar=juce::jmax(0,(int)std::floor(timelineScrollBeats/4.0));
        const int barsVisible=juce::jmax(1,(int)std::ceil(
            (double)juce::jmax(1,getWidth()-left)/(
                4.0*idw::daw::timelinePixelsPerBeat(timelineZoomLevel)))+2);
        for(int bar=firstBar;bar<firstBar+barsVisible;++bar){
            const int x=juce::roundToInt(idw::daw::timelinePixelAtBeat(
                bar*4.0,left,timelineZoomLevel,timelineScrollBeats));
            if(x<left || x>=getWidth()-30)continue;
            g.drawVerticalLine(x,(float)top,(float)(getHeight()-30));
            g.drawText(juce::String(bar+1),x+3,top,40,30,juce::Justification::centredLeft);
        }
        const int mixerHeight=neoMixerExpanded ? 190 : 112;
        const int mixerTop=getHeight()-mixerHeight-48;
        const int arrangementBottom=mixerTop>420 ? mixerTop-8 : getHeight()-48;
        // Clip all arrangement drawing so expanding the mixer does not obscure
        // or receive stray clicks from timeline regions below its viewport.
        g.saveState();
        g.reduceClipRegion(20,top,getWidth()-40,juce::jmax(0,arrangementBottom-top));
        int y=top+36;
        size_t renderedTrackIndex=0;
        for(const auto& track:session.tracks){
            if(renderedTrackIndex<arrangementScrollTrack){++renderedTrackIndex;continue;}
            g.setColour(juce::Colour(activeSkin==1?((y/trackHeight)%2?0xff33291e:0xff282016):activeSkin==2?((y/trackHeight)%2?0xff173b2f:0xff112c24):activeSkin==3?((y/trackHeight)%2?0xff302342:0xff261d36):((y/trackHeight)%2?0xff16283d:0xff102033)));
            g.fillRect(20,y,getWidth()-40,trackHeight-2);
            const bool selected=renderedTrackIndex==selectedTrack;
            const bool armed=track.type==idw::daw::TrackType::audio &&
                renderedTrackIndex<armedTracks.size() && armedTracks[renderedTrackIndex];
            const auto laneAccent=armed ? juce::Colour(0xfff44368) :
                (track.type==idw::daw::TrackType::audio ? juce::Colour(0xff159cdb) : juce::Colour(0xffa66cff));
            g.setColour(laneAccent);
            g.fillRect(20,y,selected?6:4,trackHeight-2);
            if(selected){
                g.setColour(neoTheme.accentColour().withAlpha(0.75f));
                g.drawRect(20,y,getWidth()-40,trackHeight-2,1);
            }
            if(armed){
                g.setColour(juce::Colour(0xfff44368));
                g.fillEllipse(177.0f,(float)y+24.0f,12.0f,12.0f);
            }
            g.setColour(juce::Colours::white);
            g.drawText(juce::String(track.name),35,y+5,145,25,juce::Justification::centredLeft);
            // Compact Neo Studio mixer strip in each track header.
            const float stripX=36.0f,stripY=(float)y+45.0f,stripW=120.0f;
            g.setColour(juce::Colour(0xff304456));
            g.fillRoundedRectangle(stripX,stripY,stripW,5.0f,2.0f);
            g.setColour(laneAccent.withAlpha(0.9f));
            g.fillRoundedRectangle(stripX,stripY,stripW*juce::jlimit(0.0f,1.0f,track.gain/2.0f),5.0f,2.0f);
            g.setColour(juce::Colour(0xff94bbd1));
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.drawText("VOL",36,y+32,37,12,juce::Justification::centredLeft);
            g.drawText(track.mute ? "MUTED" : (track.solo ? "SOLO" : (armed ? "REC READY" : "READY")),
                       83,y+32,100,12,juce::Justification::centredRight);
            size_t renderedClipIndex=0;
            for(const auto& clip:track.clips){
                const int clipX=juce::roundToInt(idw::daw::timelinePixelAtBeat(clip.startBeat,left,timelineZoomLevel,timelineScrollBeats));
                const int clipWidth=juce::jmax(2,juce::roundToInt(clip.lengthBeats*idw::daw::timelinePixelsPerBeat(timelineZoomLevel)));
                if(clipX>=getWidth()-25 || clipX+clipWidth<=left){
                    ++renderedClipIndex;continue;
                }
                // Prevent scrolled clips and waveform labels from painting over
                // the track controls at the left of the arrangement.
                g.saveState();
                g.reduceClipRegion(left,y,getWidth()-left-25,trackHeight);
                g.setColour(track.type==idw::daw::TrackType::audio ? neoTheme.accentColour().darker(0.72f) : neoTheme.accentColour().darker(0.92f));
                g.fillRoundedRectangle((float)clipX,(float)y+8,(float)juce::jmin(clipWidth,getWidth()-25-clipX), (float)trackHeight-18,4.0f);
                if(track.type==idw::daw::TrackType::audio && clipWidth>8){
                    const auto waveform=waveformThumbnails.find(clip.sourcePath);
                    const auto interval=idw::daw::clipWaveformWindow(clip,session.bpm);
                    if(waveform!=waveformThumbnails.end() && interval &&
                       waveform->second->getTotalLength()>0){
                        // Clip the preview to the visible lane, but map its time
                        // against the entire clip (including trimmed source offset).
                        g.saveState();
                        g.reduceClipRegion(clipX+3,y+25,
                            juce::jmax(0,juce::jmin(clipWidth-6,getWidth()-28-clipX)),
                            trackHeight-39);
                        g.setColour(juce::Colours::white.withAlpha(0.60f));
                        waveform->second->drawChannels(g,
                            juce::Rectangle<int>(clipX+3,y+25,clipWidth-6,trackHeight-39),
                            interval->startSeconds,interval->endSeconds,0.95f);
                        g.restoreState();
                    }
                }
                if(selectedClipValid && renderedTrackIndex==selectedClipTrack && renderedClipIndex==selectedClipIndex){
                    g.setColour(neoTheme.accentColour());
                    g.drawRoundedRectangle((float)clipX,(float)y+8,(float)juce::jmin(clipWidth,getWidth()-25-clipX),(float)trackHeight-18,4.0f,2.0f);
                }
                g.setColour(juce::Colours::white.withAlpha(0.8f));
                const auto drawFade=[&](bool fadeIn){
                    const double beats=fadeIn?clip.fadeInBeats:clip.fadeOutBeats;
                    const auto curve=fadeIn?clip.fadeInCurve:clip.fadeOutCurve;
                    const float width=(float)(beats*idw::daw::timelinePixelsPerBeat(timelineZoomLevel));
                    const float edge=(float)(fadeIn?clipX:clipX+clipWidth);
                    juce::Path path;
                    for(int point=0;point<=24;++point){
                        const float t=(float)point/24;
                        const float px=edge+(fadeIn?1.0f:-1.0f)*width*t;
                        const float py=(float)y+trackHeight-10-
                            idw::daw::ClipEnvelope::curve(t,curve)*(trackHeight-18);
                        if(point==0)path.startNewSubPath(px,py);else path.lineTo(px,py);
                    }
                    if(beats>0)g.strokePath(path,juce::PathStrokeType(1.5f));
                    g.fillRect(edge+(fadeIn?width:-width)-3,(float)y+8,6.0f,6.0f);
                };
                drawFade(true);drawFade(false);
                ++renderedClipIndex;
                g.setColour(juce::Colours::white);
                g.drawFittedText(juce::File(juce::String(clip.sourcePath)).getFileName(),clipX+5,y+14,juce::jmax(0,juce::jmin(clipWidth-10,getWidth()-35-clipX)),25,juce::Justification::centredLeft,1);
                g.restoreState();
            }
            y+=trackHeight;
            ++renderedTrackIndex;
            if(y>getHeight()-60)break;
        }
        const int head=juce::roundToInt(idw::daw::timelinePixelAtBeat(playheadBeats,left,timelineZoomLevel,timelineScrollBeats));
        if(head>=left && head<getWidth()-30){
            g.setColour(juce::Colour(0xff17d9ff));
            g.drawVerticalLine(head,(float)top,(float)(getHeight()-30));
        }
        g.restoreState();
        // Show which tracks are visible in large sessions without blocking the mixer.
        if(!session.tracks.empty() && arrangementBottom>366){
            const size_t visible=(size_t)juce::jmax(1,(arrangementBottom-366)/70);
            const size_t first=juce::jmin(arrangementScrollTrack,session.tracks.size()-1);
            const size_t last=juce::jmin(session.tracks.size(),first+visible);
            g.setColour(juce::Colour(0xff7baec8));
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.drawText("TRACKS "+juce::String((int)first+1)+"-"+juce::String((int)last)+
                       " / "+juce::String((int)session.tracks.size()),
                       getWidth()-177,top+5,154,19,juce::Justification::centredRight);
        }
        // Peak bars are updated from atomics, never directly from the audio thread.
        const int meterLeft=getWidth()-220,meterTop=215,meterWidth=80;
        for(int ch=0;ch<2;++ch){
            const auto peak=ch==0?peakLeft.load():peakRight.load();
            const auto clipped=ch==0?peakClipLeft.load():peakClipRight.load();
            const int y=meterTop+ch*17;
            g.setColour(juce::Colours::darkgrey);g.fillRect(meterLeft,y,meterWidth,11);
            g.setColour(clipped?juce::Colour(0xfff44368):juce::Colour(0xff10d5f9));
            g.fillRect(meterLeft,y,juce::roundToInt(meterWidth*juce::jlimit(0.0f,1.0f,peak)),11);
            g.setColour(juce::Colours::white);g.drawText(ch==0?"L":"R",meterLeft-20,y,18,11,juce::Justification::centred);
        }
        // Compact bottom mixer overview. Display-only: no audio-thread mutation.
        const int mixHeight=neoMixerExpanded ? 190 : 112;
        const int mixTop=getHeight()-mixHeight-48;
        if(mixTop>420){
            const int count=juce::jmin(10,(int)session.tracks.size());
            const int stripWidth=juce::jmax(65,juce::jmin(120,(getWidth()-180)/juce::jmax(1,count+1)));
            g.setColour(juce::Colour(activeSkin==1?0xff2b2118:activeSkin==2?0xff112d26:activeSkin==3?0xff261d39:0xff101f32));
            g.fillRoundedRectangle(20.0f,(float)mixTop,(float)getWidth()-40.0f,(float)mixHeight,8.0f);
            g.setColour(neoTheme.accentColour());
            g.setFont(juce::Font(juce::FontOptions(11.0f,juce::Font::bold)));
            g.drawText(neoMixerExpanded ? "NEO MIXER  /  EXPANDED  (CLICK TO COLLAPSE)" : "NEO MIXER  /  CLICK TO EXPAND",30,mixTop+4,320,18,juce::Justification::centredLeft);
            for(int i=0;i<count;++i){
                const auto& track=session.tracks[(size_t)i];
                if((size_t)i==selectedTrack){
                    g.setColour(neoTheme.accentColour().withAlpha(0.16f));
                    g.fillRoundedRectangle((float)(30+i*stripWidth), (float)mixTop+23,
                        (float)stripWidth-5, (float)mixHeight-29,5.0f);
                }
                const int x=30+i*stripWidth;
                const int faderX=x+stripWidth-19;
                const float channelPeak=channelPeaks[(size_t)i].load(std::memory_order_relaxed);
                g.setColour(juce::Colour(0xff203647));
                g.fillRect(faderX-13,mixTop+56,3,mixHeight-76);
                g.setColour(neoTheme.accentColour());
                const int peakHeight=juce::roundToInt((float)(mixHeight-76)*juce::jlimit(0.0f,1.0f,channelPeak));
                g.fillRect(faderX-13,mixTop+mixHeight-20-peakHeight,3,peakHeight);
                g.setColour(juce::Colour(0xff38546c));
                g.drawHorizontalLine(mixTop+49,(float)x+8,(float)faderX-13);
                const float panPosition=juce::jlimit(0.0f,1.0f,(track.pan+1.0f)/2.0f);
                const float panX=(float)x+8+panPosition*(float)juce::jmax(1,stripWidth-47);
                g.setColour(neoTheme.accentColour().brighter(0.15f));
                g.fillEllipse(panX-4.0f,(float)mixTop+45,8.0f,8.0f);
                g.setColour(juce::Colour(0xff36576b));
                g.fillRoundedRectangle((float)faderX-2,(float)mixTop+49,4.0f,(float)mixHeight-68,2.0f);
                const float normalized=juce::jlimit(0.0f,1.0f,track.gain/1.5f);
                const float thumbY=(float)mixTop+mixHeight-20.0f-normalized*(float)(mixHeight-70);
                g.setColour(neoTheme.accentColour());
                g.fillRoundedRectangle((float)faderX-9,thumbY-4,18.0f,8.0f,3.0f);
                // Numeric mix readouts are gain/pan settings, not live audio meters.
                g.setColour(juce::Colour(0xffa9cadb));
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                const auto gainDb=track.gain>0.0001f
                    ? juce::String(20.0*std::log10((double)track.gain),1)+" dB"
                    : juce::String("-inf dB");
                g.drawFittedText(gainDb,x+4,mixTop+96,stripWidth-12,12,
                                 juce::Justification::centredLeft,1);
                g.setColour(juce::Colour(0xff29445c));
                g.drawVerticalLine(x+stripWidth-4,(float)mixTop+23,(float)mixTop+mixHeight-8);
                const bool armed=track.type==idw::daw::TrackType::audio && armedTracks[(size_t)i];
                g.setColour(armed ? juce::Colour(0xffef486d) : juce::Colour(0xffd2e8f5));
                g.setFont(juce::Font(juce::FontOptions(10.0f)));
                g.drawFittedText(juce::String(track.name),x+3,mixTop+27,stripWidth-14,17,juce::Justification::centredLeft,1);
                g.setColour(juce::Colour(0xff203447));
                g.fillRoundedRectangle((float)x+8,(float)mixTop+57,(float)stripWidth-26,7.0f,3.0f);
                g.setColour(armed ? juce::Colour(0xffef486d) : neoTheme.accentColour());
                g.fillRoundedRectangle((float)x+8,(float)mixTop+57,
                    (float)(stripWidth-26)*juce::jlimit(0.0f,1.0f,track.gain/2.0f),7.0f,3.0f);
                g.setColour(juce::Colour(0xff8cb6cb));
                g.drawText(track.mute ? "MUTE" : (track.solo ? "SOLO" : (armed ? "ARMED" : "CH "+juce::String(i+1))),
                    x+4,mixTop+75,stripWidth-14,20,juce::Justification::centredLeft);
            }
        }
        if(exportRunning.load()){
            g.setColour(juce::Colours::gold);
            g.drawText("Export: "+juce::String((int)(exportProgress.load()*100.0f))+"%",getWidth()-230,80,205,24,juce::Justification::centredRight);
        }
        g.setColour(juce::Colours::grey);
        g.drawFittedText(!sources.empty() ? "Loaded audio tracks: " + juce::String(loadedCount()) + "  |  Timeline playback prototype; drag clips to move or resize their right edge." : "Add an audio track and open WAV/AIFF files to mix their playback.",25,getHeight()-44,getWidth()-50,35,juce::Justification::centredLeft,1);
    }
private:
    bool neoMixerExpanded=false;
    size_t arrangementScrollTrack=0;
    int timelineZoomLevel=idw::daw::defaultTimelineZoom;
    double timelineScrollBeats=0.0;
    double timelineContentEndBeat() const {
        double end=0.0;
        for(const auto& track:session.tracks)
            for(const auto& clip:track.clips)
                end=std::max(end,clip.startBeat+clip.lengthBeats);
        return end;
    }
    void clampTimelineScroll(){
        timelineScrollBeats=idw::daw::timelineClampScroll(
            timelineScrollBeats,timelineContentEndBeat(),
            (double)juce::jmax(1,getWidth()-230),timelineZoomLevel);
    }
    bool draggingMixerFader=false,draggingMixerPan=false;
    void updateMixerPan(int mouseX,int stripX,int stripWidth){
        if(selectedTrack>=session.tracks.size())return;
        const float range=(float)juce::jmax(1,stripWidth-47);
        const float pan=2.0f*juce::jlimit(0.0f,1.0f,(float)(mouseX-stripX-8)/range)-1.0f;
        session.setTrackMix(selectedTrack,session.tracks[selectedTrack].gain,pan);
        refreshMixer();repaint();
    }
    void updateMixerFader(int mouseY){
        if(selectedTrack>=session.tracks.size())return;
        const int mixerHeight=neoMixerExpanded ? 190 : 112;
        const int mixerTop=getHeight()-mixerHeight-48;
        const float normalized=juce::jlimit(0.0f,1.0f,
            (float)(mixerTop+mixerHeight-20-mouseY)/(float)(mixerHeight-70));
        const float newGain=normalized*1.5f;
        session.setTrackMix(selectedTrack,newGain,session.tracks[selectedTrack].pan);
        refreshMixer();
        repaint();
    }
    std::optional<idw::daw::Session> dragHistorySnapshot;
    bool draggingClip=false,dragResize=false;
    int dragFade=0;
    bool selectedClipValid=false;
    size_t selectedClipTrack=0,selectedClipIndex=0;
    size_t dragTrack=0,dragIndex=0;
    int dragOriginX=0;
    double dragOriginalStart=0.0,dragOriginalLength=0.0;
    void prepareToPlay(int samplesPerBlockExpected,double sampleRate) override {
        playing.store(false,std::memory_order_release);
        deviceRate=sampleRate;
        for(auto& state:eqState)state=0.0f;
        preparedBlockSize=juce::jmax(1,samplesPerBlockExpected);
        for(auto& slot:sources){
            slot->scratch.setSize(2,preparedBlockSize,false,true,false);
            slot->transport.prepareToPlay(samplesPerBlockExpected,sampleRate);
        }
    }
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override {
        info.clearActiveBufferRegion();
        if(!playing.load(std::memory_order_relaxed) || deviceRate<=0)return;
        // Never resize heap buffers in the real-time audio callback.
        if(info.numSamples>preparedBlockSize || info.buffer->getNumChannels()>2)return;
        // Read a single session revision for the entire audio block.
        const auto snapshot=std::atomic_load_explicit(&playbackSession,std::memory_order_acquire);
        if(!snapshot)return;
        const auto& playback=*snapshot;
        for(auto& peak:channelPeaks)peak.store(0.0f,std::memory_order_relaxed);
        const auto blockStart=sessionFrames.load(std::memory_order_relaxed);
        const bool anySolo=std::any_of(playback.tracks.begin(),playback.tracks.end(),[](const auto& t){return t.solo;});
        for(auto& slot:sources) {
            if(!slot->reader || slot->trackIndex>=playback.tracks.size())continue;
            const auto& track=playback.tracks[slot->trackIndex];
            if(track.mute || (anySolo && !track.solo) || track.clips.empty())continue;
            const float gain=juce::jlimit(0.0f,2.0f,track.gain);
            const float pan=juce::jlimit(-1.0f,1.0f,track.pan);
            const float leftGain=gain*(pan>0.0f?1.0f-pan:1.0f);
            const float rightGain=gain*(pan<0.0f?1.0f+pan:1.0f);
            float trackPeak=0.0f;
            // Each clip owns an independent timeline window and source offset.
            // Seek the existing transport before each read, so gaps stay silent
            // and overlapping clips sum rather than replacing one another.
            for(const auto& clip:track.clips){
                if(clip.sourcePath!=slot->sourcePath)continue;
                const auto window=idw::daw::readClipWindow(slot->transport,slot->scratch,
                    slot->nextSourceFrame,clip,playback.bpm,deviceRate,blockStart,info.numSamples);
                if(window.frames<=0)continue;
                for(int ch=0;ch<juce::jmin(2,slot->scratch.getNumChannels());++ch){
                    const float level=slot->scratch.getMagnitude(ch,0,window.frames) *
                        (ch==0?leftGain:rightGain);
                    trackPeak=juce::jmax(trackPeak,level);
                }
                for(int channel=0;channel<info.buffer->getNumChannels();++channel)
                    info.buffer->addFrom(channel,info.startSample+window.offset,
                        slot->scratch,channel,0,window.frames,
                        channel==0?leftGain:rightGain);
            }
            if(slot->trackIndex<channelPeaks.size()){
                // Multiple source slots can belong to one track. Preserve the
                // highest peak across all slots in this callback block.
                auto& meter=channelPeaks[slot->trackIndex];
                float observed=meter.load(std::memory_order_relaxed);
                while(observed<trackPeak &&
                      !meter.compare_exchange_weak(observed,trackPeak,
                          std::memory_order_relaxed,std::memory_order_relaxed)){}
            }
        }
        // Basic audible piano-roll preview. No MIDI device or VST routing yet.
        if(info.buffer->getNumChannels()>0){
            idw::daw::renderMidiSine(playback,
                info.buffer->getWritePointer(0,info.startSample),
                info.buffer->getNumChannels()>1
                    ?info.buffer->getWritePointer(1,info.startSample):nullptr,
                info.numSamples,blockStart,deviceRate);
        }
        // Two-band master tone: one-pole low/high crossover at 500 Hz.
        // The filter's persistent state is owned exclusively by the audio callback.
        const float alpha=(float)std::exp(-2.0*juce::MathConstants<double>::pi*500.0/deviceRate);
        const float lowGain=std::pow(10.0f,lowDb.load(std::memory_order_relaxed)/20.0f);
        const float highGain=std::pow(10.0f,highDb.load(std::memory_order_relaxed)/20.0f);
        for(int channel=0;channel<juce::jmin(2,info.buffer->getNumChannels());++channel){
            auto* samples=info.buffer->getWritePointer(channel,info.startSample);
            float state=eqState[channel];
            for(int i=0;i<info.numSamples;++i){
                const float x=samples[i];
                state=alpha*state+(1.0f-alpha)*x;
                samples[i]=lowGain*state+highGain*(x-state);
            }
            eqState[channel]=state;
        }
        info.buffer->applyGain(info.startSample,info.numSamples,masterLevel.load(std::memory_order_relaxed));
        // Basic peak-ceiling safety clamp; not a look-ahead mastering limiter.
        const float ceiling=std::pow(10.0f,ceilingDb.load(std::memory_order_relaxed)/20.0f);
        for(int ch=0;ch<info.buffer->getNumChannels();++ch){
            auto* samples=info.buffer->getWritePointer(ch,info.startSample);
            for(int i=0;i<info.numSamples;++i)samples[i]=juce::jlimit(-ceiling,ceiling,samples[i]);
        }
        if(info.buffer->getNumChannels()>0){
            const float peak=info.buffer->getMagnitude(0,info.startSample,info.numSamples);
            peakLeft.store(peak,std::memory_order_relaxed);
            if(peak>=1.0f)peakClipLeft.store(true,std::memory_order_relaxed);
        }
        if(info.buffer->getNumChannels()>1){
            const float peak=info.buffer->getMagnitude(1,info.startSample,info.numSamples);
            peakRight.store(peak,std::memory_order_relaxed);
            if(peak>=1.0f)peakClipRight.store(true,std::memory_order_relaxed);
        }
        sessionFrames.fetch_add(info.numSamples,std::memory_order_relaxed);
    }
    void releaseResources() override {
        playing.store(false,std::memory_order_release);
        for(auto& slot:sources)slot->transport.releaseResources();
    }
    void changeListenerCallback(juce::ChangeBroadcaster* source) override {
        for(const auto& entry:waveformThumbnails){
            if(entry.second.get()==source){repaint();return;}
        }
        if(source==&deviceManager){
            // Device selector changes are delivered on the message thread.
            // Do not rewrite routes while audio recording is in progress.
            if(!recorder.isRecording() && !multiRecorder.isRecording()){
                refreshMixer();
                repaint();
            }
        }
    }
    void refreshMixer(){
        if(session.tracks.empty())return;
        if(selectedTrack>=session.tracks.size())selectedTrack=0;
        const auto& track=session.tracks[selectedTrack];
        synthPreset.setEnabled(track.type==idw::daw::TrackType::midi &&
            !playing.load() && !(recorder.isRecording() || multiRecorder.isRecording()) && !exportRunning.load());
        synthPreset.setSelectedId(track.synthPreset+1,juce::dontSendNotification);
        const bool editableSynth=track.type==idw::daw::TrackType::midi &&
            !playing.load() && !(recorder.isRecording() || multiRecorder.isRecording()) && !exportRunning.load();
        attackControl.setEnabled(editableSynth);
        releaseControl.setEnabled(editableSynth);
        attackControl.setValue(track.synthAttackMs,juce::dontSendNotification);
        releaseControl.setValue(track.synthReleaseMs,juce::dontSendNotification);
        gainLabel.setText(juce::String(track.name)+" VOL",juce::dontSendNotification);
        gainControl.setValue(track.gain,juce::dontSendNotification);
        panControl.setValue(track.pan,juce::dontSendNotification);
        muteControl.setToggleState(track.mute,juce::dontSendNotification);
        soloControl.setToggleState(track.solo,juce::dontSendNotification);
        armControl.setEnabled(track.type==idw::daw::TrackType::audio &&
            !playing.load() && !(recorder.isRecording() || multiRecorder.isRecording()) && !exportRunning.load());
        const bool trackArmed=track.type==idw::daw::TrackType::audio && armedTracks[selectedTrack];
        armControl.setToggleState(trackArmed,juce::dontSendNotification);
        armControl.setButtonText(trackArmed ? "ARM INPUT - ON" : "ARM INPUT - OFF");
        armControl.setTooltip("Selected track: "+juce::String(track.name)+
            (trackArmed ? " (armed for recording)" : " (not armed). Click to arm before RECORD VOCAL."));
        inputChannel.setEnabled(track.type==idw::daw::TrackType::audio &&
            !playing.load() && !recorder.isRecording() &&
            !multiRecorder.isRecording() && !exportRunning.load());
        // Show the active device's physical channel names and enabled state.
        // Recording translates physical indices into JUCE's packed callback indices.
        if(auto* device=deviceManager.getCurrentAudioDevice()){
            const auto names=device->getInputChannelNames();
            const auto active=device->getActiveInputChannels();
            inputChannel.clear(juce::dontSendNotification);
            for(int i=0;i<names.size();++i){
                inputChannel.addItem(names[i]+(active[i] ? " (ON)" : " (disabled)"),i+1);
                inputChannel.setItemEnabled(i+1,active[i]);
            }
        }else{
            inputChannel.clear(juce::dontSendNotification);
            inputChannel.addItem("No audio input device",1);
            inputChannel.setItemEnabled(1,false);
        }
        inputChannel.setSelectedId(track.inputChannel+1,juce::dontSendNotification);
        // Keep a route-specific diagnosis accessible before pressing RECORD.
        auto* device=deviceManager.getCurrentAudioDevice();
        const int physical=track.inputChannel;
        const bool audioTrack=track.type==idw::daw::TrackType::audio;
        const auto names=device ? device->getInputChannelNames() : juce::StringArray{};
        const bool validInput=audioTrack && device && physical>=0 &&
            physical<names.size() && device->getActiveInputChannels()[physical];
        const auto route="Device: "+(device ? device->getName() : juce::String("none"))+
            "\nTrack: "+juce::String(track.name)+
            (audioTrack ? (trackArmed ? " (ARMED)" : " (not armed)") : " (MIDI, not recordable)")+
            "\nInput: "+(validInput ? names[physical]+" (enabled)" :
                juce::String("not selected or disabled"))+
            "\n"+(validInput ? juce::String("Ready to capture when RECORD VOCAL is pressed.") :
                juce::String("Open AUDIO DEVICES, enable an input, and select it on an audio track."));
        record.setTooltip(route);
        inputChannel.setTooltip(route);
        const auto ready=audioTrack && trackArmed && validInput;
        const bool recordingNow=recorder.isRecording() || multiRecorder.isRecording();
        recordReadiness.setText(recordingNow ?
            ("RECORDING  |  "+(device ? device->getName() : juce::String("No device"))+
             "  |  "+juce::String(track.name)) :
            ready ?
            ("READY  |  "+device->getName()+"  |  "+juce::String(track.name)+
             "  |  "+names[physical]) :
            ("NOT READY  |  "+(device ? device->getName() : juce::String("No device"))+
             "  |  "+juce::String(track.name)+
             (trackArmed ? "  |  Armed" : "  |  Disarmed")+
             (validInput ? "  |  Input ON" : "  |  Input unavailable")),
            juce::dontSendNotification);
        recordReadiness.setColour(juce::Label::textColourId,
            recordingNow ? juce::Colour(0xfff44368) :
            ready ? juce::Colour(0xff73f9ae) : juce::Colour(0xffffc857));
    }
    void ensureSelectedTrackVisible(){
        const int mixerHeight=neoMixerExpanded ? 190 : 112;
        const int mixerTop=getHeight()-mixerHeight-48;
        const int arrangementBottom=mixerTop>420 ? mixerTop-8 : getHeight()-48;
        const size_t visible=(size_t)juce::jmax(1,(arrangementBottom-366)/70);
        const size_t maxScroll=session.tracks.size()>visible ? session.tracks.size()-visible : 0;
        arrangementScrollTrack=juce::jmin(arrangementScrollTrack,maxScroll);
        if(selectedTrack<arrangementScrollTrack)arrangementScrollTrack=selectedTrack;
        else if(selectedTrack>=arrangementScrollTrack+visible)
            arrangementScrollTrack=juce::jmin(maxScroll,selectedTrack-visible+1);
    }
    void executeStudioCommand(juce::String spoken){
        auto command=spoken.trim().toLowerCase().replaceCharacter('-', ' ');
        command=command.trimCharactersAtEnd(".!?");
        if(command.startsWith("idw "))command=command.fromFirstOccurrenceOf("idw ",false,false);
        if(command.startsWith("studio "))command=command.fromFirstOccurrenceOf("studio ",false,false);
        if(command.startsWith("hey idw "))command=command.substring(8).trim();
        if(command.startsWith("please "))command=command.substring(7).trim();
        if(command.isEmpty())return;
        if(command!="confirm recording" && command!="confirm record" &&
           command!="start recording" && command!="record vocal" &&
           command!="record vocals" && command!="start vocal recording" &&
           command!="cancel recording command" && command!="cancel record")
            pendingVoiceRecord=false;
        const bool busy=recorder.isRecording() || multiRecorder.isRecording() ||
            exportRunning.load() || loopbackCapture.isActive();
        juce::String feedback="Unknown command: "+command;
        if(command=="command history" || command=="voice history" || command=="show history"){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                "IDW Command History",
                commandHistory.isEmpty() ? juce::String("No commands submitted yet.")
                                         : commandHistory.joinIntoString("\n"));
            feedback="Displayed recent commands";
        }else if(command=="clear command history" || command=="clear voice history"){
            commandHistory.clear();
            feedback="Command history cleared";
        }else if(command=="voice help" || command=="help" || command=="list commands"){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                "IDW Voice Commands",
                "Click the command field, press Win+H, dictate a phrase, then press Enter.\n"
                "Transport: play, stop, record vocals, stop recording.\n"
                "Recording: arm track, disarm track, input status, overdub on/off.\n"
                "Tracks: next track, previous track, select track 3, add audio track.\n"
                "Mixing: mute/unmute, solo/unsolo, expand/collapse mixer.\n"
                "Projects: save project.\n"
                "Voice recognition uses Windows dictation; the DAW is not always listening.");
            feedback="Voice command list displayed";
        }else if(command=="record status" || command=="input status" || command=="microphone status"){
            auto* device=deviceManager.getCurrentAudioDevice();
            const bool validTrack=selectedTrack<session.tracks.size();
            const bool audioTrack=validTrack && session.tracks[selectedTrack].type==idw::daw::TrackType::audio;
            const int physical=audioTrack ? session.tracks[selectedTrack].inputChannel : -1;
            const int packed=device && physical>=0
                ? IDWLoopbackCapture::callbackIndexForChannel(device->getActiveInputChannels(),physical)
                : -1;
            const bool armed=audioTrack && selectedTrack<armedTracks.size() && armedTracks[selectedTrack];
            const auto inputNames=device ? device->getInputChannelNames() : juce::StringArray{};
            const auto inputName=physical>=0 && physical<inputNames.size()
                ? inputNames[physical] : juce::String("unavailable");
            const bool inputReady=device && packed>=0 && deviceRate>0.0 && std::isfinite(deviceRate);
            const auto details="Track: "+(validTrack ? juce::String(session.tracks[selectedTrack].name) : juce::String("none"))+
                "\nAudio track: "+(audioTrack ? juce::String("yes") : juce::String("no"))+
                "\nArmed: "+(armed ? juce::String("yes") : juce::String("no"))+
                "\nDevice: "+(device ? device->getName() : juce::String("none"))+
                "\nPhysical input: "+juce::String(physical)+
                "\nEnabled callback input: "+juce::String(packed)+
                "\nInput channel name: "+inputName+
                "\nInput ready: "+(inputReady ? juce::String("yes") : juce::String("NO - check AUDIO DEVICES"))+
                "\nSample rate: "+juce::String(deviceRate,0)+
                "\nRecording active: "+((recorder.isRecording() || multiRecorder.isRecording()) ? juce::String("yes") : juce::String("no"));
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                "IDW Recording Status",details);
            feedback="Displayed current input and recording status";
        }else if(command=="stop" || command=="stop playback" || command=="stop the beat"){
            stop.triggerClick();feedback="STOP requested";
        }else if(command=="play" || command=="start playback" || command=="play the beat"){
            if(!busy){play.triggerClick();feedback="PLAY requested";}
            else feedback="Playback command blocked: recording/export busy";
        }else if(command=="arm track" || command=="arm input" || command=="disarm track" || command=="disarm input"){
            if(!busy && !playing.load() && selectedTrack<session.tracks.size() &&
               selectedTrack<armedTracks.size() &&
               session.tracks[selectedTrack].type==idw::daw::TrackType::audio){
                armedTracks[selectedTrack]=command.startsWith("arm");
                refreshMixer();repaint();
                feedback=armedTracks[selectedTrack] ? "Audio track armed" : "Audio track disarmed";
            }else feedback="Select an audio track and stop playback before changing arm state";
        }else if(command=="start recording" || command=="record vocal" || command=="record vocals" || command=="start vocal recording"){
            if(!busy && !playing.load()){
                pendingVoiceRecord=true;
                voiceRecordRequestedAt=juce::Time::getMillisecondCounter();
                feedback="Confirm recording: say CONFIRM RECORDING or type it and press Enter";
            }else{
                pendingVoiceRecord=false;
                feedback="Recording cannot start while transport or export is busy";
            }
        }else if(command=="confirm recording" || command=="confirm record"){
            if(pendingVoiceRecord && (juce::Time::getMillisecondCounter()-voiceRecordRequestedAt)<=15000u && !busy && !playing.load()){
                pendingVoiceRecord=false;
                toggleRecording();
                feedback=(recorder.isRecording() || multiRecorder.isRecording())
                    ? "Recording started" : "Recording not started - review input and track warnings";
            }else{
                pendingVoiceRecord=false;
                feedback="No recent recording request (15-second limit), or transport is busy";
            }
        }else if(command=="cancel recording command" || command=="cancel record"){
            pendingVoiceRecord=false;
            feedback="Voice recording request cancelled";
        }else if(command=="stop recording" || command=="stop record"){
            if(recorder.isRecording() || multiRecorder.isRecording()){toggleRecording();feedback="Recording stop requested";}
            else feedback="No recording is active";
        }else if(command=="overdub on" || command=="enable overdub" ||
                 command=="overdub off" || command=="disable overdub"){
            if(!busy && !playing.load()){
                const bool enabled=command=="overdub on" || command=="enable overdub";
                overdubControl.setToggleState(enabled,juce::dontSendNotification);
                feedback=enabled ? "Overdub enabled" : "Overdub disabled";
            }else feedback="Stop playback or recording before changing overdub";
        }else if(command=="add audio track" || command=="new audio track"){
            if(!busy && !playing.load()){
                addAudioTrack();refreshMixer();repaint();
                feedback="New audio track added and selected";
            }else feedback="Cannot add a track while playback or recording is active";
        }else if(command=="save project" || command=="save session"){
            if(!busy && !playing.load()){
                saveProjectFile();feedback="Project save dialog requested";
            }else feedback="Stop playback or recording before saving the project";
        }else if(command.startsWith("select track ") || command.startsWith("go to track ")){
            const auto numberText=command.fromLastOccurrenceOf("track ",false,false).trim();
            const int number=numberText.getIntValue();
            if(numberText.isEmpty() || !numberText.containsOnly("0123456789") ||
               number<1 || (size_t)number>session.tracks.size()){
                feedback="Invalid track number. Choose 1 to "+
                    juce::String((int)session.tracks.size());
            }else if(busy || playing.load()){
                feedback="Stop playback or recording before selecting a track";
            }else{
                selectedTrack=(size_t)(number-1);
                ensureSelectedTrackVisible();
                refreshMixer();repaint();
                feedback="Selected track "+juce::String(number)+": "+
                    juce::String(session.tracks[selectedTrack].name);
            }
        }else if(command=="next track" || command=="track next" || command=="go to next track"){
            if(!busy){selectTrack(1);feedback="Selected next track";}
            else feedback="Track selection unavailable while busy";
        }else if(command=="previous track" || command=="last track" || command=="track previous" || command=="go to previous track"){
            if(!busy){selectTrack(-1);feedback="Selected previous track";}
            else feedback="Track selection unavailable while busy";
        }else if(command=="expand mixer" || command=="open mixer"){
            neoMixerExpanded=true;ensureSelectedTrackVisible();repaint();feedback="Mixer expanded";
        }else if(command=="collapse mixer" || command=="close mixer"){
            neoMixerExpanded=false;ensureSelectedTrackVisible();repaint();feedback="Mixer collapsed";
        }else if(command=="mute" || command=="mute track" || command=="unmute" ||
                 command=="unmute track" || command=="solo" || command=="solo track" ||
                 command=="unsolo" || command=="unsolo track"){
            if(!playing.load() && !busy && selectedTrack<session.tracks.size()){
                auto& track=session.tracks[selectedTrack];
                if(command=="mute" || command=="mute track")track.mute=true;
                if(command=="unmute" || command=="unmute track")track.mute=false;
                if(command=="solo" || command=="solo track")track.solo=true;
                if(command=="unsolo" || command=="unsolo track")track.solo=false;
                refreshMixer();repaint();feedback="Track mix command applied";
            }else feedback="Mix command unavailable during playback or recording";
        }
        if(commandHistory.size()>=15)commandHistory.remove(0);
        commandHistory.add(command+" -> "+feedback);
        voiceCommandInput.setTooltip(feedback);
        commandStatus.setText(feedback,juce::dontSendNotification);
        juce::Logger::writeToLog("IDW command: "+command+" -> "+feedback);
        voiceCommandInput.selectAll();
    }
    void selectTrack(int direction){
        if(playing.load() || session.tracks.empty())return;
        const auto current=(int)selectedTrack;
        selectedTrack=(size_t)juce::jlimit(0,(int)session.tracks.size()-1,current+direction);
        ensureSelectedTrackVisible();
        refreshMixer();repaint();
    }
    void updateFirstTrackMix(){
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load() || session.tracks.empty())return;
        session.setTrackMix(selectedTrack,(float)gainControl.getValue(),(float)panControl.getValue());
    }
    void timerCallback() override {
        if(loopbackCapture.consumeFailed()){
            liveCalibration.setEnabled(true);
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Live loopback interrupted",
                "Audio input or output became unavailable. The incomplete calibration was discarded.");
            if(!playing.load() && !recorder.isRecording() && !multiRecorder.isRecording())stopTimer();
            return;
        }
        if(loopbackCapture.consumeComplete()){
            const auto& captured=loopbackCapture.getCapturedBuffer();
            const double rate=loopbackCapture.getSampleRate();
            std::vector<std::size_t> pulses;
            for(const int ms:{100,200,300,400,500})
                pulses.push_back(static_cast<std::size_t>(std::llround(ms*rate/1000.0)));
            const auto alignment=idw::daw::estimateLoopbackLatency(
                captured.getReadPointer(0),static_cast<std::size_t>(loopbackCapture.recordedSamples()),
                pulses,static_cast<std::size_t>(std::llround(rate*0.04)));
            loopbackCapture.cancel();
            liveCalibration.setEnabled(true);
            const auto quality=idw::daw::assessCalibration(alignment,rate);
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                "Live loopback result",quality!=idw::daw::CalibrationQuality::invalid
                    ? "Measured "+juce::String(idw::daw::alignmentMilliseconds(alignment.lagSamples,rate),3)+
                      " ms, matched "+juce::String((int)alignment.accepted)+"/5 pulses. Jitter: "+
                      juce::String(idw::daw::alignmentMilliseconds(alignment.jitterSamples,rate),3)+
                      " ms. Quality: "+
                      juce::String(quality==idw::daw::CalibrationQuality::consistent?"consistent":"repeat test")+
                      ". Use TAKE TRIM ms manually after verifying recording alignment. No automatic adjustment was applied."
                    : "No reliable loopback measurement. Confirm line-level output-to-input routing.");
            if(!playing.load() && !recorder.isRecording() && !multiRecorder.isRecording())stopTimer();
            return;
        }
        if(recorder.isRecording()){
            if(recorder.wasDeviceInterrupted()){
                toggleRecording(); // Flush and preserve any partial take on the message thread.
                return;
            }
            const float peak=recorder.consumeInputPeak();
            const auto frames=recorder.getCapturedFrames();
            const auto now=juce::Time::getMillisecondCounterHiRes();
            if(frames!=lastVocalFrames){lastVocalFrames=frames;lastVocalProgressMs=now;}
            const bool stalled=now-lastVocalProgressMs>=3000.0;
            const bool missing=recorder.isInputMissing();
            const bool sustainedSilence=recorder.hasSustainedSilence();
            const double db=20.0*std::log10(std::max(1.0e-6f,peak));
            inputLevel.setText(missing ? "INPUT UNAVAILABLE: check microphone connection / input routing" :
                stalled ? "NO AUDIO FRAMES: check audio device / permissions" :
                "VOCAL INPUT  "+juce::String(db,0)+" dBFS"+
                (peak>=0.98f ? "  |  CLIPPING" : sustainedSilence ? "  |  SILENT INPUT: check mute / gain" : ""),
                juce::dontSendNotification);
            inputLevel.setColour(juce::Label::textColourId,
                missing || stalled || peak>=0.98f ? juce::Colour(0xfff44368) :
                sustainedSilence ? juce::Colour(0xffffc857) : juce::Colour(0xff73f9ae));
        }
        if(multiRecorder.isRecording()){
            juce::String levels("RECORDING INPUTS  ");
            float maximum=0.0f;
            for(std::size_t i=0;i<multiTakes.size();++i){
                const float peak=multiRecorder.getInputPeak(i);
                maximum=std::max(maximum,peak);
                const double db=20.0*std::log10(std::max(1.0e-6f,peak));
                levels+="T"+juce::String((int)multiTakes[i].first+1)+": "+juce::String(db,0)+" dBFS  ";
            }
            inputLevel.setText(levels,juce::dontSendNotification);
            const double db=20.0*std::log10(std::max(1.0e-6f,maximum));
            record.setButtonText("REC PEAK "+juce::String(db,0)+" dBFS");
        }
        // Device shutdown can suspend playback from the audio thread.
        // Re-enable editing controls on the message thread after that transition.
        if(!playing.load() && !(recorder.isRecording() || multiRecorder.isRecording()) && !exportRunning.load()){
            for(auto& slot:sources)slot->transport.stop();
            refreshMixer();
            stopTimer();
            playheadBeats=0.0;
            repaint();
            return;
        }
        const auto activeSnapshot=std::atomic_load_explicit(&playbackSession,std::memory_order_acquire);
        const double timelineBpm=activeSnapshot?activeSnapshot->bpm:session.bpm;
        playheadBeats=idw::daw::playbackBeat(sessionFrames.load(),deviceRate,timelineBpm);
        if(playing.load() && !recorder.isRecording() && !multiRecorder.isRecording() &&
           activeSnapshot && playheadBeats>=idw::daw::arrangementEndBeat(*activeSnapshot)){
            playing.store(false);
            if(!exportRunning.load())stopTimer();
            for(auto& slot:sources)slot->transport.stop();
            refreshMixer();
        }
        repaint();
    }
    struct AudioSlot {
        juce::AudioTransportSource transport;
        std::unique_ptr<juce::AudioFormatReaderSource> reader;
        juce::File file;
        std::string sourcePath; // Prepared off the audio thread for allocation-free clip matching.
        juce::int64 nextSourceFrame=-1; // Audio-callback-owned cursor, reset before playback.
        size_t trackIndex=0;
        double sourceOffsetSeconds=0.0;
        juce::AudioBuffer<float> scratch;
    };
    void addAudioTrack(){
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return; // Avoid mutating the audio graph during playback.
        if(!session.addTrack(idw::daw::TrackType::audio))return;
        auto slot=std::make_unique<AudioSlot>();
        slot->trackIndex=session.tracks.size()-1;
        if(preparedBlockSize>0){slot->scratch.setSize(2,preparedBlockSize,false,true,false);slot->transport.prepareToPlay(preparedBlockSize,deviceRate);}
        sources.push_back(std::move(slot));
        // Select the newly added track so ARM INPUT targets it immediately.
        // Do not arm it automatically: arming must remain an explicit user action.
        selectedTrack=session.tracks.size()-1;
        refreshMixer();
        repaint();
    }
    int loadedCount() const {
        int count=0;for(auto& slot:sources)if(slot->reader)++count;
        return count;
    }
    void cacheWaveform(const juce::File& file){
        if(!file.existsAsFile())return;
        const auto path=file.getFullPathName().toStdString();
        if(waveformThumbnails.find(path)!=waveformThumbnails.end())return;
        // AudioThumbnail builds a reduced-resolution peak cache in the background.
        // Never decode a full WAV in paint() or in the real-time audio callback.
        auto thumbnail=std::make_unique<juce::AudioThumbnail>(512,formats,waveformCache);
        thumbnail->addChangeListener(this);
        if(!thumbnail->setSource(new juce::FileInputSource(file))){
            thumbnail->removeChangeListener(this);
            return;
        }
        waveformThumbnails.emplace(path,std::move(thumbnail));
    }
    bool restoreProject(const juce::File& project){
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return false;
        idw::daw::Session restored;
        if(!idw::daw::loadSession(project,restored))return false;
        // Build replacement players before replacing the current project.
        std::vector<std::unique_ptr<AudioSlot>> staged;
        int missing=0;
        for(size_t index=0;index<restored.tracks.size();++index){
            if(restored.tracks[index].type!=idw::daw::TrackType::audio)continue;
            auto& clips=restored.tracks[index].clips;
            // Build one reader/transport per distinct audio file on this track.
            // Multiple splits of the same WAV share a source; different WAVs do not.
            std::vector<juce::File> opened;
            for(auto& clip:clips){
                const auto savedPath=juce::String(clip.sourcePath);
                auto file=juce::File::isAbsolutePath(savedPath)
                    ? juce::File(savedPath)
                    : project.getParentDirectory().getChildFile(savedPath);
                if(!file.existsAsFile()){
                    const auto name=file.getFileName();
                    const auto adjacent=project.getParentDirectory().getChildFile(name);
                    const auto packaged=project.getParentDirectory().getChildFile("Audio").getChildFile(name);
                    if(adjacent.existsAsFile())file=adjacent;
                    else if(packaged.existsAsFile())file=packaged;
                }
                if(!file.existsAsFile()){++missing;continue;}
                clip.sourcePath=file.getFullPathName().toStdString();
                if(std::find(opened.begin(),opened.end(),file)!=opened.end())continue;
                std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
                if(!reader || reader->sampleRate<=0){++missing;continue;}
                auto slot=std::make_unique<AudioSlot>();
                slot->trackIndex=index;
                auto source=std::make_unique<juce::AudioFormatReaderSource>(reader.release(),true);
                const double rate=source->getAudioFormatReader()->sampleRate;
                slot->transport.setSource(source.get(),0,nullptr,rate);
                slot->reader=std::move(source);
                slot->file=file;
                slot->sourcePath=file.getFullPathName().toStdString();
                if(preparedBlockSize>0){
                    slot->scratch.setSize(2,preparedBlockSize,false,true,false);
                    slot->transport.prepareToPlay(preparedBlockSize,deviceRate);
                }
                opened.push_back(file);
                staged.push_back(std::move(slot));
            }
            if(opened.empty()){
                // Retain an empty destination slot for record/import workflows.
                auto slot=std::make_unique<AudioSlot>();
                slot->trackIndex=index;
                if(preparedBlockSize>0){
                    slot->scratch.setSize(2,preparedBlockSize,false,true,false);
                    slot->transport.prepareToPlay(preparedBlockSize,deviceRate);
                }
                staged.push_back(std::move(slot));
            }
        }
        for(auto& slot:sources){slot->transport.stop();slot->transport.setSource(nullptr);}
        sources.clear();
        session=std::move(restored);
        // Undo snapshots refer to the prior project; never restore them into a new one.
        clipUndo.clear();clipRedo.clear();dragHistorySnapshot.reset();
        draggingClip=false;selectedClipValid=false;
        sources=std::move(staged);
        waveformThumbnails.clear();
        for(const auto& slot:sources)if(slot->reader)cacheWaveform(slot->file);
        armedTracks.fill(false); // Reopening a project must never arm recording automatically.
        selectedTrack=0;sessionFrames.store(0);playheadBeats=0;
        tempo.setValue(session.bpm,juce::dontSendNotification);
        refreshMixer();repaint();
        if(missing>0)juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Missing audio",juce::String(missing)+" audio file(s) could not be located. Their clip references remain in the project.");
        return true;
    }
    void openProjectFile(){
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
        projectChooser=std::make_unique<juce::FileChooser>("Open IDW DAW Project",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.idwproj");
        projectChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& picker){
                const auto project=picker.getResult();
                if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
                if(project.existsAsFile() && !restoreProject(project))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Open failed",
                        "The project is invalid or cannot be opened during playback.");
            });
    }
    void bounceMix(){
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
        projectChooser=std::make_unique<juce::FileChooser>("Export stereo WAV",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("IDW Mix.wav"),"*.wav");
        projectChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& picker){
                auto output=picker.getResult();
                if(output==juce::File{} || playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
                output=output.withFileExtension(".wav");
                if(exportThread.joinable())exportThread.join();
                const auto snapshot=session;
                const float gain=masterLevel.load(),ceiling=ceilingDb.load(),bass=lowDb.load(),treble=highDb.load();
                bounce.setEnabled(false);
                cancelBounce.setEnabled(true);
                exportCancel.store(false);
                exportProgress.store(0.0f);
                exportRunning.store(true);
                startTimerHz(10);
                const juce::Component::SafePointer<Workstation> safeThis(this);
                exportThread=std::thread([this,safeThis,snapshot,output,gain,ceiling,bass,treble]{
                    juce::String error;
                    const bool ok=idw::daw::renderSessionWav(snapshot,output,error,44100,gain,ceiling,bass,treble,&exportProgress,&exportCancel);
                    juce::MessageManager::callAsync([safeThis,ok,error]{
                        if(!safeThis)return;
                        safeThis->exportRunning.store(false);
                        safeThis->bounce.setEnabled(true);
                        safeThis->cancelBounce.setEnabled(false);
                        if(!safeThis->playing.load())safeThis->stopTimer();
                        safeThis->repaint();
                        juce::AlertWindow::showMessageBoxAsync(ok?juce::MessageBoxIconType::InfoIcon:juce::MessageBoxIconType::WarningIcon,
                            ok?"WAV export complete":"WAV bounce unavailable",ok?"Stereo WAV written successfully.":error);
                    });
                });
            });
    }
    void exportProjectFile(){
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
        projectChooser=std::make_unique<juce::FileChooser>("Export portable IDW Project",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*");
        projectChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& picker){
                const auto folder=picker.getResult();
                if(folder==juce::File{} || playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
                juce::String error;
                if(!idw::daw::exportPortableProject(session,folder,error))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Export failed",error);
            });
    }
    void saveProjectFile(){
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
        projectChooser=std::make_unique<juce::FileChooser>("Save IDW DAW Project",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("IDW Session.idwproj"),"*.idwproj");
        projectChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& picker){
                const auto file=picker.getResult();
                if(file==juce::File{} || playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
                if(!idw::daw::saveSession(session,file.withFileExtension(".idwproj")))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Save failed","Could not write IDW project.");
            });
    }
    bool importTake(const juce::File& take){
        if(!take.existsAsFile() || playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return false;
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(take));
        if(!reader || reader->sampleRate<=0 || reader->lengthInSamples<=0)return false;
        AudioSlot* target=nullptr;
        // Prefer the audio track selected when recording began, provided it
        // remains empty. Never overwrite a previously recorded/imported take.
        for(auto& slot:sources)
            if(slot->trackIndex==recordTargetTrack && !slot->reader){target=slot.get();break;}
        if(!target)
            for(auto& slot:sources)if(!slot->reader){target=slot.get();break;}
        if(!target){
            addAudioTrack();
            if(!sources.empty() && !sources.back()->reader)target=sources.back().get();
        }
        if(!target)return false;
        auto source=std::make_unique<juce::AudioFormatReaderSource>(reader.release(),true);
        const double rate=source->getAudioFormatReader()->sampleRate;
        const double duration=(double)source->getAudioFormatReader()->lengthInSamples/rate;
        // Compensate the input device's reported capture latency when importing
        // a new take. The initial delayed frames remain in the WAV master file.
        // This is driver-reported latency only, not measured round-trip calibration.
        const double adjustedLatency=takeInputLatencySeconds+takeManualTrimSeconds;
        const double trim=std::clamp(adjustedLatency,0.0,std::max(0.0,duration-1.0/rate));
        const double beatLength=(duration-trim)*session.bpm/60.0;
        if(!session.addClip(target->trackIndex,{take.getFullPathName().toStdString(),takeStartBeat,beatLength,trim}))return false;
        target->transport.setSource(source.get(),0,nullptr,rate);
        target->file=take;target->sourcePath=take.getFullPathName().toStdString();target->reader=std::move(source);
        cacheWaveform(take);
        selectedTrack=target->trackIndex;
        ensureSelectedTrackVisible();
        juce::Logger::writeToLog("IDW vocal take imported: "+take.getFullPathName()+
            " | target track "+juce::String((int)selectedTrack+1)+
            " | duration "+juce::String(duration,2)+" s");
        refreshMixer();repaint();return true;
    }
    void startLiveCalibration(){
        if(playing.load() || recorder.isRecording() || multiRecorder.isRecording() ||
           exportRunning.load() || loopbackCapture.isActive())return;
        // Input routing belongs to audio tracks; never index a missing track.
        if(selectedTrack>=session.tracks.size() ||
           session.tracks[selectedTrack].type!=idw::daw::TrackType::audio){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Live loopback unavailable","Select an audio track with an enabled line input.");
            return;
        }
        auto* device=deviceManager.getCurrentAudioDevice();
        const int input=session.tracks[selectedTrack].inputChannel;
        // JUCE callback channel arrays are packed by enabled hardware channels.
        // Translate the selected physical input index to its callback slot.
        const int callbackInput=device
            ? IDWLoopbackCapture::callbackIndexForChannel(device->getActiveInputChannels(),input)
            : -1;
        if(!device || input<0 || input>=device->getInputChannelNames().size() ||
           device->getOutputChannelNames().isEmpty() ||
           !device->getActiveInputChannels()[input] ||
           !device->getActiveOutputChannels()[0] ||
           !loopbackCapture.prepare(device->getCurrentSampleRate(),callbackInput,0) ||
           !loopbackCapture.start()){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Live loopback unavailable","Select a track with an enabled line input and enable output channel 1.");
            return;
        }
        liveCalibration.setEnabled(false);
        startTimerHz(15);
    }
    void createCalibrationReference(){
        if(playing.load() || recorder.isRecording() || multiRecorder.isRecording() || exportRunning.load())return;
        referenceChooser=std::make_unique<juce::FileChooser>(
            "Save loopback test pulse WAV",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                .getChildFile("IDW Loopback Reference.wav"),"*.wav");
        referenceChooser->launchAsync(juce::FileBrowserComponent::saveMode |
            juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& picker){
                const auto file=picker.getResult();
                if(file==juce::File{} || playing.load() || recorder.isRecording() ||
                   multiRecorder.isRecording() || exportRunning.load())return;
                const auto destination=file.withFileExtension(".wav");
                // The generator never overwrites existing files, even if the
                // file chooser has confirmed replacement.
                if(destination.existsAsFile()){
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                        "File exists","Choose a new filename. Existing reference WAVs are preserved.");
                    return;
                }
                const bool ok=idw::daw::writeLoopbackReference(destination,48000.0,
                    {100.0,200.0,300.0,400.0,500.0});
                juce::AlertWindow::showMessageBoxAsync(
                    ok?juce::MessageBoxIconType::InfoIcon:juce::MessageBoxIconType::WarningIcon,
                    ok?"Reference WAV saved":"Reference WAV failed",
                    ok?"Saved 48 kHz pulse WAV. Expected pulse times: 100, 200, 300, 400, 500 ms.":
                       "Unable to create the reference WAV.");
            });
    }
    void analyzeCalibrationWav(){
        if(playing.load() || recorder.isRecording() || multiRecorder.isRecording() || exportRunning.load())return;
        auto* prompt=new juce::AlertWindow("Offline loopback WAV",
            "Enter known pulse times in milliseconds, separated by commas (minimum three). Analysis will not modify take trim.",
            juce::MessageBoxIconType::InfoIcon);
        prompt->addTextEditor("pulses","100,200,300,400,500","Expected pulse times (ms)");
        prompt->addTextEditor("channel","1","WAV channel (1-based)");
        prompt->addButton("Continue",1,juce::KeyPress(juce::KeyPress::returnKey));
        prompt->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
        prompt->enterModalState(true,juce::ModalCallbackFunction::create(
            [this,prompt](int result){
                if(result!=1){delete prompt;return;}
                const auto times=juce::StringArray::fromTokens(prompt->getTextEditorContents("pulses"),",","");
                const int channel=prompt->getTextEditorContents("channel").getIntValue()-1;
                delete prompt;
                continueCalibrationAnalysis(times,channel);
            }),false);
    }
    void continueCalibrationAnalysis(const juce::StringArray& times,int channel){
        if(times.size()<3 || times.size()>64 || channel<0 || channel>7){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Invalid calibration","Specify 3–64 pulse times and a valid WAV channel (1–8).");
            return;
        }
        std::vector<double> milliseconds;
        double previous=-1.0;
        for(const auto& token:times){
            const auto entry=token.trim();
            // getDoubleValue() alone accepts malformed entries such as "abc" as 0.
            bool numeric=!entry.isEmpty();
            int decimalPoints=0;
            for(const auto character:entry){
                if(character=='.')++decimalPoints;
                else if(character<'0' || character>'9')numeric=false;
            }
            if(decimalPoints>1 || entry==".")numeric=false;
            const double ms=numeric ? entry.getDoubleValue() : -1.0;
            if(!numeric || !std::isfinite(ms) || ms<=previous || ms<0 || ms>60000){
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Invalid pulse schedule","Enter strictly increasing pulse times between 0 and 60000 ms.");
                return;
            }
            milliseconds.push_back(ms);previous=ms;
        }
        calibrationChooser=std::make_unique<juce::FileChooser>("Choose recorded loopback WAV",juce::File{},"*.wav");
        calibrationChooser->launchAsync(juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
            [this,milliseconds,channel](const juce::FileChooser& picker){
                const auto file=picker.getResult();
                if(!file.existsAsFile() || playing.load() || recorder.isRecording() ||
                   multiRecorder.isRecording() || exportRunning.load())return;
                juce::WavAudioFormat wav;
                auto reader=std::unique_ptr<juce::AudioFormatReader>(
                    wav.createReaderFor(file.createInputStream().release(),true));
                if(!reader || reader->sampleRate<=0 || reader->lengthInSamples>reader->sampleRate*60.0 ||
                   reader->lengthInSamples<=0){
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                        "Invalid WAV","Select a valid recorded WAV no longer than 60 seconds.");
                    return;
                }
                std::vector<std::size_t> positions;
                for(const auto ms:milliseconds)
                    positions.push_back(static_cast<std::size_t>(std::llround(ms*reader->sampleRate/1000.0)));
                const auto radius=static_cast<std::size_t>(std::llround(reader->sampleRate*0.04));
                const auto measured=idw::daw::analyzeLoopbackWav(file,channel,positions,radius);
                const auto message=measured.alignment.valid
                    ? "Measured pulse lag: "+juce::String(measured.alignment.lagSamples)+
                      " samples ("+juce::String(measured.lagMilliseconds,3)+" ms). Matched "+
                      juce::String((int)measured.alignment.accepted)+"/"+
                      juce::String((int)measured.alignment.total)+
                      " pulses. Maximum accepted timing deviation: "+
                      juce::String(measured.alignment.jitterSamples)+" samples ("+
                      juce::String(idw::daw::alignmentMilliseconds(measured.alignment.jitterSamples,measured.sampleRate),3)+
                      " ms). Quality: "+
                      juce::String(idw::daw::assessCalibration(measured.alignment,measured.sampleRate)==
                          idw::daw::CalibrationQuality::consistent ? "consistent" : "repeat measurement")+
                      ". No trim setting was changed."
                    : "Could not establish a consistent pulse offset. Confirm the channel, sample rate, pulse schedule and recording.";
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                    "Loopback analysis",message);
            });
    }
    void startOverdubPlayback(){
        const bool hasMaterial=loadedCount()>0 ||
            std::any_of(session.tracks.begin(),session.tracks.end(),
                [](const auto& t){return !t.notes.empty();});
        if(!hasMaterial)return;
        std::atomic_store_explicit(&playbackSession,
            std::make_shared<const idw::daw::Session>(session),std::memory_order_release);
        sessionFrames.store(0);
        for(auto& slot:sources){
            slot->transport.stop();
            slot->transport.setPosition(slot->sourceOffsetSeconds);
            slot->transport.start();
        }
        playing.store(true);
        startTimerHz(30);
    }
    void showRecordingPreflight(){
        if(recorder.isRecording() || multiRecorder.isRecording() || playing.load() ||
           exportRunning.load() || loopbackCapture.isActive()){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Recording preflight unavailable",
                "Stop recording, playback, exporting, or calibration before running preflight.");
            return;
        }
        auto* device=deviceManager.getCurrentAudioDevice();
        const bool validTrack=selectedTrack<session.tracks.size();
        const bool audioTrack=validTrack &&
            session.tracks[selectedTrack].type==idw::daw::TrackType::audio;
        const bool armed=audioTrack && selectedTrack<armedTracks.size() &&
            armedTracks[selectedTrack];
        const int physical=audioTrack ? session.tracks[selectedTrack].inputChannel : -1;
        const auto inputNames=device ? device->getInputChannelNames() : juce::StringArray{};
        const int callbackIndex=device && physical>=0
            ? IDWLoopbackCapture::callbackIndexForChannel(device->getActiveInputChannels(),physical)
            : -1;
        bool occupied=false;
        if(audioTrack)
            for(const auto& slot:sources)
                if(slot->trackIndex==selectedTrack && slot->reader)occupied=true;
        const bool rateOk=std::isfinite(deviceRate) && deviceRate>0.0;
        const bool inputOk=device && callbackIndex>=0 && physical<inputNames.size() && rateOk;
        const bool outputOk=device && device->getActiveOutputChannels().countNumberOfSetBits()>0;
        const auto folder=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("IDW DAW").getChildFile("Recordings");
        const auto folderResult=folder.createDirectory();
        const bool folderOk=folderResult.wasOk() && folder.isDirectory();
        const auto freeBytes=folderOk ? folder.getBytesFreeOnVolume() : juce::int64{-1};
        // 100 MiB is a conservative warning threshold, not a recording guarantee.
        const bool spaceOk=folderOk && freeBytes>=100LL*1024LL*1024LL;
        const bool ok=audioTrack && armed && !occupied && inputOk && outputOk && spaceOk;
        const auto result=ok ? juce::String("PASS - ready to record") :
                               juce::String("ACTION NEEDED before recording");
        const auto details=
            "Overall: "+result+
            "\\nTrack: "+(validTrack ? juce::String(session.tracks[selectedTrack].name) : juce::String("none"))+
            "\\nAudio track: "+(audioTrack ? juce::String("PASS") : juce::String("FAIL - select audio track"))+
            "\\nArmed: "+(armed ? juce::String("PASS") : juce::String("FAIL - ARM INPUT"))+
            "\\nEmpty destination: "+(!occupied ? juce::String("PASS") : juce::String("FAIL - select empty track"))+
            "\\nAudio device: "+(device ? device->getName() : juce::String("none"))+
            "\\nInput: "+(inputOk ? inputNames[physical]+juce::String(" (PASS)") : juce::String("FAIL - enable/select mic input"))+
            "\\nOutput: "+(outputOk ? juce::String("PASS") : juce::String("FAIL - enable audio output"))+
            "\\nSample rate: "+(rateOk ? juce::String(deviceRate,0)+juce::String(" Hz") : juce::String("invalid"))+
            "\\nRecording folder: "+folder.getFullPathName()+
            "\\nFolder writable location: "+(folderOk ? juce::String("exists (write not tested)") : juce::String("FAIL - cannot create folder"))+
            "\\nFree disk space: "+(freeBytes>=0 ? juce::String((double)freeBytes/(1024.0*1024.0),0)+juce::String(" MiB") : juce::String("unknown"))+
            "\\nDisk capacity: "+(spaceOk ? juce::String("PASS") : juce::String("WARNING - 100 MiB minimum recommended"))+
            "\\n\\nThis checks configuration, not live mic audio or audible playback.";
        juce::AlertWindow::showMessageBoxAsync(
            ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
            "IDW Recording Preflight",details);
    }
    void toggleRecording(){
        // Calibration owns a physical input/output while active. Do not let
        // recording start and contaminate either the pulse capture or a take.
        if(loopbackCapture.isActive()){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Calibration running","Finish the live loopback test before starting a recording.");
            return;
        }
        if(exportRunning.load()){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Recording unavailable","Finish exporting before starting a recording.");
            return;
        }
        // Recording may stop while the transport runs, but must not start
        // while ordinary playback is active. Explain this instead of silently ignoring RECORD.
        if(!recorder.isRecording() && !multiRecorder.isRecording() && playing.load()){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Stop playback first",
                "Press STOP, arm an empty audio track, enable PLAY BEAT WHILE RECORDING, then press RECORD VOCAL. Do not press PLAY first.");
            return;
        }
        if((recorder.isRecording() || multiRecorder.isRecording())){
            const bool wasMulti=multiRecorder.isRecording();
            multiRecorder.setHeadphoneMonitor(false);
            monitorControl.setToggleState(false,juce::dontSendNotification);
            recorder.stop();multiRecorder.stop();record.setButtonText("RECORD VOCAL");
            refreshMixer(); // Restore READY / NOT READY immediately after capture stops.
            monitorControl.setEnabled(false);
            inputLevel.setText("INPUT LEVEL: idle",juce::dontSendNotification);
            const auto dropped=wasMulti ? multiRecorder.getDroppedBlocks() : recorder.getDroppedBlocks();
            const auto rejected=wasMulti ? multiRecorder.getRejectedWrites() : recorder.getRejectedWrites();
            const auto receivedFrames=wasMulti ? std::uint64_t{0} : recorder.getCapturedFrames();
            const auto queuedFrames=wasMulti ? std::uint64_t{0} : recorder.getQueuedFrames();
            // Playback and capture counters currently run in separate callbacks;
            // this discrepancy is diagnostic, not an automatic alignment offset.
            const bool wasOverdubbing=playing.load() && takeWasOverdub;
            const auto playbackFrames=static_cast<std::uint64_t>(
                std::max<std::int64_t>(0,sessionFrames.load(std::memory_order_relaxed)));
            // Stop the transport before changing the audio graph.
            for(auto& slot:sources){slot->transport.stop();slot->transport.setPosition(0);}
            playing.store(false);stopTimer();sessionFrames.store(0);playheadBeats=0;
            if(wasMulti){
                juce::String failures;
                for(const auto& item:multiTakes){
                    recordTargetTrack=item.first;
                    const auto bytes=item.second.existsAsFile() ? item.second.getSize() : juce::int64{0};
                    if(bytes<=44){
                        failures+="Track "+juce::String((int)item.first+1)+
                            ": empty or missing WAV ("+item.second.getFileName()+")\n";
                        continue;
                    }
                    if(!importTake(item.second))
                        failures+="Track "+juce::String((int)item.first+1)+
                            ": WAV could not be imported ("+item.second.getFileName()+")\n";
                }
                multiTakes.clear();
                if(rejected>0)juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,"Audio disk buffer overflow",
                    juce::String(rejected)+" WAV write operations were rejected. Check recorded takes for missing audio.");
                if(dropped>0)juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,"Recording may be incomplete",
                    juce::String(dropped)+" audio input blocks were skipped. Check recording synchronization.");
                if(failures.isNotEmpty())juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,"Multitrack import incomplete",failures);
            }else{
                if(wasOverdubbing && playbackFrames>0 && receivedFrames>0 && dropped==0 && rejected==0){
                    const auto difference=static_cast<juce::int64>(playbackFrames)-
                        static_cast<juce::int64>(receivedFrames);
                    juce::Logger::writeToLog("IDW overdub timing diagnostic: playback frames="+
                        juce::String(static_cast<juce::int64>(playbackFrames))+
                        ", captured frames="+juce::String(static_cast<juce::int64>(receivedFrames))+
                        ", frame difference="+juce::String(difference)+
                        " (callback counters, not a latency correction)");
                }
                if(queuedFrames<receivedFrames)juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,"Vocal recording frame mismatch",
                    juce::String(static_cast<juce::int64>(receivedFrames-queuedFrames))+
                    " captured audio frames were not accepted by the WAV queue. Inspect take timing.");
                if(rejected>0)juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,"Vocal WAV buffer overflow",
                    juce::String(rejected)+" single-take WAV writes were rejected. Inspect the saved recording.");
                if(dropped>0)juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,"Vocal capture interrupted",
                    juce::String(dropped)+" audio input blocks were skipped. Check vocal timing.");
                const auto take=recorder.file();
                const auto savedBytes=take.existsAsFile() ? take.getSize() : juce::int64{0};
                const bool interrupted=recorder.wasDeviceInterrupted();
                const bool missing=recorder.getMissingInputBlocks()>0;
                const bool silent=receivedFrames>0 && recorder.getTakePeak()<0.001f;
                juce::WavAudioFormat wav;
                auto savedStream=take.createInputStream();
                auto savedReader=std::unique_ptr<juce::AudioFormatReader>(savedStream
                    ? wav.createReaderFor(savedStream.release(),true) : nullptr);
                const auto savedFrames=savedReader ? savedReader->lengthInSamples : juce::int64{0};
                const double savedRate=savedReader ? savedReader->sampleRate : 0.0;
                savedReader.reset();
                // Flag substantial loss between frames accepted by the recorder
                // and frames actually readable from the resulting WAV.
                const auto missingSavedFrames=queuedFrames>(std::uint64_t)juce::jmax(juce::int64{0},savedFrames)
                    ? queuedFrames-(std::uint64_t)juce::jmax(juce::int64{0},savedFrames) : std::uint64_t{0};
                const bool truncatedWav=savedRate>0.0 &&
                    missingSavedFrames>(std::uint64_t)(savedRate*0.1);
                const juce::String inputAdvice=interrupted
                    ? "Microphone/audio device disconnected, stopped, or changed. Reconnect it and reselect the input in AUDIO DEVICES."
                    : missing ? "Selected microphone input was unavailable. Check its connection and input routing."
                    : silent ? "Audio frames were received, but the entire take stayed below -60 dBFS. Check microphone mute and input gain."
                    : truncatedWav ? "WAV contains fewer frames than were queued. Check disk performance and available space."
                    : "";
                juce::Logger::writeToLog(
                    "IDW vocal capture summary: received="+
                    juce::String(static_cast<juce::int64>(receivedFrames))+
                    " frames, queued="+juce::String(static_cast<juce::int64>(queuedFrames))+
                    " frames, dropped="+juce::String(static_cast<juce::int64>(dropped))+
                    " blocks, rejected="+juce::String(static_cast<juce::int64>(rejected))+
                    " writes, WAV bytes="+juce::String(savedBytes)+
                    ", saved frames="+juce::String(savedFrames)+
                    ", missing input blocks="+juce::String(static_cast<juce::int64>(recorder.getMissingInputBlocks()))+
                    ", device interrupted="+juce::String(interrupted ? "yes" : "no")+
                    ", take peak="+juce::String(recorder.getTakePeak(),6)+
                    ", path="+take.getFullPathName());
                if(receivedFrames==0 || queuedFrames==0 || savedFrames<=0){
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,"No vocal audio captured",
                        (inputAdvice.isNotEmpty() ? inputAdvice+"\n" : juce::String())+
                        (receivedFrames==0 ? "No audio frames were received. Check the audio device and microphone permissions.\n" :
                         "Audio reached the recorder, but no readable audio was saved. Check the recording folder and disk space.\n")+
                        "Destination: "+take.getFullPathName());
                }else if(!importTake(take)){
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,"Vocal take import failed",
                        "Captured "+juce::String(static_cast<juce::int64>(receivedFrames))+
                        " audio frames, but the saved take could not be imported.\n"+
                        take.getFullPathName());
                }else{
                    // Confirm the full record -> save -> import path during beta QA.
                    // A successful import is not proof that the microphone was audible.
                    juce::AlertWindow::showMessageBoxAsync(
                        inputAdvice.isNotEmpty() ? juce::MessageBoxIconType::WarningIcon : juce::MessageBoxIconType::InfoIcon,
                        interrupted || missing ? "Interrupted vocal take preserved" :
                        silent ? "Silent vocal take imported" :
                        truncatedWav ? "Incomplete vocal WAV imported" : "Vocal take imported",
                        (inputAdvice.isNotEmpty() ? inputAdvice+"\n\n" : juce::String())+
                        "WAV saved and added to track "+juce::String((int)recordTargetTrack+1)+
                        ".\nFrames received: "+juce::String(static_cast<juce::int64>(receivedFrames))+
                        "\nFile: "+take.getFullPathName()+
                        "\nApprox. captured duration: "+
                        juce::String(savedRate>0.0 ? (double)savedFrames/savedRate : 0.0,2)+" seconds"+
                        "\nDropped input blocks: "+juce::String(static_cast<juce::int64>(dropped))+
                        "\nRejected writes: "+juce::String(static_cast<juce::int64>(rejected))+
                        "\nMissing WAV frames: "+juce::String(static_cast<juce::int64>(missingSavedFrames))+
                        "\nPlay the take to confirm that microphone audio is audible.");
                }
            }
            return;
        }
        std::vector<std::size_t> selectedInputs;
        for(std::size_t i=0;i<session.tracks.size() && i<armedTracks.size();++i)
            if(session.tracks[i].type==idw::daw::TrackType::audio && armedTracks[i])
                selectedInputs.push_back(i);
        // RECORD VOCAL is itself an explicit recording action. If the user has
        // selected an audio track but no arm toggle survived the UI transition,
        // use that selected track instead of rejecting a valid recording target.
        if(selectedInputs.empty() && selectedTrack<session.tracks.size() &&
           selectedTrack<armedTracks.size() &&
           session.tracks[selectedTrack].type==idw::daw::TrackType::audio){
            armedTracks[selectedTrack]=true;
            selectedInputs.push_back(selectedTrack);
            refreshMixer();
        }
        // When there is exactly one audio track, RECORD VOCAL has an
        // unambiguous destination even if MIDI or no track is selected.
        // Never guess among multiple unarmed audio tracks.
        if(selectedInputs.empty()){
            size_t onlyAudio=session.tracks.size();
            int audioCount=0;
            for(size_t i=0;i<session.tracks.size() && i<armedTracks.size();++i){
                if(session.tracks[i].type==idw::daw::TrackType::audio){
                    onlyAudio=i;
                    ++audioCount;
                }
            }
            if(audioCount==1){
                selectedTrack=onlyAudio;
                armedTracks[onlyAudio]=true;
                selectedInputs.push_back(onlyAudio);
                refreshMixer();
                juce::Logger::writeToLog("IDW RECORD VOCAL: auto-selected sole audio track "+
                    juce::String((int)onlyAudio+1));
            }
        }
        recordTargetTrack=selectedInputs.empty()?session.tracks.size():selectedInputs.front();
        if(recordTargetTrack>=session.tracks.size()){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "No armed audio track",
                "No audio track is armed. Currently selected: "+
                (selectedTrack<session.tracks.size()
                    ? juce::String(session.tracks[selectedTrack].name)
                    : juce::String("none"))+
                ". Press STOP, select an empty AUDIO track, and click ARM INPUT until "
                "it reads ARM INPUT - ON. Then press RECORD VOCAL without pressing PLAY.");
            return;
        }
        const auto folder=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("IDW DAW").getChildFile("Recordings");
        const auto folderResult=folder.createDirectory();
        if(folderResult.failed() || !folder.isDirectory()){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Recording folder unavailable",
                "Unable to create or access the recording folder:\n"+
                folder.getFullPathName()+"\n"+folderResult.getErrorMessage());
            return;
        }
        auto destination=folder.getNonexistentChildFile("Vocal Take",".wav",false);
        // Use driver-reported input latency to trim delayed capture frames
        // at take import. Real loopback calibration remains future work.
        takeInputLatencySeconds=0.0;
        takeManualTrimSeconds=latencyAdjust.getValue()/1000.0;
        if(auto* device=deviceManager.getCurrentAudioDevice())
            if(deviceRate>0 && std::isfinite(deviceRate))
                takeInputLatencySeconds=(double)std::max(0,device->getInputLatencyInSamples())/deviceRate;
        takeWasOverdub=overdubControl.getToggleState();
        takeStartBeat=deviceRate>0 ? (double)sessionFrames.load()*session.bpm/(deviceRate*60.0) : 0.0;
        monitorControl.setToggleState(false,juce::dontSendNotification);
        multiRecorder.setHeadphoneMonitor(false);
        if(overdubControl.getToggleState()){
            const bool hasPlaybackMaterial=loadedCount()>0 ||
                std::any_of(session.tracks.begin(),session.tracks.end(),
                    [](const auto& track){return !track.notes.empty();});
            if(!hasPlaybackMaterial){
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Overdub unavailable","Load an audio clip or add MIDI notes before recording over playback.");
                return;
            }
            // The recording writer starts before transport playback. Reset the
            // reference beat to the beginning of the playback arrangement.
            // Physical capture/playback alignment still needs loopback QA.
            takeStartBeat=0.0;
            // Overdubs must use empty destination tracks. Existing material
            // is preserved and never overwritten by the recording operation.
            for(const auto index:selectedInputs)
                for(const auto& slot:sources)
                    if(slot->trackIndex==index && slot->reader){
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,"Overdub needs empty tracks",
                            "Add and arm an empty audio track to record alongside playback.");
                        return;
                    }
        }
        if(selectedInputs.size()>1){
            // A loaded track cannot accept a second AudioSlot reader yet.
            for(const auto index:selectedInputs)
                for(const auto& slot:sources)
                    if(slot->trackIndex==index && slot->reader){
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,"Track already has audio",
                            "Arm empty audio tracks for simultaneous recording.");
                        return;
                    }
            auto* device=deviceManager.getCurrentAudioDevice();
            const auto active=device ? device->getActiveInputChannels() : juce::BigInteger();
            bool validInputs=device!=nullptr;
            for(std::size_t i=0;i<selectedInputs.size();++i){
                const int channel=session.tracks[selectedInputs[i]].inputChannel;
                if(channel<0 || (device && channel>=device->getInputChannelNames().size()) ||
                   IDWLoopbackCapture::callbackIndexForChannel(active,channel)<0)validInputs=false;
                for(std::size_t j=0;j<i;++j)
                    if(session.tracks[selectedInputs[j]].inputChannel==channel)validInputs=false;
            }
            if(selectedInputs.size()>IDWMultiTakeRecorder::maxTracks ||
               !validInputs || !std::isfinite(deviceRate) || deviceRate<=0.0){
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "More inputs needed","Enable one physical input per armed audio track in audio device settings.");
                return;
            }
            std::vector<IDWMultiTakeRecorder::Input> inputs;
            multiTakes.clear();
            for(std::size_t i=0;i<selectedInputs.size();++i){
                const auto path=folder.getNonexistentChildFile(
                    "Take "+juce::String(selectedInputs[i]+1)+" "+juce::String(juce::Time::getCurrentTime().toMilliseconds()),".wav",false);
                inputs.push_back({path,IDWLoopbackCapture::callbackIndexForChannel(active,
                    session.tracks[selectedInputs[i]].inputChannel)});
                multiTakes.emplace_back(selectedInputs[i],path);
            }
            if(multiRecorder.start(inputs,deviceRate)){
                record.setButtonText("STOP RECORD");
                refreshMixer();
                monitorControl.setEnabled(true);
                if(overdubControl.getToggleState())startOverdubPlayback();
                startTimerHz(15); // UI peak updates only; never poll on the audio callback.
            }
            else{
                multiTakes.clear();
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Multitrack unavailable","Could not start all armed inputs.");
            }
        }else{
            // Single-take imports use one audio reader per track. Reject an
            // occupied destination before opening the WAV writer; otherwise
            // a successful recording can end in an avoidable import failure.
            for(const auto& slot:sources){
                if(slot->trackIndex==recordTargetTrack && slot->reader){
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Recording track already has audio",
                        "Select and arm an EMPTY audio track before pressing RECORD VOCAL. "
                        "Your existing clip will not be overwritten.");
                    return;
                }
            }
            monitorControl.setEnabled(false);
            auto* device=deviceManager.getCurrentAudioDevice();
            int physicalInput=session.tracks[recordTargetTrack].inputChannel;
            int callbackInput=device
                ? IDWLoopbackCapture::callbackIndexForChannel(device->getActiveInputChannels(),physicalInput)
                : -1;
            // If the previously configured input is no longer enabled (USB
            // interface unplugged/repaired), use the only enabled input when
            // unambiguous. Never silently redirect between multiple inputs.
            if(device && callbackInput<0){
                const auto active=device->getActiveInputChannels();
                const int first=active.findNextSetBit(0);
                const int second=first>=0 ? active.findNextSetBit(first+1) : -1;
                if(first>=0 && second<0 && first<device->getInputChannelNames().size()){
                    physicalInput=first;
                    callbackInput=0;
                    session.tracks[recordTargetTrack].inputChannel=first;
                    inputChannel.setSelectedId(first+1,juce::dontSendNotification);
                    juce::Logger::writeToLog("IDW RECORD VOCAL: recovered sole active input "+
                        juce::String(first+1)+" on "+device->getName());
                }
            }
            if(!device || physicalInput<0 ||
               physicalInput>=device->getInputChannelNames().size() || callbackInput<0 ||
               !std::isfinite(deviceRate) || deviceRate<=0.0){
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Input unavailable",
                    "Selected recording track: "+juce::String(session.tracks[recordTargetTrack].name)+
                    "\nPhysical input index: "+juce::String(physicalInput)+
                    "\nActive audio device: "+(device ? device->getName() : juce::String("none"))+
                    "\nOpen AUDIO DEVICES and enable a microphone input, then select that input on the track.");
                return;
            }
            if(recorder.start(destination,deviceRate,1,callbackInput)){
                lastVocalFrames=0;
                lastVocalProgressMs=juce::Time::getMillisecondCounterHiRes();
                // Preserve the precise routing used by the take for Windows QA.
                juce::Logger::writeToLog(
                    "IDW vocal capture started | track="+
                    juce::String((int)recordTargetTrack+1)+
                    " | device="+device->getName()+
                    " | physical input="+juce::String(physicalInput)+
                    " | callback input="+juce::String(callbackInput)+
                    " | sample rate="+juce::String(deviceRate,0)+
                    " | WAV="+destination.getFullPathName());
                record.setButtonText("STOP RECORD");
                if(overdubControl.getToggleState())startOverdubPlayback();
                startTimerHz(30); // Also meter vocal-only recordings and finalize device interruptions.
            }
            else juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Recording unavailable","Check microphone permission, device input and destination directory.");
        }
    }
    void loadAudio(){
        chooser=std::make_unique<juce::FileChooser>("Select WAV or AIFF audio file",juce::File{},"*.wav;*.aiff;*.aif");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,[this](const juce::FileChooser& picker){
            auto file=picker.getResult();if(!file.existsAsFile() || playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
            std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
            if(!reader)return;
            AudioSlot* target=nullptr;
            for(auto& slot:sources)if(!slot->reader){target=slot.get();break;}
            if(!target)return; // Add another audio track before importing another file.
            auto source=std::make_unique<juce::AudioFormatReaderSource>(reader.release(),true);
            const auto sourceRate=source->getAudioFormatReader()->sampleRate;
            target->transport.setSource(source.get(),0,nullptr,sourceRate);
            target->file=file;target->sourcePath=file.getFullPathName().toStdString();target->reader=std::move(source);
            session.addClip(target->trackIndex,{file.getFullPathName().toStdString(),0.0,
                sourceRate>0 ? (double)target->reader->getAudioFormatReader()->lengthInSamples/sourceRate*session.bpm/60.0 : 0.0,0.0});
            cacheWaveform(file);
            sessionFrames.store(0);playing=false;playheadBeats=0;repaint();
        });
    }
    ~Workstation() override {voiceListening.store(false);if(voiceListenThread.joinable())voiceListenThread.join();setLookAndFeel(nullptr);if(exportThread.joinable())exportThread.join();recorder.stop();multiRecorder.stop();loopbackCapture.cancel();deviceManager.removeAudioCallback(&loopbackCapture);deviceManager.removeAudioCallback(&multiRecorder);deviceManager.removeAudioCallback(&recorder);deviceManager.removeChangeListener(this);shutdownAudio();for(auto& slot:sources)slot->transport.setSource(nullptr);sources.clear();}
    IDWNeoLookAndFeel neoTheme;
    juce::ComboBox skinSelector;
    juce::TextButton customAccentButton,customBackgroundButton,customPanelButton,saveSkinButton,loadSkinButton,deleteSkinButton,exportSkinButton,importSkinButton,skinToolsButton;
    juce::Colour customPanel{0xff101f32};
    bool useCustomPanel=false;
    juce::Colour customBackground{0xff080f1c};
    bool useCustomBackground=false;
    juce::Colour customAccent{0xff15d4ff};
    std::uint64_t lastVocalFrames=0;
    double lastVocalProgressMs=0.0;
    int activeSkin=0;
    std::thread exportThread;
    std::atomic<bool> exportRunning{false};
    std::atomic<bool> exportCancel{false};
    std::atomic<float> exportProgress{0.0f};
    idw::daw::Session session;
    std::atomic<bool> playing{false};
    double playheadBeats=0;
    std::vector<idw::daw::Session> clipUndo,clipRedo;
    double takeStartBeat=0.0;
    bool takeWasOverdub=false;
    size_t recordTargetTrack=0;
    double takeInputLatencySeconds=0.0;
    double takeManualTrimSeconds=0.0;
    juce::Label title,tempoLabel,gainLabel,panLabel,inputLevel,latencyLabel,recordReadiness;
    juce::Slider gainControl,panControl,masterGain,latencyAdjust;
    juce::Label masterLabel,eqLabel,limitLabel;
    juce::Slider ceilingControl;
    std::atomic<float> ceilingDb{-1.0f};
    juce::Slider lowEq,highEq;
    std::atomic<float> lowDb{0.0f},highDb{0.0f};
    float eqState[2]{0.0f,0.0f};
    juce::TextButton clearClips,audioDevices;
    std::atomic<float> peakLeft{0.0f},peakRight{0.0f};
    std::array<std::atomic<float>,128> channelPeaks{}; // Audio playback peaks, not input monitoring.
    std::atomic<bool> peakClipLeft{false},peakClipRight{false};
    std::atomic<float> masterLevel{1.0f};
    juce::ToggleButton muteControl,soloControl,armControl,monitorControl,overdubControl;
    juce::ComboBox inputChannel;
    std::array<bool,128> armedTracks{}; // Temporary UI state, not persisted in projects.
    juce::TextEditor voiceCommandInput;
    juce::TextButton runVoiceCommand,voiceHistoryButton,pushToTalkButton;
    juce::Label commandStatus;
    juce::StringArray commandHistory; // Session-only; not written into project files.
    std::atomic<bool> voiceListening{false};
    std::thread voiceListenThread;
    bool pendingVoiceRecord=false; // Spoken record requires a second explicit command.
    juce::uint32 voiceRecordRequestedAt=0; // Confirmation expires after 15 seconds.
    juce::TextButton previousTrack,nextTrack;
    size_t selectedTrack=0;
    juce::TextButton preflightButton;
    juce::TextButton play,stop,audio,midi,open,record,saveProject,openProject,exportProject,bounce,cancelBounce,connectButton,aiButton,pianoButton,analyzeCalibration,generateCalibration,liveCalibration;
    juce::ComboBox synthPreset;
    juce::Label attackLabel,releaseLabel;
    juce::Slider attackControl,releaseControl;
    IDWPianoRoll pianoRoll;
    IDWAIApprovalPanel aiPanel;
    IDWConnectPanel connectPanel;
    IDWTakeRecorder recorder;
    IDWMultiTakeRecorder multiRecorder;
    IDWLoopbackCapture loopbackCapture;
    std::vector<std::pair<std::size_t,juce::File>> multiTakes;
    juce::AudioFormatManager formats;
    juce::AudioThumbnailCache waveformCache{32};
    std::map<std::string,std::unique_ptr<juce::AudioThumbnail>> waveformThumbnails;
    std::atomic<int64_t> sessionFrames{0};
    // Allocated on the message thread before transport starts; audio only reads.
    std::shared_ptr<const idw::daw::Session> playbackSession;
    double deviceRate=48000.0;
    int preparedBlockSize=0;
    std::vector<std::unique_ptr<AudioSlot>> sources;
    std::unique_ptr<juce::FileChooser> chooser,projectChooser,calibrationChooser,referenceChooser;
    juce::Slider tempo;
};
class DAWApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override {return "IDW DAW V14 Foundation";}
    const juce::String getApplicationVersion() override {return "14.0.0-dev";}
    void initialise(const juce::String&) override {
        window=std::make_unique<MainWindow>(getApplicationName());
    }
    void shutdown() override {window.reset();}
private:
    class MainWindow final : public juce::DocumentWindow {
    public:
        MainWindow(juce::String name):DocumentWindow(name,juce::Colours::black,allButtons){
            setUsingNativeTitleBar(true);
            setContentOwned(new Workstation(),true);
            centreWithSize(getWidth(),getHeight());
            setVisible(true);
        }
        void closeButtonPressed() override {juce::JUCEApplication::getInstance()->systemRequestedQuit();}
    };
    std::unique_ptr<MainWindow> window;
};
START_JUCE_APPLICATION(DAWApplication)
