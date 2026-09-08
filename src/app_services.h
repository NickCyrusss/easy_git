#pragma once
#include "git.h"
#include <map>

namespace eg {
struct AiSettings {
    bool enabled = false;
    std::string provider = "DeepSeek", base_url = "https://api.deepseek.com", model = "deepseek-v4-flash", api_key;
    std::string proxy, language = "Chinese";
    std::string instructions = "Use Conventional Commits (type(scope): summary).";
    int timeout = 120, max_tokens = 2048, max_diff_bytes = 65536;
};
struct Settings {
    bool light = false;
    AiSettings ai;
    std::map<std::string,AiSettings> ai_profiles;
    std::vector<std::string> repositories;
    std::string active_repository;
};
struct AiPreset { const char* name; const char* base_url; const char* model; };
const std::vector<AiPreset>& ai_presets();
void select_ai_provider(Settings& settings,const AiPreset& preset);
std::string default_settings_path();
Settings read_settings(const std::string& path);
std::string settings_json(const Settings& settings);
void write_settings(const std::string& path, const Settings& settings);
void validate_ai(const AiSettings& settings);
struct CommitMessage { std::string summary, description; };
CommitMessage generate_commit_message(const Git& git, const AiSettings& settings,
    const std::shared_ptr<std::atomic_bool>& cancel = {});
}
