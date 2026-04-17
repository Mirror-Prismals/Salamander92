#pragma once

#include "Structures/VoxelWorld.h"
#include <algorithm>

namespace {
    int floorDivInt(int value, int divisor) {
        if (divisor <= 0) return 0;
        if (value >= 0) return value / divisor;
        return -(((-value) + divisor - 1) / divisor);
    }

    glm::ivec3 floorDivVec(const glm::ivec3& v, int divisor) {
        return glm::ivec3(
            floorDivInt(v.x, divisor),
            floorDivInt(v.y, divisor),
            floorDivInt(v.z, divisor)
        );
    }

    int sectionSizeForTier(int baseSize, int /*ignoredTier*/) {
        return baseSize > 0 ? baseSize : 1;
    }

    int voxelIndex(const glm::ivec3& local, int size) {
        return local.x + local.y * size + local.z * size * size;
    }

}

void VoxelWorldContext::reset() {
    sections.clear();
    dirtySections.clear();
    chunkStates.clear();
}

VoxelChunkLifecycleState& VoxelWorldContext::ensureChunkState(const VoxelSectionKey& key) {
    return chunkStates[key];
}

VoxelChunkLifecycleState* VoxelWorldContext::findChunkState(const VoxelSectionKey& key) {
    auto it = chunkStates.find(key);
    if (it == chunkStates.end()) return nullptr;
    return &it->second;
}

const VoxelChunkLifecycleState* VoxelWorldContext::findChunkState(const VoxelSectionKey& key) const {
    auto it = chunkStates.find(key);
    if (it == chunkStates.end()) return nullptr;
    return &it->second;
}

void VoxelWorldContext::eraseChunkState(const VoxelSectionKey& key) {
    chunkStates.erase(key);
}

void VoxelWorldContext::setChunkStage(const VoxelSectionKey& key,
                                      VoxelChunkLifecycleStage stage,
                                      uint64_t touchFrame) {
    VoxelChunkLifecycleState& state = ensureChunkState(key);
    state.stage = stage;
    state.touchFrame = touchFrame;
}

namespace {
    VoxelSectionBuffers acquireBuffers(VoxelWorldContext& world, int size) {
        VoxelSectionBuffers buffers;
        auto it = world.bufferPools.find(size);
        if (it != world.bufferPools.end() && !it->second.empty()) {
            buffers = std::move(it->second.back());
            it->second.pop_back();
        }
        const size_t count = static_cast<size_t>(size * size * size);
        if (buffers.ids.size() != count) {
            buffers.ids.assign(count, 0);
        } else {
            std::fill(buffers.ids.begin(), buffers.ids.end(), 0);
        }
        if (buffers.colors.size() != count) {
            buffers.colors.assign(count, 0);
        } else {
            std::fill(buffers.colors.begin(), buffers.colors.end(), 0);
        }
        const uint8_t defaultSky = world.defaultSkyLightLevel;
        if (buffers.skyLight.size() != count) {
            buffers.skyLight.assign(count, defaultSky);
        } else {
            std::fill(buffers.skyLight.begin(), buffers.skyLight.end(), defaultSky);
        }
        if (buffers.blockLight.size() != count) {
            buffers.blockLight.assign(count, static_cast<uint8_t>(0));
        } else {
            std::fill(buffers.blockLight.begin(), buffers.blockLight.end(), static_cast<uint8_t>(0));
        }
        return buffers;
    }

    void releaseBuffers(VoxelWorldContext& world, int size, VoxelSectionBuffers&& buffers) {
        world.bufferPools[size].push_back(std::move(buffers));
    }
}

uint32_t VoxelWorldContext::getBlockWorld(const glm::ivec3& worldPos) const {
    int size = sectionSizeForTier(sectionSize, 0);
    glm::ivec3 sectionCoord = floorDivVec(worldPos, size);
    glm::ivec3 local = worldPos - sectionCoord * size;
    VoxelSectionKey key{sectionCoord};
    auto it = sections.find(key);
    if (it == sections.end()) return 0;
    const VoxelSection& section = it->second;
    int idx = voxelIndex(local, section.size);
    if (idx < 0 || idx >= static_cast<int>(section.ids.size())) return 0;
    return section.ids[idx];
}

