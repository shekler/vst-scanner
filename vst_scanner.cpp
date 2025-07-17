//-----------------------------------------------------------------------------
// VST Scanner
// Description: Scans for VST plugins and outputs information to JSON
//-----------------------------------------------------------------------------

#include "vst3sdk/public.sdk/source/vst/hosting/module.h"
#include "vst3sdk/public.sdk/source/vst/moduleinfo/moduleinfocreator.h"
#include "vst3sdk/public.sdk/source/vst/moduleinfo/moduleinfoparser.h"
#include "vst3sdk/public.sdk/source/vst/utility/stringconvert.h"
#include "vst3sdk/pluginterfaces/base/fplatform.h"
#include "vst3sdk/pluginterfaces/vst/vsttypes.h"
#include "vst3sdk/pluginterfaces/vst/ivstaudioprocessor.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <algorithm>
#include <sstream>

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
    int32_t cardinality;
    uint32_t flags;
    bool isValid;
    std::string errorMessage;
};

//------------------------------------------------------------------------
std::vector<std::string> findVSTFiles(const std::string& directory) {
    std::vector<std::string> vstFiles;
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                std::string filename = entry.path().filename().string();
                
                // Check for VST3 files
                #if SMTG_OS_WINDOWS
                if (extension == ".vst3" || (extension == "" && filename.find(".vst3") != std::string::npos)) {
                    vstFiles.push_back(entry.path().string());
                }
                #elif SMTG_OS_MACOS
                if (extension == ".vst3" || extension == ".bundle") {
                    vstFiles.push_back(entry.path().string());
                }
                #elif SMTG_OS_LINUX
                if (extension == ".vst3" || extension == ".so") {
                    vstFiles.push_back(entry.path().string());
                }
                #endif
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning directory: " << e.what() << std::endl;
    }
    
    return vstFiles;
}

//------------------------------------------------------------------------
PluginInfo scanPlugin(const std::string& pluginPath) {
    PluginInfo info;
    info.path = pluginPath;
    info.isValid = false;
    
    try {
        std::string errorStr;
        auto module = VST3::Hosting::Module::create(pluginPath, errorStr);
        
        if (!module) {
            info.errorMessage = errorStr;
            return info;
        }
        
        auto factory = module->getFactory();
        auto factoryInfo = factory.info();
        auto classInfos = factory.classInfos();
        
        if (classInfos.empty()) {
            info.errorMessage = "No plugin classes found";
            return info;
        }
        
        // Get the first audio effect class
        for (const auto& classInfo : classInfos) {
            if (classInfo.category() == kVstAudioEffectClass) {
                info.name = classInfo.name();
                info.vendor = classInfo.vendor();
                info.version = classInfo.version();
                info.category = classInfo.category();
                info.subCategories = classInfo.subCategories();
                info.cid = classInfo.ID().toString();
                info.sdkVersion = classInfo.sdkVersion();
                info.cardinality = classInfo.cardinality();
                info.flags = classInfo.classFlags();
                info.isValid = true;
                break;
            }
        }
        
        // If no audio effect found, use the first class
        if (!info.isValid && !classInfos.empty()) {
            const auto& classInfo = classInfos[0];
            info.name = classInfo.name();
            info.vendor = classInfo.vendor();
            info.version = classInfo.version();
            info.category = classInfo.category();
            info.subCategories = classInfo.subCategories();
            info.cid = classInfo.ID().toString();
            info.sdkVersion = classInfo.sdkVersion();
            info.cardinality = classInfo.cardinality();
            info.flags = classInfo.classFlags();
            info.isValid = true;
        }
        
    } catch (const std::exception& e) {
        info.errorMessage = e.what();
    }
    
    return info;
}

