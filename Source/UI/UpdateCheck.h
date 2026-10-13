#pragma once
// Update check (same signed manifest format as the other Kinetik products, see tools/sign_update.py):
//
//   KINETIK-UPDATE 1
//   product: J-Slice
//   version: 1.1.0
//   notes: one line of what is new
//   page: https://github.com/Kozzzmo/J-Slice/releases/tag/latest
//   signature: <128 hex>        Ed25519 over every line before it (lines trimmed, ended by '\n')
//
// At most every 20 hours (if enabled) a background thread downloads the manifest from the "latest" release. It is
// ignored unless the signature matches the Kinetik update key and the product is J-Slice; older or equal versions
// are never offered. J-Slice has no installer: "Download" only opens the release page (github.com/Kozzzmo only).
// Nothing is sent except the plain HTTPS request.

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <mutex>
#include <string>

namespace upd
{
struct Manifest
{
    bool valid = false;
    std::string product, version, notes, page;
};

/** "1.10.2" > "1.9" ; missing parts count as 0. */
bool newer (const std::string& a, const std::string& b);

/** Parses and verifies a manifest (key = 32-byte Ed25519 public key; nullptr = the Kinetik update key). */
Manifest parse (const std::string& text, const char* product, const uint8_t* key = nullptr);

/** Owned by the processor (one per plugin instance, created by the first editor). */
class Checker : private juce::Thread
{
public:
    enum State { idle, checking, available, upToDate, failed };

    /** settings: the plugin's properties file (keys updAuto, updLast, updSkip). */
    explicit Checker (juce::PropertiesFile& settings);
    ~Checker() override;

    void check (bool force);     // message thread: automatic (every 20 h at most, if enabled) or forced
    void skipThisVersion();      // message thread
    bool autoCheck() const;
    void setAutoCheck (bool on);
    void openPage() const;

    State getState() const { return (State) state.load(); }
    Manifest manifest() const { std::lock_guard<std::mutex> lk (mx); return man; }
    static juce::String currentVersion();

    static constexpr const char* manifestUrl = "https://github.com/Kozzzmo/J-Slice/releases/download/latest/update.txt";
    static constexpr const char* defaultPage = "https://github.com/Kozzzmo/J-Slice/releases/tag/latest";

private:
    void run() override;
    juce::PropertiesFile& settings;
    std::atomic<int> state { idle };
    mutable std::mutex mx;
    Manifest man;
    std::string skipped;
};
} // namespace upd
