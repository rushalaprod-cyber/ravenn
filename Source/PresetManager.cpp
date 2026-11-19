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
    };
    current = factory.front().name;
}

juce::File PresetManager::getDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("ravenn").getChildFile ("Presets");
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
