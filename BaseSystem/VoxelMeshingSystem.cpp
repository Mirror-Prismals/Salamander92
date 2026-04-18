#pragma once

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace VoxelMeshInitSystemLogic {
    int FloorDivInt(int value, int divisor);
    int SectionSizeForSection(const VoxelWorldContext& voxelWorld);
}
namespace RenderInitSystemLogic {
    bool getRegistryBool(const BaseSystem& baseSystem, const std::string& key, bool fallback);
    float getRegistryFloat(const BaseSystem& baseSystem, const std::string& key, float fallback);
    bool shouldRenderVoxelSection(const BaseSystem& baseSystem,
                                  const VoxelSection& section,
                                  const glm::vec3& cameraPos);
}
namespace VoxelMeshUploadSystemLogic {
    std::vector<VoxelMeshingPrototypeTraits> BuildVoxelMeshingPrototypeTraits(const BaseSystem& baseSystem,
                                                                              const std::vector<Entity>& prototypes);
    bool PrepareVoxelSectionMesh(const VoxelMeshingSnapshot& snapshot,
                                 const std::vector<VoxelMeshingPrototypeTraits>& prototypeTraits,
                                 PreparedVoxelSectionMesh& outMesh);
}

namespace VoxelMeshingSystemLogic {
    namespace {
        struct MeshingJob {
            VoxelMeshingSnapshot snapshot;
            std::shared_ptr<const std::vector<VoxelMeshingPrototypeTraits>> prototypeTraits;
            uint64_t epoch = 0;
        };

        struct MeshingResult {
            VoxelSectionKey key{};
            PreparedVoxelSectionMesh mesh;
            uint64_t epoch = 0;
        };

        struct MeshingRuntime {
            std::mutex mutex;
            std::condition_variable cv;
            std::vector<std::thread> workers;
            std::deque<MeshingJob> pendingJobs;
            std::deque<MeshingResult> completedJobs;
            std::unordered_map<VoxelSectionKey, uint64_t, VoxelSectionKeyHash> requestedTickets;
            std::shared_ptr<const std::vector<VoxelMeshingPrototypeTraits>> prototypeTraits;
            size_t prototypeCount = 0;
            uint64_t epoch = 1;
            size_t activeJobs = 0;
            bool stopRequested = false;
            bool workersStarted = false;
        };

        MeshingRuntime g_meshingRuntime;

        constexpr size_t kMeshingWorkerCount = 2;
        constexpr size_t kMaxOutstandingJobs = 4;
        constexpr size_t kMaxNewSnapshotsPerFrame = 2;

        int sectionVoxelIndex(int x, int y, int z, int size) {
            return x + y * size + z * size * size;
        }

        int paddedSnapshotIndex(int x, int y, int z, int paddedSize) {
            return (x + 1)
                + (y + 1) * paddedSize
                + (z + 1) * paddedSize * paddedSize;
        }

        float keyDist2ToCamera(const VoxelWorldContext& voxelWorld,
                               const VoxelSectionKey& key,
                               const glm::vec3& cameraPos) {
            int size = std::max(1, VoxelMeshInitSystemLogic::SectionSizeForSection(voxelWorld));
            const float span = static_cast<float>(size);
            const glm::vec3 center(
                (static_cast<float>(key.coord.x) + 0.5f) * span,
                (static_cast<float>(key.coord.y) + 0.5f) * span,
                (static_cast<float>(key.coord.z) + 0.5f) * span
            );
            const glm::vec3 delta = center - cameraPos;
            return glm::dot(delta, delta);
        }

        void collectTouchedSections(const VoxelWorldContext& voxelWorld,
                                    const glm::ivec3& worldCell,
                                    std::unordered_set<VoxelSectionKey, VoxelSectionKeyHash>& out) {
            const int sectionSize = std::max(1, VoxelMeshInitSystemLogic::SectionSizeForSection(voxelWorld));
            const glm::ivec3 baseCoord(
                VoxelMeshInitSystemLogic::FloorDivInt(worldCell.x, sectionSize),
                VoxelMeshInitSystemLogic::FloorDivInt(worldCell.y, sectionSize),
                VoxelMeshInitSystemLogic::FloorDivInt(worldCell.z, sectionSize)
            );
            const glm::ivec3 local = worldCell - baseCoord * sectionSize;

            auto add = [&](const glm::ivec3& coord) { out.insert(VoxelSectionKey{coord}); };
            add(baseCoord);
            if (local.x == 0) add(baseCoord + glm::ivec3(-1, 0, 0));
            if (local.x == sectionSize - 1) add(baseCoord + glm::ivec3(1, 0, 0));
            if (local.y == 0) add(baseCoord + glm::ivec3(0, -1, 0));
            if (local.y == sectionSize - 1) add(baseCoord + glm::ivec3(0, 1, 0));
            if (local.z == 0) add(baseCoord + glm::ivec3(0, 0, -1));
            if (local.z == sectionSize - 1) add(baseCoord + glm::ivec3(0, 0, 1));
        }

