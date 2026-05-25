//-----------------------------------------------------------------------------
// VST Scanner
// Description: Scans for VST plugins and outputs information to JSON
//-----------------------------------------------------------------------------

#include "vst3sdk/public.sdk/source/vst/hosting/module.h"
#include "vst3sdk/public.sdk/source/vst/moduleinfo/moduleinfoparser.h"
#include "vst3sdk/public.sdk/source/common/readfile.h"
#include "vst3sdk/pluginterfaces/base/fplatform.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

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
    std::string errorMessage;
    std::string scanSource; // "moduleinfo" or "factory"
};

//------------------------------------------------------------------------
struct ScanOptions {
    bool quiet {false};
    unsigned jsonThreads {1};
};

//------------------------------------------------------------------------
namespace {

std::ostream& logOut = std::cout;
std::mutex logMutex;

void logLine (const std::string& msg)
{
    std::lock_guard<std::mutex> lock (logMutex);
    logOut << msg << std::endl;
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
#if SMTG_OS_LINUX
    if (std::filesystem::is_regular_file (path) && path.extension () == kSharedLibExtension)
        return true;
#endif
    return isVst3BundleDirectory (path);
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

void fillPluginInfoFromModuleInfoClass (PluginInfo& info,
                                        const Steinberg::ModuleInfo::ClassInfo& classInfo)
{
    info.name = classInfo.name;
    info.vendor = classInfo.vendor;
    info.version = classInfo.version;
    info.category = classInfo.category;
    info.subCategories = classInfo.subCategories;
    info.cid = classInfo.cid;
    info.sdkVersion = classInfo.sdkVersion;
    info.cardinality = classInfo.cardinality;
    info.flags = classInfo.flags;
    info.isValid = true;
}

const Steinberg::ModuleInfo::ClassInfo* pickModuleInfoClass (
    const Steinberg::ModuleInfo& moduleInfo)
{
    for (const auto& classInfo : moduleInfo.classes)
    {
        if (isAudioEffectCategory (classInfo.category))
            return &classInfo;
    }
    if (!moduleInfo.classes.empty ())
        return &moduleInfo.classes.front ();
    return nullptr;
}

} // anonymous

//------------------------------------------------------------------------
bool tryScanFromModuleInfo (const std::string& pluginPath, PluginInfo& info)
{
    auto infoPath = VST3::Hosting::Module::getModuleInfoPath (pluginPath);
    if (!infoPath)
        return false;

    const auto jsonData = Steinberg::readFile (*infoPath);
    if (jsonData.empty ())
        return false;

    auto moduleInfo = Steinberg::ModuleInfoLib::parseJson (jsonData, nullptr);
    if (!moduleInfo)
        return false;

    const auto* classInfo = pickModuleInfoClass (*moduleInfo);
    if (!classInfo)
    {
        info.errorMessage = "No plugin classes in moduleinfo.json";
        return false;
    }

    fillPluginInfoFromModuleInfoClass (info, *classInfo);
    info.scanSource = "moduleinfo";
    return true;
}

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

    return info;
}

//------------------------------------------------------------------------
std::vector<PluginInfo> scanPlugins (const std::vector<std::string>& paths,
                                     const ScanOptions& options)
{
    std::vector<PluginInfo> results (paths.size ());
    std::vector<size_t> factoryIndices;
    factoryIndices.reserve (paths.size ());

    const unsigned numThreads =
        std::max (1u, std::min (options.jsonThreads,
                                static_cast<unsigned> (std::thread::hardware_concurrency ())));

    if (numThreads == 1)
    {
        for (size_t i = 0; i < paths.size (); ++i)
        {
            if (!options.quiet)
                logLine ("Scanning: " + paths[i]);

            PluginInfo info;
            info.path = paths[i];
            if (tryScanFromModuleInfo (paths[i], info))
                results[i] = std::move (info);
            else
                factoryIndices.push_back (i);
        }
    }
    else
    {
        std::atomic<size_t> next {0};
        std::mutex pendingMutex;

        auto jsonWorker = [&] ()
        {
            while (true)
            {
                const size_t i = next.fetch_add (1);
                if (i >= paths.size ())
                    break;

                PluginInfo info;
                info.path = paths[i];
                if (tryScanFromModuleInfo (paths[i], info))
                {
                    results[i] = std::move (info);
                    if (!options.quiet)
                        logLine ("Scanning (moduleinfo): " + paths[i]);
                }
                else
                {
                    std::lock_guard<std::mutex> lock (pendingMutex);
                    factoryIndices.push_back (i);
                }
            }
        };

        std::vector<std::thread> workers;
        workers.reserve (numThreads);
        for (unsigned t = 0; t < numThreads; ++t)
            workers.emplace_back (jsonWorker);
        for (auto& worker : workers)
            worker.join ();

        std::sort (factoryIndices.begin (), factoryIndices.end ());
    }

    for (const size_t i : factoryIndices)
    {
        if (!options.quiet)
            logLine ("Scanning (factory): " + paths[i]);
        results[i] = scanPluginFromFactory (paths[i]);
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

        if (line.find ("}") != std::string::npos && inPluginObject)
        {
            inPluginObject = false;
            existingPlugins.push_back (currentPlugin);
            continue;
        }

        if (!inPluginObject)
            continue;

        std::string trimmed = line;
        trimmed.erase (0, trimmed.find_first_not_of (" \t"));
        trimmed.erase (trimmed.find_last_not_of (" \t,") + 1);

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
int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <directory_path> [options]" << std::endl;
        std::cerr << "Options:" << std::endl;
        std::cerr << "  -o <output_file.json>     Output to file (default: stdout)" << std::endl;
        std::cerr << "  -c <cumulative_file.json> Append to existing cumulative file" << std::endl;
        std::cerr << "  -j <N>                    Parallel moduleinfo.json scans (default: 1)"
                  << std::endl;
        std::cerr << "  -q, --quiet               Suppress per-plugin progress output" << std::endl;
        std::cerr << "  -h, --help                Show this help message" << std::endl;
        return 1;
    }

    std::string directory;
    std::string outputFile;
    std::string cumulativeFile;
    bool useCumulative = false;
    VSTScanner::ScanOptions scanOptions;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help")
        {
            std::cerr << "Usage: " << argv[0] << " <directory_path> [options]" << std::endl;
            std::cerr << "Options:" << std::endl;
            std::cerr << "  -o <output_file.json>     Output to file (default: stdout)"
                      << std::endl;
            std::cerr << "  -c <cumulative_file.json> Append to existing cumulative file"
                      << std::endl;
            std::cerr << "  -j <N>                    Parallel moduleinfo.json scans"
                      << std::endl;
            std::cerr << "  -q, --quiet               Suppress per-plugin progress output"
                      << std::endl;
            std::cerr << "  -h, --help                Show this help message" << std::endl;
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
        else if ((arg == "-j" || arg == "--jobs") && i + 1 < argc)
        {
            scanOptions.jsonThreads = static_cast<unsigned> (std::stoul (argv[++i]));
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

    size_t moduleinfoCount = 0;
    size_t factoryCount = 0;
    for (const auto& p : finalPlugins)
    {
        if (p.scanSource == "moduleinfo")
            ++moduleinfoCount;
        else if (p.scanSource == "factory")
            ++factoryCount;
    }

    if (!scanOptions.quiet)
    {
        std::cout << "Scan complete in " << scanMs << " ms (moduleinfo: " << moduleinfoCount
                  << ", factory: " << factoryCount << ")" << std::endl;
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
