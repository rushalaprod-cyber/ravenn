#include "PresetManager.h"

namespace
{
    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : state (s)
{
    factory = {
        { u8 ("Défaut"), {} },
        { u8 ("Master Fort"),       { { "in", 3.0f }, { "mouth", 25.0f }, { "mask", 15.0f }, { "maskBand", 1.0f },
                                      { "clip", 4.0f }, { "clipShape", 1.0f }, { "analog", 20.0f } } },
        { u8 ("Basses Profondes"),  { { "depth", 60.0f }, { "mouth", 35.0f }, { "monoBass", 1.0f }, { "monoFreq", 120.0f },
                                      { "clip", 2.0f }, { "clipShape", 1.0f } } },
        { u8 ("Large & Aéré"),      { { "mask", 35.0f }, { "maskBand", 3.0f }, { "width", 130.0f }, { "air", 2.5f },
                                      { "tilt", 1.0f }, { "clip", 2.0f } } },
        { u8 ("Analog Glue"),       { { "analog", 60.0f }, { "clipShape", 2.0f }, { "clip", 2.0f }, { "mouth", 15.0f } } },
        { u8 ("Clip Graves Seuls"), { { "clipBand", 1.0f }, { "clip", 5.0f }, { "clipShape", 1.0f } } },
        { u8 ("Clip Mids Seuls"),   { { "clipBand", 2.0f }, { "clip", 4.0f }, { "clipShape", 1.0f } } },
        { u8 ("Aigus Domptés"),     { { "clipBand", 3.0f }, { "clip", 4.0f }, { "clipShape", 1.0f }, { "analog", 25.0f } } },
        { u8 ("Rap Master"),        { { "in", 2.0f }, { "mouth", 40.0f }, { "depth", 35.0f }, { "mask", 20.0f }, { "maskBand", 1.0f },
                                      { "clip", 5.0f }, { "clipShape", 1.0f }, { "monoBass", 1.0f }, { "analog", 30.0f } } },
        { u8 ("Grosse Bouche"),     { { "mouth", 80.0f }, { "depth", 25.0f }, { "clip", 3.0f }, { "clipShape", 1.0f } } },
        { u8 ("Side Explosif"),     { { "mask", 70.0f }, { "maskBand", 2.0f }, { "width", 120.0f }, { "clip", 2.0f } } },
        { u8 ("Live Sans Latence"), { { "noLatency", 1.0f }, { "clip", 3.0f }, { "clipShape", 1.0f }, { "mouth", 20.0f } } },
        { u8 ("Punch Kick"),        { { "punch", 45.0f }, { "mouth", 30.0f }, { "clip", 4.0f }, { "clipShape", 1.0f } } },
        { u8 ("OTT Écrasé"),        { { "ott", 45.0f }, { "ottTime", 40.0f }, { "punch", 20.0f }, { "clip", 3.0f },
                                      { "clipShape", 1.0f }, { "mask", 15.0f }, { "maskBand", 2.0f } } },
        { u8 ("Monster Brut"),      { { "in", 3.0f }, { "mouth", 50.0f }, { "mask", 25.0f }, { "maskBand", 1.0f } } },
        { u8 ("Bande Analogique"),  { { "charType", 0.0f }, { "charAmt", 60.0f }, { "charWow", 12.0f }, { "mouth", 20.0f },
                                      { "clip", 2.0f }, { "clipShape", 1.0f } } },
        { u8 ("Vinyle Lo-Fi"),      { { "charType", 6.0f }, { "charAmt", 75.0f }, { "charWow", 40.0f }, { "charNoise", 30.0f },
                                      { "satDrive2", 6.0f }, { "satStyle2", 1.0f } } },
        { u8 ("Saturation Chaude"), { { "satDrive1", 6.0f }, { "satDrive2", 4.0f }, { "satDrive3", 3.0f },
                                      { "satStyle1", 1.0f }, { "satStyle2", 0.0f }, { "satStyle3", 2.0f }, { "clip", 2.0f } } },
        { u8 ("Air & Espace"),      { { "airMid", 35.0f }, { "airHigh", 50.0f }, { "depth3d", 40.0f }, { "space", 20.0f },
                                      { "spaceSize", 35.0f } } },
        { u8 ("Voix Devant"),       { { "voice", 2.5f }, { "voiceFreq", 2800.0f }, { "airMid", 20.0f }, { "mask", 15.0f },
                                      { "maskBand", 2.0f } } },
        { u8 ("Streaming -14 Propre"), { { "limGain", 4.0f }, { "limCeil", -1.0f }, { "limStyle", 0.0f }, { "clip", 2.0f },
                                      { "clipShape", 1.0f }, { "amTarget", 4.0f } } },
        { u8 ("Loud Club"),         { { "in", 3.0f }, { "clip", 5.0f }, { "clipShape", 1.0f }, { "limGain", 4.0f }, { "limStyle", 5.0f },
                                      { "punch", 20.0f }, { "mouth", 25.0f }, { "amTarget", 1.0f } } },
    };
    current = factory.front().name;
}

juce::File PresetManager::getDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("ravenn").getChildFile ("Presets");
}