        bool isRenderableSection(const VoxelWorldContext& voxelWorld, const VoxelSectionKey& key) {
            const VoxelChunkLifecycleState* state = voxelWorld.findChunkState(key);
            if (!state) return true;
            return state->isRenderable();
        }

        std::shared_ptr<const std::vector<VoxelMeshingPrototypeTraits>>
        ensurePrototypeTraits(const BaseSystem& baseSystem, const std::vector<Entity>& prototypes) {
            {
                std::lock_guard<std::mutex> lock(g_meshingRuntime.mutex);
                if (g_meshingRuntime.prototypeTraits
                    && g_meshingRuntime.prototypeCount == prototypes.size()) {
                    return g_meshingRuntime.prototypeTraits;
                }
            }

            auto rebuilt = std::make_shared<const std::vector<VoxelMeshingPrototypeTraits>>(
                VoxelMeshUploadSystemLogic::BuildVoxelMeshingPrototypeTraits(baseSystem, prototypes)
            );

            std::lock_guard<std::mutex> lock(g_meshingRuntime.mutex);
            if (!g_meshingRuntime.prototypeTraits
                || g_meshingRuntime.prototypeCount != prototypes.size()) {
                g_meshingRuntime.prototypeTraits = rebuilt;
                g_meshingRuntime.prototypeCount = prototypes.size();
            }
            return g_meshingRuntime.prototypeTraits;
        }

        bool captureMeshingSnapshot(const BaseSystem& baseSystem,
                                    const VoxelWorldContext& voxelWorld,
                                    const VoxelSectionKey& key,
                                    const VoxelSection& section,
                                    uint64_t dirtyTicket,
                                    VoxelMeshingSnapshot& outSnapshot) {
            if (section.size <= 0) return false;

            const int sectionSize = section.size;
            const int paddedSize = sectionSize + 2;
            const size_t paddedCount = static_cast<size_t>(paddedSize) * static_cast<size_t>(paddedSize) * static_cast<size_t>(paddedSize);
            outSnapshot = {};
            outSnapshot.sectionKey = key;
            outSnapshot.sectionBase = section.coord * sectionSize;
            outSnapshot.sectionSize = sectionSize;
            outSnapshot.nonAirCount = section.nonAirCount;
            outSnapshot.dirtyTicket = dirtyTicket;
            outSnapshot.binaryGreedyEnabled = true;
            outSnapshot.voxelLightingEnabled =
                ::RenderInitSystemLogic::getRegistryBool(baseSystem, "VoxelLightingEnabled", true);
            outSnapshot.voxelLightingStrength = glm::clamp(
                ::RenderInitSystemLogic::getRegistryFloat(baseSystem, "VoxelLightingStrength", 1.0f),
                0.0f,
                1.0f
            );
            outSnapshot.voxelLightingMinBrightness = glm::clamp(
                ::RenderInitSystemLogic::getRegistryFloat(baseSystem, "VoxelLightingMinBrightness", 0.08f),
                0.0f,
                1.0f
            );
            outSnapshot.voxelLightingGamma = glm::clamp(
                ::RenderInitSystemLogic::getRegistryFloat(baseSystem, "VoxelLightingGamma", 1.35f),
                0.25f,
                4.0f
            );
            outSnapshot.paddedIds.assign(paddedCount, 0u);
            outSnapshot.paddedColors.assign(paddedCount, 0u);
            outSnapshot.paddedCombinedLight.assign(paddedCount, static_cast<uint8_t>(0));

            if (section.nonAirCount <= 0) {
                return true;
            }

            for (int z = 0; z < sectionSize; ++z) {
                for (int y = 0; y < sectionSize; ++y) {
                    const int srcIndex = sectionVoxelIndex(0, y, z, sectionSize);
                    const int dstIndex = paddedSnapshotIndex(0, y, z, paddedSize);
                    const size_t bytes = static_cast<size_t>(sectionSize) * sizeof(uint32_t);
                    std::memcpy(outSnapshot.paddedIds.data() + static_cast<size_t>(dstIndex),
                                section.ids.data() + static_cast<size_t>(srcIndex),
                                bytes);
                    std::memcpy(outSnapshot.paddedColors.data() + static_cast<size_t>(dstIndex),
                                section.colors.data() + static_cast<size_t>(srcIndex),
                                bytes);
                    if (!outSnapshot.voxelLightingEnabled) continue;
                    for (int x = 0; x < sectionSize; ++x) {
                        const int sectionIndex = srcIndex + x;
                        uint8_t sky = voxelWorld.defaultSkyLightLevel;
                        if (static_cast<size_t>(sectionIndex) < section.skyLight.size()) {
                            sky = section.skyLight[static_cast<size_t>(sectionIndex)];
                        }
                        uint8_t block = static_cast<uint8_t>(0);
                        if (static_cast<size_t>(sectionIndex) < section.blockLight.size()) {
                            block = section.blockLight[static_cast<size_t>(sectionIndex)];
                        }
                        outSnapshot.paddedCombinedLight[static_cast<size_t>(dstIndex + x)] =
                            static_cast<uint8_t>(std::max<int>(sky, block));
                    }
                }
            }

            if (!outSnapshot.voxelLightingEnabled) {
                return true;
            }

            for (int z = -1; z <= sectionSize; ++z) {
                for (int y = -1; y <= sectionSize; ++y) {
                    for (int x = -1; x <= sectionSize; ++x) {
                        const bool border = (x < 0 || x >= sectionSize
                                          || y < 0 || y >= sectionSize
                                          || z < 0 || z >= sectionSize);
                        if (!border) continue;
                        const glm::ivec3 worldCell = outSnapshot.sectionBase + glm::ivec3(x, y, z);
                        const int dstIndex = paddedSnapshotIndex(x, y, z, paddedSize);
                        outSnapshot.paddedIds[static_cast<size_t>(dstIndex)] = voxelWorld.getBlockWorld(worldCell);
                        outSnapshot.paddedColors[static_cast<size_t>(dstIndex)] = voxelWorld.getColorWorld(worldCell);
                        outSnapshot.paddedCombinedLight[static_cast<size_t>(dstIndex)] =
                            static_cast<uint8_t>(std::max<int>(
                                voxelWorld.getSkyLightWorld(worldCell),
                                voxelWorld.getBlockLightWorld(worldCell)
                            ));
                    }
                }
            }

            return true;
        }

