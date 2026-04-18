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

#include "gitignore.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

GitIgnoreMatcher::GitIgnoreMatcher(std::vector<Rule> rules)
    : rules_(std::move(rules)) {}

GitIgnoreMatcher GitIgnoreMatcher::fromFile(const fs::path& ignoreFile) {
    std::vector<Rule> rules;

    std::ifstream input(ignoreFile);
    if (!input.is_open()) {
        return GitIgnoreMatcher(std::move(rules));
    }

    std::string line;
    while (std::getline(input, line)) {
        std::string pattern = trim(line);
        if (pattern.empty() || pattern.front() == '#') {
            continue;
        }

        bool negated = false;
        if (!pattern.empty() && pattern.front() == '!') {
            negated = true;
            pattern.erase(pattern.begin());
            pattern = trim(pattern);
        }

        if (pattern.empty()) {
            continue;
        }

        bool directoryOnly = false;
        if (!pattern.empty() && pattern.back() == '/') {
            directoryOnly = true;
            pattern.pop_back();
        }

        bool anchored = false;
        if (!pattern.empty() && pattern.front() == '/') {
            anchored = true;
            pattern.erase(pattern.begin());
        }

        pattern = normalizePattern(std::move(pattern));
        if (pattern.empty()) {
            continue;
        }

        rules.push_back(Rule{
            negated,
            directoryOnly,
            std::regex(buildRegexPattern(pattern, anchored))
        });
    }

    return GitIgnoreMatcher(std::move(rules));
}

bool GitIgnoreMatcher::isIgnored(const fs::path& relativePath, bool isDirectory) const {
    const auto decision = matchDecision(relativePath, isDirectory);
    return decision.has_value() && *decision;
}

std::optional<bool> GitIgnoreMatcher::matchDecision(
    const fs::path& relativePath,
    bool isDirectory) const {
    const auto states = buildStates(relativePath, isDirectory);
    std::optional<bool> ignored;

    for (const auto& rule : rules_) {
        bool matched = false;
        for (const auto& [candidate, candidateIsDir] : states) {
            if (rule.directoryOnly && !candidateIsDir) {
                continue;
            }

            if (std::regex_match(candidate, rule.regex)) {
                matched = true;
                break;
            }
        }

        if (matched) {
            ignored = !rule.negated;
        }
    }

    return ignored;
}

std::string GitIgnoreMatcher::trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string GitIgnoreMatcher::normalizePattern(std::string pattern) {
    for (char& ch : pattern) {
        if (ch == '\\') {
            ch = '/';
        }
    }

    while (!pattern.empty() && pattern.front() == '/') {
        pattern.erase(pattern.begin());
    }

    while (!pattern.empty() && pattern.back() == '/') {
        pattern.pop_back();
    }

    return pattern;
}

std::string GitIgnoreMatcher::buildRegexPattern(const std::string& pattern, bool anchored) {
    std::ostringstream regex;

    if (anchored) {
        regex << '^';
    } else {
        regex << "(^|.*/)";
    }

    for (std::size_t i = 0; i < pattern.size(); ++i) {
        const char ch = pattern[i];
        if (ch == '*') {
            const bool isDoubleStar = (i + 1 < pattern.size() && pattern[i + 1] == '*');
            if (isDoubleStar) {
                regex << ".*";
                ++i;
            } else {
                regex << "[^/]*";
            }
            continue;
        }

        if (ch == '?') {
            regex << "[^/]";
            continue;
        }

        if (ch == '/') {
            regex << '/';
            continue;
        }

        regex << escapeRegexChar(ch);
    }

    regex << '$';
    return regex.str();
}

std::string GitIgnoreMatcher::escapeRegexChar(char ch) {
    switch (ch) {
    case '.':
    case '^':
    case '$':
    case '+':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '|':
    case '\\':
        return std::string("\\") + ch;
    default:
        return std::string(1, ch);
    }
}

std::vector<std::pair<std::string, bool>> GitIgnoreMatcher::buildStates(
    const fs::path& relativePath,
    bool isDirectory) {
    std::vector<std::pair<std::string, bool>> states;
    std::string current;
    std::vector<std::string> parts;

    for (const auto& part : relativePath) {
        const std::string piece = part.generic_string();
        if (piece.empty() || piece == ".") {
            continue;
        }
        parts.push_back(piece);
    }

    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (!current.empty()) {
            current += '/';
        }
        current += parts[i];

        const bool currentIsDir = (i + 1 < parts.size()) || isDirectory;
        states.emplace_back(current, currentIsDir);
    }

    return states;
}
