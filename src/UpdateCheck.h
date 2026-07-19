#pragma once

#include <juce_core/juce_core.h>
#include <mutex>

// One-shot GitHub release check: fetches the latest release tag in a
// background thread and flags when it's newer than this build. Fails silent
// (offline, rate-limited, JUCE_USE_CURL=0 on Linux -> no-op).
namespace update
{
struct State
{
    std::atomic<bool> available { false };
    juce::SpinLock lock;
    juce::String tag; // guarded by lock
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

inline void checkOnce()
{
    static std::once_flag flag;
    std::call_once (flag, []
    {
        juce::Thread::launch ([]
        {
            const juce::URL url ("https://api.github.com/repos/rithulkamesh/chorale/releases/latest");
            const auto text = url.readEntireTextStream (false);
            const auto json = juce::JSON::parse (text);
            const auto tag = json.getProperty ("tag_name", "").toString();
            if (tag.isNotEmpty() && isNewer (tag, JucePlugin_VersionString))
            {
                const juce::SpinLock::ScopedLockType sl (state().lock);
                state().tag = tag;
                state().available.store (true);
            }
        });
    });
}
} // namespace update