//------------------------------------------------------------------------
void outputJSON(const std::vector<PluginInfo>& plugins, std::ostream& out) {
    out << "{\n";
    out << "  \"scanTime\": \"" << std::chrono::system_clock::now().time_since_epoch().count() << "\",\n";
    out << "  \"totalPlugins\": " << plugins.size() << ",\n";
    out << "  \"validPlugins\": " << std::count_if(plugins.begin(), plugins.end(), [](const PluginInfo& p) { return p.isValid; }) << ",\n";
    out << "  \"plugins\": [\n";
    
    for (size_t i = 0; i < plugins.size(); ++i) {
        const auto& plugin = plugins[i];
        out << "    {\n";
        out << "      \"path\": \"" << plugin.path << "\",\n";
        out << "      \"isValid\": " << (plugin.isValid ? "true" : "false") << ",\n";
        
        if (plugin.isValid) {
            out << "      \"name\": \"" << plugin.name << "\",\n";
            out << "      \"vendor\": \"" << plugin.vendor << "\",\n";
            out << "      \"version\": \"" << plugin.version << "\",\n";
            out << "      \"category\": \"" << plugin.category << "\",\n";
            out << "      \"cid\": \"" << plugin.cid << "\",\n";
            out << "      \"sdkVersion\": \"" << plugin.sdkVersion << "\",\n";
            out << "      \"cardinality\": " << plugin.cardinality << ",\n";
            out << "      \"flags\": " << plugin.flags << ",\n";
            
            out << "      \"subCategories\": [";
            for (size_t j = 0; j < plugin.subCategories.size(); ++j) {
                if (j > 0) out << ", ";
                out << "\"" << plugin.subCategories[j] << "\"";
            }
            out << "]\n";
        } else {
            out << "      \"error\": \"" << plugin.errorMessage << "\"\n";
        }
        
        out << "    }";
        if (i < plugins.size() - 1) out << ",";
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
// Simple JSON parsing for reading existing scan files
std::vector<PluginInfo> parseExistingJSON(const std::string& filename) {
    std::vector<PluginInfo> existingPlugins;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        return existingPlugins;
    }
    
    std::string line;
    bool inPluginsArray = false;
    bool inPluginObject = false;
    PluginInfo currentPlugin;
    std::string currentField;
    
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line.find("//") == 0) continue;
        
        // Check if we're entering the plugins array
        if (line.find("\"plugins\"") != std::string::npos && line.find("[") != std::string::npos) {
            inPluginsArray = true;
            continue;
        }
        
        // Check if we're leaving the plugins array
        if (inPluginsArray && line.find("]") != std::string::npos) {
            inPluginsArray = false;
            break;
        }
        
        if (!inPluginsArray) continue;
        
        // Check if we're entering a plugin object
        if (line.find("{") != std::string::npos && inPluginsArray) {
            inPluginObject = true;
            currentPlugin = PluginInfo();
            continue;
        }
        
        // Check if we're leaving a plugin object
        if (line.find("}") != std::string::npos && inPluginObject) {
            inPluginObject = false;
            existingPlugins.push_back(currentPlugin);
            continue;
        }
        
        if (!inPluginObject) continue;
        
        // Parse plugin fields
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        trimmed.erase(trimmed.find_last_not_of(" \t,") + 1);
        
        if (trimmed.find("\"path\"") == 0) {
            size_t start = trimmed.find("\"") + 1;
            size_t end = trimmed.find_last_of("\"");
            if (start < end) {
                currentPlugin.path = trimmed.substr(start, end - start);
            }
        } else if (trimmed.find("\"isValid\"") == 0) {
            currentPlugin.isValid = (trimmed.find("true") != std::string::npos);
        } else if (trimmed.find("\"name\"") == 0) {
            size_t start = trimmed.find("\"") + 1;
            size_t end = trimmed.find_last_of("\"");
            if (start < end) {
                currentPlugin.name = trimmed.substr(start, end - start);
            }
        } else if (trimmed.find("\"vendor\"") == 0) {
            size_t start = trimmed.find("\"") + 1;
            size_t end = trimmed.find_last_of("\"");
            if (start < end) {
                currentPlugin.vendor = trimmed.substr(start, end - start);
            }
        } else if (trimmed.find("\"version\"") == 0) {
            size_t start = trimmed.find("\"") + 1;
            size_t end = trimmed.find_last_of("\"");
            if (start < end) {
                currentPlugin.version = trimmed.substr(start, end - start);
            }
        } else if (trimmed.find("\"category\"") == 0) {
            size_t start = trimmed.find("\"") + 1;
            size_t end = trimmed.find_last_of("\"");
            if (start < end) {
                currentPlugin.category = trimmed.substr(start, end - start);
            }
        } else if (trimmed.find("\"cid\"") == 0) {
            size_t start = trimmed.find("\"") + 1;
            size_t end = trimmed.find_last_of("\"");
            if (start < end) {
                currentPlugin.cid = trimmed.substr(start, end - start);
            }
        } else if (trimmed.find("\"sdkVersion\"") == 0) {
            size_t start = trimmed.find("\"") + 1;
            size_t end = trimmed.find_last_of("\"");
            if (start < end) {
                currentPlugin.sdkVersion = trimmed.substr(start, end - start);
            }
        } else if (trimmed.find("\"cardinality\"") == 0) {
            size_t start = trimmed.find(":") + 1;
            currentPlugin.cardinality = std::stoi(trimmed.substr(start));
        } else if (trimmed.find("\"flags\"") == 0) {
            size_t start = trimmed.find(":") + 1;
            currentPlugin.flags = std::stoul(trimmed.substr(start));
        } else if (trimmed.find("\"error\"") == 0) {
            size_t start = trimmed.find("\"") + 1;
            size_t end = trimmed.find_last_of("\"");
            if (start < end) {
                currentPlugin.errorMessage = trimmed.substr(start, end - start);
            }
        }
    }
    
    return existingPlugins;
}

