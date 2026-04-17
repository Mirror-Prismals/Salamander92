        void collectPineNubPlacements(int worldX,
                                      int worldZ,
                                      const PineSpec& spec,
                                      std::vector<PineNubPlacement>& outNubs) {
            outNubs.clear();
            const uint32_t baseSeed = hash2D(worldX + 104729, worldZ - 130363);
            const int nubCount = static_cast<int>((baseSeed >> 2u) & 0x3u); // 0..3
            if (nubCount <= 0) return;

            std::array<int, 3> bands = {0, 1, 2}; // low, mid, high
            for (int i = 0; i < 3; ++i) {
                uint32_t shuf = hash3D(worldX + 37 * i, worldZ - 53 * i, 911 + 71 * i);
                int j = i + static_cast<int>(shuf % static_cast<uint32_t>(3 - i));
                std::swap(bands[i], bands[j]);
            }

            const int safeTop = pineNubMaxTrunkOffsetY(spec);
            const int minOffset = std::min(std::max(3, spec.trunkHeight / 6), safeTop);
            const int span = safeTop - minOffset + 1;
            if (span <= 0) return;
            std::array<std::pair<int, int>, 3> bandRanges;
            for (int band = 0; band < 3; ++band) {
                int hMin = minOffset + (span * band) / 3;
                int hMax = minOffset + (span * (band + 1)) / 3 - 1;
                if (band == 2) hMax = safeTop;
                if (hMax < hMin) hMax = hMin;
                bandRanges[static_cast<size_t>(band)] = std::make_pair(hMin, std::min(hMax, safeTop));
            }

            const std::array<glm::ivec2, 4> dirs = {
                glm::ivec2(1, 0), glm::ivec2(-1, 0),
                glm::ivec2(0, 1), glm::ivec2(0, -1)
            };
            std::array<bool, 4> dirUsed = {false, false, false, false};
            outNubs.reserve(static_cast<size_t>(nubCount));

            for (int i = 0; i < nubCount; ++i) {
                int band = bands[static_cast<size_t>(i)];
                int hMin = bandRanges[static_cast<size_t>(band)].first;
                int hMax = bandRanges[static_cast<size_t>(band)].second;
                if (hMax < hMin) std::swap(hMin, hMax);
                const uint32_t hHash = hash3D(worldX + 17 * (i + 1), worldZ - 29 * (i + 1), 733 + band * 97);
                const int trunkOffsetY = hMin + static_cast<int>(hHash % static_cast<uint32_t>(hMax - hMin + 1));

                int dirIndex = static_cast<int>(hash3D(worldX - 31 * (i + 1), worldZ + 47 * (i + 1), 1181 + band * 53) % 4u);
                for (int attempt = 0; attempt < 4; ++attempt) {
                    int candidate = (dirIndex + attempt) % 4;
                    if (!dirUsed[static_cast<size_t>(candidate)]) {
                        dirIndex = candidate;
                        break;
                    }
                }
                dirUsed[static_cast<size_t>(dirIndex)] = true;
                outNubs.push_back({trunkOffsetY, dirs[static_cast<size_t>(dirIndex)]});
            }
        }

        const std::vector<glm::ivec3>& pineCanopyOffsets(const PineSpec& spec) {
            static std::unordered_map<std::uint64_t, std::vector<glm::ivec3>> offsetsCache;
            auto makeSignature = [&](const PineSpec& s) -> std::uint64_t {
                std::uint64_t sig = 1469598103934665603ull;
                auto mix = [&](std::uint64_t v) {
                    sig ^= v;
                    sig *= 1099511628211ull;
                };
                auto mixInt = [&](int v) { mix(static_cast<std::uint64_t>(static_cast<std::uint32_t>(v))); };
                auto mixFloat = [&](float v) {
                    std::uint32_t bits = 0;
                    std::memcpy(&bits, &v, sizeof(bits));
                    mix(static_cast<std::uint64_t>(bits));
                };
                mixInt(s.trunkHeight);
                mixInt(s.canopyOffset);
                mixInt(s.canopyLayers);
                mixFloat(s.canopyBottomRadius);
                mixFloat(s.canopyTopRadius);
                mixInt(s.canopyLowerExtension);
                mixFloat(s.canopyLowerRadiusBoost);
                return sig;
            };
            const std::uint64_t sig = makeSignature(spec);
            auto cached = offsetsCache.find(sig);
            if (cached != offsetsCache.end()) return cached->second;

            std::vector<glm::ivec3> offsets;

            const int canopyBase = spec.trunkHeight - spec.canopyOffset;
            for (int layer = -spec.canopyLowerExtension; layer < spec.canopyLayers; ++layer) {
                float radius = spec.canopyBottomRadius;
                if (layer < 0) {
                    float depthT = (spec.canopyLowerExtension > 0)
                        ? static_cast<float>(-layer) / static_cast<float>(spec.canopyLowerExtension)
                        : 0.0f;
                    radius += spec.canopyLowerRadiusBoost * depthT;
                } else {
                    float t = (spec.canopyLayers > 1)
                        ? static_cast<float>(layer) / static_cast<float>(spec.canopyLayers - 1)
                        : 0.0f;
                    radius = spec.canopyBottomRadius + t * (spec.canopyTopRadius - spec.canopyBottomRadius);
                }
                int r = static_cast<int>(std::ceil(radius));
                int y = canopyBase + layer;
                for (int dz = -r; dz <= r; ++dz) {
                    for (int dx = -r; dx <= r; ++dx) {
                        float dist = std::sqrt(static_cast<float>(dx * dx + dz * dz));
                        if (dist <= radius) {
                            offsets.push_back(glm::ivec3(dx, y, dz));
                        }
                    }
                }
            }
            auto inserted = offsetsCache.emplace(sig, std::move(offsets));
            return inserted.first->second;
        }

        bool cellBelongsToSection(const glm::ivec3& worldCell, const glm::ivec3& sectionCoord, int sectionSize) {
            return floorDivInt(worldCell.x, sectionSize) == sectionCoord.x
                && floorDivInt(worldCell.y, sectionSize) == sectionCoord.y
                && floorDivInt(worldCell.z, sectionSize) == sectionCoord.z;
        }

        bool isTrunkPrototype(uint32_t id, int trunkPrototypeIDA, int trunkPrototypeIDB) {
            if (trunkPrototypeIDA >= 0 && static_cast<int>(id) == trunkPrototypeIDA) return true;
            if (trunkPrototypeIDB >= 0 && static_cast<int>(id) == trunkPrototypeIDB) return true;
            return false;
        }

        bool hasNearbyConflictingTrunk(const VoxelWorldContext& voxelWorld,
                                       int sectionTier,
                                       int trunkPrototypeIDA,
                                       int trunkPrototypeIDB,
                                       int baseX,
                                       int baseY,
                                       int baseZ,
                                       int radius) {
            for (int dz = -radius; dz <= radius; ++dz) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (dx == 0 && dz == 0) continue; // same tree trunk column
                    for (int dy = 0; dy < 6; ++dy) {
                        uint32_t id = getBlockAt(voxelWorld, glm::ivec3(baseX + dx, baseY + dy, baseZ + dz));
                        if (isTrunkPrototype(id, trunkPrototypeIDA, trunkPrototypeIDB)) return true;
                    }
                }
            }
            return false;
        }

        bool trunkColumnCanExist(const VoxelWorldContext& voxelWorld,
                                 int sectionTier,
                                 int trunkPrototypeIDA,
                                 int trunkPrototypeIDB,
                                 int worldX,
                                 int groundY,
                                 int worldZ,
                                 int trunkHeight) {
            // Require valid terrain support under the trunk.
            if (getBlockAt(voxelWorld, glm::ivec3(worldX, groundY, worldZ)) == 0u) return false;
            for (int i = 1; i <= trunkHeight; ++i) {
                glm::ivec3 pos(worldX, groundY + i, worldZ);
                uint32_t id = getBlockAt(voxelWorld, pos);
                if (id == 0) continue;
                if (isTrunkPrototype(id, trunkPrototypeIDA, trunkPrototypeIDB)) continue;
                return false;
            }
            return true;
        }

        int horizontalLogPrototypeFor(const std::vector<Entity>& prototypes,
                                      int sourcePrototypeID,
                                      FallDirection direction);
        int horizontalNubLogPrototypeFor(const std::vector<Entity>& prototypes,
                                         int sourcePrototypeID,
                                         FallDirection direction);
        int topLogPrototypeFor(const std::vector<Entity>& prototypes,
                               int sourcePrototypeID);
        bool leafCellTouchesAir(const VoxelWorldContext& voxelWorld,
                                int sectionTier,
                                const glm::ivec3& cell) {
            static const std::array<glm::ivec3, 6> kNeighborDirs = {
                glm::ivec3(1, 0, 0),
                glm::ivec3(-1, 0, 0),
                glm::ivec3(0, 1, 0),
                glm::ivec3(0, -1, 0),
                glm::ivec3(0, 0, 1),
                glm::ivec3(0, 0, -1)
            };
            for (const glm::ivec3& dir : kNeighborDirs) {
                if (getBlockAt(voxelWorld, cell + dir) == 0u) {
                    return true;
                }
            }
            return false;
        }

        void applyLeafFanShell(VoxelWorldContext& voxelWorld,
                               int sectionTier,
                               int sectionSize,
                               int leafFanPrototypeID,
                               uint32_t leafColor,
                               const std::vector<glm::ivec3>& placedLeafCells,
                               std::unordered_set<glm::ivec3, IVec3Hash>& outTouchedSections,
                               bool& modified) {
            if (leafFanPrototypeID < 0 || placedLeafCells.empty()) return;
            for (const glm::ivec3& cell : placedLeafCells) {
                if (!leafCellTouchesAir(voxelWorld, sectionTier, cell)) continue;
                if (getBlockAt(voxelWorld, cell) == 0u) continue;
                voxelWorld.setBlock(cell,
                    static_cast<uint32_t>(leafFanPrototypeID),
                    leafColor,
                    false
                );
                outTouchedSections.insert(glm::ivec3(
                    floorDivInt(cell.x, sectionSize),
                    floorDivInt(cell.y, sectionSize),
                    floorDivInt(cell.z, sectionSize)
                ));
                modified = true;
            }
        }

        void convertExposedLeafShellInSection(VoxelWorldContext& voxelWorld,
                                              const std::vector<Entity>& prototypes,
                                              int sectionTier,
                                              const glm::ivec3& sectionCoord,
                                              int sectionSize,
                                              int pineLeafPrototypeID,
                                              int pineLeafFanPrototypeID,
                                              int oakLeafFanPrototypeID,
                                              uint32_t leafColor,
                                              std::unordered_set<glm::ivec3, IVec3Hash>& outTouchedSections,
                                              bool& modified) {
            if (!voxelWorld.enabled || sectionSize <= 0) return;
            if (pineLeafFanPrototypeID < 0 && oakLeafFanPrototypeID < 0) return;

            const int minX = sectionCoord.x * sectionSize;
            const int minY = sectionCoord.y * sectionSize;
            const int minZ = sectionCoord.z * sectionSize;
            const int maxX = minX + sectionSize - 1;
            const int maxY = minY + sectionSize - 1;
            const int maxZ = minZ + sectionSize - 1;

            for (int y = minY; y <= maxY; ++y) {
                for (int z = minZ; z <= maxZ; ++z) {
                    for (int x = minX; x <= maxX; ++x) {
                        const glm::ivec3 cell(x, y, z);
                        const uint32_t id = getBlockAt(voxelWorld, cell);
                        if (id == 0u || id >= prototypes.size()) continue;

                        int targetFanPrototypeID = -1;
                        if (pineLeafFanPrototypeID >= 0
                            && pineLeafPrototypeID >= 0
                            && static_cast<int>(id) == pineLeafPrototypeID) {
                            targetFanPrototypeID = pineLeafFanPrototypeID;
                        } else if (oakLeafFanPrototypeID >= 0
                                   && isJungleLeafPrototypeName(prototypes[static_cast<size_t>(id)].name)) {
                            targetFanPrototypeID = oakLeafFanPrototypeID;
                        } else {
                            continue;
                        }

                        if (!leafCellTouchesAir(voxelWorld, sectionTier, cell)) continue;
                        voxelWorld.setBlock(cell,
                            static_cast<uint32_t>(targetFanPrototypeID),
                            leafColor,
                            false
                        );
                        outTouchedSections.insert(sectionCoord);
                        modified = true;
                    }
                }
            }
        }

        void writeTreeToWorld(VoxelWorldContext& voxelWorld,
                              const std::vector<Entity>& prototypes,
                              int sectionTier,
                              int sectionSize,
                              const glm::ivec3& rootSectionCoord,
                              int trunkPrototypeID,
                              int topPrototypeID,
                              int nubPrototypeID,
                              int leafPrototypeID,
                              int leafFanPrototypeID,
                              uint32_t trunkColor,
                              uint32_t leafColor,
                              int worldX,
                              int groundY,
                              int worldZ,
                              const PineSpec& spec,
                              std::unordered_set<glm::ivec3, IVec3Hash>& outTouchedSections,
                              bool& modified) {
            (void)rootSectionCoord;
            auto setIfEmpty = [&](const glm::ivec3& cell, uint32_t id, uint32_t color) -> bool {
                if (getBlockAt(voxelWorld, cell) != 0u) return false;
                voxelWorld.setBlock(cell, id, color, false);
                outTouchedSections.insert(glm::ivec3(
                    floorDivInt(cell.x, sectionSize),
                    floorDivInt(cell.y, sectionSize),
                    floorDivInt(cell.z, sectionSize)
                ));
                modified = true;
                return true;
            };

            for (int i = 1; i <= spec.trunkHeight; ++i) {
                const uint32_t placeID = (i == spec.trunkHeight && topPrototypeID >= 0)
                    ? static_cast<uint32_t>(topPrototypeID)
                    : static_cast<uint32_t>(trunkPrototypeID);
                setIfEmpty(glm::ivec3(worldX, groundY + i, worldZ),
                           placeID,
                           trunkColor);
            }

            std::vector<PineNubPlacement> nubs;
            collectPineNubPlacements(worldX, worldZ, spec, nubs);
            for (const auto& nub : nubs) {
                const int trunkY = groundY + nub.trunkOffsetY;
                if (trunkY <= groundY + 1 || trunkY >= groundY + spec.trunkHeight) continue;
                glm::ivec3 nubCell(worldX + nub.dir.x, trunkY, worldZ + nub.dir.y);
                FallDirection axisDir = (nub.dir.x != 0) ? FallDirection::PosX : FallDirection::PosZ;
                int horizontalID = horizontalNubLogPrototypeFor(prototypes, nubPrototypeID, axisDir);
                if (horizontalID < 0) horizontalID = trunkPrototypeID;
                setIfEmpty(nubCell, static_cast<uint32_t>(horizontalID), trunkColor);
            }

            const auto& canopy = pineCanopyOffsets(spec);
            std::vector<glm::ivec3> placedLeafCells;
            placedLeafCells.reserve(canopy.size());
            for (const auto& off : canopy) {
                glm::ivec3 cell(worldX + off.x, groundY + off.y, worldZ + off.z);
                // Keep canopy above trunk base to preserve pine silhouette.
                if (cell.y <= groundY + 1) continue;
                if (setIfEmpty(cell, static_cast<uint32_t>(leafPrototypeID), leafColor)) {
                    placedLeafCells.push_back(cell);
                }
            }
            applyLeafFanShell(
                voxelWorld,
                sectionTier,
                sectionSize,
                leafFanPrototypeID,
                leafColor,
                placedLeafCells,
                outTouchedSections,
                modified
            );
        }

        void writeBareTreeToWorld(VoxelWorldContext& voxelWorld,
                                  int sectionTier,
                                  int sectionSize,
                                  const glm::ivec3& rootSectionCoord,
                                  int trunkPrototypeID,
                                  int branchPrototypeIDX,
                                  int branchPrototypeIDZ,
                                  int wallBranchLongPosXID,
                                  int wallBranchLongNegXID,
                                  int wallBranchLongPosZID,
                                  int wallBranchLongNegZID,
                                  int wallBranchLongTipPosXID,
                                  int wallBranchLongTipNegXID,
                                  int wallBranchLongTipPosZID,
                                  int wallBranchLongTipNegZID,
                                  uint32_t trunkColor,
                                  uint32_t branchColor,
                                  int worldX,
                                  int groundY,
                                  int worldZ,
                                  int trunkHeight,
                                  uint32_t seed,
                                  std::unordered_set<glm::ivec3, IVec3Hash>& outTouchedSections,
                                  bool& modified) {
            if (trunkPrototypeID < 0 || trunkHeight < 2) return;
            (void)rootSectionCoord;
            auto setIfEmpty = [&](const glm::ivec3& cell, uint32_t id, uint32_t color) -> bool {
                if (getBlockAt(voxelWorld, cell) != 0u) return false;
                voxelWorld.setBlock(cell, id, color, false);
                outTouchedSections.insert(glm::ivec3(
                    floorDivInt(cell.x, sectionSize),
                    floorDivInt(cell.y, sectionSize),
                    floorDivInt(cell.z, sectionSize)
                ));
                modified = true;
                return true;
            };

            for (int i = 1; i <= trunkHeight; ++i) {
                setIfEmpty(
                    glm::ivec3(worldX, groundY + i, worldZ),
                    static_cast<uint32_t>(trunkPrototypeID),
                    trunkColor
                );
            }

            const std::array<glm::ivec2, 4> dirs = {
                glm::ivec2(1, 0),
                glm::ivec2(-1, 0),
                glm::ivec2(0, 1),
                glm::ivec2(0, -1)
            };
            auto longBranchIDForDir = [&](const glm::ivec2& dir,
                                          int posXID,
                                          int negXID,
                                          int posZID,
                                          int negZID) {
                if (dir.x > 0) return negXID;
                if (dir.x < 0) return posXID;
                if (dir.y > 0) return negZID;
                if (dir.y < 0) return posZID;
                return -1;
            };
            const int branchHeightMin = std::max(2, trunkHeight / 3);
            const int branchHeightMax = std::max(branchHeightMin, trunkHeight - 2);
            const int branchCount = 1 + static_cast<int>((seed >> 3u) & 1u); // 1..2
            std::array<bool, 4> usedDirs = {false, false, false, false};
            for (int i = 0; i < branchCount; ++i) {
                const uint32_t branchSeed = hash3D(worldX + 911 * (i + 1), groundY + 67 * (i + 1), worldZ - 503 * (i + 1));
                const int branchHeight = branchHeightMin + static_cast<int>(
                    branchSeed % static_cast<uint32_t>(branchHeightMax - branchHeightMin + 1)
                );
                int dirIdx = static_cast<int>((branchSeed >> 9u) % dirs.size());
                for (int k = 0; k < 4; ++k) {
                    const int candidate = (dirIdx + k) % 4;
                    if (!usedDirs[static_cast<size_t>(candidate)]) {
                        dirIdx = candidate;
                        break;
                    }
                }
                usedDirs[static_cast<size_t>(dirIdx)] = true;
                const glm::ivec2 dir = dirs[static_cast<size_t>(dirIdx)];
                const glm::ivec3 shortBranchCell(worldX + dir.x, groundY + branchHeight, worldZ + dir.y);
                int shortBranchID = (dir.x != 0) ? branchPrototypeIDX : branchPrototypeIDZ;
                if (shortBranchID < 0) shortBranchID = (branchPrototypeIDX >= 0) ? branchPrototypeIDX : branchPrototypeIDZ;
                if (shortBranchID >= 0) {
                    setIfEmpty(shortBranchCell, static_cast<uint32_t>(shortBranchID), branchColor);
                }

                // Two-piece long branch set: piece 1 then piece 2 shifted +1 in the branch direction and +1 up.
                if (((branchSeed >> 17u) % 100u) < 60u) {
                    const int longBaseID = longBranchIDForDir(
                        dir,
                        wallBranchLongPosXID,
                        wallBranchLongNegXID,
                        wallBranchLongPosZID,
                        wallBranchLongNegZID
                    );
                    const int longTipID = longBranchIDForDir(
                        dir,
                        wallBranchLongTipPosXID,
                        wallBranchLongTipNegXID,
                        wallBranchLongTipPosZID,
                        wallBranchLongTipNegZID
                    );
                    if (longBaseID >= 0 && longTipID >= 0) {
                        const glm::ivec3 longBaseCell(worldX + dir.x, groundY + branchHeight + 1, worldZ + dir.y);
                        const glm::ivec3 longTipCell(worldX + dir.x * 2, groundY + branchHeight + 2, worldZ + dir.y * 2);
                        if (setIfEmpty(longBaseCell, static_cast<uint32_t>(longBaseID), branchColor)) {
                            setIfEmpty(longTipCell, static_cast<uint32_t>(longTipID), branchColor);
                        }
                    }
                }
            }
        }

        void writeJungleTreeToWorld(VoxelWorldContext& voxelWorld,
                                    int sectionTier,
                                    int sectionSize,
                                    const glm::ivec3& rootSectionCoord,
                                    int trunkPrototypeID,
                                    int defaultLeafPrototypeID,
                                    int leafFanPrototypeID,
                                    const std::array<int, kJungleLeafVariantCount>& jungleLeafPrototypeIDs,
                                    int jungleLeafPrototypeCount,
                                    uint32_t trunkColor,
                                    uint32_t leafColor,
                                    int worldX,
                                    int groundY,
                                    int worldZ,
                                    int trunkHeight,
                                    int canopyRadius,
                                    std::unordered_set<glm::ivec3, IVec3Hash>& outTouchedSections,
                                    bool& modified) {
            (void)rootSectionCoord;
            auto setIfEmpty = [&](const glm::ivec3& cell, uint32_t id, uint32_t color) -> bool {
                if (getBlockAt(voxelWorld, cell) != 0u) return false;
                voxelWorld.setBlock(cell, id, color, false);
                outTouchedSections.insert(glm::ivec3(
                    floorDivInt(cell.x, sectionSize),
                    floorDivInt(cell.y, sectionSize),
                    floorDivInt(cell.z, sectionSize)
                ));
                modified = true;
                return true;
            };

            for (int i = 1; i <= trunkHeight; ++i) {
                setIfEmpty(glm::ivec3(worldX, groundY + i, worldZ),
                           static_cast<uint32_t>(trunkPrototypeID),
                           trunkColor);
            }

            const int centerY = groundY + trunkHeight + 2;
            const int r = std::max(2, canopyRadius);
            std::vector<glm::ivec3> placedLeafCells;
            placedLeafCells.reserve(static_cast<size_t>((2 * r + 1) * (2 * r + 1) * (2 * r + 1)));
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    for (int dz = -r; dz <= r; ++dz) {
                        const float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy + dz * dz));
                        if (dist > static_cast<float>(r)) continue;
                        const glm::ivec3 leafCell(worldX + dx, centerY + dy, worldZ + dz);
                        if (leafCell.y <= groundY + 1) continue;
                        int leafPrototypeID = chooseJungleLeafPrototypeIDForCell(
                            jungleLeafPrototypeIDs,
                            jungleLeafPrototypeCount,
                            leafCell
                        );
                        if (leafPrototypeID < 0) leafPrototypeID = defaultLeafPrototypeID;
                        if (leafPrototypeID < 0) continue;
                        if (setIfEmpty(leafCell, static_cast<uint32_t>(leafPrototypeID), leafColor)) {
                            placedLeafCells.push_back(leafCell);
                        }
                    }
                }
            }
            applyLeafFanShell(
                voxelWorld,
                sectionTier,
                sectionSize,
                leafFanPrototypeID,
                leafColor,
                placedLeafCells,
                outTouchedSections,
                modified
            );
        }
