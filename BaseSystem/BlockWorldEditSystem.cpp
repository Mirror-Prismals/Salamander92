        struct RemovedBlockInfo {
            int prototypeID = -1;
            glm::vec3 color = glm::vec3(1.0f);
            uint32_t packedColor = 0u;
            bool fromVoxel = false;
            glm::ivec3 voxelCell = glm::ivec3(0);
            bool hasSourceCell = false;
            glm::ivec3 sourceCell = glm::ivec3(0);
        };

        bool RemoveBlockAtPosition(BaseSystem& baseSystem,
                                   LevelContext& level,
                                   std::vector<Entity>& prototypes,
                                   const glm::vec3& position,
                                   int worldIndex,
                                   RemovedBlockInfo* removedInfo) {
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) return false;
            Entity& world = level.worlds[worldIndex];
            glm::ivec3 cell = glm::ivec3(glm::round(position));
            if (baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                uint32_t id = baseSystem.voxelWorld->getBlockWorld(cell);
                if (id != 0 && id < prototypes.size()) {
                    const Entity& proto = prototypes[id];
                    if (isRemovableGameplayBlock(proto)) {
                        const uint32_t packedColor = baseSystem.voxelWorld->getColorWorld(cell);
                        if (isSurfaceStonePrototypeName(proto.name)) {
                            const int pileCount = decodeSurfaceStonePileCount(packedColor);
                            if (pileCount > SURFACE_STONE_PILE_MIN) {
                                if (removedInfo) {
                                    removedInfo->prototypeID = static_cast<int>(id);
                                    removedInfo->color = unpackColor(packedColor);
                                    removedInfo->packedColor = withSurfaceStonePileCount(packedColor, SURFACE_STONE_PILE_MIN);
                                    removedInfo->fromVoxel = true;
                                    removedInfo->voxelCell = cell;
                                    removedInfo->hasSourceCell = true;
                                    removedInfo->sourceCell = cell;
                                }
                                baseSystem.voxelWorld->setBlockWorld(
                                    cell,
                                    id,
                                    withSurfaceStonePileCount(packedColor, pileCount - 1)
                                );
                                VoxelMeshingSystemLogic::RequestPriorityVoxelRemesh(baseSystem, prototypes, cell);
                                return true;
                            }
                        }
                        if (removedInfo) {
                            uint32_t heldPackedColor = packedColor;
                            if (isSurfaceStonePrototypeName(proto.name)) {
                                heldPackedColor = withSurfaceStonePileCount(packedColor, SURFACE_STONE_PILE_MIN);
                            }
                            removedInfo->prototypeID = static_cast<int>(id);
                            removedInfo->color = unpackColor(packedColor);
                            removedInfo->packedColor = heldPackedColor;
                            removedInfo->fromVoxel = true;
                            removedInfo->voxelCell = cell;
                            removedInfo->hasSourceCell = true;
                            removedInfo->sourceCell = cell;
                        }
                        baseSystem.voxelWorld->setBlockWorld(cell, 0, 0);
                        TreeGenerationSystemLogic::NotifyPineLogRemoved(cell, static_cast<int>(id));
                        VoxelMeshingSystemLogic::RequestPriorityVoxelRemesh(baseSystem, prototypes, cell);

                        const bool pruneLegacyInstances = readRegistryBool(baseSystem, "voxelEditPruneLegacyInstances", false);
                        if (pruneLegacyInstances) {
                            // Optional maintenance pass for legacy worlds that still contain duplicate
                            // chunkable instances alongside voxel data. Disabled by default because
                            // scanning large instance arrays can spike edit latency.
                            for (size_t i = 0; i < world.instances.size();) {
                                const EntityInstance& inst = world.instances[i];
                                if (inst.prototypeID < 0 || inst.prototypeID >= static_cast<int>(prototypes.size())) {
                                    ++i;
                                    continue;
                                }
                                const Entity& instProto = prototypes[inst.prototypeID];
                                if (!isRemovableGameplayBlock(instProto)) {
                                    ++i;
                                    continue;
                                }
                                glm::ivec3 instCell = glm::ivec3(glm::round(inst.position));
                                if (instCell != cell) {
                                    ++i;
                                    continue;
                                }
                                world.instances[i] = world.instances.back();
                                world.instances.pop_back();
                            }
                        }
                        return true;
                    }
                }
            }

            for (size_t i = 0; i < world.instances.size(); ++i) {
                const EntityInstance& inst = world.instances[i];
                if (glm::distance(inst.position, position) > POSITION_EPSILON) continue;
                if (inst.prototypeID < 0 || inst.prototypeID >= static_cast<int>(prototypes.size())) continue;
                const Entity& proto = prototypes[inst.prototypeID];
                if (!isRemovableGameplayBlock(proto)) continue;
                if (removedInfo) {
                    uint32_t packed = packColor(inst.color);
                    if (isSurfaceStonePrototypeName(proto.name)) {
                        packed = withSurfaceStonePileCount(packed, SURFACE_STONE_PILE_MIN);
                    }
                    removedInfo->prototypeID = inst.prototypeID;
                    removedInfo->color = inst.color;
                    removedInfo->packedColor = packed;
                    removedInfo->hasSourceCell = true;
                    removedInfo->sourceCell = glm::ivec3(glm::round(inst.position));
                }
                world.instances[i] = world.instances.back();
                world.instances.pop_back();
                return true;
            }
            return false;
        }

        bool SpawnCavePotLoot(BaseSystem& baseSystem,
                              LevelContext& level,
                              std::vector<Entity>& prototypes,
                              int worldIndex,
                              const RemovedBlockInfo& removedBlock,
                              const PlayerContext& player) {
            if (removedBlock.prototypeID < 0 || removedBlock.prototypeID >= static_cast<int>(prototypes.size())) return false;
            if (!isCavePotPrototypeName(prototypes[static_cast<size_t>(removedBlock.prototypeID)].name)) return false;
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) return false;

            struct BlueprintVariantPrototypes {
                int x = -1;
                int z = -1;
            };
            std::array<BlueprintVariantPrototypes, 5> blueprintVariants;
            for (const auto& proto : prototypes) {
                if (!proto.isBlock) continue;
                if (proto.name == "GrassCoverBlueprintAxeheadTexX") blueprintVariants[0].x = proto.prototypeID;
                else if (proto.name == "GrassCoverBlueprintAxeheadTexZ") blueprintVariants[0].z = proto.prototypeID;
                else if (proto.name == "GrassCoverBlueprintHiltTexX") blueprintVariants[1].x = proto.prototypeID;
                else if (proto.name == "GrassCoverBlueprintHiltTexZ") blueprintVariants[1].z = proto.prototypeID;
                else if (proto.name == "GrassCoverBlueprintPickaxeTexX") blueprintVariants[2].x = proto.prototypeID;
                else if (proto.name == "GrassCoverBlueprintPickaxeTexZ") blueprintVariants[2].z = proto.prototypeID;
                else if (proto.name == "GrassCoverBlueprintScytheTexX") blueprintVariants[3].x = proto.prototypeID;
                else if (proto.name == "GrassCoverBlueprintScytheTexZ") blueprintVariants[3].z = proto.prototypeID;
                else if (proto.name == "GrassCoverBlueprintSwordTexX") blueprintVariants[4].x = proto.prototypeID;
                else if (proto.name == "GrassCoverBlueprintSwordTexZ") blueprintVariants[4].z = proto.prototypeID;
            }
            std::vector<int> availableBlueprintVariants;
            for (int i = 0; i < static_cast<int>(blueprintVariants.size()); ++i) {
                if (blueprintVariants[static_cast<size_t>(i)].x >= 0
                    || blueprintVariants[static_cast<size_t>(i)].z >= 0) {
                    availableBlueprintVariants.push_back(i);
                }
            }
            if (availableBlueprintVariants.empty()) return false;

            const glm::ivec3 lootCell = removedBlock.fromVoxel
                ? removedBlock.voxelCell
                : glm::ivec3(glm::round(player.targetedBlockPosition));
            const uint32_t seed = hash3D(
                lootCell.x + static_cast<int>(baseSystem.frameIndex & 0x7fffffffu),
                lootCell.y + worldIndex * 31,
                lootCell.z - 17
            );

            int rewardPrototype = -1;
            const int variantIdx = availableBlueprintVariants[static_cast<size_t>(
                (seed >> 13u) % static_cast<uint32_t>(availableBlueprintVariants.size())
            )];
            const BlueprintVariantPrototypes& variant = blueprintVariants[static_cast<size_t>(variantIdx)];
            rewardPrototype = (((seed >> 18u) & 1u) == 0u) ? variant.x : variant.z;
            if (rewardPrototype < 0) rewardPrototype = (variant.x >= 0) ? variant.x : variant.z;
            const glm::vec3 rewardColor(1.0f);
            if (rewardPrototype < 0) return false;

            if (removedBlock.fromVoxel && baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                if (baseSystem.voxelWorld->getBlockWorld(lootCell) == 0u) {
                    baseSystem.voxelWorld->setBlockWorld(
                        lootCell,
                        static_cast<uint32_t>(rewardPrototype),
                        packColor(rewardColor)
                    );
                    VoxelMeshingSystemLogic::RequestPriorityVoxelRemesh(baseSystem, prototypes, lootCell);
                    StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, worldIndex, glm::vec3(lootCell));
                }
            } else {
                const glm::vec3 rewardPos = glm::vec3(lootCell);
                if (!BlockSelectionSystemLogic::HasBlockAt(baseSystem, prototypes, worldIndex, rewardPos)) {
                    Entity& world = level.worlds[static_cast<size_t>(worldIndex)];
                    world.instances.push_back(
                        HostLogic::CreateInstance(baseSystem, rewardPrototype, rewardPos, rewardColor)
                    );
                    BlockSelectionSystemLogic::AddBlockToCache(
                        baseSystem,
                        prototypes,
                        worldIndex,
                        rewardPos,
                        rewardPrototype
                    );
                    StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, worldIndex, rewardPos);
                }
            }
            return true;
        }
    }

