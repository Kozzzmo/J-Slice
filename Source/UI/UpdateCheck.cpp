#include "UpdateCheck.h"
#include <cstdlib>

extern "C" {
#include "third_party/monocypher/monocypher-ed25519.h"
}

namespace upd
{
// public key of the Kinetik update signer (same as the other Kinetik products; private key = CI secret KINETIK_UPDATE_KEY)
static const uint8_t kUpdateKey[32] = {
    0x59, 0x1d, 0xdc, 0xfe, 0xd6, 0x59, 0xcf, 0xeb, 0x44, 0x1e, 0x69, 0x1f, 0x8b, 0xa6, 0xc6, 0xdc,
    0x93, 0xe7, 0xef, 0x76, 0x13, 0x32, 0x25, 0x6f, 0xe1, 0xcf, 0xf1, 0x9d, 0x96, 0x0b, 0x7d, 0xfa };

static std::string trim (const std::string& s)
{
    size_t a = 0, b = s.size();
    auto ws = [] (unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (a < b && (ws ((unsigned char) s[a]) || (unsigned char) s[a] == 0xEF || (unsigned char) s[a] == 0xBB || (unsigned char) s[a] == 0xBF)) ++a;
    while (b > a && ws ((unsigned char) s[b - 1])) --b;
    return s.substr (a, b - a);
}

static bool fromHex (const std::string& h, uint8_t* out, size_t n)
{
    if (h.size() != n * 2) return false;
    for (size_t i = 0; i < n; ++i)
    {
        unsigned v = 0;
        for (int k = 0; k < 2; ++k)
        {
            const char c = h[i * 2 + (size_t) k];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned) (c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned) (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned) (c - 'A' + 10);
            else return false;
        }
        out[i] = (uint8_t) v;
    }
    return true;
}

bool newer (const std::string& a, const std::string& b)
{
    const char* pa = a.c_str();
    const char* pb = b.c_str();
    while (*pa || *pb)
    {
        char* ea; char* eb;
        const long x = std::strtol (pa, &ea, 10), y = std::strtol (pb, &eb, 10);
        pa = ea; pb = eb;
        if (x != y) return x > y;
        if (*pa == '.') ++pa;
        if (*pb == '.') ++pb;
        if ((*pa && ! (*pa >= '0' && *pa <= '9')) || (*pb && ! (*pb >= '0' && *pb <= '9'))) break;
    }
    return false;
}

Manifest parse (const std::string& text, const char* product, const uint8_t* key)
{
    Manifest m;
    std::string signedPart, sigHex;
    bool header = false;
    size_t pos = 0;
    while (pos <= text.size())
    {
        size_t e = text.find ('\n', pos);
        if (e == std::string::npos) e = text.size();
        const std::string line = trim (text.substr (pos, e - pos));
        pos = e + 1;
        if (line.empty()) continue;
        if (! header)
        {
            if (line != "KINETIK-UPDATE 1") continue;
            header = true;
            signedPart = line + "\n";
            continue;
        }
        const size_t c = line.find (':');
        if (c == std::string::npos) continue;
        const std::string k = trim (line.substr (0, c)), v = trim (line.substr (c + 1));
        if (k == "signature") { sigHex = v; break; }
        signedPart += k + ": " + v + "\n";
        if (k == "product") m.product = v;
        else if (k == "version") m.version = v;
        else if (k == "notes") m.notes = v;
        else if (k == "page") m.page = v;
    }
    uint8_t sig[64];
    if (! header || ! fromHex (sigHex, sig, 64)) return m;
    if (crypto_ed25519_check (sig, key ? key : kUpdateKey, (const uint8_t*) signedPart.data(), signedPart.size()) != 0) return m;
    if (m.product != product || m.version.empty()) return m;
    if (m.page.rfind ("https://github.com/Kozzzmo/", 0) != 0) m.page = Checker::defaultPage;
    m.valid = true;
    return m;
}

//==============================================================================
Checker::Checker (juce::PropertiesFile& s) : juce::Thread ("J-Slice update check"), settings (s) {}

Checker::~Checker() { stopThread (15000); }

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "0"   // test program
#endif
juce::String Checker::currentVersion() { return JucePlugin_VersionString; }

bool Checker::autoCheck() const { return settings.getBoolValue ("updAuto", true); }

void Checker::setAutoCheck (bool on)
{
    settings.setValue ("updAuto", on);
    settings.saveIfNeeded();
}

void Checker::check (bool force)
{
    if (isThreadRunning() || state.load() == checking) return;
    const auto now = juce::Time::currentTimeMillis();
    if (! force)
    {
        if (! autoCheck()) return;
        const auto last = (juce::int64) settings.getDoubleValue ("updLast", 0.0);
        if (now - last >= 0 && now - last < (juce::int64) 20 * 3600 * 1000) return;
    }
    settings.setValue ("updLast", (double) now);
    settings.saveIfNeeded();
    skipped = force ? std::string() : settings.getValue ("updSkip").toStdString();
    state.store (checking);
    startThread();
}

void Checker::skipThisVersion()
{
    settings.setValue ("updSkip", juce::String (manifest().version));
    settings.saveIfNeeded();
    state.store (upToDate);
}

void Checker::openPage() const
{
    auto page = manifest().page;
    if (page.empty()) page = defaultPage;
    juce::URL (juce::String (page)).launchInDefaultBrowser();
}

void Checker::run()
{
    juce::String text;
    {
        int status = 0;
        const auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                              .withConnectionTimeoutMs (10000)
                              .withNumRedirectsToFollow (5)
                              .withStatusCode (&status);
        if (auto in = juce::URL (manifestUrl).createInputStream (opts))
            if (status == 200 || status == 0)
                text = in->readString();
    }
    if (threadShouldExit()) return;
    if (text.isEmpty()) { state.store (failed); return; }

    const auto m = parse (text.substring (0, 16384).toStdString(), "J-Slice");
    if (! m.valid) { state.store (failed); return; }
    {
        std::lock_guard<std::mutex> lk (mx);
        man = m;
    }
    const bool offer = newer (m.version, currentVersion().toStdString()) && m.version != skipped;
    state.store (offer ? available : upToDate);
}
} // namespace upd
