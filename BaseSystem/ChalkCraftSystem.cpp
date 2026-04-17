        uint32_t packColor(const glm::vec3& color) {
            auto clampByte = [](float v) {
                int iv = static_cast<int>(std::round(v * 255.0f));
                if (iv < 0) iv = 0;
                if (iv > 255) iv = 255;
                return static_cast<uint32_t>(iv);
            };
            uint32_t r = clampByte(color.r);
            uint32_t g = clampByte(color.g);
            uint32_t b = clampByte(color.b);
            return (r << 16) | (g << 8) | b;
        }

        glm::vec3 unpackColor(uint32_t packed) {
            if (packed == 0) return glm::vec3(1.0f);
            float r = static_cast<float>((packed >> 16) & 0xff) / 255.0f;
            float g = static_cast<float>((packed >> 8) & 0xff) / 255.0f;
            float b = static_cast<float>(packed & 0xff) / 255.0f;
            return glm::vec3(r, g, b);
        }

        int decodeSurfaceStonePileCount(uint32_t packedColor) {
            const int encoded = static_cast<int>((packedColor >> 24) & 0xffu);
            if (encoded <= 0) return SURFACE_STONE_PILE_MIN;
            return std::clamp(encoded, SURFACE_STONE_PILE_MIN, SURFACE_STONE_PILE_MAX);
        }

        uint32_t withSurfaceStonePileCount(uint32_t packedColorRgb, int pileCount) {
            const uint32_t rgb = packedColorRgb & 0x00ffffffu;
            const uint32_t encoded = static_cast<uint32_t>(
                std::clamp(pileCount, SURFACE_STONE_PILE_MIN, SURFACE_STONE_PILE_MAX)
            ) << 24u;
            return rgb | encoded;
        }

        int encodeChalkSnapshotMarker(int tileIndex, int quarterTurns) {
            if (tileIndex < CHALK_TILE_CORNER || tileIndex > CHALK_TILE_T) return 0;
            const int variant = tileIndex - CHALK_TILE_CORNER;
            const int turns = quarterTurns & 3;
            const int encoded = variant * 4 + turns + 1;
            return std::clamp(encoded, 0, 255);
        }

        uint32_t withSnapshotMarker(uint32_t packedColorRgb, int marker) {
            const uint32_t rgb = packedColorRgb & 0x00ffffffu;
            const uint32_t high = static_cast<uint32_t>(std::clamp(marker, 0, 255)) << 24;
            return rgb | high;
        }

        uint32_t packChalkDustSnapshotColor(int tileIndex, int quarterTurns) {
            return withSnapshotMarker(packColor(glm::vec3(1.0f)), encodeChalkSnapshotMarker(tileIndex, quarterTurns));
        }

        struct CellBlockInfo {
            bool present = false;
            bool fromVoxel = false;
            int prototypeID = -1;
            glm::vec3 color = glm::vec3(1.0f);
            size_t instanceIndex = 0;
        };

        bool queryBlockAtCell(const BaseSystem& baseSystem,
                              const LevelContext& level,
                              const std::vector<Entity>& prototypes,
                              int worldIndex,
                              const glm::ivec3& cell,
                              CellBlockInfo* outInfo) {
            if (outInfo) *outInfo = CellBlockInfo{};
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) return false;

            if (baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                const uint32_t id = baseSystem.voxelWorld->getBlockWorld(cell);
                if (id > 0 && id < prototypes.size()) {
                    if (outInfo) {
                        outInfo->present = true;
                        outInfo->fromVoxel = true;
                        outInfo->prototypeID = static_cast<int>(id);
                        outInfo->color = unpackColor(baseSystem.voxelWorld->getColorWorld(cell));
                    }
                    return true;
                }
            }

            const Entity& world = level.worlds[static_cast<size_t>(worldIndex)];
            const glm::vec3 cellPos = glm::vec3(cell);
            for (size_t i = 0; i < world.instances.size(); ++i) {
                const EntityInstance& inst = world.instances[i];
                if (glm::distance(inst.position, cellPos) > POSITION_EPSILON) continue;
                if (inst.prototypeID < 0 || inst.prototypeID >= static_cast<int>(prototypes.size())) continue;
                if (outInfo) {
                    outInfo->present = true;
                    outInfo->fromVoxel = false;
                    outInfo->prototypeID = inst.prototypeID;
                    outInfo->color = inst.color;
                    outInfo->instanceIndex = i;
                }
                return true;
            }
            return false;
        }

        bool replaceBlockAtCell(BaseSystem& baseSystem,
                                LevelContext& level,
                                std::vector<Entity>& prototypes,
                                int worldIndex,
                                const glm::ivec3& cell,
                                int expectedPrototypeID,
                                int newPrototypeID,
                                const glm::vec3& newColor,
                                uint32_t newPackedColor) {
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) return false;
            if (newPrototypeID < 0 || newPrototypeID >= static_cast<int>(prototypes.size())) return false;

            CellBlockInfo current;
            if (!queryBlockAtCell(baseSystem, level, prototypes, worldIndex, cell, &current) || !current.present) return false;
            if (expectedPrototypeID >= 0 && current.prototypeID != expectedPrototypeID) return false;

            if (current.fromVoxel && baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                baseSystem.voxelWorld->setBlockWorld(
                    cell,
                    static_cast<uint32_t>(newPrototypeID),
                    newPackedColor
                );
                VoxelMeshingSystemLogic::RequestPriorityVoxelRemesh(baseSystem, prototypes, cell);
                StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, worldIndex, glm::vec3(cell));
                return true;
            }

            Entity& world = level.worlds[static_cast<size_t>(worldIndex)];
            if (current.instanceIndex >= world.instances.size()) return false;
            const glm::vec3 cellPos = glm::vec3(cell);
            BlockSelectionSystemLogic::RemoveBlockFromCache(baseSystem, prototypes, worldIndex, cellPos);
            world.instances[current.instanceIndex].prototypeID = newPrototypeID;
            world.instances[current.instanceIndex].color = newColor;
            BlockSelectionSystemLogic::AddBlockToCache(baseSystem, prototypes, worldIndex, cellPos, newPrototypeID);
            StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, worldIndex, cellPos);
            return true;
        }

        bool isChalkDustPrototypeID(const std::vector<Entity>& prototypes, int prototypeID) {
            if (prototypeID < 0 || prototypeID >= static_cast<int>(prototypes.size())) return false;
            return isChalkDustPrototypeName(prototypes[static_cast<size_t>(prototypeID)].name);
        }

        void pruneLegacyChalkDustInstancesAtCell(BaseSystem& baseSystem,
                                                 LevelContext& level,
                                                 const std::vector<Entity>& prototypes,
                                                 int worldIndex,
                                                 const glm::ivec3& cell) {
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) return;
            Entity& world = level.worlds[static_cast<size_t>(worldIndex)];
            const glm::vec3 cellPos = glm::vec3(cell);
            for (size_t i = 0; i < world.instances.size();) {
                const EntityInstance& inst = world.instances[i];
                if (glm::distance(inst.position, cellPos) > POSITION_EPSILON) {
                    ++i;
                    continue;
                }
                if (inst.prototypeID < 0 || inst.prototypeID >= static_cast<int>(prototypes.size())) {
                    ++i;
                    continue;
                }
                if (!isChalkDustPrototypeID(prototypes, inst.prototypeID)) {
                    ++i;
                    continue;
                }
                BlockSelectionSystemLogic::RemoveBlockFromCache(baseSystem, prototypes, worldIndex, inst.position);
                world.instances[i] = world.instances.back();
                world.instances.pop_back();
            }
        }

        int resolveChalkDustPrototypeID(const std::vector<Entity>& prototypes, bool preferX) {
            int fallback = -1;
            const char* primary = preferX ? "GrassCoverChalkTexX" : "GrassCoverChalkTexZ";
            const char* secondary = preferX ? "GrassCoverChalkTexZ" : "GrassCoverChalkTexX";
            for (const auto& proto : prototypes) {
                if (!proto.isBlock) continue;
                if (proto.name == primary) return proto.prototypeID;
                if (fallback < 0 && proto.name == secondary) fallback = proto.prototypeID;
            }
            return fallback;
        }

        int resolveWorkbenchPrototypeID(const std::vector<Entity>& prototypes, const glm::vec3& facingDir) {
            int posX = -1;
            int negX = -1;
            int posZ = -1;
            int negZ = -1;
            for (const auto& proto : prototypes) {
                if (!proto.isBlock) continue;
                if (proto.name == "WorkbenchTexPosX") posX = proto.prototypeID;
                else if (proto.name == "WorkbenchTexNegX") negX = proto.prototypeID;
                else if (proto.name == "WorkbenchTexPosZ") posZ = proto.prototypeID;
                else if (proto.name == "WorkbenchTexNegZ") negZ = proto.prototypeID;
            }

            const glm::vec3 dir = normalizeOrDefault(facingDir, glm::vec3(0.0f, 0.0f, 1.0f));
            if (std::abs(dir.x) >= std::abs(dir.z)) {
                if (dir.x >= 0.0f && posX >= 0) return posX;
                if (dir.x < 0.0f && negX >= 0) return negX;
            } else {
                if (dir.z >= 0.0f && posZ >= 0) return posZ;
                if (dir.z < 0.0f && negZ >= 0) return negZ;
            }
            if (posX >= 0) return posX;
            if (negX >= 0) return negX;
            if (posZ >= 0) return posZ;
            return negZ;
        }

        bool isValidChalkCraftRing(const BaseSystem& baseSystem,
                                   const LevelContext& level,
                                   const std::vector<Entity>& prototypes,
                                   int worldIndex,
                                   const glm::ivec3& centerCell) {
            for (int dz = -2; dz <= 2; ++dz) {
                for (int dx = -2; dx <= 2; ++dx) {
                    if (std::abs(dx) != 2 && std::abs(dz) != 2) continue;
                    const glm::ivec3 cell = centerCell + glm::ivec3(dx, 0, dz);
                    CellBlockInfo info;
                    if (!queryBlockAtCell(baseSystem, level, prototypes, worldIndex, cell, &info)) return false;
                    if (!info.present || !isChalkDustPrototypeID(prototypes, info.prototypeID)) return false;
                }
            }
            return true;
        }

        int chooseChalkDustTile(bool north, bool east, bool south, bool west, int& outQuarterTurns) {
            const int count = static_cast<int>(north) + static_cast<int>(east) + static_cast<int>(south) + static_cast<int>(west);
            outQuarterTurns = 0;
            if (count <= 0) return CHALK_TILE_DOT;
            if (count == 4) return CHALK_TILE_CROSS;

            if (count == 1) {
                if (west) outQuarterTurns = 0;
                else if (north) outQuarterTurns = 1;
                else if (east) outQuarterTurns = 2;
                else outQuarterTurns = 3;
                return CHALK_TILE_END;
            }

            if (count == 2) {
                if (west && east) {
                    outQuarterTurns = 0;
                    return CHALK_TILE_STRAIGHT;
                }
                if (north && south) {
                    outQuarterTurns = 1;
                    return CHALK_TILE_STRAIGHT;
                }
                // Corner base orientation (turn=0) is south+east (bottom+right).
                if (south && east) outQuarterTurns = 0;
                else if (west && south) outQuarterTurns = 1;
                else if (west && north) outQuarterTurns = 2;
                else outQuarterTurns = 3; // north+east
                outQuarterTurns = (outQuarterTurns + 2) & 3;
                return CHALK_TILE_CORNER;
            }

            if (!north) outQuarterTurns = 0;
            else if (!east) outQuarterTurns = 1;
            else if (!south) outQuarterTurns = 2;
            else outQuarterTurns = 3;
            outQuarterTurns = (outQuarterTurns + 2) & 3;
            return CHALK_TILE_T;
        }

        bool isChalkDustAtCell(const BaseSystem& baseSystem,
                               const LevelContext& level,
                               const std::vector<Entity>& prototypes,
                               int worldIndex,
                               const glm::ivec3& cell) {
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) return false;
            if (baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                const uint32_t id = baseSystem.voxelWorld->getBlockWorld(cell);
                return id > 0
                    && id < prototypes.size()
                    && isChalkDustPrototypeID(prototypes, static_cast<int>(id));
            }
            CellBlockInfo info;
            if (!queryBlockAtCell(baseSystem, level, prototypes, worldIndex, cell, &info)) return false;
            return info.present && isChalkDustPrototypeID(prototypes, info.prototypeID);
        }

        void refreshChalkDustCell(BaseSystem& baseSystem,
                                  LevelContext& level,
                                  std::vector<Entity>& prototypes,
                                  int worldIndex,
                                  const glm::ivec3& cell) {
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) return;
            // Keep voxel-mode chalk authoritative by pruning legacy per-instance chalk at touched cells.
            pruneLegacyChalkDustInstancesAtCell(baseSystem, level, prototypes, worldIndex, cell);
            if (baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                const uint32_t id = baseSystem.voxelWorld->getBlockWorld(cell);
                if (!(id > 0 && id < prototypes.size())) return;
                if (!isChalkDustPrototypeID(prototypes, static_cast<int>(id))) return;

                const bool north = isChalkDustAtCell(baseSystem, level, prototypes, worldIndex, cell + glm::ivec3(0, 0, -1));
                const bool east  = isChalkDustAtCell(baseSystem, level, prototypes, worldIndex, cell + glm::ivec3(1, 0, 0));
                const bool south = isChalkDustAtCell(baseSystem, level, prototypes, worldIndex, cell + glm::ivec3(0, 0, 1));
                const bool west  = isChalkDustAtCell(baseSystem, level, prototypes, worldIndex, cell + glm::ivec3(-1, 0, 0));

                int quarterTurns = 0;
                const int tileIndex = chooseChalkDustTile(north, east, south, west, quarterTurns);
                const uint32_t packed = packChalkDustSnapshotColor(tileIndex, quarterTurns);
                baseSystem.voxelWorld->setBlockWorld(cell, id, packed);
                VoxelMeshingSystemLogic::RequestPriorityVoxelRemesh(baseSystem, prototypes, cell);
                StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, worldIndex, glm::vec3(cell));
                return;
            }

            CellBlockInfo info;
            if (!queryBlockAtCell(baseSystem, level, prototypes, worldIndex, cell, &info)) return;
            if (!info.present || !isChalkDustPrototypeID(prototypes, info.prototypeID)) return;

            const bool north = isChalkDustAtCell(baseSystem, level, prototypes, worldIndex, cell + glm::ivec3(0, 0, -1));
            const bool east  = isChalkDustAtCell(baseSystem, level, prototypes, worldIndex, cell + glm::ivec3(1, 0, 0));
            const bool south = isChalkDustAtCell(baseSystem, level, prototypes, worldIndex, cell + glm::ivec3(0, 0, 1));
            const bool west  = isChalkDustAtCell(baseSystem, level, prototypes, worldIndex, cell + glm::ivec3(-1, 0, 0));

            int quarterTurns = 0;
            const int tileIndex = chooseChalkDustTile(north, east, south, west, quarterTurns);
            const uint32_t packed = packChalkDustSnapshotColor(tileIndex, quarterTurns);

            if (info.fromVoxel && baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                baseSystem.voxelWorld->setBlockWorld(
                    cell,
                    static_cast<uint32_t>(info.prototypeID),
                    packed
                );
                VoxelMeshingSystemLogic::RequestPriorityVoxelRemesh(baseSystem, prototypes, cell);
                StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, worldIndex, glm::vec3(cell));
                return;
            }

            Entity& world = level.worlds[static_cast<size_t>(worldIndex)];
            if (info.instanceIndex >= world.instances.size()) return;
            world.instances[info.instanceIndex].color = glm::vec3(1.0f);
            StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, worldIndex, glm::vec3(cell));
        }

        void refreshChalkDustNeighborhood(BaseSystem& baseSystem,
                                          LevelContext& level,
                                          std::vector<Entity>& prototypes,
                                          int worldIndex,
                                          const glm::ivec3& centerCell) {
            refreshChalkDustCell(baseSystem, level, prototypes, worldIndex, centerCell);
            refreshChalkDustCell(baseSystem, level, prototypes, worldIndex, centerCell + glm::ivec3(0, 0, -1));
            refreshChalkDustCell(baseSystem, level, prototypes, worldIndex, centerCell + glm::ivec3(1, 0, 0));
            refreshChalkDustCell(baseSystem, level, prototypes, worldIndex, centerCell + glm::ivec3(0, 0, 1));
            refreshChalkDustCell(baseSystem, level, prototypes, worldIndex, centerCell + glm::ivec3(-1, 0, 0));
        }

