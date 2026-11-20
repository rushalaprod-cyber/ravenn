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
    void setColumns (int c) { columns = c; resized(); }

private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int columns = 0;
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
    int dragKind = 0, dragIdx = -1;
    float xOfF (float f) const;
    float fOfX (float x) const;
    float thrY (float t) const;
    float rangeY (float r) const;
    juce::Point<float> bandNode (int b) const;
    juce::Point<float> dynNode (int f) const;
    int hitTest (juce::Point<float> p, int& idx) const;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    juce::Rectangle<float> plotArea;
public:
    bool showBands = false;   // onglet MULTIBAND : edition directe sur le spectre
    int selectedBand = -1;
};

// Vectorscope : image stereo (Lissajous M/S) + correlation
class StereoView : public juce::Component, private juce::Timer
{
public:
    explicit StereoView (RavennProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    RavennProcessor& proc;
    float corrDisp = 1.f, widthDisp = 0.f;
};

// Vue du limiteur : crete avant limitation, reduction de gain, plafond
class LimiterView : public juce::Component, private juce::Timer
{
public:
    explicit LimiterView (RavennProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    RavennProcessor& proc;
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
    float lufsM = -100.f, lufsS = -100.f, lufsI = -100.f, lufsL = -100.f, tp = -100.f, agc = 0.f, clipEff = 0.f, plrDisp = 0.f;
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
// Onglet MULTIBAND : compresseur 4 bandes + EQ dynamique 2 bandes
class MultibandPage : public juce::Component, private juce::Timer
{
public:
    explicit MultibandPage (RavennProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    NeonKnob* knob (const juce::String& id, const juce::String& title, juce::Colour c, const juce::String& tip);
    RavennProcessor& proc;
    juce::OwnedArray<NeonKnob> knobs;
    juce::OwnedArray<Pill> pills;
    std::array<NeonKnob*, 4> thr {}, ratio {}, att {}, rel {}, gain {};
    std::array<Pill*, 4> solo {}, byp {};
    std::array<NeonKnob*, 3> xo {};
    NeonKnob *knee = nullptr, *mix = nullptr;
    std::array<NeonKnob*, 2> dFreq {}, dQ {}, dThr {}, dRange {};
    Pill* onPill = nullptr;
    Pill* autoPill = nullptr;
    std::array<juce::Rectangle<int>, 4> meterArea;
    std::array<juce::Rectangle<int>, 2> dynArea;
    juce::Rectangle<int> mbPanel, dynPanel;
    std::array<float, 4> grDisp {};
public:
    const int* selBand = nullptr;
    int lastSel = -2;
};

//==============================================================================
// Page des presets (s'ouvre / se ferme) : categories, recherche, tuiles
class PresetBrowser : public juce::Component
{
public:
    explicit PresetBrowser (RavennProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    std::function<void()> onClose, onSave, onImport;

private:
    void rebuildTiles();
    RavennProcessor& proc;
    juce::TextEditor search;
    juce::TextButton closeBtn { "FERMER" }, saveBtn { "SAVE" }, importBtn { "IMPORT" }, folderBtn { "DOSSIER" };
    juce::OwnedArray<juce::TextButton> catBtns, tiles;
    juce::String category { "TOUS" };
    juce::Rectangle<int> panel, tileArea;
};

//==============================================================================
class RavennEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit RavennEditor (RavennProcessor&);
    ~RavennEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setPage (int page);
    void showPresetBrowser (bool show) { browser.refresh(); browser.setVisible (show); browser.toFront (false); }
    void selectBand (int b) { spectrum.selectedBand = b; }

    // glisser-deposer d'un morceau de reference
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

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
             fatten, fatColor, imgLow, imgMid, imgHigh, stabilize, voiceFocus,
             lowCut, highCut, clipKnee, spaceLow, depth3dFreq, dsFreq, dsThr, dsRange, specRes, specSpeed;
    Segmented maskBand, clipShape, clipBand, eqPos, os, dither;
    Pill link, monoBass, delta, autoGain, noLatency, bypass, body, limOn, limTP, limUnity, clipMS, dsListen, specCut, dsAuto;
    ChoiceBox limStyle, satStyle1, satStyle2, satStyle3, charType, specTarget;
    juce::OwnedArray<juce::TextButton> slotBtns;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn, folderBtn, importBtn, abBtn, copyBtn,
                     undoBtn, redoBtn, resetBtn, refImportBtn, refClearBtn;
    juce::OwnedArray<juce::TextButton> tabs;
    std::unique_ptr<juce::FileChooser> chooser;

    Spectrum spectrum;
    StereoView stereoView;
    LimiterView limView;
    Scope scope;
    MeterColumn meters;
    AutoMasterPanel autoMaster;
    MultibandPage mbPage;
    PresetBrowser browser;
    juce::TextButton presetNameBtn;

    std::vector<Panel> panels;
    std::vector<std::pair<juce::Component*, int>> pageOf;
    juce::Rectangle<int> header, logoArea, tabArea, refInfoArea, scInfoArea, dsInfoArea;
    juce::Image background, ravenImg, ravenCrisp, coverImg;
    juce::String shownPreset, refText, scText;
    int shownAB = -1, page = 0, bgPage = -1;
    float bgScale = 0.f;
    bool shownBypass = false;
    float dsGrDisp = 0.f;
    bool dropHover = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RavennEditor)
};
