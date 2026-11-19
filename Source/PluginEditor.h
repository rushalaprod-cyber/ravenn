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
    const juce::Colour red     { 0xfff4f7fb };   // blanc os (danger / GR)
    const juce::Colour amber   { 0xffc9d6e3 };   // argent
    const juce::Colour green   { 0xff5fb4ff };   // bleu clair
    const juce::Colour blue    { 0xff2f5fd0 };   // bleu nuit
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

// Selecteur segmente lie a un parametre "choice"
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

// Bouton pilule on/off lie a un parametre bool
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

//==============================================================================
// Oscilloscope du clipper : enveloppe de sortie + reduction de gain
class Scope : public juce::Component, private juce::Timer
{
public:
    explicit Scope (RavennProcessor& p) : proc (p) { startTimerHz (40); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    RavennProcessor& proc;
};

// Analyseur de spectre : entree (trait fin) et sortie (rempli)
class Spectrum : public juce::Component, private juce::Timer
{
public:
    explicit Spectrum (RavennProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void analyse (const std::array<float, RavennProcessor::specSize>& src, int w0, std::vector<float>& out);
    RavennProcessor& proc;
    static constexpr int order = 12, fftSize = 1 << order;
    juce::dsp::FFT fft { order };
    std::vector<float> work, window, dbIn, dbOut;
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
    float grDisp = 0.f, inDisp = -100.f, outDisp = -100.f, inHold = -100.f, outHold = -100.f;
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
    Segmented target;
    juce::String status;
    float progress = 0.f, pulse = 0.f;
    int lastState = -1;
};

//==============================================================================
class RavennEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RavennEditor (RavennProcessor&);
    ~RavennEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshPresets();
    void savePreset();
    void buildBackground();

    struct Panel
    {
        juce::String title;
        juce::Colour colour;
        juce::Rectangle<int> area;
    };

    RavennProcessor& proc;
    RavennLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    NeonKnob mouth, mask, depth, analog, in, clip, out, tilt, lowShelf, air, width, monoFreq, mix,
             ott, ottTime, punch;
    Segmented maskBand, clipShape, clipBand, eqPos, os;
    Pill link, monoBass, delta, autoGain, noLatency, bypass, body;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn, deleteBtn, abBtn, copyBtn;

    Spectrum spectrum;
    Scope scope;
    MeterColumn meters;
    AutoMasterPanel autoMaster;

    std::array<Panel, 7> panels;
    juce::Rectangle<int> header, logoArea;
    juce::Image background, ravenImg, coverImg;
    juce::String shownPreset;
    int shownAB = -1;
    bool shownBypass = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RavennEditor)
};
