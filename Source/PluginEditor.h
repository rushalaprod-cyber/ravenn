#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
    inline juce::String tr (const char* s) { return juce::String::fromUTF8 (s); }
    inline juce::Font font (float h, bool bold = false, float kerning = 0.f)
    {
        return juce::Font (juce::FontOptions (juce::jmax (6.f, h), bold ? juce::Font::bold : juce::Font::plain)
                               .withKerningFactor (kerning));
    }

    // Palette sombre : noir, blanc, nuances de bleu (acier / glace / nuit)
    const juce::Colour bg      { 0xff030406 };
    const juce::Colour panel   { 0xff07090d };
    const juce::Colour edge    { 0xff1d2733 };
    const juce::Colour track   { 0xff121821 };
    const juce::Colour text    { 0xffe9eef3 };
    const juce::Colour dim     { 0xff738191 };
    const juce::Colour violet  { 0xff3d7bff };   // bleu electrique
    const juce::Colour cyan    { 0xff9ccfff };   // glace
    const juce::Colour pink    { 0xff6f93c4 };   // acier bleute
    const juce::Colour red     { 0xfff4f7fb };   // blanc os
    const juce::Colour amber   { 0xffc9d6e3 };   // argent
    const juce::Colour green   { 0xff5fb4ff };   // bleu clair
    const juce::Colour blue    { 0xff2f5fd0 };   // bleu nuit

    // Image floutee (pour ne garder que les teintes de la cover)
    juce::Image blurred (const juce::Image& src, int w, int h, int smallSize = 40);
}

//==============================================================================
class RavennLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RavennLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hover, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool hover, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

//==============================================================================
class NeonKnob : public juce::Component
{
public:
    NeonKnob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title,
              juce::Colour c, const juce::String& tip);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;

private:
    juce::String title;
    juce::Colour colour;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class Segmented : public juce::Component
{
public:
    Segmented (juce::AudioProcessorValueTreeState& s, const juce::String& id, juce::Colour c,
               const juce::String& tip, juce::StringArray labels = {});
    void resized() override;

private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

class Pill : public juce::Component
{
public:
    Pill (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text,
          juce::Colour c, const juce::String& tip);
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::TextButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// Liste deroulante liee a un parametre "choice"
class ChoiceBox : public juce::Component
{
public:
    ChoiceBox (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& tip);
    void resized() override { box.setBounds (getLocalBounds()); }
    juce::ComboBox box;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

//==============================================================================
class Scope : public juce::Component, private juce::Timer
{
public:
    explicit Scope (RavennProcessor& p) : proc (p) { startTimerHz (40); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    RavennProcessor& proc;
};

// Analyseur de spectre : entree (trait), sortie (rempli), reference (pointilles),
// echelle en dB a droite qui suit le niveau du morceau
class Spectrum : public juce::Component, private juce::Timer
{
public:
    explicit Spectrum (RavennProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void analyse (const std::array<float, RavennProcessor::specSize>& src, int w0, std::vector<float>& out);
    float binDb (const std::vector<float>& d, float f, double fs) const;
    RavennProcessor& proc;
    static constexpr int order = 12, fftSize = 1 << order;
    juce::dsp::FFT fft { order };
    std::vector<float> work, window, dbIn, dbOut;
    float viewTop = 0.f;
    bool inSilent = true, outSilent = true;
    juce::Image backdrop;
};

class MeterColumn : public juce::Component, private juce::Timer
{
public:
    explicit MeterColumn (RavennProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    RavennProcessor& proc;
    juce::TextButton resetBtn;
    float grDisp = 0.f, limDisp = 0.f, inDisp = -100.f, outDisp = -100.f, inHold = -100.f, outHold = -100.f;
    int inHoldT = 0, outHoldT = 0;
    float lufsM = -100.f, lufsS = -100.f, lufsI = -100.f, tp = -100.f, agc = 0.f, clipEff = 0.f;
};

class AutoMasterPanel : public juce::Component, private juce::Timer
{
public:
    AutoMasterPanel (RavennProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    RavennProcessor& proc;
    juce::TextButton goBtn;
    ChoiceBox target;
    juce::String status;
    float progress = 0.f, pulse = 0.f;
};

//==============================================================================
class RavennEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RavennEditor (RavennProcessor&);
    ~RavennEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setPage (int page);

private:
    void timerCallback() override;
    void refreshPresets();
    void savePreset();
    void importPresets();
    void importReference();
    void buildBackground (float pxScale);
    void place (juce::Component& c, int page);

    struct Panel
    {
        juce::String title;
        juce::Colour colour;
        int page = 0;
        juce::Rectangle<int> area;
        juce::String help;   // texte d'aide dessine en bas du panneau
    };

    RavennProcessor& proc;
    RavennLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    NeonKnob mouth, mask, depth, analog, in, clip, out, tilt, lowShelf, air, width, monoFreq, mix,
             ott, ottTime, punch, ottXLo, ottXHi, ottLow, ottMid, ottHigh,
             limGain, limCeil, limLook, limAttack, limRelease, limLinkT, limLinkR, limSoft,
             satXLo, satXHi, satMix, satDrive1, satDrive2, satDrive3,
             charAmt, charWow, charNoise, airMid, airHigh, voice, voiceFreq,
             depth3d, space, spaceSize, scDepth, refMatch,
             fatten, fatColor, imgLow, imgMid, imgHigh, stabilize, voiceFocus;
    Segmented maskBand, clipShape, clipBand, eqPos, os, dither;
    Pill link, monoBass, delta, autoGain, noLatency, bypass, body, limOn, limTP, limUnity;
    ChoiceBox limStyle, satStyle1, satStyle2, satStyle3, charType;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn, folderBtn, importBtn, abBtn, copyBtn,
                     undoBtn, redoBtn, refImportBtn, refClearBtn;
    juce::OwnedArray<juce::TextButton> tabs;
    std::unique_ptr<juce::FileChooser> chooser;

    Spectrum spectrum;
    Scope scope;
    MeterColumn meters;
    AutoMasterPanel autoMaster;

    std::vector<Panel> panels;
    std::vector<std::pair<juce::Component*, int>> pageOf;
    juce::Rectangle<int> header, logoArea, tabArea, refInfoArea, scInfoArea;
    juce::Image background, ravenImg, ravenCrisp, coverImg;
    juce::String shownPreset, refText, scText;
    int shownAB = -1, page = 0, bgPage = -1;
    float bgScale = 0.f;
    bool shownBypass = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RavennEditor)
};