//------------------------------------------------------------------------
// Merge new plugins with existing ones, avoiding duplicates by path
std::vector<PluginInfo> mergePlugins(const std::vector<PluginInfo>& existing, const std::vector<PluginInfo>& newPlugins) {
    std::vector<PluginInfo> merged = existing;
    
    for (const auto& newPlugin : newPlugins) {
        // Check if plugin already exists by path
        bool exists = false;
        for (const auto& existingPlugin : existing) {
            if (existingPlugin.path == newPlugin.path) {
                exists = true;
                break;
            }
        }
        
        if (!exists) {
            merged.push_back(newPlugin);
        }
    }
    
    return merged;
}

//------------------------------------------------------------------------
void outputCumulativeJSON(const std::vector<PluginInfo>& plugins, std::ostream& out) {
    out << "{\n";
    out << "  \"scanTime\": \"" << std::chrono::system_clock::now().time_since_epoch().count() << "\",\n";
    out << "  \"totalPlugins\": " << plugins.size() << ",\n";
    out << "  \"validPlugins\": " << std::count_if(plugins.begin(), plugins.end(), [](const PluginInfo& p) { return p.isValid; }) << ",\n";
    out << "  \"plugins\": [\n";
    
    for (size_t i = 0; i < plugins.size(); ++i) {
        const auto& plugin = plugins[i];
        out << "    {\n";
        out << "      \"path\": \"" << plugin.path << "\",\n";
        out << "      \"isValid\": " << (plugin.isValid ? "true" : "false") << ",\n";
        
        if (plugin.isValid) {
            out << "      \"name\": \"" << plugin.name << "\",\n";
            out << "      \"vendor\": \"" << plugin.vendor << "\",\n";
            out << "      \"version\": \"" << plugin.version << "\",\n";
            out << "      \"category\": \"" << plugin.category << "\",\n";
            out << "      \"cid\": \"" << plugin.cid << "\",\n";
            out << "      \"sdkVersion\": \"" << plugin.sdkVersion << "\",\n";
            out << "      \"cardinality\": " << plugin.cardinality << ",\n";
            out << "      \"flags\": " << plugin.flags << ",\n";
            
            out << "      \"subCategories\": [";
            for (size_t j = 0; j < plugin.subCategories.size(); ++j) {
                if (j > 0) out << ", ";
                out << "\"" << plugin.subCategories[j] << "\"";
            }
            out << "]\n";
        } else {
            out << "      \"error\": \"" << plugin.errorMessage << "\"\n";
        }
        
        out << "    }";
        if (i < plugins.size() - 1) out << ",";
        out << "\n";
    }
    
    out << "  ]\n";
    out << "}\n";
}

//------------------------------------------------------------------------
} // namespace VSTScanner

