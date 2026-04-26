#include <cstdlib>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "core/config.hpp"
#include "engine/engine.hpp"

namespace fr = frankie;

namespace {

[[nodiscard]] std::vector<std::string_view> tokenize(std::string_view line) noexcept {
  auto view = line | std::views::split(' ') |
              std::views::transform([](auto &&range) { return std::string_view(range.begin(), range.end()); }) |
              std::views::filter([](std::string_view tok) { return !tok.empty(); });
  return {view.begin(), view.end()};
}

}  // namespace

int main() {
  fr::core::config config{};

  auto engine = fr::engine::engine::create(config);
  if (!engine.has_value()) {
    std::println("Failed to create a engine. Exiting...");
    return EXIT_FAILURE;
  }

  std::string cmdline;
  std::print("> ");
  while (std::getline(std::cin, cmdline)) {
    const auto tokens = tokenize(cmdline);
    if (tokens.empty()) {
      std::print("> ");
      continue;
    }

    const std::string_view command = tokens.front();
    if (command == "PUT") {
      if (tokens.size() != 3) {
        std::println("Usage: PUT <key> <value>");
      } else if (auto s = engine->put(tokens[1], tokens[2]); !s) {
        std::println("PUT failed");
      } else {
        std::println("OK");
      }
    } else if (command == "GET") {
      if (tokens.size() != 2) {
        std::println("Usage: GET <key>");
      } else if (auto value = engine->get(tokens[1]); value.has_value()) {
        std::println("{}", *value);
      } else {
        std::println("(nil)");
      }
    } else if (command == "DEL") {
      if (tokens.size() != 2) {
        std::println("Usage: DEL <key>");
      } else if (!engine->del(tokens[1])) {
        std::println("DEL failed");
      } else {
        std::println("OK");
      }
    } else if (command == "EXIT" || command == "QUIT") {
      break;
    } else {
      std::println("Unknown command: {}", command);
    }

    std::print("> ");
  }

  return 0;
}
