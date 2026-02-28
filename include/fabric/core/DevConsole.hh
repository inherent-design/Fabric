#pragma once

#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Rml {
class Context;
class ElementDocument;
} // namespace Rml

namespace fabric {

class DevConsole {
  public:
    using CommandCallback = std::function<void(const std::vector<std::string>& args)>;
    using CVarRef = std::variant<int*, float*, bool*, std::string*>;

    static constexpr size_t kMaxOutputLines = 500;

    DevConsole() = default;
    ~DevConsole() = default;

    void init(Rml::Context* rmlContext);
    void shutdown();
    bool isValid() const;

    // Visibility
    void toggle();
    void show();
    void hide();
    bool isVisible() const;

    // Command registration
    void bind(const std::string& name, CommandCallback callback);
    void unbind(const std::string& name);

    // Execute a command string (whitespace-split, first token = command)
    void execute(const std::string& input);

    // Output log
    void print(const std::string& message);
    void clear();
    const std::deque<std::string>& output() const;

    // CVar system
    void registerCVar(const std::string& name, int* ref);
    void registerCVar(const std::string& name, float* ref);
    void registerCVar(const std::string& name, bool* ref);
    void registerCVar(const std::string& name, std::string* ref);
    void unregisterCVar(const std::string& name);

    // Quit callback (wired to engine shutdown request)
    void setQuitCallback(std::function<void()> cb);

  private:
    void registerBuiltins();
    std::vector<std::string> tokenize(const std::string& input) const;

    // Built-in command handlers
    void cmdHelp(const std::vector<std::string>& args);
    void cmdSet(const std::vector<std::string>& args);
    void cmdGet(const std::vector<std::string>& args);
    void cmdClear(const std::vector<std::string>& args);
    void cmdQuit(const std::vector<std::string>& args);

    bool initialized_ = false;
    bool visible_ = false;

    std::unordered_map<std::string, CommandCallback> commands_;
    std::unordered_map<std::string, CVarRef> cvars_;
    std::deque<std::string> output_;
    std::function<void()> quitCallback_;

    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
};

} // namespace fabric