        void workerMain() {
            for (;;) {
                MeshingJob job;
                {
                    std::unique_lock<std::mutex> lock(g_meshingRuntime.mutex);
                    g_meshingRuntime.cv.wait(lock, [] {
                        return g_meshingRuntime.stopRequested || !g_meshingRuntime.pendingJobs.empty();
                    });
                    if (g_meshingRuntime.stopRequested && g_meshingRuntime.pendingJobs.empty()) {
                        break;
                    }
                    job = std::move(g_meshingRuntime.pendingJobs.front());
                    g_meshingRuntime.pendingJobs.pop_front();
                    const auto requestedIt = g_meshingRuntime.requestedTickets.find(job.snapshot.sectionKey);
                    if (job.epoch != g_meshingRuntime.epoch
                        || requestedIt == g_meshingRuntime.requestedTickets.end()
                        || requestedIt->second != job.snapshot.dirtyTicket) {
                        continue;
                    }
                    g_meshingRuntime.activeJobs += 1;
                }

                PreparedVoxelSectionMesh preparedMesh;
                const bool built = job.prototypeTraits
                    && VoxelMeshUploadSystemLogic::PrepareVoxelSectionMesh(
                        job.snapshot,
                        *job.prototypeTraits,
                        preparedMesh
                    );

                {
                    std::lock_guard<std::mutex> lock(g_meshingRuntime.mutex);
                    if (g_meshingRuntime.activeJobs > 0) {
                        g_meshingRuntime.activeJobs -= 1;
                    }
                    if (!built || job.epoch != g_meshingRuntime.epoch) {
                        continue;
                    }
                    const auto requestedIt = g_meshingRuntime.requestedTickets.find(job.snapshot.sectionKey);
                    if (requestedIt == g_meshingRuntime.requestedTickets.end()
                        || requestedIt->second != job.snapshot.dirtyTicket) {
                        continue;
                    }
                    MeshingResult result;
                    result.key = job.snapshot.sectionKey;
                    result.mesh = std::move(preparedMesh);
                    result.epoch = job.epoch;
                    g_meshingRuntime.completedJobs.push_back(std::move(result));
                }
            }
        }

