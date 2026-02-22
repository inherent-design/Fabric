#pragma once

#include "fabric/parser/SyntaxTree.hh"
#include "fabric/core/Types.hh"
#include <map>
#include <optional>

namespace fabric {
// ArgumentParser
class ArgumentParser {
public:
  ArgumentParser() = default;

  std::string getErrorMsg() const { return errorMsg; }
  bool isValid() const { return valid; }

  const TokenMap &getArguments() const;
  const OptionalToken getArgument(const std::string &name) const;

  // Add a command-line argument definition
  void addArgument(const std::string &name, const std::string &description,
                   bool required = false);

  // Check if an argument was provided in the command line
  bool hasArgument(const std::string &name) const;

  void parse(int argc, char *argv[]);
  void parse(const std::string &args);

  bool validateArgs(const TokenTypeOptionsMap &options);

private:
  TokenMap arguments;
  TokenTypeOptionsMap availableArgs;
  StringStringMap argumentDescriptions;

  std::string errorMsg;
  bool valid = true;

  // Allow ArgumentParserBuilder to access private members
  friend class ArgumentParserBuilder;
};

// ArgumentParserBuilder
//
// Builder pattern for ArgumentParser
class ArgumentParserBuilder {
public:
  const TokenTypeOptionsMap &getOptions() const {
    return options;
  }

  ArgumentParserBuilder &addOption(const std::string &name, TokenType type,
                                   bool optional = false);
  ArgumentParser build() const;

private:
  TokenTypeOptionsMap options;
};
} // namespace fabric
