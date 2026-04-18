#pragma once
#include "Host/PlatformInput.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace TerrainSystemLogic {
    bool IsSectionTerrainReady(const VoxelSectionKey& key);
}

namespace TreeSectionSchedulerSystemLogic {
    namespace {
        int readRegistryInt(const BaseSystem& baseSystem, const char* key, int fallback) {
            if (!baseSystem.registry || !key) return fallback;
            auto it = baseSystem.registry->find(key);
            if (it == baseSystem.registry->end()) return fallback;
            if (!std::holds_alternative<std::string>(it->second)) return fallback;
            try {
                return std::stoi(std::get<std::string>(it->second));
            } catch (...) {
                return fallback;
            }
        }

        float distanceSqToCamera(const glm::ivec3& coord,
                                 int sectionSize,
                                 const glm::vec3& cameraPosition) {
            const glm::vec3 center(
                (static_cast<float>(coord.x) + 0.5f) * static_cast<float>(sectionSize),
                (static_cast<float>(coord.y) + 0.5f) * static_cast<float>(sectionSize),
                (static_cast<float>(coord.z) + 0.5f) * static_cast<float>(sectionSize)
            );
            const glm::vec3 delta = center - cameraPosition;
            return glm::dot(delta, delta);
        }

        glm::ivec3 worldToSectionCoord(const glm::vec3& worldPosition, int sectionSize) {
            const float safeSize = static_cast<float>(std::max(1, sectionSize));
            return glm::ivec3(
                static_cast<int>(std::floor(worldPosition.x / safeSize)),
                static_cast<int>(std::floor(worldPosition.y / safeSize)),
                static_cast<int>(std::floor(worldPosition.z / safeSize))
            );
        }
    }

    void UpdateTreeSectionScheduler(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        (void)dt;
        (void)win;

        if (!baseSystem.player || !baseSystem.voxelWorld) return;
        VoxelWorldContext& voxelWorld = *baseSystem.voxelWorld;
        if (!voxelWorld.enabled || voxelWorld.sections.empty()) return;

        const int sectionSize = std::max(1, voxelWorld.sectionSize);
        const glm::vec3 cameraPosition = baseSystem.player->cameraPosition;
        const glm::ivec3 cameraSection = worldToSectionCoord(cameraPosition, sectionSize);

        const int queueRadiusXZ = std::max(0, readRegistryInt(baseSystem, "TreeSchedulerQueueRadiusXZ", 2));
        const int queueRadiusY = std::max(0, readRegistryInt(baseSystem, "TreeSchedulerQueueRadiusY", 1));
        const int syncRadiusXZ = std::max(0, readRegistryInt(baseSystem, "TreeSchedulerSyncRadiusXZ", 1));
        const int syncRadiusY = std::max(0, readRegistryInt(baseSystem, "TreeSchedulerSyncRadiusY", 1));
        const int syncSectionsPerFrame = std::max(0, readRegistryInt(baseSystem, "TreeSchedulerSyncSectionsPerFrame", 2));
        const int syncPassesPerSection = std::max(1, readRegistryInt(baseSystem, "TreeSchedulerSyncPassesPerSection", 1));

        std::vector<std::pair<VoxelSectionKey, float>> syncCandidates;
        if (syncSectionsPerFrame > 0) {
            syncCandidates.reserve(32);
        }

        for (const auto& [key, section] : voxelWorld.sections) {
            (void)section;
            const glm::ivec3 delta = key.coord - cameraSection;
            const int absDx = std::abs(delta.x);
            const int absDy = std::abs(delta.y);
            const int absDz = std::abs(delta.z);

            if (absDx > queueRadiusXZ || absDy > queueRadiusY || absDz > queueRadiusXZ) {
                continue;
            }
            if (!TerrainSystemLogic::IsSectionTerrainReady(key)) {
                continue;
            }
            if (TreeGenerationSystemLogic::IsSectionFoliageReady(baseSystem, key)) {
                continue;
            }

            TreeGenerationSystemLogic::RequestImmediateSectionFoliage(key);

            if (syncSectionsPerFrame <= 0) continue;
            if (absDx > syncRadiusXZ || absDy > syncRadiusY || absDz > syncRadiusXZ) {
                continue;
            }
            syncCandidates.emplace_back(key, distanceSqToCamera(key.coord, sectionSize, cameraPosition));
        }

        if (syncCandidates.empty()) return;

        std::sort(
            syncCandidates.begin(),
            syncCandidates.end(),
            [](const auto& a, const auto& b) {
                if (a.second != b.second) return a.second < b.second;
                if (a.first.coord.x != b.first.coord.x) return a.first.coord.x < b.first.coord.x;
                if (a.first.coord.y != b.first.coord.y) return a.first.coord.y < b.first.coord.y;
                return a.first.coord.z < b.first.coord.z;
            }
        );

        const int syncCount = std::min(syncSectionsPerFrame, static_cast<int>(syncCandidates.size()));
        for (int i = 0; i < syncCount; ++i) {
            const VoxelSectionKey key = syncCandidates[static_cast<size_t>(i)].first;
            if (TreeGenerationSystemLogic::IsSectionFoliageReady(baseSystem, key)) continue;
            TreeGenerationSystemLogic::ProcessSectionSurfaceFoliageNow(
                baseSystem,
                prototypes,
                key,
                syncPassesPerSection
            );
        }
    }
}