        void ensureWorkersStarted() {
            std::lock_guard<std::mutex> lock(g_meshingRuntime.mutex);
            if (g_meshingRuntime.workersStarted) return;
            g_meshingRuntime.stopRequested = false;
            g_meshingRuntime.workers.reserve(kMeshingWorkerCount);
            for (size_t i = 0; i < kMeshingWorkerCount; ++i) {
                g_meshingRuntime.workers.emplace_back(workerMain);
            }
            g_meshingRuntime.workersStarted = true;
        }
    }

    void ResetMeshingRuntime() {
        std::vector<std::thread> workers;
        {
            std::lock_guard<std::mutex> lock(g_meshingRuntime.mutex);
            g_meshingRuntime.stopRequested = true;
            g_meshingRuntime.epoch += 1;
            g_meshingRuntime.pendingJobs.clear();
            g_meshingRuntime.completedJobs.clear();
            g_meshingRuntime.requestedTickets.clear();
            g_meshingRuntime.prototypeTraits.reset();
            g_meshingRuntime.prototypeCount = 0;
            workers = std::move(g_meshingRuntime.workers);
            g_meshingRuntime.workersStarted = false;
        }
        g_meshingRuntime.cv.notify_all();

        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        std::lock_guard<std::mutex> lock(g_meshingRuntime.mutex);
        g_meshingRuntime.stopRequested = false;
        g_meshingRuntime.activeJobs = 0;
    }

    void RequestPriorityVoxelRemesh(BaseSystem& baseSystem,
                                    std::vector<Entity>&,
                                    const glm::ivec3& worldCell) {
        if (!baseSystem.voxelWorld || !baseSystem.voxelWorld->enabled) return;
        VoxelWorldContext& voxelWorld = *baseSystem.voxelWorld;

        std::unordered_set<VoxelSectionKey, VoxelSectionKeyHash> touched;
        touched.reserve(7);
        collectTouchedSections(voxelWorld, worldCell, touched);

        for (const VoxelSectionKey& key : touched) {
            voxelWorld.markSectionDirty(key);
        }
    }

