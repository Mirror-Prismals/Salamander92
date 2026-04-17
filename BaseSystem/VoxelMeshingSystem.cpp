#pragma once

#include <algorithm>
#include <unordered_set>

namespace VoxelMeshInitSystemLogic {
    int FloorDivInt(int value, int divisor);
    int SectionSizeForSection(const VoxelWorldContext& voxelWorld);
}

namespace VoxelMeshingSystemLogic {
    namespace {
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
    }

    void ResetMeshingRuntime() {}

    void RequestPriorityVoxelRemesh(BaseSystem& baseSystem,
                                    std::vector<Entity>&,
                                    const glm::ivec3& worldCell) {
        if (!baseSystem.voxelWorld || !baseSystem.voxelWorld->enabled) return;
        VoxelWorldContext& voxelWorld = *baseSystem.voxelWorld;

        std::unordered_set<VoxelSectionKey, VoxelSectionKeyHash> touched;
        touched.reserve(7);
        collectTouchedSections(voxelWorld, worldCell, touched);

        for (const VoxelSectionKey& key : touched) {
            voxelWorld.dirtySections.insert(key);
            if (baseSystem.voxelRender) {
                baseSystem.voxelRender->renderBuffersDirty.insert(key);
            }
        }
    }

    void UpdateVoxelMeshing(BaseSystem&, std::vector<Entity>&, float, PlatformWindowHandle) {}
}
