//-----------------------------------------------------------------------------
// VST Scanner
// Description: Scans for VST plugins and outputs information to JSON
//-----------------------------------------------------------------------------

#include "vst3sdk/public.sdk/source/vst/hosting/module.h"
#include "vst3sdk/pluginterfaces/base/fplatform.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#if SMTG_OS_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

//------------------------------------------------------------------------
namespace VSTScanner {

//------------------------------------------------------------------------
struct PluginInfo {
    std::string path;
    std::string name;
    std::string vendor;
    std::string version;
    std::string category;
    std::vector<std::string> subCategories;
    std::string cid;
    std::string sdkVersion;
    int32_t cardinality {0};
    uint32_t flags {0};
    bool isValid {false};
    bool missingLicense {false};
    bool loadFailed {false};
    bool loadTimedOut {false};
    std::string errorMessage;
    std::string scanSource; // "factory" or "skipped"
};

//------------------------------------------------------------------------
struct ScanOptions {
    bool quiet {false};
    unsigned factoryLoadTimeoutSec {3};
    unsigned protectedPluginTimeoutSec {2};
    bool skipFactoryLoad {false};
    bool fastLicenseSkip {true};
    bool isolateFactoryLoad {false};
};

//------------------------------------------------------------------------
std::string toLowerAscii (std::string value)
{
    for (char& c : value)
    {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char> (c - 'A' + 'a');
    }
    return value;
}

bool stringContainsInsensitive (const std::string& haystack, const std::string& needle)
{
    const auto h = toLowerAscii (haystack);
    const auto n = toLowerAscii (needle);
    return h.find (n) != std::string::npos;
}

bool isLikelyLicenseError (const std::string& errorMessage)
{
    static const char* keywords[] = {
        "ilok",
        "pace",
        "license",
        "licence",
        "activation",
        "not authorized",
        "unauthorized",
        "0x715",
        "cannot be found",
        "reinstall",
        "noauthorizedlicenses",
        "authorization required",
        "licensedaemon",
        nullptr,
    };

    for (const char** kw = keywords; *kw != nullptr; ++kw)
    {
        if (stringContainsInsensitive (errorMessage, *kw))
            return true;
    }
    return false;
}

void classifyPluginFailure (PluginInfo& info)
{
    if (info.isValid)
        return;

    if (stringContainsInsensitive (info.errorMessage, "timed out"))
    {
        info.loadTimedOut = true;
        info.loadFailed = true;
        return;
    }

    if (isLikelyLicenseError (info.errorMessage))
    {
        info.missingLicense = true;
        if (info.errorMessage.empty ())
            info.errorMessage = "Missing or invalid license (iLok/PACE)";
        return;
    }

    info.loadFailed = true;
}

//------------------------------------------------------------------------
namespace {

std::ostream& logOut = std::cout;
std::mutex logMutex;

void logLine (const std::string& msg)
{
    std::lock_guard<std::mutex> lock (logMutex);
    logOut << msg << std::endl;
    logOut.flush ();
}

bool dllNameSuggestsLicenseWrapper (const std::string& fileName)
{
    const auto lower = toLowerAscii (fileName);
    return lower.find ("pace") != std::string::npos || lower.find ("ilok") != std::string::npos ||
           lower.find ("wraptool") != std::string::npos ||
           lower.find ("pacefusion") != std::string::npos ||
           lower.find ("paceeden") != std::string::npos;
}

bool bundleUsesLicenseWrapper (const std::filesystem::path& modulePath)
{
    std::error_code ec;
    if (!std::filesystem::is_directory (modulePath, ec))
        return false;

    const auto contents = modulePath / "Contents";
    if (!std::filesystem::exists (contents, ec))
        return false;

    for (const auto& archEntry : std::filesystem::directory_iterator (contents, ec))
    {
        if (ec || !archEntry.is_directory (ec))
            continue;

        for (const auto& fileEntry :
             std::filesystem::directory_iterator (archEntry.path (), ec))
        {
            if (ec || !fileEntry.is_regular_file (ec))
                continue;

            if (fileEntry.path ().extension () == ".dll" &&
                dllNameSuggestsLicenseWrapper (fileEntry.path ().filename ().string ()))
                return true;
        }
    }

    return false;
}

bool bufferContainsInsensitive (const char* data, size_t size, const char* needle)
{
    const size_t nlen = std::strlen (needle);
    if (nlen == 0 || size < nlen)
        return false;

    for (size_t i = 0; i <= size - nlen; ++i)
    {
        bool match = true;
        for (size_t j = 0; j < nlen; ++j)
        {
            auto c = static_cast<unsigned char> (data[i + j]);
            auto t = static_cast<unsigned char> (needle[j]);
            if (c >= 'A' && c <= 'Z')
                c = static_cast<unsigned char> (c - 'A' + 'a');
            if (t >= 'A' && t <= 'Z')
                t = static_cast<unsigned char> (t - 'A' + 'a');
            if (c != t)
            {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }
    return false;
}

bool binaryChunkHasLicenseMarker (const char* data, size_t size)
{
    static const char* markers[] = {
        "pacefusion",
        "paceeden",
        "wraptool",
        "ilok",
        "noauthorizedlicensesfound",
        "authorization required",
        "licensedaemon",
        nullptr,
    };

    for (const char** m = markers; *m != nullptr; ++m)
    {
        if (bufferContainsInsensitive (data, size, *m))
            return true;
    }
    return false;
}

using BinaryMarkerCheck = bool (*) (const char* data, size_t size);

bool scanFileRangeForMarkers (std::ifstream& file, size_t offset, size_t length,
                              BinaryMarkerCheck check)
{
    file.clear ();
    file.seekg (static_cast<std::streamoff> (offset), std::ios::beg);
    if (!file)
        return false;

    constexpr size_t kChunkSize = 256 * 1024;
    constexpr size_t kOverlap = 64;
    std::vector<char> buffer (kChunkSize);
    std::vector<char> carry;
    size_t remaining = length;

    while (remaining > 0 && file)
    {
        const auto toRead = (std::min) (kChunkSize, remaining);
        file.read (buffer.data (), static_cast<std::streamsize> (toRead));
        const auto bytesRead = static_cast<size_t> (file.gcount ());
        if (bytesRead == 0)
            break;

        remaining -= bytesRead;

        std::vector<char> chunk;
        chunk.reserve (carry.size () + bytesRead);
        chunk.insert (chunk.end (), carry.begin (), carry.end ());
        chunk.insert (chunk.end (), buffer.data (), buffer.data () + bytesRead);

        if (check (chunk.data (), chunk.size ()))
            return true;

        if (chunk.size () > kOverlap)
        {
            carry.assign (chunk.end () - static_cast<ptrdiff_t> (kOverlap), chunk.end ());
        }
        else
        {
            carry = std::move (chunk);
        }
    }

    return false;
}

bool binaryFileHasMarkers (const std::filesystem::path& filePath, BinaryMarkerCheck check)
{
    std::error_code ec;
    const auto fileSize = std::filesystem::file_size (filePath, ec);
    if (ec || fileSize == 0)
        return false;

    std::ifstream file (filePath, std::ios::binary);
    if (!file)
        return false;

    constexpr size_t kHeadScan = 4 * 1024 * 1024;
    constexpr size_t kMidOffset = 20 * 1024 * 1024;
    constexpr size_t kMidScan = 8 * 1024 * 1024;
    constexpr size_t kTailScan = 2 * 1024 * 1024;

    if (scanFileRangeForMarkers (file, 0, (std::min) (fileSize, kHeadScan), check))
        return true;

    if (fileSize > kMidOffset &&
        scanFileRangeForMarkers (file, kMidOffset, (std::min) (kMidScan, fileSize - kMidOffset),
                                 check))
        return true;

    if (fileSize > kTailScan &&
        scanFileRangeForMarkers (file, fileSize - kTailScan, kTailScan, check))
        return true;

    return false;
}

bool monolithicModuleHasLicenseMarkers (const std::filesystem::path& modulePath)
{
    return binaryFileHasMarkers (modulePath, binaryChunkHasLicenseMarker);
}

bool pathSuggestsHeadlessUnsafeVendor (const std::filesystem::path& modulePath)
{
    // Win32 APIs like MessageBeep/PlaySound appear in most plugin DLLs — do not scan for those.
    // Known vendors that beep/hang on headless factory load (e.g. iZotope monolithic .vst3).
    const auto lower = toLowerAscii (modulePath.string ());
    return lower.find ("\\izotope\\") != std::string::npos;
}

bool pathHasVst3Extension (const std::filesystem::path& modulePath);
bool moduleLikelyNeedsLicense (const std::filesystem::path& modulePath);

bool moduleLikelyUnsafeFactoryLoad (const std::filesystem::path& modulePath)
{
    if (moduleLikelyNeedsLicense (modulePath))
        return false;

    return pathSuggestsHeadlessUnsafeVendor (modulePath);
}

bool pathHasVst3Extension (const std::filesystem::path& modulePath)
{
    return toLowerAscii (modulePath.extension ().string ()) == ".vst3";
}

bool moduleLikelyNeedsLicense (const std::filesystem::path& modulePath)
{
    if (bundleUsesLicenseWrapper (modulePath))
        return true;

    std::error_code ec;
    if (std::filesystem::is_regular_file (modulePath, ec) && pathHasVst3Extension (modulePath))
        return monolithicModuleHasLicenseMarkers (modulePath);

    return false;
}

void logProgress (const ScanOptions& options, size_t index, size_t total,
                  const std::string& path, const char* phase)
{
    if (options.quiet)
        return;

    std::ostringstream line;
    line << "[" << index << "/" << total << "] " << phase << ": " << path;
    logLine (line.str ());
}

void logPluginResult (const ScanOptions& options, const PluginInfo& info)
{
    if (options.quiet)
        return;

    if (info.isValid)
    {
        logLine ("  -> OK: " + info.name + " (" + info.vendor + ")");
        return;
    }

    if (info.missingLicense)
        logLine ("  -> missing license (skipped)");
    else if (info.loadFailed && info.scanSource == "skipped")
        logLine ("  -> failed (skipped)");
    else if (info.loadTimedOut)
        logLine ("  -> timed out");
    else if (info.loadFailed)
        logLine ("  -> failed: " + info.errorMessage);
    else
        logLine ("  -> failed: " + info.errorMessage);
}

//------------------------------------------------------------------------
constexpr const char* kVstAudioEffectClassName = "Audio Module Class";

bool isAudioEffectCategory (const std::string& category)
{
    return category == kVstAudioEffectClassName;
}

//------------------------------------------------------------------------
#if SMTG_OS_WINDOWS
constexpr const char* kVst3Extension = ".vst3";
#elif SMTG_OS_MACOS
constexpr const char* kVst3Extension = ".vst3";
constexpr const char* kBundleExtension = ".bundle";
#elif SMTG_OS_LINUX
constexpr const char* kVst3Extension = ".vst3";
constexpr const char* kSharedLibExtension = ".so";
#endif

//------------------------------------------------------------------------
bool isVst3BundleDirectory (const std::filesystem::path& path)
{
    if (!std::filesystem::is_directory (path))
        return false;

    const auto ext = path.extension ().string ();
#if SMTG_OS_MACOS
    if (ext != kVst3Extension && ext != kBundleExtension)
        return false;
#else
    if (ext != kVst3Extension)
        return false;
#endif
    std::error_code ec;
    return std::filesystem::exists (path / "Contents", ec);
}

//------------------------------------------------------------------------
bool isCandidateModulePath (const std::filesystem::path& path)
{
    std::error_code ec;

    if (isVst3BundleDirectory (path))
        return true;

#if SMTG_OS_WINDOWS
    // Legacy / monolithic Windows VST3: single .vst3 file (DLL), not a bundle folder
    if (std::filesystem::is_regular_file (path, ec) && path.extension () == kVst3Extension)
        return true;
#elif SMTG_OS_LINUX
    if (std::filesystem::is_regular_file (path, ec) && path.extension () == kSharedLibExtension)
        return true;
#endif

    return false;
}

//------------------------------------------------------------------------
void findVSTFilesRecursive (const std::filesystem::path& directory,
                            std::unordered_set<std::string>& seen,
                            std::vector<std::string>& vstFiles)
{
    std::error_code ec;
    std::filesystem::directory_options opts =
        std::filesystem::directory_options::skip_permission_denied;

    for (const auto& entry : std::filesystem::directory_iterator (directory, opts, ec))
    {
        if (ec)
        {
            ec.clear ();
            continue;
        }

        const auto& path = entry.path ();

        if (isCandidateModulePath (path))
        {
            const auto canonical = path.string ();
            if (seen.insert (canonical).second)
                vstFiles.push_back (canonical);
            continue;
        }

        if (entry.is_directory (ec))
            findVSTFilesRecursive (path, seen, vstFiles);
    }
}

} // anonymous

//------------------------------------------------------------------------
std::vector<std::string> findVSTFiles (const std::string& directory)
{
    std::vector<std::string> vstFiles;
    std::unordered_set<std::string> seen;

    try
    {
        std::error_code ec;
        if (!std::filesystem::exists (directory, ec))
            return vstFiles;

        findVSTFilesRecursive (std::filesystem::path (directory), seen, vstFiles);
        std::sort (vstFiles.begin (), vstFiles.end ());
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error scanning directory: " << e.what () << std::endl;
    }

    return vstFiles;
}

//------------------------------------------------------------------------
namespace {

void fillPluginInfoFromHostingClass (PluginInfo& info,
                                     const VST3::Hosting::ClassInfo& classInfo)
{
    info.name = classInfo.name ();
    info.vendor = classInfo.vendor ();
    info.version = classInfo.version ();
    info.category = classInfo.category ();
    info.subCategories = classInfo.subCategories ();
    info.cid = classInfo.ID ().toString ();
    info.sdkVersion = classInfo.sdkVersion ();
    info.cardinality = classInfo.cardinality ();
    info.flags = classInfo.classFlags ();
    info.isValid = true;
}

} // anonymous

//------------------------------------------------------------------------
PluginInfo scanPluginFromFactory (const std::string& pluginPath)
{
    PluginInfo info;
    info.path = pluginPath;
    info.isValid = false;

    try
    {
        std::string errorStr;
        auto module = VST3::Hosting::Module::create (pluginPath, errorStr);

        if (!module)
        {
            info.errorMessage = errorStr;
            return info;
        }

        auto factory = module->getFactory ();
        auto classInfos = factory.classInfos ();

        if (classInfos.empty ())
        {
            info.errorMessage = "No plugin classes found";
            return info;
        }

        for (const auto& classInfo : classInfos)
        {
            if (isAudioEffectCategory (classInfo.category ()))
            {
                fillPluginInfoFromHostingClass (info, classInfo);
                info.scanSource = "factory";
                return info;
            }
        }

        fillPluginInfoFromHostingClass (info, classInfos[0]);
        info.scanSource = "factory";
    }
    catch (const std::exception& e)
    {
        info.errorMessage = e.what ();
    }
    catch (...)
    {
        info.errorMessage = "Unknown exception while loading plugin";
    }

    return info;
}

//------------------------------------------------------------------------
std::vector<PluginInfo> parseExistingJSON (const std::string& filename);

//------------------------------------------------------------------------
namespace {

std::string quoteArg (const std::string& arg)
{
    std::string out = "\"";
    for (char c : arg)
    {
        if (c == '\\' || c == '"')
            out += '\\';
        out += c;
    }
    out += "\"";
    return out;
}

#if SMTG_OS_WINDOWS
PluginInfo scanPluginFromFactoryIsolated (const std::string& pluginPath, unsigned timeoutSec)
{
    PluginInfo fallback;
    fallback.path = pluginPath;
    fallback.isValid = false;

    char exePath[MAX_PATH] {};
    if (GetModuleFileNameA (nullptr, exePath, MAX_PATH) == 0)
    {
        fallback.errorMessage = "Could not resolve scanner executable path";
        return fallback;
    }

    const auto tempDir = std::filesystem::temp_directory_path ();
    std::ostringstream tempName;
    tempName << "vst_scan_worker_" << GetCurrentProcessId () << "_"
             << std::chrono::steady_clock::now ().time_since_epoch ().count () << ".json";
    const auto outPath = tempDir / tempName.str ();

    std::ostringstream cmd;
    SetEnvironmentVariableA ("VST_SCANNER_WORKER_PLUGIN", pluginPath.c_str ());
    SetEnvironmentVariableA ("VST_SCANNER_WORKER_OUTPUT", outPath.string ().c_str ());

    cmd << quoteArg (exePath) << " --worker-env -q";
    std::string cmdStr = cmd.str ();
    std::vector<char> cmdBuf (cmdStr.begin (), cmdStr.end ());
    cmdBuf.push_back ('\0');

    STARTUPINFOA si {};
    si.cb = sizeof (si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi {};

    if (!CreateProcessA (nullptr, cmdBuf.data (), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                         nullptr, nullptr, &si, &pi))
    {
        fallback.errorMessage = "Failed to start isolated plugin scan worker";
        return fallback;
    }

    const DWORD timeoutMs = timeoutSec * 1000u;
    const DWORD waitResult = WaitForSingleObject (pi.hProcess, timeoutMs);

    if (waitResult == WAIT_TIMEOUT)
    {
        TerminateProcess (pi.hProcess, 1);
        CloseHandle (pi.hProcess);
        CloseHandle (pi.hThread);
        std::error_code ec;
        std::filesystem::remove (outPath, ec);
        fallback.errorMessage =
            "Timed out loading plugin (hung initializer or modal UI)";
        fallback.loadTimedOut = true;
        classifyPluginFailure (fallback);
        return fallback;
    }

    DWORD exitCode = 1;
    GetExitCodeProcess (pi.hProcess, &exitCode);
    CloseHandle (pi.hProcess);
    CloseHandle (pi.hThread);

    std::vector<PluginInfo> parsed;
    std::error_code ec;
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        if (std::filesystem::exists (outPath, ec))
        {
            parsed = VSTScanner::parseExistingJSON (outPath.string ());
            if (!parsed.empty ())
                break;
        }
        Sleep (25);
    }

    std::filesystem::remove (outPath, ec);

    if (!parsed.empty ())
        return parsed.front ();

    if (!std::filesystem::exists (outPath, ec))
    {
        std::ostringstream msg;
        msg << "Isolated worker exited without result (exit code " << exitCode
            << "; plugin may have crashed on load)";
        fallback.errorMessage = msg.str ();
    }
    else
    {
        fallback.errorMessage = "Could not parse worker scan result (exit code " +
                               std::to_string (exitCode) + ")";
    }
    classifyPluginFailure (fallback);
    return fallback;
}
#endif

PluginInfo scanPluginFromFactoryWithTimeout (const std::string& pluginPath, unsigned timeoutSec)
{
    PluginInfo info;
    info.path = pluginPath;
    info.isValid = false;

    auto future = std::async (std::launch::async,
                              [&] () { return scanPluginFromFactory (pluginPath); });

    if (future.wait_for (std::chrono::seconds (timeoutSec)) == std::future_status::timeout)
    {
        info.errorMessage = "Timed out loading plugin";
        return info;
    }

    return future.get ();
}

PluginInfo loadPluginViaFactory (const std::string& pluginPath, const ScanOptions& options)
{
    PluginInfo info;
    info.path = pluginPath;

    if (options.fastLicenseSkip && moduleLikelyNeedsLicense (pluginPath))
    {
        info.isValid = false;
        info.missingLicense = true;
        info.scanSource = "skipped";
        info.errorMessage = "License protection detected; DLL load skipped";
        return info;
    }

    if (options.fastLicenseSkip && moduleLikelyUnsafeFactoryLoad (pluginPath))
    {
        info.isValid = false;
        info.loadFailed = true;
        info.scanSource = "skipped";
        info.errorMessage =
            "Factory load skipped (plugin alerts or blocks headless scan, e.g. iZotope)";
        return info;
    }

    unsigned timeoutSec = options.factoryLoadTimeoutSec;
    if (moduleLikelyNeedsLicense (pluginPath))
        timeoutSec = (std::min) (timeoutSec, options.protectedPluginTimeoutSec);

#if SMTG_OS_WINDOWS
    if (options.isolateFactoryLoad)
        info = scanPluginFromFactoryIsolated (pluginPath, timeoutSec);
    else
#endif
    if (timeoutSec > 0)
        info = scanPluginFromFactoryWithTimeout (pluginPath, timeoutSec);
    else
        info = scanPluginFromFactory (pluginPath);

    classifyPluginFailure (info);
    return info;
}

} // anonymous

//------------------------------------------------------------------------
std::vector<PluginInfo> scanPlugins (const std::vector<std::string>& paths,
                                     const ScanOptions& options)
{
    std::vector<PluginInfo> results (paths.size ());
    const size_t total = paths.size ();

    for (size_t i = 0; i < paths.size (); ++i)
    {
        logProgress (options, i + 1, total, paths[i], "scanning");

        if (options.skipFactoryLoad)
        {
            PluginInfo info;
            info.path = paths[i];
            info.loadFailed = true;
            info.errorMessage = "Factory load skipped (--no-factory)";
            results[i] = std::move (info);
        }
        else
        {
            results[i] = loadPluginViaFactory (paths[i], options);
        }

        logPluginResult (options, results[i]);
    }

    return results;
}

//------------------------------------------------------------------------
std::string escapeJSONString (const std::string& input)
{
    std::string result;
    result.reserve (input.length ());

    for (char c : input)
    {
        switch (c)
        {
            case '\\': result += "\\\\"; break;
            case '\"': result += "\\\""; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }

    return result;
}

//------------------------------------------------------------------------
void outputJSON (const std::vector<PluginInfo>& plugins, std::ostream& out)
{
    out << "{\n";
    out << "  \"scanTime\": \""
        << std::chrono::system_clock::now ().time_since_epoch ().count () << "\",\n";
    out << "  \"totalPlugins\": " << plugins.size () << ",\n";
    out << "  \"validPlugins\": "
        << std::count_if (plugins.begin (), plugins.end (),
                          [] (const PluginInfo& p) { return p.isValid; })
        << ",\n";
    out << "  \"plugins\": [\n";

    for (size_t i = 0; i < plugins.size (); ++i)
    {
        const auto& plugin = plugins[i];
        out << "    {\n";
        out << "      \"path\": \"" << escapeJSONString (plugin.path) << "\",\n";
        out << "      \"isValid\": " << (plugin.isValid ? "true" : "false") << ",\n";

        if (plugin.isValid)
        {
            if (!plugin.scanSource.empty ())
                out << "      \"scanSource\": \"" << escapeJSONString (plugin.scanSource)
                    << "\",\n";
            out << "      \"name\": \"" << escapeJSONString (plugin.name) << "\",\n";
            out << "      \"vendor\": \"" << escapeJSONString (plugin.vendor) << "\",\n";
            out << "      \"version\": \"" << escapeJSONString (plugin.version) << "\",\n";
            out << "      \"category\": \"" << escapeJSONString (plugin.category) << "\",\n";
            out << "      \"cid\": \"" << escapeJSONString (plugin.cid) << "\",\n";
            out << "      \"sdkVersion\": \"" << escapeJSONString (plugin.sdkVersion)
                << "\",\n";
            out << "      \"cardinality\": " << plugin.cardinality << ",\n";
            out << "      \"flags\": " << plugin.flags << ",\n";

            out << "      \"subCategories\": [";
            for (size_t j = 0; j < plugin.subCategories.size (); ++j)
            {
                if (j > 0)
                    out << ", ";
                out << "\"" << escapeJSONString (plugin.subCategories[j]) << "\"";
            }
            out << "]\n";
        }
        else
        {
            if (!plugin.scanSource.empty ())
                out << "      \"scanSource\": \"" << escapeJSONString (plugin.scanSource)
                    << "\",\n";
            out << "      \"missingLicense\": " << (plugin.missingLicense ? "true" : "false")
                << ",\n";
            out << "      \"failed\": " << (plugin.loadFailed ? "true" : "false") << ",\n";
            out << "      \"loadTimedOut\": " << (plugin.loadTimedOut ? "true" : "false")
                << ",\n";
            out << "      \"error\": \"" << escapeJSONString (plugin.errorMessage) << "\"\n";
        }

        out << "    }";
        if (i < plugins.size () - 1)
            out << ",";
        out << "\n";
    }

    out << "  ]\n";
    out << "}\n";
}

//------------------------------------------------------------------------
} // namespace VSTScanner

//------------------------------------------------------------------------
namespace VSTScanner {

//------------------------------------------------------------------------
std::vector<PluginInfo> parseExistingJSON (const std::string& filename)
{
    std::vector<PluginInfo> existingPlugins;
    std::ifstream file (filename);

    if (!file.is_open ())
        return existingPlugins;

    std::string line;
    bool inPluginsArray = false;
    bool inPluginObject = false;
    PluginInfo currentPlugin;

    while (std::getline (file, line))
    {
        if (line.empty () || line.find ("//") == 0)
            continue;

        if (line.find ("\"plugins\"") != std::string::npos && line.find ("[") != std::string::npos)
        {
            inPluginsArray = true;
            continue;
        }

        if (inPluginsArray && line.find ("]") != std::string::npos)
        {
            inPluginsArray = false;
            break;
        }

        if (!inPluginsArray)
            continue;

        if (line.find ("{") != std::string::npos && inPluginsArray)
        {
            inPluginObject = true;
            currentPlugin = PluginInfo ();
            continue;
        }

        if (!inPluginObject)
            continue;

        std::string trimmed = line;
        trimmed.erase (0, trimmed.find_first_not_of (" \t"));
        trimmed.erase (trimmed.find_last_not_of (" \t,") + 1);

        if (trimmed == "}" || trimmed == "},")
        {
            inPluginObject = false;
            existingPlugins.push_back (currentPlugin);
            continue;
        }

        auto extractQuoted = [&] (const std::string& key) -> std::string
        {
            if (trimmed.find (key) != 0)
                return {};
            size_t start = trimmed.find ('"') + 1;
            size_t end = trimmed.find_last_of ('"');
            if (start < end)
                return trimmed.substr (start, end - start);
            return {};
        };

        if (trimmed.find ("\"path\"") == 0)
            currentPlugin.path = extractQuoted ("\"path\"");
        else if (trimmed.find ("\"isValid\"") == 0)
            currentPlugin.isValid = (trimmed.find ("true") != std::string::npos);
        else if (trimmed.find ("\"missingLicense\"") == 0)
            currentPlugin.missingLicense = (trimmed.find ("true") != std::string::npos);
        else if (trimmed.find ("\"failed\"") == 0)
            currentPlugin.loadFailed = (trimmed.find ("true") != std::string::npos);
        else if (trimmed.find ("\"loadTimedOut\"") == 0)
            currentPlugin.loadTimedOut = (trimmed.find ("true") != std::string::npos);
        else if (trimmed.find ("\"name\"") == 0)
            currentPlugin.name = extractQuoted ("\"name\"");
        else if (trimmed.find ("\"vendor\"") == 0)
            currentPlugin.vendor = extractQuoted ("\"vendor\"");
        else if (trimmed.find ("\"version\"") == 0)
            currentPlugin.version = extractQuoted ("\"version\"");
        else if (trimmed.find ("\"category\"") == 0)
            currentPlugin.category = extractQuoted ("\"category\"");
        else if (trimmed.find ("\"cid\"") == 0)
            currentPlugin.cid = extractQuoted ("\"cid\"");
        else if (trimmed.find ("\"sdkVersion\"") == 0)
            currentPlugin.sdkVersion = extractQuoted ("\"sdkVersion\"");
        else if (trimmed.find ("\"cardinality\"") == 0)
            currentPlugin.cardinality = std::stoi (trimmed.substr (trimmed.find (':') + 1));
        else if (trimmed.find ("\"flags\"") == 0)
            currentPlugin.flags = std::stoul (trimmed.substr (trimmed.find (':') + 1));
        else if (trimmed.find ("\"error\"") == 0)
            currentPlugin.errorMessage = extractQuoted ("\"error\"");
    }

    return existingPlugins;
}

//------------------------------------------------------------------------
std::vector<PluginInfo> mergePlugins (const std::vector<PluginInfo>& existing,
                                      const std::vector<PluginInfo>& newPlugins)
{
    std::vector<PluginInfo> merged = existing;

    for (const auto& newPlugin : newPlugins)
    {
        bool exists = false;
        for (const auto& existingPlugin : existing)
        {
            if (existingPlugin.path == newPlugin.path)
            {
                exists = true;
                break;
            }
        }

        if (!exists)
            merged.push_back (newPlugin);
    }

    return merged;
}

//------------------------------------------------------------------------
} // namespace VSTScanner

//------------------------------------------------------------------------
void printUsage (const char* argv0)
{
    std::cerr << "Usage: " << argv0 << " <directory_path> [options]" << std::endl;
    std::cerr << "       " << argv0 << " --worker <plugin_path> -o <file.json>" << std::endl;
    std::cerr << "       " << argv0 << " --worker-env  (internal; uses env vars)" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  -o <output_file.json>     Output to file (default: stdout)" << std::endl;
    std::cerr << "  -c <cumulative_file.json> Append to existing cumulative file" << std::endl;
    std::cerr << "  --timeout <seconds>       Per-plugin factory load timeout (default: 3)"
              << std::endl;
    std::cerr << "  --try-license-load        Attempt DLL load for PACE/iLok bundles (slower)"
              << std::endl;
    std::cerr << "  --no-isolate              Load plugins in-process (Windows only, risky)"
              << std::endl;
    std::cerr << "  --no-factory              List paths only; never load plugin DLLs"
              << std::endl;
    std::cerr << "  -q, --quiet               Suppress per-plugin progress output" << std::endl;
    std::cerr << "  -h, --help                Show this help message" << std::endl;
}

//------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        printUsage (argv[0]);
        return 1;
    }

