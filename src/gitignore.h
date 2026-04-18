// Copyright (C) 2026 gitzip contributors
//
// This file is part of gitzip.
//
// gitzip is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// gitzip is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with gitzip.  If not, see <https://www.gnu.org/licenses/>.

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
