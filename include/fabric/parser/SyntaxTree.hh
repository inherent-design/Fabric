#pragma once

#include "fabric/parser/Token.hh"
#include <memory>
#include <vector>

namespace fabric {

// Represents a node in the abstract syntax tree
class ASTNode {
public:
  ASTNode(const Token &token);

  // Getters
  const Token &getToken() const;
  const std::vector<std::shared_ptr<ASTNode>> &getChildren() const;

  void addChild(std::shared_ptr<ASTNode> child);

private:
  Token token;
  std::vector<std::shared_ptr<ASTNode>> children;
};

// Helper functions
TokenType determineTokenType(const std::string &token);

Variant parseValue(const std::string &token, TokenType type);
} // namespace fabric
