// Traductions de l'interface de ravenn.
// Les textes source sont en francais (cle) ; chaque entree donne
// EN, DE, JA, ZH, PT, ES, NL. Une cle absente reste en francais.
#include "PluginEditor.h"
#include <unordered_map>

namespace
{
    // ordre d'affichage du menu : EN, FR, DE, JA, ZH, PT, ES, NL
    constexpr int numLangs = 8;
    const char* const codes[numLangs] = { "EN", "FR", "DE", "JA", "ZH", "PT", "ES", "NL" };
    const char* const names[numLangs] = { "English", "Français", "Deutsch", "日本語", "中文", "Português", "Español", "Nederlands" };

    struct Entry { const char* fr; const char* t[7]; };   // EN, DE, JA, ZH, PT, ES, NL

    #include "LangTable.inc"

    int& currentLang()
    {
        static int lang = [] {
            auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("ravenn").getChildFile ("language.txt");
            const int v = f.existsAsFile() ? f.loadFileAsString().trim().getIntValue() : 1;
            return juce::jlimit (0, numLangs - 1, v);
        }();
        return lang;
    }

    const std::unordered_map<std::string, const Entry*>& index()
    {
        static const auto map = [] {
            std::unordered_map<std::string, const Entry*> m;
            for (auto& e : table) m[e.fr] = &e;
            return m;
        }();
        return map;
    }
}

namespace ui
{
    int language() { return currentLang(); }

    void setLanguage (int lang)
    {
        currentLang() = juce::jlimit (0, numLangs - 1, lang);
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("ravenn");
        dir.createDirectory();
        dir.getChildFile ("language.txt").replaceWithText (juce::String (currentLang()));
    }

    juce::String languageCode() { return codes[currentLang()]; }

    juce::StringArray languageNames()
    {
        juce::StringArray a;
        for (auto* n : names) a.add (juce::String::fromUTF8 (n));
        return a;
    }

    juce::String tr (const char* s)
    {
        const int l = currentLang();
        if (l == 1) return juce::String::fromUTF8 (s);          // francais : texte source
        auto it = index().find (s);
        if (it == index().end()) return juce::String::fromUTF8 (s);
        const int slot = l == 0 ? 0 : l - 1;                   // EN, puis DE..NL
        const char* t = it->second->t[slot];
        return juce::String::fromUTF8 (t != nullptr && *t != 0 ? t : s);
    }
}