    std::string directory;
    std::string outputFile;
    std::string cumulativeFile;
    std::string workerPlugin;
    bool useCumulative = false;
    VSTScanner::ScanOptions scanOptions;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help")
        {
            printUsage (argv[0]);
            return 0;
        }
        else if (arg == "-o" && i + 1 < argc)
        {
            outputFile = argv[++i];
        }
        else if (arg == "-c" && i + 1 < argc)
        {
            cumulativeFile = argv[++i];
            useCumulative = true;
        }
        else if (arg == "--timeout" && i + 1 < argc)
        {
            scanOptions.factoryLoadTimeoutSec = static_cast<unsigned> (std::stoul (argv[++i]));
        }
        else if (arg == "--no-isolate")
        {
            scanOptions.isolateFactoryLoad = false;
        }
        else if (arg == "--no-factory")
        {
            scanOptions.skipFactoryLoad = true;
        }
        else if (arg == "--try-license-load")
        {
            scanOptions.fastLicenseSkip = false;
        }
        else if (arg == "--worker" && i + 1 < argc)
        {
            workerPlugin = argv[++i];
        }
        else if (arg == "--worker-env")
        {
            if (const char* p = std::getenv ("VST_SCANNER_WORKER_PLUGIN"))
                workerPlugin = p;
            if (const char* o = std::getenv ("VST_SCANNER_WORKER_OUTPUT"))
                outputFile = o;
        }
        else if (arg == "-q" || arg == "--quiet")
        {
            scanOptions.quiet = true;
        }
        else if (directory.empty ())
        {
            directory = arg;
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return 1;
        }
    }

