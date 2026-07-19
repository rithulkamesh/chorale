#pragma once

#include <juce_core/juce_core.h>
#include <mutex>

// GitHub release auto-update: one background check per process for a newer
// release, then an in-app download of the platform zip to ~/Downloads on
// request. Fails silent (offline, rate-limited, JUCE_USE_CURL=0 on Linux).
namespace update
{
enum class Dl { idle, downloading, done, failed };

struct State
{
    std::atomic<bool> available { false };
    std::atomic<Dl> dl { Dl::idle };
    juce::SpinLock lock;
    juce::String tag, assetUrl, assetName; // guarded by lock
    juce::File downloaded;                 // guarded by lock
};

inline State& state()
{
    static State s;
    return s;
}

inline bool isNewer (const juce::String& remote, const juce::String& local)
{
    const auto a = juce::StringArray::fromTokens (remote.trimCharactersAtStart ("vV"), ".", "");
    const auto b = juce::StringArray::fromTokens (local, ".", "");
    for (int i = 0; i < 3; ++i)
    {
        const int ra = a[i].getIntValue(), rb = b[i].getIntValue();
        if (ra != rb)
            return ra > rb;
    }
    return false;
}

inline const char* platformTag()
{
#if JUCE_MAC
    return "macOS";
#elif JUCE_WINDOWS
    return "Windows";
#else
    return "Linux";
#endif
}

inline void checkOnce()
{
    static std::once_flag flag;
    std::call_once (flag, []
    {
        juce::Thread::launch ([]
        {
            const juce::URL url ("https://api.github.com/repos/rithulkamesh/chorale/releases/latest");
            const auto json = juce::JSON::parse (url.readEntireTextStream (false));
            const auto tag = json.getProperty ("tag_name", "").toString();
            if (tag.isEmpty() || ! isNewer (tag, JucePlugin_VersionString))
                return;

            juce::String assetUrl, assetName;
            if (const auto* assets = json.getProperty ("assets", juce::var()).getArray())
                for (const auto& a : *assets)
                {
                    const auto name = a.getProperty ("name", "").toString();
                    if (name.contains (platformTag()))
                    {
                        assetUrl = a.getProperty ("browser_download_url", "").toString();
                        assetName = name;
                        break;
                    }
                }

            const juce::SpinLock::ScopedLockType sl (state().lock);
            state().tag = tag;
            state().assetUrl = assetUrl;
            state().assetName = assetName;
            state().available.store (true);
        });
    });
}

// Download the release zip to ~/Downloads in the background. The UI polls
// state().dl and reveals state().downloaded when done.
inline void startDownload()
{
    auto expected = Dl::idle;
    if (! state().dl.compare_exchange_strong (expected, Dl::downloading))
        return;

    juce::Thread::launch ([]
    {
        juce::String urlText, name;
        {
            const juce::SpinLock::ScopedLockType sl (state().lock);
            urlText = state().assetUrl;
            name = state().assetName;
        }
        if (urlText.isEmpty())
        {
            state().dl.store (Dl::failed);
            return;
        }

        auto dest = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                        .getChildFile ("Downloads")
                        .getChildFile (name);
        dest = dest.getNonexistentSibling(); // don't clobber an older download

        auto stream = juce::URL (urlText).createInputStream (
            juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs (10000)
                .withNumRedirectsToFollow (5));
        if (stream == nullptr)
        {
            state().dl.store (Dl::failed);
            return;
        }
        juce::FileOutputStream out (dest);
        if (! out.openedOk() || out.writeFromInputStream (*stream, -1) <= 0)
        {
            dest.deleteFile();
            state().dl.store (Dl::failed);
            return;
        }
        out.flush();
        {
            const juce::SpinLock::ScopedLockType sl (state().lock);
            state().downloaded = dest;
        }
        state().dl.store (Dl::done);
    });
}
} // namespace update
