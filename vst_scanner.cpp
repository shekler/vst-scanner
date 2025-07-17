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
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <memory>
#include <chrono>

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
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <directory_path> [output_file.json]" << std::endl;
        std::cerr << "  directory_path: Path to scan for VST plugins" << std::endl;
        std::cerr << "  output_file.json: Optional output file (default: stdout)" << std::endl;
        return 1;
    }
    
    std::string directory = argv[1];
    std::string outputFile = (argc > 2) ? argv[2] : "";
    
    std::cout << "Scanning directory: " << directory << std::endl;
    
    // Find VST files
    auto vstFiles = VSTScanner::findVSTFiles(directory);
    std::cout << "Found " << vstFiles.size() << " VST files" << std::endl;
    
    // Scan each plugin
    std::vector<VSTScanner::PluginInfo> plugins;
    for (const auto& file : vstFiles) {
        std::cout << "Scanning: " << file << std::endl;
        auto pluginInfo = VSTScanner::scanPlugin(file);
        plugins.push_back(pluginInfo);
    }
    
    // Output results
    if (outputFile.empty()) {
        VSTScanner::outputJSON(plugins, std::cout);
    } else {
        std::ofstream outFile(outputFile);
        if (outFile.is_open()) {
            VSTScanner::outputJSON(plugins, outFile);
            std::cout << "Results written to: " << outputFile << std::endl;
        } else {
            std::cerr << "Error: Could not open output file: " << outputFile << std::endl;
            return 1;
        }
    }
    
    return 0;
} 