    if (!workerPlugin.empty ())
    {
        VSTScanner::PluginInfo info;
        info.path = workerPlugin;
        info = VSTScanner::scanPluginFromFactory (workerPlugin);
        VSTScanner::classifyPluginFailure (info);

        if (outputFile.empty ())
            VSTScanner::outputJSON ({info}, std::cout);
        else
        {
            std::ofstream out (outputFile);
            if (!out.is_open ())
            {
                std::cerr << "Error: Could not open output file: " << outputFile << std::endl;
                return 1;
            }
            VSTScanner::outputJSON ({info}, out);
            out.flush ();
        }
        return 0;
    }

    if (directory.empty ())
    {
        std::cerr << "Error: Directory path is required" << std::endl;
        return 1;
    }

    if (useCumulative && !outputFile.empty ())
    {
        std::cerr << "Error: Cannot use both -o and -c options" << std::endl;
        return 1;
    }

    const auto scanStart = std::chrono::steady_clock::now ();

    if (!scanOptions.quiet)
        std::cout << "Scanning directory: " << directory << std::endl;

    std::vector<VSTScanner::PluginInfo> existingPlugins;
    if (useCumulative && !cumulativeFile.empty ())
    {
        if (!scanOptions.quiet)
            std::cout << "Loading existing plugins from: " << cumulativeFile << std::endl;
        existingPlugins = VSTScanner::parseExistingJSON (cumulativeFile);
        if (!scanOptions.quiet)
            std::cout << "Found " << existingPlugins.size () << " existing plugins" << std::endl;
    }

