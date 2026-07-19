#include "Theme.h"
#include "BinaryData.h"

using namespace juce;

namespace ui
{
Typeface::Ptr plexSans()
{
    static Typeface::Ptr t = Typeface::createSystemTypefaceFor (
        BinaryData::IBMPlexSansRegular_ttf, (size_t) BinaryData::IBMPlexSansRegular_ttfSize);
    return t;
}

Typeface::Ptr plexSansSemi()
{
    static Typeface::Ptr t = Typeface::createSystemTypefaceFor (
        BinaryData::IBMPlexSansSemiBold_ttf, (size_t) BinaryData::IBMPlexSansSemiBold_ttfSize);
    return t;
}

Typeface::Ptr plexMono()
{
    static Typeface::Ptr t = Typeface::createSystemTypefaceFor (
        BinaryData::IBMPlexMonoRegular_ttf, (size_t) BinaryData::IBMPlexMonoRegular_ttfSize);
    return t;
}

Font sans (float size, bool bold)
{
    return Font (FontOptions (bold ? plexSansSemi() : plexSans()).withHeight (size));
}

Font mono (float size)
{
    return Font (FontOptions (plexMono()).withHeight (size));
}

Colour voiceInk (int v)
{
    const auto c = kVoice[v];
    return c.getPerceivedBrightness() < 0.55f ? c.brighter (0.9f) : c;
}

Colour textOn (Colour fill)
{
    return fill.getPerceivedBrightness() > 0.5f ? kBg : kText;
}

String voiceLabel (AudioProcessorValueTreeState& apvts, int v)
{
    const auto id = String (v + 1);
    const int mode = (int) apvts.getRawParameterValue ("v" + id + "Mode")->load();
    if (mode == 0)
        return "-";
    if (mode == 3)
        return "MIDI";
    if (mode == 2)
    {
        static const char* pcs[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int note = 36 + (int) apvts.getRawParameterValue ("v" + id + "Note")->load();
        return String (pcs[note % 12]) + String (note / 12 - 1);
    }
    const int deg = (int) apvts.getRawParameterValue ("v" + id + "Degree")->load() - 7;
    if (deg == 0)
        return "UNI";
    static const char* names[] = { "UNI", "2ND", "3RD", "4TH", "5TH", "6TH", "7TH", "OCT" };
    return String (names[std::min (std::abs (deg), 7)]) + (deg > 0 ? String::charToString (0x2191)
                                                                   : String::charToString (0x2193));
}

float gainToDb (float gain)
{
    if (gain <= 0.0001f)
        return kMinGainDb;
    return jmax (kMinGainDb, Decibels::gainToDecibels (gain));
}

float dbToGain (float db)
{
    if (db <= kMinGainDb)
        return 0.0f;
    return Decibels::decibelsToGain (db);
}

String gainDbString (float db)
{
    if (db <= kMinGainDb + 0.05f)
        return "-inf";
    return String (db, 1) + " dB";
}
} // namespace ui
