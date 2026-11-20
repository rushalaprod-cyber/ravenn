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
                                      { "clipShape", 1.0f }, { "amTarget", 5.0f } } },
        { u8 ("Gros Saucisson"),    { { "fatten", 55.0f }, { "fatColor", 60.0f }, { "clip", 2.0f }, { "clipShape", 1.0f } } },
        { u8 ("Large 3 Bandes"),    { { "imgLow", 60.0f }, { "imgMid", 115.0f }, { "imgHigh", 140.0f }, { "depth3d", 30.0f }, { "monoBass", 1.0f } } },
        { u8 ("Voix En Avant"),     { { "voiceFocus", 60.0f }, { "voiceFreq", 2800.0f }, { "stabilize", 30.0f } } },
        { u8 ("Loud Club"),         { { "in", 3.0f }, { "clip", 5.0f }, { "clipShape", 1.0f }, { "limGain", 4.0f }, { "limStyle", 5.0f },
                                      { "punch", 20.0f }, { "mouth", 25.0f }, { "amTarget", 2.0f } } },
    };
    factory.push_back ({ u8 ("Master Spectral"),  { { "stabilize", 40.0f }, { "specRes", 30.0f }, { "clip", 2.0f }, { "clipShape", 1.0f } } });
    factory.push_back ({ u8 ("De-Ess Doux"),      { { "dsRange", 6.0f }, { "dsThr", -32.0f }, { "dsFreq", 6500.0f } } });
    factory.push_back ({ u8 ("Multiband Glue"),   { { "mbOn", 1.0f }, { "mbAuto", 1.0f }, { "mbThr1", -24.0f }, { "mbThr2", -24.0f },
                                                    { "mbThr3", -24.0f }, { "mbThr4", -26.0f }, { "mbRatio1", 2.0f }, { "mbRatio2", 2.0f },
                                                    { "mbRatio3", 2.0f }, { "mbRatio4", 2.5f } } });
    factory.push_back ({ u8 ("Stealth Chaud"),    { { "limGain", 5.0f }, { "limStyle", 10.0f }, { "limCeil", -0.5f } } });

    // 1.7 : presets par stem (piste seule). Le limiteur reste en garde-fou, sans pousser.
    auto stem = [&] (const char* n, std::initializer_list<std::pair<const char*, float>> v)
    {
        std::vector<std::pair<juce::String, float>> vals;
        vals.push_back ({ "limCeil", -1.0f });
        for (auto& p : v) vals.push_back ({ p.first, p.second });
        factory.push_back ({ juce::String::fromUTF8 (n), vals });
    };
    // VOIX
    stem ("Voix Lead Rap",      { { "lowCut", 90.f }, { "glueOn", 1.f }, { "glueThr", -16.f }, { "glueRatio", 2.f }, { "glueAtt", 2.f },
                                  { "dsAuto", 1.f }, { "dsRange", 6.f }, { "dsFreqHi", 12000.f }, { "voice", 2.0f }, { "voiceFreq", 3000.f },
                                  { "voiceQ", 1.0f }, { "airHigh", 30.f }, { "satDrive2", 3.f }, { "dynFreq1", 350.f }, { "dynRange1", -3.f } });
    stem ("Voix Lead Chant",    { { "lowCut", 80.f }, { "glueOn", 1.f }, { "glueThr", -14.f }, { "glueRatio", 1.f }, { "glueAtt", 3.f },
                                  { "dsAuto", 1.f }, { "dsRange", 5.f }, { "voice", 1.5f }, { "voiceFreq", 2500.f }, { "voiceQ", 0.8f },
                                  { "airMid", 20.f }, { "airHigh", 40.f }, { "space", 12.f }, { "spaceSize", 30.f } });
    stem ("Voix Backs",         { { "lowCut", 150.f }, { "glueOn", 1.f }, { "glueThr", -18.f }, { "glueRatio", 2.f }, { "dsAuto", 1.f },
                                  { "dsRange", 8.f }, { "voice", -1.5f }, { "voiceFreq", 2500.f }, { "imgMid", 140.f }, { "imgHigh", 150.f },
                                  { "space", 20.f }, { "airHigh", 25.f } });
    stem ("Voix Podcast",       { { "lowCut", 80.f }, { "glueOn", 1.f }, { "glueThr", -20.f }, { "glueRatio", 2.f }, { "dsAuto", 1.f },
                                  { "dsRange", 6.f }, { "voice", 2.0f }, { "voiceFreq", 2000.f }, { "voiceQ", 0.7f }, { "width", 0.f } });
    // DRUMS
    stem ("Drums Bus Punch",    { { "glueOn", 1.f }, { "glueThr", -14.f }, { "glueRatio", 2.f }, { "glueAtt", 4.f }, { "glueHpf", 2.f },
                                  { "punch", 40.f }, { "clip", 3.f }, { "clipShape", 1.f }, { "satDrive1", 3.f }, { "monoBass", 1.f } });
    stem ("Drums Trap",         { { "glueOn", 1.f }, { "glueThr", -12.f }, { "glueRatio", 2.f }, { "glueAtt", 4.f }, { "glueHpf", 2.f },
                                  { "punch", 55.f }, { "clip", 5.f }, { "clipShape", 0.f }, { "airHigh", 25.f }, { "monoBass", 1.f } });
    stem ("Drums Live Rock",    { { "glueOn", 1.f }, { "glueThr", -16.f }, { "glueRatio", 2.f }, { "glueAtt", 5.f }, { "glueMix", 60.f },
                                  { "punch", 25.f }, { "charType", 0.f }, { "charAmt", 35.f }, { "space", 10.f } });
    // BASSE
    stem ("Basse 808",          { { "lowCut", 25.f }, { "monoBass", 1.f }, { "monoFreq", 200.f }, { "depth", 40.f }, { "satDrive1", 5.f },
                                  { "satDrive2", 6.f }, { "glueOn", 1.f }, { "glueThr", -14.f }, { "glueRatio", 2.f }, { "width", 0.f } });
    stem ("Basse Electrique",   { { "lowCut", 35.f }, { "monoBass", 1.f }, { "monoFreq", 150.f }, { "glueOn", 1.f }, { "glueThr", -18.f },
                                  { "glueRatio", 2.f }, { "glueAtt", 3.f }, { "satDrive2", 4.f }, { "charType", 3.f }, { "charAmt", 30.f } });
    // INSTRUS
    stem ("Synthés Larges",     { { "lowCut", 120.f }, { "imgMid", 130.f }, { "imgHigh", 150.f }, { "depth3d", 35.f }, { "glueOn", 1.f },
                                  { "glueThr", -16.f }, { "glueRatio", 1.f }, { "airHigh", 25.f } });
    stem ("Guitares",           { { "lowCut", 90.f }, { "glueOn", 1.f }, { "glueThr", -16.f }, { "glueRatio", 1.f }, { "glueAtt", 4.f },
                                  { "dynFreq1", 3200.f }, { "dynRange1", -3.f }, { "imgMid", 120.f }, { "charType", 2.f }, { "charAmt", 25.f } });
    stem ("Piano / Claviers",   { { "lowCut", 50.f }, { "glueOn", 1.f }, { "glueThr", -18.f }, { "glueRatio", 0.f }, { "glueAtt", 4.f },
                                  { "airMid", 15.f }, { "airHigh", 25.f }, { "space", 12.f }, { "imgHigh", 120.f } });

    // Categories (ordre d'affichage du menu)
    auto cat = [] (const juce::String& n) -> juce::String
    {
        auto in = [&] (std::initializer_list<const char*> l) { for (auto* s : l) if (n == juce::String::fromUTF8 (s)) return true; return false; };
        if (in ({ "Défaut" })) return "BASE";
        if (in ({ "Master Fort", "Rap Master", "Streaming -14 Propre", "Loud Club", "Monster Brut", "Master Spectral", "Stealth Chaud" })) return "MASTER";
        if (in ({ "Basses Profondes", "Punch Kick", "Gros Saucisson", "Clip Graves Seuls" })) return "BASSES & PUNCH";
        if (in ({ "Analog Glue", "Grosse Bouche", "Saturation Chaude", "OTT Écrasé", "Clip Mids Seuls", "Aigus Domptés", "Multiband Glue" })) return juce::String::fromUTF8 ("COULEUR & DYNAMIQUE");
        if (in ({ "Large & Aéré", "Side Explosif", "Air & Espace", "Large 3 Bandes" })) return juce::String::fromUTF8 ("STÉRÉO & ESPACE");
        if (in ({ "Voix Devant", "Voix En Avant", "De-Ess Doux" })) return "VOIX";
        if (in ({ "Bande Analogique", "Vinyle Lo-Fi" })) return "VINTAGE & LO-FI";
        if (in ({ "Live Sans Latence" })) return "LIVE";
        if (in ({ "Voix Lead Rap", "Voix Lead Chant", "Voix Backs", "Voix Podcast" })) return "STEM : VOIX";
        if (in ({ "Drums Bus Punch", "Drums Trap", "Drums Live Rock" })) return "STEM : DRUMS";
        if (in ({ "Basse 808", "Basse Electrique" })) return "STEM : BASSE";
        if (in ({ "Synthés Larges", "Guitares", "Piano / Claviers" })) return "STEM : INSTRUS";
        return "AUTRES";
    };
    for (auto& f : factory) f.category = cat (f.name);
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

std::vector<std::pair<juce::String, juce::StringArray>> PresetManager::getFactoryByCategory() const
{
    static const char* order[] = { "BASE", "MASTER", "BASSES & PUNCH", "COULEUR & DYNAMIQUE", "STÉRÉO & ESPACE", "VOIX",
                                    "VINTAGE & LO-FI", "LIVE", "STEM : VOIX", "STEM : DRUMS", "STEM : BASSE", "STEM : INSTRUS", "AUTRES" };
    std::vector<std::pair<juce::String, juce::StringArray>> out;
    for (auto* o : order)
    {
        juce::StringArray names;
        for (auto& f : factory) if (f.category == juce::String::fromUTF8 (o)) names.add (f.name);
        if (! names.isEmpty()) out.push_back ({ juce::String::fromUTF8 (o), names });
    }
    return out;
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