    auto vstFiles = VSTScanner::findVSTFiles (directory);
    if (!scanOptions.quiet)
        std::cout << "Found " << vstFiles.size () << " VST modules" << std::endl;

    auto newPlugins = VSTScanner::scanPlugins (vstFiles, scanOptions);

    std::vector<VSTScanner::PluginInfo> finalPlugins;
    if (useCumulative)
    {
        finalPlugins = VSTScanner::mergePlugins (existingPlugins, newPlugins);
        if (!scanOptions.quiet)
        {
            std::cout << "Merged " << newPlugins.size () << " new plugins with "
                      << existingPlugins.size () << " existing plugins. Total: "
                      << finalPlugins.size () << std::endl;
        }
    }
    else
    {
        finalPlugins = std::move (newPlugins);
    }

    const auto scanEnd = std::chrono::steady_clock::now ();
    const auto scanMs =
        std::chrono::duration_cast<std::chrono::milliseconds> (scanEnd - scanStart).count ();

    size_t validCount = 0;
    size_t licenseSkipCount = 0;
    size_t failedCount = 0;
    for (const auto& p : finalPlugins)
    {
        if (p.isValid)
            ++validCount;
        if (p.missingLicense)
            ++licenseSkipCount;
        if (p.loadFailed)
            ++failedCount;
    }

    if (!scanOptions.quiet)
    {
        std::cout << "Scan complete in " << scanMs << " ms (valid: " << validCount
                  << ", missingLicense: " << licenseSkipCount << ", failed: " << failedCount
                  << ")" << std::endl;
    }

    if (outputFile.empty () && cumulativeFile.empty ())
    {
        VSTScanner::outputJSON (finalPlugins, std::cout);
    }
    else
    {
        const std::string outputFileName = useCumulative ? cumulativeFile : outputFile;
        std::ofstream outFile (outputFileName);
        if (outFile.is_open ())
        {
            VSTScanner::outputJSON (finalPlugins, outFile);
            if (!scanOptions.quiet)
                std::cout << "Results written to: " << outputFileName << std::endl;
        }
        else
        {
            std::cerr << "Error: Could not open output file: " << outputFileName << std::endl;
            return 1;
        }
    }

    return 0;
}
