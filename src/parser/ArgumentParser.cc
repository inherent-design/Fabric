#include "fabric/parser/ArgumentParser.hh"
#include "fabric/core/Constants.g.hh"
#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <iostream>
#include <sstream>

namespace fabric {
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
        // Skip the program name (argv[0])
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];

            // Check if it's an option (starts with --)
            if (arg.length() >= 2 && arg.substr(0, 2) == "--") {
                std::string optionName = arg;

                // Check if the next argument is a value (not an option)
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    // For simplicity in testing, always use LiteralString for values
                    // and we'll trust the proper token type conversion elsewhere
                    Variant value = argv[i + 1];
                    arguments[optionName] = Token(TokenType::LiteralString, value);
                    i++; // Skip the value in the next iteration
                } else {
                    // Flag without value, set to true
                    Variant value = true;
                    arguments[optionName] = Token(TokenType::CLIFlag, value);
                }
            } else {
                // Handle positional arguments if needed
                // For now, store them with a special prefix
                std::string posName = "pos" + std::to_string(i);
                Variant value = arg;
                arguments[posName] = Token(TokenType::LiteralString, value);
            }
        }

        // After parsing, validate required options
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
        std::vector<std::string> argv;

        // Split the string into tokens
        while (stream >> arg) {
            argv.push_back(arg);
        }

        // Process the tokens similar to the other overload
        for (size_t i = 0; i < argv.size(); i++) {
            // Check if it's an option (starts with --)
            if (argv[i].length() >= 2 && argv[i].substr(0, 2) == "--") {
                std::string optionName = argv[i];

                // Check if the next argument is a value (not an option)
                if (i + 1 < argv.size() && argv[i + 1][0] != '-') {
                    // For simplicity in testing, always use LiteralString for values
                    Variant value = argv[i + 1];
                    arguments[optionName] = Token(TokenType::LiteralString, value);
                    i++; // Skip the value in the next iteration
                } else {
                    // Flag without value, set to true
                    Variant value = true;
                    arguments[optionName] = Token(TokenType::CLIFlag, value);
                }
            } else {
                // Handle positional arguments if needed
                std::string posName = "pos" + std::to_string(i);
                Variant value = argv[i];
                arguments[posName] = Token(TokenType::LiteralString, value);
            }
        }

        // After parsing, validate required options
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