    void UpdateVoxelMeshing(BaseSystem& baseSystem,
                            std::vector<Entity>& prototypes,
                            float,
                            PlatformWindowHandle) {
        if (!baseSystem.voxelWorld || !baseSystem.voxelWorld->enabled || !baseSystem.voxelRender) return;
        VoxelWorldContext& voxelWorld = *baseSystem.voxelWorld;
        VoxelRenderContext& voxelRender = *baseSystem.voxelRender;

        ensureWorkersStarted();
        std::shared_ptr<const std::vector<VoxelMeshingPrototypeTraits>> prototypeTraits =
            ensurePrototypeTraits(baseSystem, prototypes);
        if (!prototypeTraits) return;

        std::vector<MeshingResult> completed;
        uint64_t runtimeEpoch = 0;
        {
            std::lock_guard<std::mutex> lock(g_meshingRuntime.mutex);
            runtimeEpoch = g_meshingRuntime.epoch;
            completed.reserve(g_meshingRuntime.completedJobs.size());
            while (!g_meshingRuntime.completedJobs.empty()) {
                completed.push_back(std::move(g_meshingRuntime.completedJobs.front()));
                g_meshingRuntime.completedJobs.pop_front();
            }
        }

        for (MeshingResult& result : completed) {
            {
                std::lock_guard<std::mutex> lock(g_meshingRuntime.mutex);
                auto requestedIt = g_meshingRuntime.requestedTickets.find(result.key);
                if (requestedIt != g_meshingRuntime.requestedTickets.end()
                    && requestedIt->second == result.mesh.dirtyTicket) {
                    g_meshingRuntime.requestedTickets.erase(requestedIt);
                }
            }

            if (result.epoch != runtimeEpoch) continue;

            auto sectionIt = voxelWorld.sections.find(result.key);
            if (sectionIt == voxelWorld.sections.end() || sectionIt->second.nonAirCount <= 0) {
                voxelRender.preparedMeshes.erase(result.key);
                voxelRender.renderBuffersDirty.erase(result.key);
                voxelWorld.clearSectionDirty(result.key);
                continue;
            }

            const uint64_t currentTicket = voxelWorld.getSectionDirtyTicket(result.key);
            if (currentTicket == 0 || currentTicket != result.mesh.dirtyTicket) {
                voxelRender.preparedMeshes.erase(result.key);
                continue;
            }
            if (!isRenderableSection(voxelWorld, result.key)) {
                continue;
            }

            voxelRender.preparedMeshes[result.key] = std::move(result.mesh);
            voxelRender.renderBuffersDirty.insert(result.key);
        }

        struct Candidate {
            VoxelSectionKey key;
            uint64_t dirtyTicket = 0;
            float dist2 = 0.0f;
            bool visible = false;
        };

        const glm::vec3 cameraPos = baseSystem.player
            ? baseSystem.player->cameraPosition
            : glm::vec3(0.0f);

        std::vector<VoxelSectionKey> missingSections;
        std::vector<Candidate> candidates;
        candidates.reserve(voxelWorld.dirtySections.size());
        for (const VoxelSectionKey& key : voxelWorld.dirtySections) {
            auto sectionIt = voxelWorld.sections.find(key);
            if (sectionIt == voxelWorld.sections.end() || sectionIt->second.nonAirCount <= 0) {
                missingSections.push_back(key);
                continue;
            }
            if (!isRenderableSection(voxelWorld, key)) continue;

            const uint64_t dirtyTicket = voxelWorld.getSectionDirtyTicket(key);
            if (dirtyTicket == 0) continue;

            auto preparedIt = voxelRender.preparedMeshes.find(key);
            if (preparedIt != voxelRender.preparedMeshes.end()) {
                if (preparedIt->second.dirtyTicket == dirtyTicket) {
                    if (voxelRender.renderBuffersDirty.count(key) == 0) {
                        voxelRender.renderBuffersDirty.insert(key);
                    }
                    continue;
                }
                voxelRender.preparedMeshes.erase(preparedIt);
            }

            candidates.push_back({
                key,
                dirtyTicket,
                keyDist2ToCamera(voxelWorld, key, cameraPos),
                ::RenderInitSystemLogic::shouldRenderVoxelSection(baseSystem, sectionIt->second, cameraPos)
            });
        }

        for (const VoxelSectionKey& key : missingSections) {
            voxelRender.preparedMeshes.erase(key);
            voxelRender.renderBuffersDirty.erase(key);
            voxelWorld.clearSectionDirty(key);
        }

        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            if (a.visible != b.visible) return a.visible && !b.visible;
            if (a.dist2 != b.dist2) return a.dist2 < b.dist2;
            if (a.key.coord.x != b.key.coord.x) return a.key.coord.x < b.key.coord.x;
            if (a.key.coord.y != b.key.coord.y) return a.key.coord.y < b.key.coord.y;
            return a.key.coord.z < b.key.coord.z;
        });

        size_t enqueuedThisFrame = 0;
        for (const Candidate& candidate : candidates) {
            if (enqueuedThisFrame >= kMaxNewSnapshotsPerFrame) break;

            auto sectionIt = voxelWorld.sections.find(candidate.key);
            if (sectionIt == voxelWorld.sections.end() || sectionIt->second.nonAirCount <= 0) {
                voxelRender.preparedMeshes.erase(candidate.key);
                voxelRender.renderBuffersDirty.erase(candidate.key);
                voxelWorld.clearSectionDirty(candidate.key);
                continue;
            }
            if (!isRenderableSection(voxelWorld, candidate.key)) continue;
            if (voxelWorld.getSectionDirtyTicket(candidate.key) != candidate.dirtyTicket) continue;

            VoxelMeshingSnapshot snapshot;
            if (!captureMeshingSnapshot(baseSystem, voxelWorld, candidate.key, sectionIt->second, candidate.dirtyTicket, snapshot)) {
                continue;
            }

            bool queued = false;
            {
                std::lock_guard<std::mutex> lock(g_meshingRuntime.mutex);
                const auto requestedIt = g_meshingRuntime.requestedTickets.find(candidate.key);
                if (requestedIt != g_meshingRuntime.requestedTickets.end()
                    && requestedIt->second == candidate.dirtyTicket) {
                    continue;
                }
                const size_t outstandingJobs = g_meshingRuntime.pendingJobs.size() + g_meshingRuntime.activeJobs;
                if (outstandingJobs >= kMaxOutstandingJobs) {
                    break;
                }
                MeshingJob job;
                job.snapshot = std::move(snapshot);
                job.prototypeTraits = prototypeTraits;
                job.epoch = g_meshingRuntime.epoch;
                g_meshingRuntime.requestedTickets[candidate.key] = candidate.dirtyTicket;
                g_meshingRuntime.pendingJobs.push_back(std::move(job));
                queued = true;
            }
            if (queued) {
                g_meshingRuntime.cv.notify_one();
                enqueuedThisFrame += 1;
            }
        }
    }
}