int PresetManager::importFiles (const juce::Array<juce::File>& files)
{
    auto dir = getDirectory();
    dir.createDirectory();
    int n = 0;
    for (auto& f : files)
    {
        if (! f.existsAsFile()) continue;
        // on verifie que c'est bien un preset ravenn (XML avec l'etat RAVENN)
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName (state.state.getType())) continue;
        auto dest = dir.getNonexistentChildFile (f.getFileNameWithoutExtension(), ".preset", false);
        if (f.copyFileTo (dest)) ++n;
    }
    return n;
}

void PresetManager::openFolder() const
{
    auto dir = getDirectory();
    dir.createDirectory();
    dir.startAsProcess();
}

juce::StringArray PresetManager::getFactoryNames() const
{
    juce::StringArray names;
    for (auto& f : factory) names.add (f.name);
    return names;
}

juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray names;
    for (auto& f : getDirectory().findChildFiles (juce::File::findFiles, false, "*.preset"))
        if (! isFactory (f.getFileNameWithoutExtension()))
            names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

juce::StringArray PresetManager::getAllNames() const
{
    auto all = getFactoryNames();
    all.addArray (getUserNames());
    return all;
}

bool PresetManager::isFactory (const juce::String& name) const
{
    for (auto& f : factory)
        if (f.name == name) return true;
    return false;
}

void PresetManager::resetAll()
{
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
}

void PresetManager::setParam (const juce::String& id, float value)
{
    if (auto* rp = state.getParameter (id))
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

void PresetManager::load (const juce::String& name)
{
    for (auto& f : factory)
    {
        if (f.name == name)
        {
            resetAll();
            for (auto& [id, v] : f.values) setParam (id, v);
            current = name;
            return;
        }
    }

    auto file = getDirectory().getChildFile (name + ".preset");
    if (! file.existsAsFile()) return;

    if (auto xml = juce::parseXML (file))
    {
        if (xml->hasTagName (state.state.getType()))
        {
            state.replaceState (juce::ValueTree::fromXml (*xml));
            current = name;
        }
    }
}

bool PresetManager::save (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty() || isFactory (name)) return false;

    auto dir = getDirectory();
    if (! dir.createDirectory()) return false;

    auto st = state.copyState();
    st.setProperty ("preset", name, nullptr);
    if (auto xml = st.createXml())
    {
        if (xml->writeTo (dir.getChildFile (name + ".preset")))
        {
            current = name;
            return true;
        }
    }
    return false;
}

bool PresetManager::remove (const juce::String& name)
{
    if (isFactory (name)) return false;
    auto file = getDirectory().getChildFile (name + ".preset");
    if (! file.existsAsFile()) return false;
    const bool ok = file.moveToTrash();
    if (ok) current = factory.front().name;
    return ok;
}

void PresetManager::next()
{
    auto all = getAllNames();
    if (all.isEmpty()) return;
    const int idx = all.indexOf (current);
    load (all[(idx + 1) % all.size()]);
}

void PresetManager::previous()
{
    auto all = getAllNames();
    if (all.isEmpty()) return;
    const int idx = all.indexOf (current);
    load (all[(idx <= 0 ? all.size() : idx) - 1]);
}
