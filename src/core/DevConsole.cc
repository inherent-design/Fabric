#include "fabric/core/DevConsole.hh"

#include "fabric/core/Log.hh"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace fabric {

void DevConsole::init(Rml::Context* rmlContext) {
    if (!rmlContext) {
        FABRIC_LOG_ERROR("DevConsole::init called with null context");
        return;
    }

    context_ = rmlContext;
    registerBuiltins();

    document_ = context_->LoadDocument("assets/ui/console.rml");
    if (document_) {
        document_->Hide();
        FABRIC_LOG_INFO("DevConsole document loaded");
    } else {
        FABRIC_LOG_WARN("DevConsole: failed to load console.rml");
    }

    initialized_ = true;
    print("Developer console initialized. Type 'help' for commands.");
}

void DevConsole::shutdown() {
    if (document_) {
        document_->Close();
        document_ = nullptr;
    }

    commands_.clear();
    cvars_.clear();
    output_.clear();

    initialized_ = false;
    visible_ = false;
    context_ = nullptr;
}

bool DevConsole::isValid() const {
    return initialized_;
}

void DevConsole::toggle() {
    if (visible_) {
        hide();
    } else {
        show();
    }
}

void DevConsole::show() {
    visible_ = true;
    if (document_) {
        document_->Show();
    }
}

void DevConsole::hide() {
    visible_ = false;
    if (document_) {
        document_->Hide();
    }
}

bool DevConsole::isVisible() const {
    return visible_;
}

void DevConsole::bind(const std::string& name, CommandCallback callback) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    commands_[lower] = std::move(callback);
}

void DevConsole::unbind(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    commands_.erase(lower);
}

void DevConsole::execute(const std::string& input) {
    auto tokens = tokenize(input);
    if (tokens.empty()) {
        return;
    }

    // Echo the input
    print("> " + input);

    // Lowercase the command name for case-insensitive lookup
    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) { return std::tolower(c); });

    std::vector<std::string> args(tokens.begin() + 1, tokens.end());

    auto it = commands_.find(cmd);
    if (it != commands_.end()) {
        it->second(args);
    } else {
        print("Unknown command: " + tokens[0]);
    }
}

void DevConsole::print(const std::string& message) {
    output_.push_back(message);
    while (output_.size() > kMaxOutputLines) {
        output_.pop_front();
    }
}

void DevConsole::clear() {
    output_.clear();
}

const std::deque<std::string>& DevConsole::output() const {
    return output_;
}

void DevConsole::registerCVar(const std::string& name, int* ref) {
    cvars_[name] = ref;
}

void DevConsole::registerCVar(const std::string& name, float* ref) {
    cvars_[name] = ref;
}

void DevConsole::registerCVar(const std::string& name, bool* ref) {
    cvars_[name] = ref;
}

void DevConsole::registerCVar(const std::string& name, std::string* ref) {
    cvars_[name] = ref;
}

void DevConsole::unregisterCVar(const std::string& name) {
    cvars_.erase(name);
}

void DevConsole::setQuitCallback(std::function<void()> cb) {
    quitCallback_ = std::move(cb);
}

void DevConsole::registerBuiltins() {
    bind("help", [this](const auto& args) { cmdHelp(args); });
    bind("set", [this](const auto& args) { cmdSet(args); });
    bind("get", [this](const auto& args) { cmdGet(args); });
    bind("clear", [this](const auto& args) { cmdClear(args); });
    bind("quit", [this](const auto& args) { cmdQuit(args); });
}

std::vector<std::string> DevConsole::tokenize(const std::string& input) const {
    std::vector<std::string> tokens;
    std::istringstream stream(input);
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void DevConsole::cmdHelp(const std::vector<std::string>& /*args*/) {
    print("Available commands:");
    std::vector<std::string> names;
    names.reserve(commands_.size());
    for (const auto& [name, _] : commands_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    for (const auto& name : names) {
        print("  " + name);
    }
}

void DevConsole::cmdSet(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        print("Usage: set <cvar> <value>");
        return;
    }

    auto it = cvars_.find(args[0]);
    if (it == cvars_.end()) {
        print("Unknown cvar: " + args[0]);
        return;
    }

    const std::string& valueStr = args[1];

    std::visit(
        [&](auto* ref) {
            using T = std::remove_pointer_t<decltype(ref)>;
            if constexpr (std::is_same_v<T, int>) {
                try {
                    *ref = std::stoi(valueStr);
                    print(args[0] + " = " + std::to_string(*ref));
                } catch (...) {
                    print("Invalid integer value: " + valueStr);
                }
            } else if constexpr (std::is_same_v<T, float>) {
                try {
                    *ref = std::stof(valueStr);
                    print(args[0] + " = " + std::to_string(*ref));
                } catch (...) {
                    print("Invalid float value: " + valueStr);
                }
            } else if constexpr (std::is_same_v<T, bool>) {
                if (valueStr == "true" || valueStr == "1") {
                    *ref = true;
                    print(args[0] + " = true");
                } else if (valueStr == "false" || valueStr == "0") {
                    *ref = false;
                    print(args[0] + " = false");
                } else {
                    print("Invalid bool value: " + valueStr + " (use true/false/0/1)");
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                *ref = valueStr;
                print(args[0] + " = " + *ref);
            }
        },
        it->second);
}

void DevConsole::cmdGet(const std::vector<std::string>& args) {
    if (args.empty()) {
        print("Usage: get <cvar>");
        return;
    }

    auto it = cvars_.find(args[0]);
    if (it == cvars_.end()) {
        print("Unknown cvar: " + args[0]);
        return;
    }

    std::visit(
        [&](auto* ref) {
            using T = std::remove_pointer_t<decltype(ref)>;
            if constexpr (std::is_same_v<T, int>) {
                print(args[0] + " = " + std::to_string(*ref));
            } else if constexpr (std::is_same_v<T, float>) {
                print(args[0] + " = " + std::to_string(*ref));
            } else if constexpr (std::is_same_v<T, bool>) {
                print(args[0] + " = " + (*ref ? "true" : "false"));
            } else if constexpr (std::is_same_v<T, std::string>) {
                print(args[0] + " = " + *ref);
            }
        },
        it->second);
}

void DevConsole::cmdClear(const std::vector<std::string>& /*args*/) {
    clear();
}

void DevConsole::cmdQuit(const std::vector<std::string>& /*args*/) {
    print("Requesting engine shutdown...");
    if (quitCallback_) {
        quitCallback_();
    }
}

} // namespace fabric
