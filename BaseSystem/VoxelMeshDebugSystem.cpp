#pragma once

#include "Host/PlatformInput.h"
#include <chrono>
#include <algorithm>
#include <iostream>
#include <vector>

namespace RenderInitSystemLogic {
    int getRegistryInt(const BaseSystem& baseSystem, const std::string& key, int fallback);
    bool getRegistryBool(const BaseSystem& baseSystem, const std::string& key, bool fallback);
}
namespace VoxelMeshingSystemLogic {
}
namespace TerrainSystemLogic {
    void GetVoxelStreamingPerfStats(size_t& pending,
                                    size_t& desired,
                                    size_t& generated,
                                    size_t& jobs,
                                    int& stepped,
                                    int& built,
                                    int& consumed,
                                    int& skippedExisting,
                                    int& filteredOut,
                                    int& rescueSurfaceQueued,
                                    int& rescueMissingQueued,
                                    int& droppedByCap,
                                    int& reprioritized,
                                    float& prepMs,
                                    float& generationMs);
}
namespace TreeGenerationSystemLogic {
    void GetTreeFoliagePerfStats(size_t& pendingSections,
                                 size_t& pendingDependencies,
                                 size_t& backfillVisited,
                                 int& selectedSections,
                                 int& processedSections,
                                 int& deferredByTimeBudget,
                                 int& backfillAppended,
                                 bool& backfillRan,
                                 float& updateMs);
}

namespace VoxelMeshDebugSystemLogic {
    void UpdateVoxelMeshDebug(BaseSystem& baseSystem, std::vector<Entity>&, float, PlatformWindowHandle) {
        if (!baseSystem.voxelWorld) return;
        VoxelWorldContext& voxelWorld = *baseSystem.voxelWorld;

        static auto lastPerfLog = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        const bool voxelPerfLogEnabled = ::RenderInitSystemLogic::getRegistryBool(
            baseSystem, "voxelPerfLogEnabled", true
        );
        const int voxelPerfLogIntervalMs = std::max(
            250,
            ::RenderInitSystemLogic::getRegistryInt(baseSystem, "voxelPerfLogIntervalMs", 1000)
        );
        if (voxelPerfLogEnabled && now - lastPerfLog >= std::chrono::milliseconds(voxelPerfLogIntervalMs)) {
            size_t voxelRenderDirtyCount = baseSystem.voxelRender ? baseSystem.voxelRender->renderBuffersDirty.size() : 0;
            size_t terrainPending = 0, terrainDesired = 0, terrainGenerated = 0, terrainJobs = 0;
            int terrainStepped = 0;
            int terrainBuilt = 0, terrainConsumed = 0, terrainSkipped = 0, terrainFiltered = 0;
            int terrainRescueSurface = 0, terrainRescueMissing = 0, terrainCapDrop = 0, terrainReprio = 0;
            float terrainPrepMs = 0.0f;
            float terrainMs = 0.0f;
            TerrainSystemLogic::GetVoxelStreamingPerfStats(
                terrainPending,
                terrainDesired,
                terrainGenerated,
                terrainJobs,
                terrainStepped,
                terrainBuilt,
                terrainConsumed,
                terrainSkipped,
                terrainFiltered,
                terrainRescueSurface,
                terrainRescueMissing,
                terrainCapDrop,
                terrainReprio,
                terrainPrepMs,
                terrainMs
            );
            size_t treePending = 0, treePendingDeps = 0, treeVisited = 0;
            int treeSelected = 0, treeProcessed = 0, treeDeferred = 0, treeBackfillAppended = 0;
            bool treeBackfillRan = false;
            float treeMs = 0.0f;
            TreeGenerationSystemLogic::GetTreeFoliagePerfStats(
                treePending,
                treePendingDeps,
                treeVisited,
                treeSelected,
                treeProcessed,
                treeDeferred,
                treeBackfillAppended,
                treeBackfillRan,
                treeMs
            );
            std::cout << "[VoxelPerf] dirty=" << voxelWorld.dirtySections.size()
                      << " voxelRenderDirty=" << voxelRenderDirtyCount
                      << " terrainPending=" << terrainPending
                      << " terrainDesired=" << terrainDesired
                      << " terrainGenerated=" << terrainGenerated
                      << " terrainJobs=" << terrainJobs
                      << " terrainStepped=" << terrainStepped
                      << " terrainBuilt=" << terrainBuilt
                      << " terrainConsumed=" << terrainConsumed
                      << " terrainSkipped=" << terrainSkipped
                      << " terrainFiltered=" << terrainFiltered
                      << " terrainRescue=" << terrainRescueSurface << "/" << terrainRescueMissing
                      << " terrainCapDrop=" << terrainCapDrop
                      << " terrainReprio=" << terrainReprio
                      << " terrainPrepMs=" << terrainPrepMs
                      << " terrainMs=" << terrainMs
                      << " treePending=" << treePending
                      << " treePendingDeps=" << treePendingDeps
                      << " treeVisited=" << treeVisited
                      << " treeSelected=" << treeSelected
                      << " treeProcessed=" << treeProcessed
                      << " treeDeferred=" << treeDeferred
                      << " treeBackfill=" << (treeBackfillRan ? treeBackfillAppended : 0)
                      << " treeMs=" << treeMs
                      << " sections=" << voxelWorld.sections.size()
                      << " renderMeshes=" << (baseSystem.voxelRender ? baseSystem.voxelRender->renderBuffers.size() : 0);
            std::cout << std::endl;
            lastPerfLog = now;
        }

        if (::RenderInitSystemLogic::getRegistryBool(baseSystem, "DebugVoxelMesh", false)) {
            static auto lastDebugLog = std::chrono::steady_clock::now();
            auto nowDbg = std::chrono::steady_clock::now();
            if (nowDbg - lastDebugLog >= std::chrono::seconds(1)) {
                size_t worldDirty = baseSystem.voxelWorld ? baseSystem.voxelWorld->dirtySections.size() : 0;
                size_t renderDirty = baseSystem.voxelRender ? baseSystem.voxelRender->renderBuffersDirty.size() : 0;
                std::cout << "[DebugVoxelMesh] worldDirty=" << worldDirty
                          << " renderDirty=" << renderDirty
                          << std::endl;
                lastDebugLog = nowDbg;
            }
        }
    }
}
