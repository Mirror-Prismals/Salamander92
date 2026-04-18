#pragma once

#include "Host/PlatformInput.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <deque>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace VoxelLightingSystemLogic {

    namespace {
        constexpr uint8_t kWaterFoliageMarkerNone = 0u;
        constexpr uint8_t kWaterFoliageMarkerSandDollarX = 4u;
        constexpr uint8_t kWaterFoliageMarkerSandDollarZ = 5u;
        constexpr uint8_t kWaterWaveClassOcean = 4u;

        std::string trimCopy(const std::string& value) {
            size_t start = 0;
            while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
                start += 1;
            }
            size_t end = value.size();
            while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
                end -= 1;
            }
            return value.substr(start, end - start);
        }

        std::string lowerCopy(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        bool getRegistryBool(const BaseSystem& baseSystem, const std::string& key, bool fallback) {
            if (!baseSystem.registry) return fallback;
            auto it = baseSystem.registry->find(key);
            if (it == baseSystem.registry->end()) return fallback;
            if (std::holds_alternative<bool>(it->second)) return std::get<bool>(it->second);
            if (std::holds_alternative<std::string>(it->second)) {
                const std::string value = lowerCopy(trimCopy(std::get<std::string>(it->second)));
                return value == "1" || value == "true" || value == "yes" || value == "on";
            }
            return fallback;
        }

        int getRegistryInt(const BaseSystem& baseSystem, const std::string& key, int fallback) {
            if (!baseSystem.registry) return fallback;
            auto it = baseSystem.registry->find(key);
            if (it == baseSystem.registry->end() || !std::holds_alternative<std::string>(it->second)) return fallback;
            try {
                return std::stoi(std::get<std::string>(it->second));
            } catch (...) {
                return fallback;
            }
        }

        float getRegistryFloat(const BaseSystem& baseSystem, const std::string& key, float fallback) {
            if (!baseSystem.registry) return fallback;
            auto it = baseSystem.registry->find(key);
            if (it == baseSystem.registry->end() || !std::holds_alternative<std::string>(it->second)) return fallback;
            try {
                return std::stof(std::get<std::string>(it->second));
            } catch (...) {
                return fallback;
            }
        }

        std::string getRegistryString(const BaseSystem& baseSystem, const std::string& key, const std::string& fallback) {
            if (!baseSystem.registry) return fallback;
            auto it = baseSystem.registry->find(key);
            if (it == baseSystem.registry->end() || !std::holds_alternative<std::string>(it->second)) return fallback;
            return std::get<std::string>(it->second);
        }

        bool blocksLightForMinecraft(const Entity& proto) {
            if (!proto.isBlock) return false;
            bool blocks = proto.isOpaque;
            const std::string name = lowerCopy(proto.name);
            if (name.find("water") != std::string::npos) blocks = false;
            if (name.find("leaf") != std::string::npos) blocks = false;
            if (name.find("grass") != std::string::npos) blocks = false;
            if (name.find("flower") != std::string::npos) blocks = false;
            if (name.find("vine") != std::string::npos) blocks = false;
            return blocks;
        }

        bool overlapsBounds(const glm::ivec3& sectionMin,
                            const glm::ivec3& sectionMax,
                            const glm::ivec3& minCoord,
                            const glm::ivec3& maxCoord) {
            return sectionMax.x > minCoord.x && sectionMin.x < maxCoord.x
                && sectionMax.y > minCoord.y && sectionMin.y < maxCoord.y
                && sectionMax.z > minCoord.z && sectionMin.z < maxCoord.z;
        }

        float currentDayFractionFromLocalClock() {
            auto now = std::chrono::system_clock::now();
            double epochSeconds = std::chrono::duration<double>(now.time_since_epoch()).count();
            std::time_t ct = static_cast<std::time_t>(std::floor(epochSeconds));
            double subSecond = epochSeconds - static_cast<double>(ct);
            std::tm lt{};
#ifdef _WIN32
            localtime_s(&lt, &ct);
#else
            localtime_r(&ct, &lt);
#endif
            double daySeconds = static_cast<double>(lt.tm_hour) * 3600.0
                              + static_cast<double>(lt.tm_min) * 60.0
                              + static_cast<double>(lt.tm_sec)
                              + subSecond;
            double dayFraction = daySeconds / 86400.0;
            dayFraction -= std::floor(dayFraction);
            return static_cast<float>(dayFraction);
        }

        uint8_t computeSkySeedLevel(float dayFraction,
                                    float moonlightStrength,
                                    float nightLevel,
                                    float dayLevel) {
            const float hour = dayFraction * 24.0f;
            const float sunU = (hour - 6.0f) / 12.0f;
            const float moonHour = (hour < 6.0f) ? (hour + 24.0f) : hour;
            const float moonU = (moonHour - 18.0f) / 12.0f;
            const float sunY = std::max(0.0f, std::sin(sunU * 3.14159265359f));
            const float moonY = std::max(0.0f, std::sin(moonU * 3.14159265359f));
            const float sky01 = std::clamp(std::max(sunY, moonY * moonlightStrength), 0.0f, 1.0f);
            const float level = nightLevel + (dayLevel - nightLevel) * sky01;
            const int rounded = static_cast<int>(std::lround(std::clamp(level, 0.0f, 15.0f)));
            return static_cast<uint8_t>(std::clamp(rounded, 0, 15));
        }

        int parseEmitterLevel(const std::string& token, int fallback) {
            try {
                return std::stoi(token);
            } catch (...) {
                return fallback;
            }
        }

        int floorDivInt(int value, int divisor) {
            if (divisor == 0) return 0;
            int q = value / divisor;
            const int r = value % divisor;
            if (r != 0 && ((r > 0) != (divisor > 0))) {
                --q;
            }
            return q;
        }

        uint8_t decodeWaterFoliageMarkerFromPackedColor(uint32_t packedColor) {
            const uint8_t encoded = static_cast<uint8_t>((packedColor >> 24) & 0xffu);
            if (encoded <= kWaterFoliageMarkerSandDollarZ) {
                return encoded;
            }
            const uint8_t marker = static_cast<uint8_t>(encoded & 0x0fu);
            const uint8_t waveClass = static_cast<uint8_t>((encoded >> 4u) & 0x0fu);
            if (marker <= kWaterFoliageMarkerSandDollarZ && waveClass <= kWaterWaveClassOcean) {
                return marker;
            }
            return kWaterFoliageMarkerNone;
        }

        std::unordered_map<int, uint8_t> parseEmitterRegistry(const std::string& spec,
                                                               const std::vector<Entity>& prototypes) {
            std::unordered_map<int, uint8_t> out;
            if (spec.empty()) return out;

            std::unordered_map<std::string, int> prototypeByName;
            prototypeByName.reserve(prototypes.size());
            for (size_t i = 0; i < prototypes.size(); ++i) {
                prototypeByName[lowerCopy(prototypes[i].name)] = static_cast<int>(i);
            }

            size_t start = 0;
            while (start < spec.size()) {
                size_t comma = spec.find(',', start);
                std::string token = trimCopy(spec.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
                if (!token.empty()) {
                    size_t colon = token.find(':');
                    std::string name = trimCopy(token.substr(0, colon));
                    std::string levelToken = (colon == std::string::npos)
                        ? std::string("15")
                        : trimCopy(token.substr(colon + 1));
                    if (!name.empty()) {
                        const std::string query = lowerCopy(name);
                        auto it = prototypeByName.find(query);
                        int level = std::clamp(parseEmitterLevel(levelToken, 15), 0, 15);
                        if (level <= 0) {
                            if (comma == std::string::npos) break;
                            start = comma + 1;
                            continue;
                        }
                        if (it != prototypeByName.end()) {
                            auto cur = out.find(it->second);
                            if (cur == out.end() || static_cast<int>(cur->second) < level) {
                                out[it->second] = static_cast<uint8_t>(level);
                            }
                        } else {
                            for (size_t i = 0; i < prototypes.size(); ++i) {
                                const std::string protoName = lowerCopy(prototypes[i].name);
                                if (protoName.find(query) == std::string::npos) continue;
                                auto cur = out.find(static_cast<int>(i));
                                if (cur == out.end() || static_cast<int>(cur->second) < level) {
                                    out[static_cast<int>(i)] = static_cast<uint8_t>(level);
                                }
                            }
                        }
                    }
                }
                if (comma == std::string::npos) break;
                start = comma + 1;
            }
            return out;
        }
    }

    void UpdateVoxelLighting(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float, PlatformWindowHandle) {
        if (!baseSystem.voxelWorld) return;
        VoxelWorldContext& voxelWorld = *baseSystem.voxelWorld;

        static uint64_t s_lastAppliedFrame = 0;
        static bool s_warnedForCellBudget = false;
        static bool s_hasCachedConfig = false;
        static bool s_cachedUseSkyLight = true;
        static bool s_cachedUseBlockLight = true;
        static bool s_cachedFastSkyLight = true;
        static std::string s_cachedEmitterSpec;
        static int s_cachedSkySeedLevel = -1;
        static std::unordered_map<VoxelSectionKey, uint32_t, VoxelSectionKeyHash> s_lastLitEditVersion;
        static std::unordered_map<VoxelSectionKey, uint64_t, VoxelSectionKeyHash> s_lastLitFingerprint;
        static std::deque<VoxelSectionKey> s_patchQueue;
        static std::unordered_set<VoxelSectionKey, VoxelSectionKeyHash> s_patchQueuedSet;
        static bool s_dynamicEmitterActive = false;
        static glm::ivec3 s_dynamicEmitterCell(0);
        static uint8_t s_dynamicEmitterLevel = static_cast<uint8_t>(0);

        const bool enabled = getRegistryBool(baseSystem, "VoxelLightingEnabled", true);
        if (!enabled || !voxelWorld.enabled) return;

        const bool useSkyLight = getRegistryBool(baseSystem, "VoxelLightingUseSkyLight", true);
        const bool useBlockLightRequested = getRegistryBool(baseSystem, "VoxelLightingUseBlockLight", true);
        bool useBlockLight = useBlockLightRequested;
        const bool fastSkyLight = getRegistryBool(baseSystem, "VoxelLightingFastSky", true);
        const std::string emitterSpec = getRegistryString(baseSystem, "VoxelLightingEmitters", "LavaBlockTex:15,DepthLavaTile:15,LanternBlockTex:14");
        const uint8_t sandDollarMarkerLightLevel = static_cast<uint8_t>(std::clamp(
            getRegistryInt(baseSystem, "DepthSandDollarLightLevel", 14),
            0,
            15
        ));
        const float moonlightStrength = std::clamp(
            getRegistryFloat(baseSystem, "VoxelLightingMoonlightStrength", 0.10f),
            0.0f,
            1.0f
        );
        const float skyDayLevel = std::clamp(
            getRegistryFloat(baseSystem, "VoxelLightingSkyDayLevel", 15.0f),
            0.0f,
            15.0f
        );
        const float skyNightLevel = std::clamp(
            getRegistryFloat(baseSystem, "VoxelLightingSkyNightLevel", 0.0f),
            0.0f,
            skyDayLevel
        );
        const float dayFraction = currentDayFractionFromLocalClock();
        const uint8_t skySeedLevel = useSkyLight
            ? computeSkySeedLevel(dayFraction, moonlightStrength, skyNightLevel, skyDayLevel)
            : static_cast<uint8_t>(0);
        voxelWorld.defaultSkyLightLevel = skySeedLevel;
        if (baseSystem.registry) {
            (*baseSystem.registry)["VoxelLightingCurrentSkyLevel"] = std::to_string(static_cast<int>(skySeedLevel));
        }

        const bool bootstrapEnabled = getRegistryBool(baseSystem, "VoxelLightingBootstrapEnabled", true);
        const int bootstrapMinSections = std::max(
            1,
            getRegistryInt(baseSystem, "VoxelLightingBootstrapMinSections", 32)
        );
        const float bootstrapMeshCoverageTarget = std::clamp(
            getRegistryFloat(baseSystem, "VoxelLightingBootstrapMeshCoverageTarget", 0.60f),
            0.05f,
            1.0f
        );
        const size_t sectionCount = voxelWorld.sections.size();
        const size_t meshCount = baseSystem.voxelRender ? baseSystem.voxelRender->renderBuffers.size() : 0;
        const bool bootstrapActive = bootstrapEnabled
            && sectionCount >= static_cast<size_t>(bootstrapMinSections)
            && static_cast<double>(meshCount) < static_cast<double>(sectionCount) * static_cast<double>(bootstrapMeshCoverageTarget);
        const bool deferBlockLightInBootstrap = getRegistryBool(
            baseSystem,
            "VoxelLightingBootstrapDeferBlockLight",
            true
        );
        if (bootstrapActive && deferBlockLightInBootstrap) {
            useBlockLight = false;
        }

        bool configChanged = false;
        if (!s_hasCachedConfig) {
            s_cachedUseSkyLight = useSkyLight;
            s_cachedUseBlockLight = useBlockLight;
            s_cachedFastSkyLight = fastSkyLight;
            s_cachedEmitterSpec = emitterSpec;
            s_hasCachedConfig = true;
        } else if (s_cachedUseSkyLight != useSkyLight
            || s_cachedUseBlockLight != useBlockLight
            || s_cachedFastSkyLight != fastSkyLight
            || s_cachedEmitterSpec != emitterSpec) {
            configChanged = true;
            s_cachedUseSkyLight = useSkyLight;
            s_cachedUseBlockLight = useBlockLight;
            s_cachedFastSkyLight = fastSkyLight;
            s_cachedEmitterSpec = emitterSpec;
        }

        bool skySeedChanged = false;
        if (s_cachedSkySeedLevel < 0) {
            s_cachedSkySeedLevel = static_cast<int>(skySeedLevel);
        } else if (s_cachedSkySeedLevel != static_cast<int>(skySeedLevel)) {
            skySeedChanged = true;
            s_cachedSkySeedLevel = static_cast<int>(skySeedLevel);
        }

        const bool refreshAllOnConfigChange = getRegistryBool(
            baseSystem,
            "VoxelLightingRefreshAllOnConfigChange",
            true
        );
        const bool refreshAllOnSkySeedChange = getRegistryBool(
            baseSystem,
            "VoxelLightingRefreshAllOnSkySeedChange",
            true
        );
        const bool refreshAllRequested =
            (configChanged && refreshAllOnConfigChange)
            || (useSkyLight && skySeedChanged && refreshAllOnSkySeedChange);

        const bool forceFullRebuild = getRegistryBool(baseSystem, "VoxelLightingForceFullRebuild", false)
            || (configChanged && getRegistryBool(baseSystem, "VoxelLightingForceFullRebuildOnConfigChange", false))
            || ((useSkyLight && skySeedChanged)
                && getRegistryBool(baseSystem, "VoxelLightingForceFullRebuildOnSkySeedChange", false));

        std::vector<uint8_t> blocksLightById(prototypes.size(), 0u);
        std::vector<uint8_t> waterLikeById(prototypes.size(), 0u);
        for (size_t i = 0; i < prototypes.size(); ++i) {
            blocksLightById[i] = blocksLightForMinecraft(prototypes[i]) ? static_cast<uint8_t>(1u) : static_cast<uint8_t>(0u);
            const std::string name = lowerCopy(prototypes[i].name);
            if (name.find("water") != std::string::npos) {
                waterLikeById[i] = static_cast<uint8_t>(1u);
            }
        }

        const std::unordered_map<int, uint8_t> emitterLevels = useBlockLight
            ? parseEmitterRegistry(emitterSpec, prototypes)
            : std::unordered_map<int, uint8_t>{};
        std::vector<uint8_t> emitterLevelById(prototypes.size(), static_cast<uint8_t>(0u));
        bool hasAnyEmitterLevels = false;
        if (!emitterLevels.empty()) {
            for (const auto& [id, level] : emitterLevels) {
                if (id < 0 || id >= static_cast<int>(emitterLevelById.size())) continue;
                if (level > emitterLevelById[static_cast<size_t>(id)]) {
                    emitterLevelById[static_cast<size_t>(id)] = level;
                }
            }
            for (uint8_t level : emitterLevelById) {
                if (level > 0u) {
                    hasAnyEmitterLevels = true;
                    break;
                }
            }
        }

        bool dynamicEmitterActive = false;
        glm::ivec3 dynamicEmitterCell(0);
        uint8_t dynamicEmitterLevel = static_cast<uint8_t>(0);
        if (useBlockLight && baseSystem.player) {
            const PlayerContext& player = *baseSystem.player;
            if (player.isHoldingBlock
                && player.heldPrototypeID >= 0
                && player.heldPrototypeID < static_cast<int>(emitterLevelById.size())) {
                const uint8_t heldLevel = emitterLevelById[static_cast<size_t>(player.heldPrototypeID)];
                if (heldLevel > 0u) {
                    const float heldItemForward = std::clamp(
                        getRegistryFloat(baseSystem, "HeldItemViewForward", 0.8f),
                        0.2f,
                        2.5f
                    );
                    const float heldItemVertical = std::clamp(
                        getRegistryFloat(baseSystem, "HeldItemViewVertical", 0.08f),
                        -1.0f,
                        1.0f
                    );
                    glm::vec3 cameraForward(
                        std::cos(glm::radians(player.cameraYaw)) * std::cos(glm::radians(player.cameraPitch)),
                        std::sin(glm::radians(player.cameraPitch)),
                        std::sin(glm::radians(player.cameraYaw)) * std::cos(glm::radians(player.cameraPitch))
                    );
                    if (glm::dot(cameraForward, cameraForward) > 0.0f) {
                        cameraForward = glm::normalize(cameraForward);
                    }
                    const glm::vec3 heldPos = player.cameraPosition
                        + cameraForward * heldItemForward
                        + glm::vec3(0.0f, heldItemVertical, 0.0f);
                    dynamicEmitterCell = glm::ivec3(glm::round(heldPos));
                    dynamicEmitterLevel = heldLevel;
                    dynamicEmitterActive = true;
                }
            }
        }

        const bool dynamicEmitterChanged = (dynamicEmitterActive != s_dynamicEmitterActive)
            || (dynamicEmitterActive && (
                dynamicEmitterCell != s_dynamicEmitterCell
                || dynamicEmitterLevel != s_dynamicEmitterLevel));
        std::unordered_set<VoxelSectionKey, VoxelSectionKeyHash> forcedDirtySections;
        if (dynamicEmitterChanged) {
            const int sectionSize = std::max(1, voxelWorld.sectionSize);
            auto markForcedSections = [&](const glm::ivec3& worldCell, uint8_t level) {
                const int radius = std::max(16, static_cast<int>(level) + 2);
                const glm::ivec3 minCell = worldCell - glm::ivec3(radius);
                const glm::ivec3 maxCell = worldCell + glm::ivec3(radius);
                const glm::ivec3 minSection(
                    floorDivInt(minCell.x, sectionSize),
                    floorDivInt(minCell.y, sectionSize),
                    floorDivInt(minCell.z, sectionSize)
                );
                const glm::ivec3 maxSection(
                    floorDivInt(maxCell.x, sectionSize),
                    floorDivInt(maxCell.y, sectionSize),
                    floorDivInt(maxCell.z, sectionSize)
                );
                for (int sz = minSection.z; sz <= maxSection.z; ++sz) {
                    for (int sy = minSection.y; sy <= maxSection.y; ++sy) {
                        for (int sx = minSection.x; sx <= maxSection.x; ++sx) {
                            const VoxelSectionKey key{glm::ivec3(sx, sy, sz)};
                            if (voxelWorld.sections.find(key) == voxelWorld.sections.end()) continue;
                            forcedDirtySections.insert(key);
                        }
                    }
                }
            };
            if (s_dynamicEmitterActive) {
                markForcedSections(s_dynamicEmitterCell, s_dynamicEmitterLevel);
            }
            if (dynamicEmitterActive) {
                markForcedSections(dynamicEmitterCell, dynamicEmitterLevel);
            }
        }

        auto computeLightingFingerprint = [&](const VoxelSection& section) -> uint64_t {
            uint64_t h = 1469598103934665603ull;
            const size_t count = section.ids.size();
            for (size_t idx = 0; idx < count; ++idx) {
                const uint32_t id = section.ids[idx];
                if (id == 0u || id >= blocksLightById.size()) continue;
                const uint8_t blocks = blocksLightById[static_cast<size_t>(id)];
                uint8_t emit = emitterLevelById[static_cast<size_t>(id)];
                uint8_t marker = 0u;
                if (id < waterLikeById.size()
                    && waterLikeById[static_cast<size_t>(id)] != 0u
                    && idx < section.colors.size()) {
                    marker = decodeWaterFoliageMarkerFromPackedColor(section.colors[idx]);
                    if ((marker == kWaterFoliageMarkerSandDollarX
                        || marker == kWaterFoliageMarkerSandDollarZ)
                        && sandDollarMarkerLightLevel > emit) {
                        emit = sandDollarMarkerLightLevel;
                    }
                }
                if (blocks == 0u && emit == 0u && marker == 0u) continue;
                const uint64_t token =
                    (static_cast<uint64_t>(idx) << 16)
                    ^ (static_cast<uint64_t>(id) << 1)
                    ^ static_cast<uint64_t>(blocks)
                    ^ (static_cast<uint64_t>(emit) << 8)
                    ^ (static_cast<uint64_t>(marker) << 12);
                h ^= token + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            }
            h ^= static_cast<uint64_t>(section.nonAirCount) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            return h;
        };

        // Cull stale lighting-state entries as streaming adds/removes sections.
        if (!s_lastLitEditVersion.empty() || !s_lastLitFingerprint.empty()) {
            for (auto it = s_lastLitEditVersion.begin(); it != s_lastLitEditVersion.end();) {
                auto secIt = voxelWorld.sections.find(it->first);
                if (secIt == voxelWorld.sections.end()) {
                    s_lastLitFingerprint.erase(it->first);
                    it = s_lastLitEditVersion.erase(it);
                } else {
                    ++it;
                }
            }
            for (auto it = s_lastLitFingerprint.begin(); it != s_lastLitFingerprint.end();) {
                auto secIt = voxelWorld.sections.find(it->first);
                if (secIt == voxelWorld.sections.end()) {
                    it = s_lastLitFingerprint.erase(it);
                } else {
                    ++it;
                }
            }
        }

        std::vector<VoxelSectionKey> dirtySectionsNearCamera;
        dirtySectionsNearCamera.reserve(voxelWorld.dirtySections.size());
        std::unordered_set<VoxelSectionKey, VoxelSectionKeyHash> dirtySeen;
        for (const VoxelSectionKey& key : voxelWorld.dirtySections) {
            auto secIt = voxelWorld.sections.find(key);
            if (secIt == voxelWorld.sections.end()) continue;
            const VoxelSection& section = secIt->second;
            if (!forceFullRebuild && s_patchQueuedSet.find(key) != s_patchQueuedSet.end()) {
                continue;
            }
            auto litIt = s_lastLitEditVersion.find(key);
            if (litIt != s_lastLitEditVersion.end() && litIt->second == section.editVersion) {
                continue;
            }
            if (!forceFullRebuild) {
                const uint64_t fingerprint = computeLightingFingerprint(section);
                auto fpIt = s_lastLitFingerprint.find(key);
                if (fpIt != s_lastLitFingerprint.end() && fpIt->second == fingerprint) {
                    // Section changed (e.g. color-only/wind/metadata), but not in a lighting-relevant way.
                    s_lastLitEditVersion[key] = section.editVersion;
                    continue;
                }
            }
            if (dirtySeen.insert(key).second) {
                dirtySectionsNearCamera.push_back(key);
            }
        }
        const bool hasForcedDirty = !forcedDirtySections.empty();
        if (dirtySectionsNearCamera.empty() && !hasForcedDirty && !forceFullRebuild && !refreshAllRequested) return;

        const int rebuildIntervalFrames = std::max(
            1,
            getRegistryInt(
                baseSystem,
                "VoxelLightingRebuildIntervalFrames",
                bootstrapActive
                    ? getRegistryInt(baseSystem, "VoxelLightingBootstrapRebuildIntervalFrames", 8)
                    : 6
            )
        );
        if (baseSystem.frameIndex < s_lastAppliedFrame + static_cast<uint64_t>(rebuildIntervalFrames)) return;

        glm::ivec3 globalMinCoord(std::numeric_limits<int>::max());
        glm::ivec3 globalMaxCoord(std::numeric_limits<int>::lowest());
        int activeSectionCount = 0;
        for (const auto& [key, section] : voxelWorld.sections) {
            const glm::ivec3 sectionMin = section.coord * section.size;
            const glm::ivec3 sectionMax = sectionMin + glm::ivec3(section.size);
            globalMinCoord = glm::min(globalMinCoord, sectionMin);
            globalMaxCoord = glm::max(globalMaxCoord, sectionMax);
            activeSectionCount += 1;
        }
        if (activeSectionCount <= 0) {
            s_lastAppliedFrame = baseSystem.frameIndex;
            s_warnedForCellBudget = false;
            return;
        }

        int lightPaddingXZ = std::max(
            0,
            getRegistryInt(
                baseSystem,
                "VoxelLightingPaddingXZ",
                bootstrapActive
                    ? getRegistryInt(baseSystem, "VoxelLightingBootstrapPaddingXZ", 4)
                    : 6
            )
        );
        int lightPaddingY = std::max(
            0,
            getRegistryInt(
                baseSystem,
                "VoxelLightingPaddingY",
                bootstrapActive
                    ? getRegistryInt(baseSystem, "VoxelLightingBootstrapPaddingY", 6)
                    : 10
            )
        );
        const bool checkAboveBoundsOcclusionFull = getRegistryBool(
            baseSystem,
            "VoxelLightingCheckAboveBoundsOcclusionFull",
            true
        );
        const bool checkAboveBoundsOcclusionIncremental = getRegistryBool(
            baseSystem,
            "VoxelLightingCheckAboveBoundsOcclusionIncremental",
            false
        );
        const int aboveBoundsMaxYScan = std::max(
            0,
            getRegistryInt(baseSystem, "VoxelLightingAboveBoundsMaxYScan", 96)
        );
        int64_t incrementalTargetCells = static_cast<int64_t>(std::max(
            1024,
            getRegistryInt(
                baseSystem,
                "VoxelLightingIncrementalTargetCells",
                bootstrapActive
                    ? getRegistryInt(baseSystem, "VoxelLightingBootstrapIncrementalTargetCells", 280000)
                    : 350000
            )
        ));
        if (!forceFullRebuild) {
            lightPaddingXZ = std::max(
                0,
                getRegistryInt(baseSystem, "VoxelLightingIncrementalPaddingXZ", lightPaddingXZ)
            );
            lightPaddingY = std::max(
                0,
                getRegistryInt(baseSystem, "VoxelLightingIncrementalPaddingY", lightPaddingY)
            );
        }
        const int dirtySectionsPerFrame = std::max(1, getRegistryInt(baseSystem, "VoxelLightingDirtySectionsPerFrame", 1));
        const size_t forcedDirtySectionsPerFrame = static_cast<size_t>(std::max(
            0,
            getRegistryInt(baseSystem, "VoxelLightingForcedDirtySectionsPerFrame", 2)
        ));
        const int patchSectionsPerFrame = std::max(
            1,
            getRegistryInt(baseSystem, "VoxelLightingPatchSectionsPerFrame", dirtySectionsPerFrame)
        );
        const float patchMaxMsPerFrame = std::max(
            0.0f,
            getRegistryFloat(baseSystem, "VoxelLightingPatchMaxMsPerFrame", 0.0f)
        );
        const int patchTargetCellsPerMs = std::max(
            1000,
            getRegistryInt(baseSystem, "VoxelLightingPatchTargetCellsPerMs", 12000)
        );
        if (!forceFullRebuild && patchMaxMsPerFrame > 0.0f) {
            const int64_t timeBasedTargetCells = std::max<int64_t>(
                1024,
                static_cast<int64_t>(std::round(patchMaxMsPerFrame * static_cast<float>(patchTargetCellsPerMs)))
            );
            incrementalTargetCells = std::min(incrementalTargetCells, timeBasedTargetCells);
        }

        auto sectionDistance2 = [&](const VoxelSectionKey& key) -> float {
            auto it = voxelWorld.sections.find(key);
            if (it == voxelWorld.sections.end()) return std::numeric_limits<float>::max();
            if (!baseSystem.player) return 0.0f;
            const glm::vec3 cameraPos = baseSystem.player->cameraPosition;
            const VoxelSection& section = it->second;
            const float half = static_cast<float>(section.size) * 0.5f;
            const glm::vec3 center = glm::vec3(
                static_cast<float>(section.coord.x * section.size) + half,
                static_cast<float>(section.coord.y * section.size) + half,
                static_cast<float>(section.coord.z * section.size) + half
            );
            const glm::vec3 d = center - cameraPos;
            return d.x * d.x + d.y * d.y + d.z * d.z;
        };

        auto enqueuePatch = [&](const VoxelSectionKey& key, bool highPriority) {
            if (voxelWorld.sections.find(key) == voxelWorld.sections.end()) return;
            if (!s_patchQueuedSet.insert(key).second) return;
            if (highPriority) {
                s_patchQueue.push_front(key);
            } else {
                s_patchQueue.push_back(key);
            }
        };

        std::vector<VoxelSectionKey> selectedDirtySections;
        if (forceFullRebuild) {
            s_patchQueue.clear();
            s_patchQueuedSet.clear();
        } else {
            if (refreshAllRequested) {
                std::vector<VoxelSectionKey> refreshKeys;
                refreshKeys.reserve(voxelWorld.sections.size());
                for (const auto& [key, _] : voxelWorld.sections) {
                    refreshKeys.push_back(key);
                }
                if (baseSystem.player) {
                    std::sort(
                        refreshKeys.begin(),
                        refreshKeys.end(),
                        [&](const VoxelSectionKey& a, const VoxelSectionKey& b) {
                            return sectionDistance2(a) < sectionDistance2(b);
                        }
                    );
                }
                for (const VoxelSectionKey& key : refreshKeys) {
                    enqueuePatch(key, false);
                }
            }
            if (!dirtySectionsNearCamera.empty() && baseSystem.player) {
                std::sort(
                    dirtySectionsNearCamera.begin(),
                    dirtySectionsNearCamera.end(),
                    [&](const VoxelSectionKey& a, const VoxelSectionKey& b) {
                        return sectionDistance2(a) < sectionDistance2(b);
                    }
                );
            }
            for (const VoxelSectionKey& key : dirtySectionsNearCamera) {
                enqueuePatch(key, false);
            }

            if (hasForcedDirty && forcedDirtySectionsPerFrame > 0u) {
                std::vector<VoxelSectionKey> forcedDirtySections(
                    forcedDirtySections.begin(),
                    forcedDirtySections.end()
                );
                if (baseSystem.player) {
                    std::sort(
                        forcedDirtySections.begin(),
                        forcedDirtySections.end(),
                        [&](const VoxelSectionKey& a, const VoxelSectionKey& b) {
                            return sectionDistance2(a) < sectionDistance2(b);
                        }
                    );
                }
                if (forcedDirtySections.size() > forcedDirtySectionsPerFrame) {
                    forcedDirtySections.resize(forcedDirtySectionsPerFrame);
                }
                for (auto it = forcedDirtySections.rbegin(); it != forcedDirtySections.rend(); ++it) {
                    enqueuePatch(*it, true);
                }
            }

            while (!s_patchQueue.empty()
                && static_cast<int>(selectedDirtySections.size()) < patchSectionsPerFrame) {
                const VoxelSectionKey key = s_patchQueue.front();
                s_patchQueue.pop_front();
                s_patchQueuedSet.erase(key);
                if (voxelWorld.sections.find(key) == voxelWorld.sections.end()) continue;
                selectedDirtySections.push_back(key);
            }
        }

        if (!forceFullRebuild && selectedDirtySections.empty()) {
            s_lastAppliedFrame = baseSystem.frameIndex;
            return;
        }

        glm::ivec3 minCoord = globalMinCoord;
        glm::ivec3 maxCoord = globalMaxCoord;
        glm::ivec3 dirtyMin(std::numeric_limits<int>::max());
        glm::ivec3 dirtyMax(std::numeric_limits<int>::lowest());
        if (!forceFullRebuild) {
            for (const VoxelSectionKey& key : selectedDirtySections) {
                auto it = voxelWorld.sections.find(key);
                if (it == voxelWorld.sections.end()) continue;
                const VoxelSection& section = it->second;
                const glm::ivec3 sectionMin = section.coord * section.size;
                const glm::ivec3 sectionMax = sectionMin + glm::ivec3(section.size);
                dirtyMin = glm::min(dirtyMin, sectionMin);
                dirtyMax = glm::max(dirtyMax, sectionMax);
            }

        }

        auto cellCountForBounds = [](const glm::ivec3& minBounds, const glm::ivec3& maxBounds) {
            const glm::ivec3 boundsDims = maxBounds - minBounds;
            if (boundsDims.x <= 0 || boundsDims.y <= 0 || boundsDims.z <= 0) return int64_t(0);
            return static_cast<int64_t>(boundsDims.x)
                * static_cast<int64_t>(boundsDims.y)
                * static_cast<int64_t>(boundsDims.z);
        };

        const int64_t maxCells = static_cast<int64_t>(std::max(
            1024,
            getRegistryInt(
                baseSystem,
                "VoxelLightingMaxCells",
                bootstrapActive
                    ? getRegistryInt(baseSystem, "VoxelLightingBootstrapMaxCells", 2500000)
                    : 12000000
            )
        ));
        int64_t effectiveMaxCells = maxCells;
        if (!forceFullRebuild) {
            const int64_t incrementalMaxCells = static_cast<int64_t>(std::max(
                1024,
                getRegistryInt(baseSystem, "VoxelLightingIncrementalMaxCells", 1500000)
            ));
            effectiveMaxCells = std::min(effectiveMaxCells, incrementalMaxCells);
        }
        if (!forceFullRebuild) {
            const int64_t targetCells = std::min(effectiveMaxCells, incrementalTargetCells);
            int adaptivePaddingXZ = lightPaddingXZ;
            int adaptivePaddingY = lightPaddingY;

            auto applyDirtyBoundsWithPadding = [&](int padXZ, int padY) {
                minCoord = glm::ivec3(
                    std::max(globalMinCoord.x, dirtyMin.x - padXZ),
                    std::max(globalMinCoord.y, dirtyMin.y - padY),
                    std::max(globalMinCoord.z, dirtyMin.z - padXZ)
                );
                maxCoord = glm::ivec3(
                    std::min(globalMaxCoord.x, dirtyMax.x + padXZ),
                    std::min(globalMaxCoord.y, dirtyMax.y + padY),
                    std::min(globalMaxCoord.z, dirtyMax.z + padXZ)
                );
            };

            applyDirtyBoundsWithPadding(adaptivePaddingXZ, adaptivePaddingY);
            int64_t adaptiveCellCount = cellCountForBounds(minCoord, maxCoord);
            // Keep incremental lighting rebuilds bounded to avoid rare >100ms spikes.
            while (adaptiveCellCount > targetCells && (adaptivePaddingXZ > 0 || adaptivePaddingY > 0)) {
                adaptivePaddingXZ = std::max(0, adaptivePaddingXZ - 1);
                adaptivePaddingY = std::max(0, adaptivePaddingY - 1);
                applyDirtyBoundsWithPadding(adaptivePaddingXZ, adaptivePaddingY);
                adaptiveCellCount = cellCountForBounds(minCoord, maxCoord);
            }
            lightPaddingXZ = adaptivePaddingXZ;
            lightPaddingY = adaptivePaddingY;
        }

        int64_t cellCount64 = cellCountForBounds(minCoord, maxCoord);

        if (!forceFullRebuild && cellCount64 > effectiveMaxCells && !selectedDirtySections.empty()) {
            const VoxelSectionKey fallbackKey = selectedDirtySections.front();
            auto fallbackIt = voxelWorld.sections.find(fallbackKey);
            if (fallbackIt != voxelWorld.sections.end()) {
                const VoxelSection& section = fallbackIt->second;
                const glm::ivec3 sectionMin = section.coord * section.size;
                const glm::ivec3 sectionMax = sectionMin + glm::ivec3(section.size);
                minCoord = glm::ivec3(
                    std::max(globalMinCoord.x, sectionMin.x - lightPaddingXZ),
                    std::max(globalMinCoord.y, sectionMin.y - lightPaddingY),
                    std::max(globalMinCoord.z, sectionMin.z - lightPaddingXZ)
                );
                maxCoord = glm::ivec3(
                    std::min(globalMaxCoord.x, sectionMax.x + lightPaddingXZ),
                    std::min(globalMaxCoord.y, sectionMax.y + lightPaddingY),
                    std::min(globalMaxCoord.z, sectionMax.z + lightPaddingXZ)
                );
                cellCount64 = cellCountForBounds(minCoord, maxCoord);
            }
        }

        const glm::ivec3 dims = maxCoord - minCoord;
        if (dims.x <= 0 || dims.y <= 0 || dims.z <= 0 || cellCount64 <= 0 || cellCount64 > effectiveMaxCells) {
            if (!s_warnedForCellBudget) {
                std::cout << "[VoxelLighting] skip rebuild: cell budget exceeded (" << cellCount64
                          << " > " << effectiveMaxCells << ")" << std::endl;
                s_warnedForCellBudget = true;
            }
            s_lastAppliedFrame = baseSystem.frameIndex;
            return;
        }
        s_warnedForCellBudget = false;

        const size_t cellCount = static_cast<size_t>(cellCount64);
        auto indexOf = [&](int x, int y, int z) -> size_t {
            return static_cast<size_t>((x * dims.y + y) * dims.z + z);
        };

        std::vector<uint32_t> ids(cellCount, 0u);
        std::vector<uint32_t> colors(cellCount, 0u);
        std::vector<uint8_t> solid(cellCount, static_cast<uint8_t>(0u));
        std::vector<uint8_t> sky(cellCount, static_cast<uint8_t>(0u));
        std::vector<uint8_t> block(cellCount, static_cast<uint8_t>(0u));

        for (const auto& [key, section] : voxelWorld.sections) {
            const int size = section.size;
            const glm::ivec3 sectionMin = section.coord * size;
            const glm::ivec3 sectionMax = sectionMin + glm::ivec3(size);
            if (!overlapsBounds(sectionMin, sectionMax, minCoord, maxCoord)) continue;
            for (int z = 0; z < size; ++z) {
                for (int y = 0; y < size; ++y) {
                    for (int x = 0; x < size; ++x) {
                        const int srcIdx = x + y * size + z * size * size;
                        if (srcIdx < 0 || srcIdx >= static_cast<int>(section.ids.size())) continue;
                        const glm::ivec3 worldCell = sectionMin + glm::ivec3(x, y, z);
                        const int lx = worldCell.x - minCoord.x;
                        const int ly = worldCell.y - minCoord.y;
                        const int lz = worldCell.z - minCoord.z;
                        if (lx < 0 || ly < 0 || lz < 0 || lx >= dims.x || ly >= dims.y || lz >= dims.z) continue;
                        const size_t dstIdx = indexOf(lx, ly, lz);
                        const uint32_t id = section.ids[static_cast<size_t>(srcIdx)];
                        ids[dstIdx] = id;
                        if (srcIdx >= 0 && srcIdx < static_cast<int>(section.colors.size())) {
                            colors[dstIdx] = section.colors[static_cast<size_t>(srcIdx)];
                        }
                        if (id == 0u) continue;
                        if (id < blocksLightById.size()) {
                            solid[dstIdx] = blocksLightById[static_cast<size_t>(id)];
                        } else {
                            solid[dstIdx] = static_cast<uint8_t>(1u);
                        }
                    }
                }
            }
        }

        auto inBounds = [&](const glm::ivec3& p) {
            return p.x >= 0 && p.y >= 0 && p.z >= 0 && p.x < dims.x && p.y < dims.y && p.z < dims.z;
        };

        auto propagate = [&](std::vector<uint8_t>& channel, std::deque<glm::ivec3>& queue) {
            static const std::array<glm::ivec3, 6> kNeighbors = {
                glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0),
                glm::ivec3(0, 1, 0), glm::ivec3(0, -1, 0),
                glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)
            };
            while (!queue.empty()) {
                const glm::ivec3 p = queue.front();
                queue.pop_front();
                const uint8_t current = channel[indexOf(p.x, p.y, p.z)];
                if (current <= 1u) continue;
                const uint8_t nextValue = static_cast<uint8_t>(current - 1u);
                for (const glm::ivec3& delta : kNeighbors) {
                    const glm::ivec3 n = p + delta;
                    if (!inBounds(n)) continue;
                    const size_t nIdx = indexOf(n.x, n.y, n.z);
                    if (solid[nIdx] != 0u) continue;
                    if (channel[nIdx] >= nextValue) continue;
                    channel[nIdx] = nextValue;
                    queue.push_back(n);
                }
            }
        };

        auto columnBlockedAboveLocalBounds = [&](int worldX, int worldZ) -> bool {
            if (maxCoord.y >= globalMaxCoord.y) return false;
            const bool checkAboveBounds = forceFullRebuild
                ? checkAboveBoundsOcclusionFull
                : checkAboveBoundsOcclusionIncremental;
            if (!checkAboveBounds) return false;

            const int sectionSizeBase = std::max(1, voxelWorld.sectionSize);
            const int sectionX = floorDivInt(worldX, sectionSizeBase);
            const int sectionZ = floorDivInt(worldZ, sectionSizeBase);
            const int localX = worldX - sectionX * sectionSizeBase;
            const int localZ = worldZ - sectionZ * sectionSizeBase;

            const int scanMinWorldY = maxCoord.y;
            int scanMaxWorldY = globalMaxCoord.y - 1;
            if (aboveBoundsMaxYScan > 0) {
                scanMaxWorldY = std::min(scanMaxWorldY, scanMinWorldY + aboveBoundsMaxYScan - 1);
            }
            if (scanMaxWorldY < scanMinWorldY) return false;

            const int sectionMinY = floorDivInt(scanMinWorldY, sectionSizeBase);
            const int sectionMaxY = floorDivInt(scanMaxWorldY, sectionSizeBase);
            for (int sectionY = sectionMinY; sectionY <= sectionMaxY; ++sectionY) {
                const VoxelSectionKey sectionKey{glm::ivec3(sectionX, sectionY, sectionZ)};
                auto sectionIt = voxelWorld.sections.find(sectionKey);
                if (sectionIt == voxelWorld.sections.end()) continue;
                const VoxelSection& section = sectionIt->second;
                if (section.nonAirCount <= 0 || section.ids.empty()) continue;

                const int sectionWorldMinY = sectionY * sectionSizeBase;
                const int sectionWorldMaxY = sectionWorldMinY + sectionSizeBase - 1;
                const int yStart = std::max(scanMinWorldY, sectionWorldMinY);
                const int yEnd = std::min(scanMaxWorldY, sectionWorldMaxY);
                for (int worldY = yStart; worldY <= yEnd; ++worldY) {
                    const int localY = worldY - sectionWorldMinY;
                    const int idx = localX
                        + localY * sectionSizeBase
                        + localZ * sectionSizeBase * sectionSizeBase;
                    if (idx < 0 || idx >= static_cast<int>(section.ids.size())) continue;
                    const uint32_t id = section.ids[static_cast<size_t>(idx)];
                    if (id == 0u) continue;
                    const bool blocks = (id < blocksLightById.size())
                        ? (blocksLightById[static_cast<size_t>(id)] != 0u)
                        : true;
                    if (blocks) return true;
                }
            }
            return false;
        };

        if (useSkyLight) {
            if (fastSkyLight) {
                for (int x = 0; x < dims.x; ++x) {
                    for (int z = 0; z < dims.z; ++z) {
                        const int worldX = minCoord.x + x;
                        const int worldZ = minCoord.z + z;
                        bool blocked = columnBlockedAboveLocalBounds(worldX, worldZ);
                        for (int y = dims.y - 1; y >= 0; --y) {
                            const size_t idx = indexOf(x, y, z);
                            if (!blocked && solid[idx] == 0u) {
                                sky[idx] = skySeedLevel;
                            } else if (!blocked) {
                                blocked = true;
                            }
                        }
                    }
                }
            } else {
                std::deque<glm::ivec3> queue;
                for (int x = 0; x < dims.x; ++x) {
                    for (int z = 0; z < dims.z; ++z) {
                        const int worldX = minCoord.x + x;
                        const int worldZ = minCoord.z + z;
                        bool blocked = columnBlockedAboveLocalBounds(worldX, worldZ);
                        for (int y = dims.y - 1; y >= 0; --y) {
                            const size_t idx = indexOf(x, y, z);
                            if (!blocked && solid[idx] == 0u) {
                                if (sky[idx] < skySeedLevel) {
                                    sky[idx] = skySeedLevel;
                                    queue.emplace_back(x, y, z);
                                }
                            } else if (!blocked) {
                                blocked = true;
                            }
                        }
                    }
                }
                propagate(sky, queue);
            }
        }

        if (useBlockLight && (hasAnyEmitterLevels || dynamicEmitterActive)) {
            std::deque<glm::ivec3> queue;
            if (hasAnyEmitterLevels) {
                for (int x = 0; x < dims.x; ++x) {
                    for (int y = 0; y < dims.y; ++y) {
                        for (int z = 0; z < dims.z; ++z) {
                            const size_t idx = indexOf(x, y, z);
                            const uint32_t id = ids[idx];
                            if (id == 0u || id >= emitterLevelById.size()) continue;
                            uint8_t level = emitterLevelById[static_cast<size_t>(id)];
                            if (id < waterLikeById.size() && waterLikeById[static_cast<size_t>(id)] != 0u) {
                                const uint8_t marker = decodeWaterFoliageMarkerFromPackedColor(colors[idx]);
                                if (marker == kWaterFoliageMarkerSandDollarX
                                    || marker == kWaterFoliageMarkerSandDollarZ) {
                                    level = std::max(level, sandDollarMarkerLightLevel);
                                }
                            }
                            if (level == 0u) continue;
                            if (block[idx] >= level) continue;
                            block[idx] = level;
                            queue.emplace_back(x, y, z);
                        }
                    }
                }
            }
            if (dynamicEmitterActive) {
                const glm::ivec3 dynamicLocal = dynamicEmitterCell - minCoord;
                if (inBounds(dynamicLocal)) {
                    const size_t idx = indexOf(dynamicLocal.x, dynamicLocal.y, dynamicLocal.z);
                    if (block[idx] < dynamicEmitterLevel) {
                        block[idx] = dynamicEmitterLevel;
                        queue.push_back(dynamicLocal);
                    }
                }
            }
            propagate(block, queue);
        }

        int changedSections = 0;
        for (auto& [key, section] : voxelWorld.sections) {
            const int size = section.size;
            const glm::ivec3 sectionMin = section.coord * size;
            const glm::ivec3 sectionMax = sectionMin + glm::ivec3(size);
            if (!overlapsBounds(sectionMin, sectionMax, minCoord, maxCoord)) continue;

            const size_t sectionCellCount = static_cast<size_t>(size * size * size);
            if (section.skyLight.size() != sectionCellCount) {
                section.skyLight.assign(sectionCellCount, skySeedLevel);
            }
            if (section.blockLight.size() != sectionCellCount) {
                section.blockLight.assign(sectionCellCount, static_cast<uint8_t>(0));
            }

            bool sectionChanged = false;
            for (int z = 0; z < size; ++z) {
                for (int y = 0; y < size; ++y) {
                    for (int x = 0; x < size; ++x) {
                        const int localIdx = x + y * size + z * size * size;
                        const glm::ivec3 worldCell = sectionMin + glm::ivec3(x, y, z);
                        const int lx = worldCell.x - minCoord.x;
                        const int ly = worldCell.y - minCoord.y;
                        const int lz = worldCell.z - minCoord.z;
                        if (lx < 0 || ly < 0 || lz < 0 || lx >= dims.x || ly >= dims.y || lz >= dims.z) continue;
                        const size_t globalIdx = indexOf(lx, ly, lz);
                        const uint8_t nextSky = useSkyLight ? sky[globalIdx] : static_cast<uint8_t>(0);
                        const uint8_t nextBlock = useBlockLight ? block[globalIdx] : static_cast<uint8_t>(0);
                        const size_t idx = static_cast<size_t>(localIdx);
                        if (section.skyLight[idx] == nextSky && section.blockLight[idx] == nextBlock) continue;
                        section.skyLight[idx] = nextSky;
                        section.blockLight[idx] = nextBlock;
                        sectionChanged = true;
                    }
                }
            }

            if (!sectionChanged) continue;
            section.editVersion += 1;
            section.dirty = true;
            voxelWorld.markSectionDirty(key);
            changedSections += 1;
        }

        if (changedSections > 0 && getRegistryBool(baseSystem, "VoxelLightingDebug", false)) {
            std::cout << "[VoxelLighting] rebuilt " << changedSections << " section(s)." << std::endl;
        }

        if (forceFullRebuild) {
            for (const auto& [key, section] : voxelWorld.sections) {
                s_lastLitEditVersion[key] = section.editVersion;
                s_lastLitFingerprint[key] = computeLightingFingerprint(section);
            }
        } else {
            for (const VoxelSectionKey& key : selectedDirtySections) {
                auto it = voxelWorld.sections.find(key);
                if (it == voxelWorld.sections.end()) continue;
                s_lastLitEditVersion[key] = it->second.editVersion;
                s_lastLitFingerprint[key] = computeLightingFingerprint(it->second);
            }
        }

        s_lastAppliedFrame = baseSystem.frameIndex;
        s_dynamicEmitterActive = dynamicEmitterActive;
        s_dynamicEmitterCell = dynamicEmitterCell;
        s_dynamicEmitterLevel = dynamicEmitterLevel;
    }
}
