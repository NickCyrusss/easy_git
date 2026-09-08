#pragma once
#include "git.h"
#include <map>

namespace eg {
struct AiSettings {
    bool enabled = false;
    std::string provider = "DeepSeek", base_url = "https://api.deepseek.com", model = "deepseek-v4-flash", api_key;
    std::string proxy, language = "Chinese";
    std::string format = "OpenAI", token_parameter = "max_tokens";
    std::string instructions = "Use Conventional Commits (type(scope): summary).";
    int timeout = 120, max_tokens = 2048, max_diff_bytes = 65536;
};
struct WindowSettings { int width = 1440, height = 900; };
struct Settings {
    bool light = false;
    AiSettings ai;
    std::map<std::string,AiSettings> ai_profiles;
    std::vector<std::string> repositories;
    std::string active_repository;
    WindowSettings window;
};
struct AiPreset {
    const char* name; const char* base_url; const char* model;
    const char* format = "OpenAI";
    const char* token_parameter = "max_tokens";
};
const std::vector<AiPreset>& ai_presets();
bool is_ai_preset(const std::string& name);
void select_ai_provider(Settings& settings,const AiPreset& preset);
void select_ai_model(Settings& settings,const std::string& name);
void add_ai_model(Settings& settings,const std::string& name);
void delete_ai_model(Settings& settings,const std::string& name);
std::string default_settings_path();
Settings read_settings(const std::string& path);
std::string settings_json(const Settings& settings);
void write_settings(const std::string& path, const Settings& settings);
void validate_ai(const AiSettings& settings);
struct CommitMessage { std::string summary, description; };
CommitMessage generate_commit_message(const Git& git, const AiSettings& settings,
    const std::shared_ptr<std::atomic_bool>& cancel = {});
}
