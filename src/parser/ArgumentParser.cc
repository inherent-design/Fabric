#include "fabric/parser/ArgumentParser.hh"
#include "fabric/core/Constants.g.hh"
#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <sstream>

namespace fabric {

namespace {
void parseTokens(const std::vector<std::string>& tokens, size_t posOffset, TokenMap& arguments) {
    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i].length() >= 2 && tokens[i].substr(0, 2) == "--") {
            std::string optionName = tokens[i];

            if (i + 1 < tokens.size() && tokens[i + 1][0] != '-') {
                Variant value = tokens[i + 1];
                arguments[optionName] = Token(TokenType::LiteralString, value);
                i++;
            } else {
                Variant value = true;
                arguments[optionName] = Token(TokenType::CLIFlag, value);
            }
        } else {
            std::string posName = "pos" + std::to_string(i + posOffset);
            Variant value = tokens[i];
            arguments[posName] = Token(TokenType::LiteralString, value);
        }
    }
}
} // namespace

// Constructor is defaulted in the header, no need to define here

// Add a new command line argument
void ArgumentParser::addArgument(const std::string& name, const std::string& description, bool required) {
    // Store the argument definition
    availableArgs[name] = {TokenType::LiteralString, !required};
    argumentDescriptions[name] = description;
    FABRIC_LOG_DEBUG("Added argument: {} ({})", name, required ? "required" : "optional");
}

// Check if an argument exists in parsed arguments
bool ArgumentParser::hasArgument(const std::string& name) const {
    return arguments.find(name) != arguments.end();
}

// Get argument value
const OptionalToken ArgumentParser::getArgument(const std::string& name) const {
    auto it = arguments.find(name);
    if (it != arguments.end()) {
        return it->second;
    }
    return std::nullopt;
}

const TokenMap& ArgumentParser::getArguments() const {
    return arguments;
}

// Parse arguments using SyntaxTree
void ArgumentParser::parse(int argc, char* argv[]) {
    try {
        std::vector<std::string> tokens;
        tokens.reserve(argc - 1);
        for (int i = 1; i < argc; i++)
            tokens.emplace_back(argv[i]);

        parseTokens(tokens, 1, arguments);
        validateArgs(availableArgs);
        if (!valid)
            return;
    } catch (const std::exception& e) {
        FABRIC_LOG_ERROR("Error parsing arguments: {}", e.what());
    }
}

void ArgumentParser::parse(const std::string& args) {
    try {
        std::istringstream stream(args);
        std::string arg;
        std::vector<std::string> tokens;
        while (stream >> arg)
            tokens.push_back(arg);

        parseTokens(tokens, 0, arguments);
        validateArgs(availableArgs);
        if (!valid)
            return;
    } catch (const std::exception& e) {
        FABRIC_LOG_ERROR("Error parsing arguments: {}", e.what());
    }
}

bool ArgumentParser::validateArgs(const TokenTypeOptionsMap& options) {
    bool valid = true;
    std::vector<std::string> missingArgs;

    for (const auto& arg_pair : options) {
        const std::string& name = arg_pair.first;
        const bool optional = arg_pair.second.second;

        if (!optional && arguments.find(name) == arguments.end()) {
            valid = false;
            missingArgs.push_back(name);
        }
    }

    if (!valid) {
        std::string errorMsg = "Missing required arguments: ";
        for (size_t i = 0; i < missingArgs.size(); ++i) {
            errorMsg += missingArgs[i];
            if (i < missingArgs.size() - 1) {
                errorMsg += ", ";
            }
        }

        this->errorMsg = errorMsg;
        FABRIC_LOG_ERROR("{}", errorMsg);
    }

    this->valid = valid;
    return valid;
}

// Builder pattern implementation
ArgumentParserBuilder& ArgumentParserBuilder::addOption(const std::string& name, TokenType type, bool optional) {
    options[name] = std::make_pair(type, optional);
    return *this;
}

ArgumentParser ArgumentParserBuilder::build() const {
    ArgumentParser parser;
    parser.availableArgs = options;
    return parser;
}
} // namespace fabric
