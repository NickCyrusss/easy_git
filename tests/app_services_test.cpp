#include "app_services.h"
#include <json-c/json.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <functional>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;
void require(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F action,const char* message) {
    bool failed = false; try { action(); } catch (const std::exception&) { failed = true; } require(failed,message);
}
std::string read(const fs::path& file) { std::ifstream in(file); return {std::istreambuf_iterator<char>(in),{}}; }
struct Server {
    int fd = -1; std::string url; std::future<std::string> request;
    Server(std::string response,int status = 200,std::function<void()> before_reply = {}) {
        fd = socket(AF_INET,SOCK_STREAM,0); require(fd >= 0,"Cannot create local mock HTTP socket");
        sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        require(bind(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr)) == 0 && listen(fd,1) == 0,"Cannot bind mock HTTP server");
        socklen_t length = sizeof(addr); getsockname(fd,reinterpret_cast<sockaddr*>(&addr),&length);
        url = "http://127.0.0.1:" + std::to_string(ntohs(addr.sin_port)) + "/v1";
        request = std::async(std::launch::async,[this,response,status,before_reply] {
            pollfd p{fd,POLLIN,0}; require(poll(&p,1,5000) > 0,"No HTTP request received");
            int client = accept(fd,nullptr,nullptr); require(client >= 0,"Mock accept failed");
            timeval timeout{5,0}; setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
            std::string data; char buf[4096]; size_t body_at = std::string::npos,bytes = 0;
            for (;;) {
                auto n = recv(client,buf,sizeof(buf),0); if (n <= 0) break; data.append(buf,size_t(n));
                if (body_at == std::string::npos) {
                    auto end = data.find("\r\n\r\n");
                    if (end != std::string::npos) {
                        body_at = end+4; auto field = data.find("Content-Length:");
                        require(field != std::string::npos,"Missing Content-Length"); bytes = std::stoul(data.substr(field+15));
                    }
                }
                if (body_at != std::string::npos && data.size() >= body_at+bytes) break;
            }
            if (before_reply) before_reply();
            auto wire = "HTTP/1.1 " + std::to_string(status) + " Result\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " + std::to_string(response.size()) + "\r\n\r\n" + response;
            size_t sent = 0; while (sent < wire.size()) { auto n = send(client,wire.data()+sent,wire.size()-sent,MSG_NOSIGNAL); if (n <= 0) break; sent += size_t(n); }
            close(client); return data;
        });
    }
    ~Server() { if (request.valid()) request.wait(); if (fd >= 0) close(fd); }
};
int main() {
    char pattern[] = "/tmp/easy-git-services-XXXXXX"; char* temp = mkdtemp(pattern); if (!temp) return 1;
    fs::path root(temp), config = root / ".easy_git";
    try {
        eg::Settings settings; settings.light = true; settings.ai.api_key = "test-secret";
        require(settings.window.width == 1440 && settings.window.height == 900,"Default window size changed");
        settings.window = {1280,800};
        settings.ai.instructions = "中文 \"quoted\"\nsecond line";
        settings.repositories = {(root / "仓库 A").string(),(root / "仓库 B").string()}; settings.active_repository = settings.repositories[1];
        eg::write_settings(config.string(),settings);
        require(eg::settings_json(eg::read_settings(config.string())) == eg::settings_json(settings),"Settings round trip failed");
        auto* saved_json = json_tokener_parse(read(config).c_str());
        require(json_object_object_length(json_object_object_get(saved_json,"window")) == 2 &&
            !json_object_object_get(saved_json,"layout") && !json_object_object_get(saved_json,"repository_layouts"),"Window settings saved position/state or layout");
        json_object_put(saved_json);
        struct stat info{}; stat(config.c_str(),&info); require((info.st_mode & 0777) == 0600,"Settings permissions are not 0600");
        settings.repositories.erase(settings.repositories.begin()); settings.ai.model = "edited-model";
        eg::write_settings(config.string(),settings);
        require(eg::read_settings(config.string()).repositories.size() == 1 && read(config).find("edited-model") != std::string::npos,"Settings were appended rather than replaced");
        auto previous = read(config); settings.ai.timeout = 0;
        rejects([&] { eg::write_settings(config.string(),settings); },"Invalid settings were saved");
        require(read(config) == previous,"Failed save replaced valid settings");
        settings.ai.timeout = 120; settings.window.width = 0;
        rejects([&] { eg::write_settings(config.string(),settings); },"Invalid window size was saved");
        require(read(config) == previous,"Invalid window size replaced valid settings");
        for (const auto* data : {R"({"window":{"width":-1}})",R"({"window":{"height":9999999999}})",R"({"window":{"width":"1280"}})",R"({"window":[]})"}) {
            std::ofstream(config) << data;
            rejects([&] { eg::read_settings(config.string()); },"Invalid saved window size was accepted");
        }
        std::ofstream(config) << "{broken";
        rejects([&] { eg::read_settings(config.string()); },"Malformed settings accepted");
        require(read(config) == "{broken","Malformed settings were overwritten while loading");

        // Existing single-provider files migrate without resetting customized fields.
        std::ofstream(config) << R"({"version":1,"ai":{"provider":"DeepSeek","model":"legacy-model","api_key":"legacy-key"}})";
        auto profiles = eg::read_settings(config.string());
        require(profiles.window.width == 1440 && profiles.window.height == 900,"Legacy file did not use default window size");
        require(profiles.ai_profiles.at("DeepSeek").api_key == "legacy-key","Legacy AI settings were not migrated");
        eg::select_ai_provider(profiles,eg::ai_presets()[1]);
        require(profiles.ai.api_key.empty() && profiles.ai.model == eg::ai_presets()[1].model,"New provider inherited another provider's key or model");
        eg::select_ai_provider(profiles,eg::ai_presets()[0]);
        require(profiles.ai.model == "legacy-model" && profiles.ai.api_key == "legacy-key","Switching lost the legacy configuration");
        auto ai_text = [](const eg::AiSettings& ai) { eg::Settings s; s.ai = ai; return eg::settings_json(s); };
        std::map<std::string,std::string> expected;
        profiles.ai.enabled = true;
        int index = 0;
        for (const auto& preset : eg::ai_presets()) {
            eg::select_ai_provider(profiles,preset);
            auto& ai = profiles.ai;
            require(eg::is_ai_preset(preset.name) && std::string(preset.name) != "Custom","Preset registry contains Custom");
            require(ai.format == preset.format && ai.token_parameter == preset.token_parameter,"Preset protocol defaults were not loaded");
            rejects([&] { eg::delete_ai_model(profiles,preset.name); },"A preset was deleted");
            ai.format = index % 2 ? "Anthropic" : "OpenAI";
            ai.token_parameter = index % 2 ? "max_completion_tokens" : "max_tokens";
            ai.model = std::string(preset.name) + "-edited"; ai.api_key = std::string(preset.name) + "-secret";
            ai.base_url = "https://example.invalid/" + std::string(preset.name);
            ai.proxy = "http://127.0.0.1:" + std::to_string(32766 + index);
            ai.language = index % 2 ? "English" : "Chinese"; ai.instructions = "style " + std::to_string(index);
            ai.timeout = 60 + index; ai.max_tokens = 1024 + index; ai.max_diff_bytes = 4096 + index++;
            expected[preset.name] = ai_text(ai);
            eg::select_ai_provider(profiles,preset);
            require(ai_text(profiles.ai) == expected.at(preset.name),"Reselecting the current provider reset customized settings");
        }
        eg::write_settings(config.string(),profiles);
        profiles = eg::read_settings(config.string());
        require(profiles.ai.provider == eg::ai_presets().back().name && profiles.ai_profiles.size() == eg::ai_presets().size(),"Restart lost the active provider or saved profiles");
        for (const auto& preset : eg::ai_presets()) {
            eg::select_ai_provider(profiles,preset);
            require(ai_text(profiles.ai) == expected.at(preset.name),"Provider settings did not survive switching and restart");
        }
        profiles.ai.enabled = false;
        eg::select_ai_provider(profiles,eg::ai_presets()[0]);
        require(!profiles.ai.enabled,"Switching a provider unexpectedly enabled AI");
        profiles.ai.model = "latest-active-edit";
        eg::write_settings(config.string(),profiles);
        require(eg::read_settings(config.string()).ai_profiles.at("DeepSeek").model == "latest-active-edit","A stale saved profile overwrote the active edit");
        previous = read(config); profiles.ai_profiles.at("Kimi").timeout = 0;
        rejects([&] { eg::write_settings(config.string(),profiles); },"Invalid inactive profile was saved");
        require(read(config) == previous,"Invalid inactive profile replaced the previous settings");
        std::ofstream(config) << R"({"ai_profiles":{"Kimi":{"provider":"DeepSeek"}}})";
        rejects([&] { eg::read_settings(config.string()); },"Mismatched profile provider accepted");
        std::ofstream(config) << R"({"ai":{"format":"Other"}})";
        rejects([&] { eg::read_settings(config.string()); },"Unknown request format accepted");

        // Custom profiles are independent, protected from name collisions, and removed permanently.
        eg::Settings models; models.ai.api_key = "preset-secret"; models.ai.enabled = true;
        eg::add_ai_model(models,"  Work Claude  ");
        require(models.ai.provider == "Work Claude" && models.ai.enabled && models.ai.api_key.empty() && models.ai.base_url.empty() && models.ai.proxy.empty(),"New model inherited credentials or incorrect defaults");
        models.ai.format = "Anthropic"; models.ai.model = "my-claude"; models.ai.api_key = "own-key";
        for (const auto* name : {"", "  ", "deepseek", "WORK CLAUDE", "Custom", "bad##name", "bad\nname"})
            rejects([&] { eg::add_ai_model(models,name); },"Invalid or duplicate model name accepted");
        eg::add_ai_model(models,"Local model"); models.ai.base_url = "http://localhost:1234/v1"; models.ai.model = "local";
        eg::select_ai_model(models,"Work Claude"); require(models.ai.api_key == "own-key" && models.ai.format == "Anthropic","Custom model switching lost settings");
        eg::write_settings(config.string(),models); models = eg::read_settings(config.string());
        require(models.ai.provider == "Work Claude" && models.ai_profiles.count("Local model"),"Custom models did not survive restart");
        eg::delete_ai_model(models,"Local model");
        eg::delete_ai_model(models,models.ai.provider);
        require(models.ai.provider == "DeepSeek" && models.ai.api_key == "preset-secret" && models.ai.enabled,"Deleting active model did not restore preset");
        eg::write_settings(config.string(),models); models = eg::read_settings(config.string());
        require(!models.ai_profiles.count("Work Claude") && !models.ai_profiles.count("Local model"),"Deleted models reappeared after restart");
        rejects([&] { eg::select_ai_model(models,"Work Claude"); },"Deleted model was selectable");
        std::ofstream(config) << R"({"ai":{"provider":"Custom","base_url":"http://localhost:11434/v1","model":"legacy","api_key":"legacy-key"},"ai_profiles":{"Imported model":{"provider":"Imported model","model":"existing"}}})";
        models = eg::read_settings(config.string());
        require(models.ai.provider == "Imported model 2" && models.ai.model == "legacy" && models.ai.api_key == "legacy-key" && models.ai.format == "OpenAI" && !models.ai_profiles.count("Custom"),"Old Custom profile was lost or not migrated");
        eg::write_settings(config.string(),models); models = eg::read_settings(config.string());
        require(models.ai.provider == "Imported model 2","Custom migration repeated on restart");
        eg::delete_ai_model(models,models.ai.provider);

        eg::Git git(root.string()); git.checked({"init","--initial-branch=main"});
        git.checked({"config","user.name","AI Test"}); git.checked({"config","user.email","ai@example.invalid"}); git.checked({"config","commit.gpgsign","false"});
        std::ofstream(root / "code.txt") << "base\n"; git.checked({"add","code.txt"}); git.commit("Base");
        std::ofstream(root / "code.txt") << "STAGED_ONLY\n"; git.checked({"add","code.txt"});
        std::ofstream(root / "code.txt") << "UNSTAGED_PRIVATE\n";
        eg::AiSettings ai; ai.enabled = true; ai.provider = "Test model"; ai.model = "test-model"; ai.api_key = "test-secret";
        auto initial_tree = git.checked({"write-tree"});
        const std::string success = R"({"choices":[{"finish_reason":"stop","message":{"content":"feat: 生成测试提交\n\nDescribe the staged change."}}]})";
        {
            Server server(success); ai.base_url = server.url;
            auto message = eg::generate_commit_message(git,ai);
            require(message.summary == "feat: 生成测试提交" && message.description == "Describe the staged change.","AI response was not split into summary/body");
            auto request = server.request.get();
            require(request.rfind("POST /v1/chat/completions HTTP/1.1",0) == 0 && request.find("Authorization: Bearer test-secret") != std::string::npos,"Incorrect HTTP endpoint or auth");
            require(request.find("STAGED_ONLY") != std::string::npos && request.find("UNSTAGED_PRIVATE") == std::string::npos && request.find("test-model") != std::string::npos,"AI received unstaged changes or wrong model");
            auto* json = json_tokener_parse(request.substr(request.find("\r\n\r\n")+4).c_str()); require(json != nullptr,"Request body is not valid JSON"); json_object_put(json);
            require(git.checked({"write-tree"}) == initial_tree && read(root / "code.txt") == "UNSTAGED_PRIVATE\n","AI generation changed worktree/index");
        }
        {
            Server server(success); ai.base_url = server.url + "/chat/completions";
            eg::generate_commit_message(git,ai);
            require(server.request.get().rfind("POST /v1/chat/completions HTTP/1.1",0) == 0,"Full endpoint was duplicated");
        }
        {
            Server server(success); ai.base_url = server.url + "/messages/"; ai.token_parameter = "max_completion_tokens";
            eg::generate_commit_message(git,ai);
            auto request = server.request.get();
            require(request.rfind("POST /v1/chat/completions HTTP/1.1",0) == 0 && request.find("\"max_completion_tokens\":2048") != std::string::npos && request.find("\"max_tokens\"") == std::string::npos,"OpenAI format switch or modern token field failed");
        }
        const std::string anthropic_success = R"({"stop_reason":"end_turn","content":[{"type":"thinking","thinking":"PRIVATE_REASONING"},{"type":"text","text":"feat: anthropic\n\n"},{"type":"text","text":"Describe changes."}]})";
        ai.format = "Anthropic"; ai.provider = "Qwen"; // Format must override provider-specific OpenAI extensions.
        for (const auto* suffix : {"", "/", "/messages", "/messages/", "/chat/completions"}) {
            Server server(anthropic_success); ai.base_url = server.url + suffix;
            auto message = eg::generate_commit_message(git,ai);
            require(message.summary == "feat: anthropic" && message.description == "Describe changes.","Anthropic text blocks or reasoning filtering failed");
            auto wire = server.request.get();
            require(wire.rfind("POST /v1/messages HTTP/1.1",0) == 0 && wire.find("x-api-key: test-secret") != std::string::npos && wire.find("anthropic-version: 2023-06-01") != std::string::npos && wire.find("Authorization:") == std::string::npos,"Anthropic endpoint or auth is wrong");
            require(wire.find("UNSTAGED_PRIVATE") == std::string::npos && wire.find("STAGED_ONLY") != std::string::npos,"Anthropic received unstaged contents");
            auto* json = json_tokener_parse(wire.substr(wire.find("\r\n\r\n")+4).c_str()); require(json,"Invalid Anthropic request JSON");
            auto* messages = json_object_object_get(json,"messages");
            require(json_object_is_type(json_object_object_get(json,"system"),json_type_string) && json_object_array_length(messages) == 1 && std::string(json_object_get_string(json_object_object_get(json_object_array_get_idx(messages,0),"role"))) == "user","Anthropic system prompt was sent as a message");
            require(json_object_get_int(json_object_object_get(json,"max_tokens")) == 2048 && !json_object_object_get(json,"max_completion_tokens") && !json_object_object_get(json,"enable_thinking") && !json_object_object_get(json,"thinking"),"OpenAI-only fields leaked into Anthropic request");
            json_object_put(json);
        }
        for (const auto* path : {"", "/anthropic", "/gateway/messages"}) {
            Server server(anthropic_success); ai.base_url = server.url.substr(0,server.url.size()-3) + path;
            eg::generate_commit_message(git,ai);
            std::string target = std::string(path) == "/gateway/messages" ? path : std::string(path) + "/v1/messages";
            require(server.request.get().rfind("POST " + target + " HTTP/1.1",0) == 0,"Anthropic base/full path normalization failed");
        }
        for (const auto* reason : {"max_tokens", "model_context_window_exceeded", "tool_use", "pause_turn", "refusal"}) {
            Server server(std::string(R"({"content":[{"type":"text","text":"partial"}],"stop_reason":")") + reason + "\"}"); ai.base_url = server.url;
            rejects([&] { eg::generate_commit_message(git,ai); },"Incomplete Anthropic message accepted"); server.request.get();
        }
        for (const auto* response : {R"({"stop_reason":"end_turn","content":[]})",R"({"stop_reason":"end_turn","content":"invalid"})",R"({"content":[{"type":"text","text":"unfinished"}]})",R"({"stop_reason":"end_turn","content":[{"type":"thinking","thinking":"private"}]})"}) {
            Server server(response); ai.base_url = server.url;
            rejects([&] { eg::generate_commit_message(git,ai); },"Malformed or empty Anthropic message accepted"); server.request.get();
        }
        ai.format = "OpenAI"; ai.provider = "Test model"; ai.token_parameter = "max_tokens";
        for (const auto& response : {std::string("not JSON"),std::string(R"({"choices":[]})"),std::string(R"({"choices":[{"finish_reason":"length","message":{"content":"partial"}}]})")}) {
            Server server(response); ai.base_url = server.url;
            rejects([&] { eg::generate_commit_message(git,ai); },"Malformed or truncated AI response accepted"); server.request.get();
        }
        {
            Server server("test-secret should not be shown",401); ai.base_url = server.url;
            try { eg::generate_commit_message(git,ai); require(false,"HTTP failure accepted"); }
            catch (const std::exception& e) { require(std::string(e.what()).find("401") != std::string::npos && std::string(e.what()).find("test-secret") == std::string::npos,"HTTP error leaked secret or hid status"); }
            server.request.get();
        }
        {
            Server server(success,200,[&] { git.checked({"add","code.txt"}); }); ai.base_url = server.url;
            rejects([&] { eg::generate_commit_message(git,ai); },"AI result accepted after staged changes changed"); server.request.get();
        }
        {
            auto stop = std::make_shared<std::atomic_bool>(false);
            Server server(success,200,[stop] { *stop = true; }); ai.base_url = server.url;
            rejects([&] { eg::generate_commit_message(git,ai,stop); },"Cancelled request returned a commit message"); server.request.get();
        }
        ai.base_url = "http://example.com/v1";
        rejects([&] { eg::validate_ai(ai); },"Remote cleartext endpoint accepted");
        ai.base_url = "https://example.com/v1?query=bad";
        rejects([&] { eg::validate_ai(ai); },"Malformed base URL accepted");
        ai.base_url = "http://127.0.0.1:1/v1"; ai.api_key = "bad\r\nInjected: header";
        rejects([&] { eg::validate_ai(ai); },"Header injection accepted");
        ai.api_key.clear(); ai.max_diff_bytes = 1024;
        std::ofstream(root / "code.txt") << std::string(2048,'x'); git.checked({"add","code.txt"});
        rejects([&] { eg::generate_commit_message(git,ai); },"Oversized diff was sent");
        std::cout << "PASS: atomic settings/0600, provider switching/restart/legacy migration, real HTTP JSON/auth, staged-only payload, error handling and stale index protection\n";
        fs::remove_all(root); return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << "\nFixture: " << root << '\n'; return 1; }
}
