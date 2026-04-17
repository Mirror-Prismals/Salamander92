#pragma once

    namespace {
        ThrownHeldBlockRuntime& thrownHeldBlockRuntime() {
            static ThrownHeldBlockRuntime s_thrownHeldBlock;
            return s_thrownHeldBlock;
        }

        int normalizeThrowWorldIndex(const LevelContext& level, int worldIndex) {
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) {
                worldIndex = level.activeWorldIndex;
            }
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) {
                worldIndex = 0;
            }
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) {
                return -1;
            }
            return worldIndex;
        }

        EntityInstance* findWorldInstanceByID(LevelContext& level, int worldIndex, int instanceID) {
            if (instanceID <= 0) return nullptr;
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) return nullptr;
            Entity& world = level.worlds[static_cast<size_t>(worldIndex)];
            for (EntityInstance& inst : world.instances) {
                if (inst.instanceID == instanceID) return &inst;
            }
            return nullptr;
        }

        bool isSolidThrowCell(const BaseSystem& baseSystem,
                              const LevelContext& level,
                              const std::vector<Entity>& prototypes,
                              int worldIndex,
                              const glm::ivec3& cell,
                              int ignoreInstanceID) {
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) return false;
            auto prototypeIsSolid = [&](int prototypeID) -> bool {
                if (prototypeID < 0 || prototypeID >= static_cast<int>(prototypes.size())) return false;
                const Entity& proto = prototypes[static_cast<size_t>(prototypeID)];
                if (!proto.isBlock) return false;
                if (proto.name == "Water") return false;
                return true;
            };

            if (baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                const uint32_t voxelId = baseSystem.voxelWorld->getBlockWorld(cell);
                if (voxelId > 0 && voxelId < prototypes.size() && prototypeIsSolid(static_cast<int>(voxelId))) {
                    return true;
                }
            }

            const Entity& world = level.worlds[static_cast<size_t>(worldIndex)];
            for (const EntityInstance& inst : world.instances) {
                if (inst.instanceID == ignoreInstanceID) continue;
                if (glm::ivec3(glm::round(inst.position)) != cell) continue;
                if (prototypeIsSolid(inst.prototypeID)) return true;
            }
            return false;
        }

        void clearThrownHeldVisual(LevelContext& level) {
            ThrownHeldBlockRuntime& thrownHeldBlock = thrownHeldBlockRuntime();
            if (thrownHeldBlock.instanceID <= 0) return;
            const int worldIndex = normalizeThrowWorldIndex(level, thrownHeldBlock.worldIndex);
            if (worldIndex >= 0) {
                Entity& world = level.worlds[static_cast<size_t>(worldIndex)];
                for (size_t i = 0; i < world.instances.size(); ++i) {
                    if (world.instances[i].instanceID != thrownHeldBlock.instanceID) continue;
                    world.instances[i] = world.instances.back();
                    world.instances.pop_back();
                    break;
                }
            }
            thrownHeldBlock.instanceID = -1;
        }

        void clearThrownHeldState(LevelContext& level) {
            clearThrownHeldVisual(level);
            thrownHeldBlockRuntime() = ThrownHeldBlockRuntime{};
        }

        void shatterThrownCavePotAtCell(BaseSystem& baseSystem,
                                        LevelContext& level,
                                        std::vector<Entity>& prototypes,
                                        PlayerContext& player,
                                        const glm::ivec3& cell) {
            ThrownHeldBlockRuntime& thrownHeldBlock = thrownHeldBlockRuntime();
            if (!thrownHeldBlock.active) return;
            if (thrownHeldBlock.prototypeID < 0 || thrownHeldBlock.prototypeID >= static_cast<int>(prototypes.size())) return;
            const Entity& proto = prototypes[static_cast<size_t>(thrownHeldBlock.prototypeID)];
            if (!isCavePotPrototypeName(proto.name)) return;
            const int worldIndex = normalizeThrowWorldIndex(level, thrownHeldBlock.worldIndex);
            if (worldIndex < 0) return;

            RemovedBlockInfo removedBlock;
            removedBlock.prototypeID = thrownHeldBlock.prototypeID;
            removedBlock.color = thrownHeldBlock.color;
            removedBlock.packedColor = (thrownHeldBlock.packedColor & 0x00ffffffu) == 0u
                ? packColor(thrownHeldBlock.color)
                : thrownHeldBlock.packedColor;
            removedBlock.fromVoxel = true;
            removedBlock.voxelCell = cell;
            removedBlock.hasSourceCell = true;
            removedBlock.sourceCell = cell;
            (void)SpawnCavePotLoot(
                baseSystem,
                level,
                prototypes,
                worldIndex,
                removedBlock,
                player
            );
            triggerGameplaySfx(baseSystem, "break_stone.ck", 0.02f);
        }

        bool placeThrownHeldBlockAtCell(BaseSystem& baseSystem,
                                        LevelContext& level,
                                        std::vector<Entity>& prototypes,
                                        const glm::ivec3& cell) {
            ThrownHeldBlockRuntime& thrownHeldBlock = thrownHeldBlockRuntime();
            if (!thrownHeldBlock.active) return false;
            if (thrownHeldBlock.prototypeID < 0 || thrownHeldBlock.prototypeID >= static_cast<int>(prototypes.size())) return false;
            const int worldIndex = normalizeThrowWorldIndex(level, thrownHeldBlock.worldIndex);
            if (worldIndex < 0) return false;
            if (isSolidThrowCell(baseSystem, level, prototypes, worldIndex, cell, thrownHeldBlock.instanceID)) return false;

            const Entity& proto = prototypes[static_cast<size_t>(thrownHeldBlock.prototypeID)];
            if (baseSystem.voxelWorld && baseSystem.voxelWorld->enabled && proto.isChunkable) {
                uint32_t packedColor = thrownHeldBlock.packedColor;
                if ((packedColor & 0x00ffffffu) == 0u) {
                    packedColor = packColor(thrownHeldBlock.color);
                }
                if (isSurfaceStonePrototypeName(proto.name)) {
                    packedColor = withSurfaceStonePileCount(packedColor, SURFACE_STONE_PILE_MIN);
                }
                baseSystem.voxelWorld->setBlockWorld(
                    cell,
                    static_cast<uint32_t>(thrownHeldBlock.prototypeID),
                    packedColor
                );
                VoxelMeshingSystemLogic::RequestPriorityVoxelRemesh(baseSystem, prototypes, cell);
                StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, worldIndex, glm::vec3(cell));
            } else {
                const glm::vec3 placePos = glm::vec3(cell);
                if (BlockSelectionSystemLogic::HasBlockAt(baseSystem, prototypes, worldIndex, placePos)) return false;
                Entity& world = level.worlds[static_cast<size_t>(worldIndex)];
                world.instances.push_back(HostLogic::CreateInstance(
                    baseSystem,
                    thrownHeldBlock.prototypeID,
                    placePos,
                    thrownHeldBlock.color
                ));
                BlockSelectionSystemLogic::AddBlockToCache(
                    baseSystem,
                    prototypes,
                    worldIndex,
                    placePos,
                    thrownHeldBlock.prototypeID
                );
                StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, worldIndex, placePos);
            }
            if (isChalkDustPrototypeID(prototypes, thrownHeldBlock.prototypeID)) {
                refreshChalkDustNeighborhood(baseSystem, level, prototypes, worldIndex, cell);
            }
            if (proto.name == "AudioVisualizer") {
                RayTracedAudioSystemLogic::InvalidateSourceCache(baseSystem);
            }
            triggerGameplaySfx(baseSystem, "place_block.ck", 0.02f);
            return true;
        }

        void ensureThrownHeldVisual(BaseSystem& baseSystem,
                                    LevelContext& level,
                                    std::vector<Entity>& prototypes) {
            ThrownHeldBlockRuntime& thrownHeldBlock = thrownHeldBlockRuntime();
            if (!thrownHeldBlock.active) return;
            thrownHeldBlock.worldIndex = normalizeThrowWorldIndex(level, thrownHeldBlock.worldIndex);
            if (thrownHeldBlock.worldIndex < 0) {
                clearThrownHeldState(level);
                return;
            }
            EntityInstance* visual = findWorldInstanceByID(level, thrownHeldBlock.worldIndex, thrownHeldBlock.instanceID);
            if (!visual) {
                EntityInstance inst = HostLogic::CreateInstance(
                    baseSystem,
                    thrownHeldBlock.prototypeID,
                    thrownHeldBlock.position,
                    thrownHeldBlock.color
                );
                if (inst.instanceID > 0) {
                    inst.name = "__ThrownHeldBlockVisual";
                    thrownHeldBlock.instanceID = inst.instanceID;
                    level.worlds[static_cast<size_t>(thrownHeldBlock.worldIndex)].instances.push_back(inst);
                    visual = findWorldInstanceByID(level, thrownHeldBlock.worldIndex, thrownHeldBlock.instanceID);
                } else {
                    thrownHeldBlock.instanceID = -1;
                }
            }
            if (visual) {
                visual->position = thrownHeldBlock.position;
                visual->color = thrownHeldBlock.color;
            }
        }
    }

    bool HasActiveThrownHeldBlock() {
        return thrownHeldBlockRuntime().active;
    }

    bool StartThrownHeldBlockFromPlayer(BaseSystem& baseSystem,
                                        LevelContext& level,
                                        PlayerContext& player,
                                        std::vector<Entity>& prototypes) {
        const int throwWorldIndex = normalizeThrowWorldIndex(level, player.targetedWorldIndex);
        if (throwWorldIndex < 0) return false;

        glm::vec3 forward = cameraForwardDirection(player);
        if (glm::length(forward) < 0.01f) forward = glm::vec3(0.0f, 0.0f, -1.0f);
        forward = glm::normalize(forward);

        const float launchSpeed = std::max(0.5f, readRegistryFloat(baseSystem, "HeldThrowLaunchSpeed", 10.5f));
        const float launchUp = readRegistryFloat(baseSystem, "HeldThrowLaunchUp", 1.2f);
        const float spawnForwardOffset = std::max(0.2f, readRegistryFloat(baseSystem, "HeldThrowSpawnForwardOffset", 0.75f));
        const float spawnLift = readRegistryFloat(baseSystem, "HeldThrowSpawnLift", 0.12f);

        glm::vec3 spawnPos = cameraEyePosition(baseSystem, player)
            + forward * spawnForwardOffset
            + glm::vec3(0.0f, spawnLift, 0.0f);
        for (int i = 0; i < 6; ++i) {
            const glm::ivec3 spawnCell = glm::ivec3(glm::round(spawnPos));
            if (!isSolidThrowCell(baseSystem, level, prototypes, throwWorldIndex, spawnCell, -1)) break;
            spawnPos += forward * 0.35f;
        }

        clearThrownHeldState(level);
        ThrownHeldBlockRuntime& thrownHeldBlock = thrownHeldBlockRuntime();
        thrownHeldBlock.active = true;
        thrownHeldBlock.worldIndex = throwWorldIndex;
        thrownHeldBlock.prototypeID = player.heldPrototypeID;
        thrownHeldBlock.color = player.heldBlockColor;
        thrownHeldBlock.packedColor = player.heldPackedColor;
        thrownHeldBlock.hasSourceCell = player.heldHasSourceCell;
        thrownHeldBlock.sourceCell = player.heldSourceCell;
        thrownHeldBlock.position = spawnPos;
        thrownHeldBlock.velocity = forward * launchSpeed + glm::vec3(0.0f, launchUp, 0.0f);
        thrownHeldBlock.instanceID = -1;
        thrownHeldBlock.age = 0.0f;
        ensureThrownHeldVisual(baseSystem, level, prototypes);
        triggerGameplaySfx(baseSystem, "pickup_block.ck", 0.02f);

        player.isHoldingBlock = false;
        player.heldPrototypeID = -1;
        player.heldPackedColor = 0u;
        player.heldHasSourceCell = false;
        player.heldSourceCell = glm::ivec3(0);
        return true;
    }

    void UpdateThrownHeldBlockRuntime(BaseSystem& baseSystem,
                                      LevelContext& level,
                                      PlayerContext& player,
                                      std::vector<Entity>& prototypes,
                                      float dt) {
        ThrownHeldBlockRuntime& thrownHeldBlock = thrownHeldBlockRuntime();
        if (!thrownHeldBlock.active) return;
        if (thrownHeldBlock.prototypeID < 0 || thrownHeldBlock.prototypeID >= static_cast<int>(prototypes.size())) {
            clearThrownHeldState(level);
            return;
        }
        thrownHeldBlock.worldIndex = normalizeThrowWorldIndex(level, thrownHeldBlock.worldIndex);
        if (thrownHeldBlock.worldIndex < 0) {
            clearThrownHeldState(level);
            return;
        }
        ensureThrownHeldVisual(baseSystem, level, prototypes);
        if (dt <= 0.0f) return;

        const float gravity = -std::abs(readRegistryFloat(baseSystem, "HeldThrowGravity", 18.0f));
        const float maxSpeed = std::max(1.0f, readRegistryFloat(baseSystem, "HeldThrowMaxSpeed", 24.0f));
        const float maxStepDistance = std::max(0.04f, readRegistryFloat(baseSystem, "HeldThrowMaxStepDistance", 0.20f));
        const float maxLifetime = std::max(0.5f, readRegistryFloat(baseSystem, "HeldThrowLifetimeSeconds", 10.0f));

        thrownHeldBlock.age += dt;
        if (thrownHeldBlock.age >= maxLifetime) {
            const glm::ivec3 expiryCell = glm::ivec3(glm::round(thrownHeldBlock.position));
            const bool thrownIsCavePot =
                isCavePotPrototypeName(prototypes[static_cast<size_t>(thrownHeldBlock.prototypeID)].name);
            if (thrownIsCavePot) {
                shatterThrownCavePotAtCell(baseSystem, level, prototypes, player, expiryCell);
            } else {
                (void)placeThrownHeldBlockAtCell(baseSystem, level, prototypes, expiryCell);
            }
            clearThrownHeldState(level);
            return;
        }

        const float frameDistance = glm::length(thrownHeldBlock.velocity) * dt;
        int substeps = static_cast<int>(std::ceil(frameDistance / maxStepDistance));
        substeps = std::clamp(substeps, 1, 32);
        const float stepDt = dt / static_cast<float>(substeps);
        bool collided = false;
        glm::ivec3 collisionCell = glm::ivec3(0);
        glm::ivec3 lastFreeCell = glm::ivec3(glm::round(thrownHeldBlock.position));
        glm::vec3 collisionDir = thrownHeldBlock.velocity;

        for (int step = 0; step < substeps; ++step) {
            thrownHeldBlock.velocity.y += gravity * stepDt;
            const float speed = glm::length(thrownHeldBlock.velocity);
            if (speed > maxSpeed) {
                thrownHeldBlock.velocity *= (maxSpeed / speed);
            }
            const glm::vec3 stepDir = thrownHeldBlock.velocity * stepDt;
            const glm::vec3 nextPos = thrownHeldBlock.position + stepDir;
            const glm::ivec3 nextCell = glm::ivec3(glm::round(nextPos));
            if (isSolidThrowCell(baseSystem, level, prototypes, thrownHeldBlock.worldIndex, nextCell, thrownHeldBlock.instanceID)) {
                collided = true;
                collisionCell = nextCell;
                collisionDir = stepDir;
                break;
            }
            thrownHeldBlock.position = nextPos;
            lastFreeCell = nextCell;
        }

        ensureThrownHeldVisual(baseSystem, level, prototypes);

        if (!collided) return;

        glm::ivec3 travelAxis(0);
        const glm::vec3 dir = normalizeOrDefault(collisionDir, glm::vec3(0.0f, -1.0f, 0.0f));
        const glm::vec3 absDir = glm::abs(dir);
        if (absDir.x >= absDir.y && absDir.x >= absDir.z) {
            travelAxis.x = dir.x >= 0.0f ? 1 : -1;
        } else if (absDir.y >= absDir.x && absDir.y >= absDir.z) {
            travelAxis.y = dir.y >= 0.0f ? 1 : -1;
        } else {
            travelAxis.z = dir.z >= 0.0f ? 1 : -1;
        }

        const std::array<glm::ivec3, 9> candidateCells = {
            lastFreeCell,
            collisionCell - travelAxis,
            lastFreeCell + glm::ivec3(0, 1, 0),
            collisionCell + glm::ivec3(0, 1, 0),
            collisionCell + glm::ivec3(1, 0, 0),
            collisionCell + glm::ivec3(-1, 0, 0),
            collisionCell + glm::ivec3(0, 0, 1),
            collisionCell + glm::ivec3(0, 0, -1),
            glm::ivec3(glm::round(thrownHeldBlock.position))
        };

        const bool thrownIsCavePot =
            isCavePotPrototypeName(prototypes[static_cast<size_t>(thrownHeldBlock.prototypeID)].name);
        if (thrownIsCavePot) {
            glm::ivec3 shatterCell = lastFreeCell;
            for (const glm::ivec3& candidate : candidateCells) {
                if (!isSolidThrowCell(baseSystem, level, prototypes, thrownHeldBlock.worldIndex, candidate, thrownHeldBlock.instanceID)) {
                    shatterCell = candidate;
                    break;
                }
            }
            shatterThrownCavePotAtCell(baseSystem, level, prototypes, player, shatterCell);
            clearThrownHeldState(level);
            return;
        }

        bool placed = false;
        for (const glm::ivec3& candidate : candidateCells) {
            if (placeThrownHeldBlockAtCell(baseSystem, level, prototypes, candidate)) {
                placed = true;
                break;
            }
        }

        if (!placed && !player.isHoldingBlock) {
            player.isHoldingBlock = true;
            player.heldPrototypeID = thrownHeldBlock.prototypeID;
            player.heldBlockColor = thrownHeldBlock.color;
            player.heldPackedColor = thrownHeldBlock.packedColor;
            player.heldHasSourceCell = thrownHeldBlock.hasSourceCell;
            player.heldSourceCell = thrownHeldBlock.sourceCell;
            triggerGameplaySfx(baseSystem, "pickup_block.ck", 0.02f);
        }
        clearThrownHeldState(level);
    }