uint32_t VoxelWorldContext::getColorWorld(const glm::ivec3& worldPos) const {
    int size = sectionSizeForTier(sectionSize, 0);
    glm::ivec3 sectionCoord = floorDivVec(worldPos, size);
    glm::ivec3 local = worldPos - sectionCoord * size;
    VoxelSectionKey key{sectionCoord};
    auto it = sections.find(key);
    if (it == sections.end()) return 0;
    const VoxelSection& section = it->second;
    int idx = voxelIndex(local, section.size);
    if (idx < 0 || idx >= static_cast<int>(section.colors.size())) return 0;
    return section.colors[idx];
}

uint8_t VoxelWorldContext::getSkyLightWorld(const glm::ivec3& worldPos) const {
    int size = sectionSizeForTier(sectionSize, 0);
    glm::ivec3 sectionCoord = floorDivVec(worldPos, size);
    glm::ivec3 local = worldPos - sectionCoord * size;
    VoxelSectionKey key{sectionCoord};
    auto it = sections.find(key);
    if (it == sections.end()) return defaultSkyLightLevel;
    const VoxelSection& section = it->second;
    int idx = voxelIndex(local, section.size);
    if (idx < 0 || idx >= static_cast<int>(section.skyLight.size())) return defaultSkyLightLevel;
    return section.skyLight[static_cast<size_t>(idx)];
}

uint8_t VoxelWorldContext::getBlockLightWorld(const glm::ivec3& worldPos) const {
    int size = sectionSizeForTier(sectionSize, 0);
    glm::ivec3 sectionCoord = floorDivVec(worldPos, size);
    glm::ivec3 local = worldPos - sectionCoord * size;
    VoxelSectionKey key{sectionCoord};
    auto it = sections.find(key);
    if (it == sections.end()) return static_cast<uint8_t>(0);
    const VoxelSection& section = it->second;
    int idx = voxelIndex(local, section.size);
    if (idx < 0 || idx >= static_cast<int>(section.blockLight.size())) return static_cast<uint8_t>(0);
    return section.blockLight[static_cast<size_t>(idx)];
}

void VoxelWorldContext::setBlockWorld(const glm::ivec3& worldPos, uint32_t id, uint32_t color) {
    setBlock(worldPos, id, color, true);
}

void VoxelWorldContext::setBlock(const glm::ivec3& worldPos, uint32_t id, uint32_t color, bool markDirty) {
    int size = sectionSizeForTier(sectionSize, 0);
    glm::ivec3 sectionCoord = floorDivVec(worldPos, size);
    glm::ivec3 local = worldPos - sectionCoord * size;
    VoxelSectionKey key{sectionCoord};

    auto it = sections.find(key);
    if (it == sections.end()) {
        if (id == 0) return;
        VoxelSection section;
        section.size = size;
        section.coord = sectionCoord;
        VoxelSectionBuffers buffers = acquireBuffers(*this, size);
        section.ids = std::move(buffers.ids);
        section.colors = std::move(buffers.colors);
        section.skyLight = std::move(buffers.skyLight);
        section.blockLight = std::move(buffers.blockLight);
        auto [insertedIt, _] = sections.emplace(key, std::move(section));
        it = insertedIt;
        ensureChunkState(key).hasSection = true;
    }

    VoxelSection& section = it->second;
    int idx = voxelIndex(local, section.size);
    uint32_t oldId = section.ids[idx];
    uint32_t oldColor = section.colors[idx];
    if (oldId == id && oldColor == color) return;
    section.ids[idx] = id;
    section.colors[idx] = (id == 0) ? 0 : color;
    if (oldId == 0 && id != 0) section.nonAirCount += 1;
    if (oldId != 0 && id == 0) section.nonAirCount -= 1;
    if (markDirty) {
        section.editVersion += 1;
        section.dirty = true;
        dirtySections.insert(key);
    }

    if (section.nonAirCount <= 0) {
        releaseSection(key);
    }
}

void VoxelWorldContext::releaseSection(const VoxelSectionKey& key) {
    auto it = sections.find(key);
    if (it == sections.end()) return;
    VoxelSectionBuffers buffers;
    buffers.ids = std::move(it->second.ids);
    buffers.colors = std::move(it->second.colors);
    buffers.skyLight = std::move(it->second.skyLight);
    buffers.blockLight = std::move(it->second.blockLight);
    releaseBuffers(*this, it->second.size, std::move(buffers));
    sections.erase(it);
    dirtySections.erase(key);
    auto chunkIt = chunkStates.find(key);
    if (chunkIt != chunkStates.end()) {
        chunkIt->second.hasSection = false;
    }
}