//------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <directory_path> [options]" << std::endl;
        std::cerr << "Options:" << std::endl;
        std::cerr << "  -o <output_file.json>     Output to file (default: stdout)" << std::endl;
        std::cerr << "  -c <cumulative_file.json> Append to existing cumulative file" << std::endl;
        std::cerr << "  -h, --help                Show this help message" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Examples:" << std::endl;
        std::cerr << "  " << argv[0] << " C:\\VSTPlugins" << std::endl;
        std::cerr << "  " << argv[0] << " C:\\VSTPlugins -o scan_results.json" << std::endl;
        std::cerr << "  " << argv[0] << " C:\\VSTPlugins -c cumulative_scan.json" << std::endl;
        return 1;
    }
    
    std::string directory;
    std::string outputFile;
    std::string cumulativeFile;
    bool useCumulative = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            std::cerr << "Usage: " << argv[0] << " <directory_path> [options]" << std::endl;
            std::cerr << "Options:" << std::endl;
            std::cerr << "  -o <output_file.json>     Output to file (default: stdout)" << std::endl;
            std::cerr << "  -c <cumulative_file.json> Append to existing cumulative file" << std::endl;
            std::cerr << "  -h, --help                Show this help message" << std::endl;
            std::cerr << std::endl;
            std::cerr << "Examples:" << std::endl;
            std::cerr << "  " << argv[0] << " C:\\VSTPlugins" << std::endl;
            std::cerr << "  " << argv[0] << " C:\\VSTPlugins -o scan_results.json" << std::endl;
            std::cerr << "  " << argv[0] << " C:\\VSTPlugins -c cumulative_scan.json" << std::endl;
            return 0;
        } else if (arg == "-o" && i + 1 < argc) {
            outputFile = argv[++i];
        } else if (arg == "-c" && i + 1 < argc) {
            cumulativeFile = argv[++i];
            useCumulative = true;
        } else if (directory.empty()) {
            directory = arg;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return 1;
        }
    }
    
    if (directory.empty()) {
        std::cerr << "Error: Directory path is required" << std::endl;
        return 1;
    }
    
    if (useCumulative && !outputFile.empty()) {
        std::cerr << "Error: Cannot use both -o and -c options" << std::endl;
        return 1;
    }
    
    std::cout << "Scanning directory: " << directory << std::endl;
    
    // Load existing plugins if using cumulative mode
    std::vector<VSTScanner::PluginInfo> existingPlugins;
    if (useCumulative && !cumulativeFile.empty()) {
        std::cout << "Loading existing plugins from: " << cumulativeFile << std::endl;
        existingPlugins = VSTScanner::parseExistingJSON(cumulativeFile);
        std::cout << "Found " << existingPlugins.size() << " existing plugins" << std::endl;
    }
    
    // Find VST files
    auto vstFiles = VSTScanner::findVSTFiles(directory);
    std::cout << "Found " << vstFiles.size() << " VST files" << std::endl;
    
    // Scan each plugin
    std::vector<VSTScanner::PluginInfo> newPlugins;
    for (const auto& file : vstFiles) {
        std::cout << "Scanning: " << file << std::endl;
        auto pluginInfo = VSTScanner::scanPlugin(file);
        newPlugins.push_back(pluginInfo);
    }
    
    // Merge plugins if using cumulative mode
    std::vector<VSTScanner::PluginInfo> finalPlugins;
    if (useCumulative) {
        finalPlugins = VSTScanner::mergePlugins(existingPlugins, newPlugins);
        std::cout << "Merged " << newPlugins.size() << " new plugins with " << existingPlugins.size() 
                  << " existing plugins. Total: " << finalPlugins.size() << std::endl;
    } else {
        finalPlugins = newPlugins;
    }
    
    // Output results
    if (outputFile.empty() && cumulativeFile.empty()) {
        VSTScanner::outputJSON(finalPlugins, std::cout);
    } else {
        std::string outputFileName = useCumulative ? cumulativeFile : outputFile;
        std::ofstream outFile(outputFileName);
        if (outFile.is_open()) {
            if (useCumulative) {
                VSTScanner::outputCumulativeJSON(finalPlugins, outFile);
            } else {
                VSTScanner::outputJSON(finalPlugins, outFile);
            }
            std::cout << "Results written to: " << outputFileName << std::endl;
        } else {
            std::cerr << "Error: Could not open output file: " << outputFileName << std::endl;
            return 1;
        }
    }
    
    return 0;
} 