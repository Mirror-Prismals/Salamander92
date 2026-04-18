#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <glm/glm.hpp>

struct VoxelSectionKey {
    glm::ivec3 coord{0};
    VoxelSectionKey() = default;
    explicit VoxelSectionKey(const glm::ivec3& c) : coord(c) {}
    bool operator==(const VoxelSectionKey& other) const noexcept {
        return coord == other.coord;
    }
};

struct VoxelSectionKeyHash {
    std::size_t operator()(const VoxelSectionKey& key) const noexcept {
        std::size_t hx = std::hash<int>()(key.coord.x);
        std::size_t hy = std::hash<int>()(key.coord.y);
        std::size_t hz = std::hash<int>()(key.coord.z);
        return (hx << 1) ^ (hy << 2) ^ (hz << 3);
    }
};

struct VoxelSection {
    int size = 0;
    glm::ivec3 coord{0};
    std::vector<uint32_t> ids;
    std::vector<uint32_t> colors;
    std::vector<uint8_t> skyLight;
    std::vector<uint8_t> blockLight;
    int nonAirCount = 0;
    uint32_t editVersion = 0;
    bool dirty = false;
};

enum class VoxelChunkLifecycleStage : uint8_t {
    Unknown = 0,
    Desired,
    BaseQueued,
    BaseInProgress,
    BaseGenerated,
    FeatureQueued,
    FeatureInProgress,
    Ready
};

struct VoxelChunkLifecycleState {
    VoxelChunkLifecycleStage stage = VoxelChunkLifecycleStage::Unknown;
    bool desired = false;
    bool generated = false;
    bool hasSection = false;
    bool postFeaturesComplete = false;
    bool surfaceFoliageComplete = false;
    uint16_t featureDependencyReadyMask = 0;
    bool featureDependencyMaskInitialized = false;
    uint64_t touchFrame = 0;

    bool isRenderable() const noexcept {
        return hasSection && isFullyReady();
    }

    bool isFullyReady() const noexcept {
        return generated
            && postFeaturesComplete
            && (!hasSection || surfaceFoliageComplete);
    }
};

struct VoxelSectionBuffers {
    std::vector<uint32_t> ids;
    std::vector<uint32_t> colors;
    std::vector<uint8_t> skyLight;
    std::vector<uint8_t> blockLight;
};

struct VoxelWorldContext {
    int sectionSize = 128;
    bool enabled = false;
    uint8_t defaultSkyLightLevel = static_cast<uint8_t>(15);
    std::unordered_map<VoxelSectionKey, VoxelSection, VoxelSectionKeyHash> sections;
    std::unordered_set<VoxelSectionKey, VoxelSectionKeyHash> dirtySections;
    std::unordered_map<VoxelSectionKey, uint64_t, VoxelSectionKeyHash> dirtyTickets;
    std::unordered_map<VoxelSectionKey, VoxelChunkLifecycleState, VoxelSectionKeyHash> chunkStates;
    std::unordered_map<int, std::vector<VoxelSectionBuffers>> bufferPools;
    uint64_t nextDirtyTicket = 1;

    void reset();
    VoxelChunkLifecycleState& ensureChunkState(const VoxelSectionKey& key);
    VoxelChunkLifecycleState* findChunkState(const VoxelSectionKey& key);
    const VoxelChunkLifecycleState* findChunkState(const VoxelSectionKey& key) const;
    void eraseChunkState(const VoxelSectionKey& key);
    void setChunkStage(const VoxelSectionKey& key,
                       VoxelChunkLifecycleStage stage,
                       uint64_t touchFrame = 0);
    uint32_t getBlockWorld(const glm::ivec3& worldPos) const;
    uint32_t getColorWorld(const glm::ivec3& worldPos) const;
    uint8_t getSkyLightWorld(const glm::ivec3& worldPos) const;
    uint8_t getBlockLightWorld(const glm::ivec3& worldPos) const;
    void setBlockWorld(const glm::ivec3& worldPos, uint32_t id, uint32_t color);
    void setBlock(const glm::ivec3& worldPos, uint32_t id, uint32_t color, bool markDirty = true);
    uint64_t markSectionDirty(const VoxelSectionKey& key);
    void clearSectionDirty(const VoxelSectionKey& key);
    uint64_t getSectionDirtyTicket(const VoxelSectionKey& key) const;
    void releaseSection(const VoxelSectionKey& key);
};
