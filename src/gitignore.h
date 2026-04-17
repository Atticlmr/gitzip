#pragma once

#include <filesystem>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

class GitIgnoreMatcher {
public:
    static GitIgnoreMatcher fromFile(const std::filesystem::path& ignoreFile);

    bool isIgnored(const std::filesystem::path& relativePath, bool isDirectory) const;
    std::optional<bool> matchDecision(
        const std::filesystem::path& relativePath,
        bool isDirectory) const;

private:
    struct Rule {
        bool negated = false;
        bool directoryOnly = false;
        std::regex regex;
    };

    explicit GitIgnoreMatcher(std::vector<Rule> rules);

    static std::string trim(const std::string& text);
    static std::string normalizePattern(std::string pattern);
    static std::string buildRegexPattern(const std::string& pattern, bool anchored);
    static std::string escapeRegexChar(char ch);
    static std::vector<std::pair<std::string, bool>> buildStates(
        const std::filesystem::path& relativePath,
        bool isDirectory);

    std::vector<Rule> rules_;
};
