        void writeGroundFoliageToSection(const std::vector<Entity>& prototypes,
                                         const WorldContext& worldCtx,
                                         VoxelWorldContext& voxelWorld,
                                         int sectionTier,
                                         const glm::ivec3& sectionCoord,
                                         int sectionSize,
                                         int sectionScale,
                                         int grassPrototypeID,
                                         int shortGrassPrototypeID,
                                         int grassPrototypeBiome1ID,
                                         int shortGrassPrototypeBiome1ID,
                                         int grassPrototypeBiome3ID,
                                         const std::array<int, 4>& grassPrototypeBiome4IDs,
                                         int grassPrototypeBiome4Count,
                                         int grassCoverPrototypeIDX,
                                         int grassCoverPrototypeIDZ,
                                         int grassCoverPrototypeMeadowIDX,
                                         int grassCoverPrototypeMeadowIDZ,
                                         int grassCoverPrototypeForestIDX,
                                         int grassCoverPrototypeForestIDZ,
                                        int grassCoverPrototypeBiome3IDX,
                                        int grassCoverPrototypeBiome3IDZ,
                                         int grassCoverPrototypeBiome4IDX,
                                         int grassCoverPrototypeBiome4IDZ,
                                         int grassCoverPrototypeBiome2IDX,
                                         int grassCoverPrototypeBiome2IDZ,
                                         int pebblePatchPrototypeX,
                                         int pebblePatchPrototypeZ,
                                         int autumnLeafPatchPrototypeX,
                                         int autumnLeafPatchPrototypeZ,
                                         int deadLeafPatchPrototypeX,
                                         int deadLeafPatchPrototypeZ,
                                         int lilypadPatchPrototypeX,
                                         int lilypadPatchPrototypeZ,
                                         int bareBranchPrototypeX,
                                         int bareBranchPrototypeZ,
                                         int miniPineBottomPrototypeID,
                                         int miniPineTopPrototypeID,
                                         int miniPineTripleBottomPrototypeID,
                                         int miniPineTripleMiddlePrototypeID,
                                         int miniPineTripleTopPrototypeID,
                                         int flaxDoubleBottomPrototypeID,
                                         int flaxDoubleTopPrototypeID,
                                         int hempDoubleBottomPrototypeID,
                                         int hempDoubleTopPrototypeID,
                                        const std::array<int, 16>& blueFlowerPrototypeIDs,
                                        int blueFlowerPrototypeCount,
                                         int blueRareFlowerPrototypeID,
                                         int flaxRareFlowerPrototypeID,
                                         int hempRareFlowerPrototypeID,
                                         int fernRareFlowerPrototypeID,
                                         int flowerPrototypeID,
                                         int succulentPrototypeID,
                                         int succulentPrototypeVariantID,
                                         int jungleOrangeUnderLeafPrototypeID,
                                         int stickPrototypeIDX,
                                         int stickPrototypeIDZ,
                                         int winterStickPrototypeIDX,
                                         int winterStickPrototypeIDZ,
                                         int leafPrototypeID,
                                         int waterPrototypeID,
                                         const FoliageSpec& spec,
                                         bool& unresolvedDependencies,
                                         bool& modified) {
            (void)unresolvedDependencies;
            if (!spec.enabled) return;
            const bool hasAnyGrassPrototype =
                (grassPrototypeID >= 0 || shortGrassPrototypeID >= 0
                    || grassPrototypeBiome1ID >= 0 || shortGrassPrototypeBiome1ID >= 0
                    || grassPrototypeBiome3ID >= 0
                    || grassPrototypeBiome4Count > 0);
            const bool hasAnyGrassCoverPrototype =
                (grassCoverPrototypeIDX >= 0 || grassCoverPrototypeIDZ >= 0
                    || grassCoverPrototypeMeadowIDX >= 0 || grassCoverPrototypeMeadowIDZ >= 0
                    || grassCoverPrototypeForestIDX >= 0 || grassCoverPrototypeForestIDZ >= 0
                    || grassCoverPrototypeBiome3IDX >= 0 || grassCoverPrototypeBiome3IDZ >= 0
                    || grassCoverPrototypeBiome4IDX >= 0 || grassCoverPrototypeBiome4IDZ >= 0
                    || grassCoverPrototypeBiome2IDX >= 0 || grassCoverPrototypeBiome2IDZ >= 0);
            const bool hasAnyStickPrototype =
                (stickPrototypeIDX >= 0 || stickPrototypeIDZ >= 0
                    || winterStickPrototypeIDX >= 0 || winterStickPrototypeIDZ >= 0);
            const bool hasAnyGrassOrCoverPrototype = hasAnyGrassPrototype || hasAnyGrassCoverPrototype;
            std::array<int, kRareFlowerPoolMax> rareFlowerPrototypeIDs{};
            int rareFlowerPrototypeCount = 0;
            for (int i = 0;
                 i < blueFlowerPrototypeCount && rareFlowerPrototypeCount < static_cast<int>(rareFlowerPrototypeIDs.size());
                 ++i) {
                const int id = blueFlowerPrototypeIDs[static_cast<size_t>(i)];
                if (id < 0) continue;
                rareFlowerPrototypeIDs[static_cast<size_t>(rareFlowerPrototypeCount)] = id;
                rareFlowerPrototypeCount += 1;
            }
            auto addRareFlowerToPool = [&](int id) {
                if (id < 0 || rareFlowerPrototypeCount >= static_cast<int>(rareFlowerPrototypeIDs.size())) return;
                rareFlowerPrototypeIDs[static_cast<size_t>(rareFlowerPrototypeCount)] = id;
                rareFlowerPrototypeCount += 1;
            };
            addRareFlowerToPool(blueRareFlowerPrototypeID);
            addRareFlowerToPool(flaxRareFlowerPrototypeID);
            addRareFlowerToPool(hempRareFlowerPrototypeID);
            addRareFlowerToPool(fernRareFlowerPrototypeID);
            const bool hasAnyRareFlower = rareFlowerPrototypeCount > 0;
            const bool hasAnySucculentPrototype =
                (succulentPrototypeID >= 0)
                || (succulentPrototypeVariantID >= 0);
            const bool hasAnyFlowerPrototype =
                hasAnyRareFlower
                || (flowerPrototypeID >= 0)
                || hasAnySucculentPrototype
                || (jungleOrangeUnderLeafPrototypeID >= 0);
            const bool hasMiniPine = miniPineBottomPrototypeID >= 0 && miniPineTopPrototypeID >= 0;
            const bool hasMiniPineTriple =
                miniPineTripleBottomPrototypeID >= 0
                && miniPineTripleMiddlePrototypeID >= 0
                && miniPineTripleTopPrototypeID >= 0;
            const bool hasFlaxDouble = flaxDoubleBottomPrototypeID >= 0 && flaxDoubleTopPrototypeID >= 0;
            const bool hasHempDouble = hempDoubleBottomPrototypeID >= 0 && hempDoubleTopPrototypeID >= 0;
            const int miniPineModulo = std::max(1, spec.miniPineSpawnModulo);
            const int miniPineTripleModulo = std::max(
                1,
                miniPineModulo * std::max(1, kMiniPineRarityMultiplier / 10)
            );
            const int tallDoublePlantModulo = std::max(1, spec.flowerSpawnModulo * kBlueFlowerRareChance);
            const int rareFlowerSpawnModulo = std::max(1, spec.flowerSpawnModulo * kBlueFlowerRareChance);
            const bool hasLilypadPrototype = (lilypadPatchPrototypeX >= 0 || lilypadPatchPrototypeZ >= 0);
            if ((!spec.grassEnabled || !hasAnyGrassOrCoverPrototype)
                && (!spec.flowerEnabled || !hasAnyFlowerPrototype)
                && (!spec.lilypadPatchEnabled || !hasLilypadPrototype)) return;

            const int minX = sectionCoord.x * sectionSize;
            const int minZ = sectionCoord.z * sectionSize;
            const int minY = sectionCoord.y * sectionSize;
            const int maxX = minX + sectionSize - 1;
            const int maxY = minY + sectionSize - 1;
            const int maxZ = minZ + sectionSize - 1;
            const int pebbleNearWaterRadiusTier = std::max(
                1,
                (std::max(1, spec.pebblePatchNearWaterRadius) + sectionScale - 1) / sectionScale
            );
            const int pebbleNearWaterVerticalRangeTier = std::max(
                0,
                (std::max(0, spec.pebblePatchNearWaterVerticalRange) + sectionScale - 1) / sectionScale
            );

            auto isNearSurfaceWater = [&](const glm::ivec3& centerCell) {
                if (waterPrototypeID < 0) return false;
                const int radiusSq = pebbleNearWaterRadiusTier * pebbleNearWaterRadiusTier;
                for (int dz = -pebbleNearWaterRadiusTier; dz <= pebbleNearWaterRadiusTier; ++dz) {
                    for (int dx = -pebbleNearWaterRadiusTier; dx <= pebbleNearWaterRadiusTier; ++dx) {
                        if ((dx * dx + dz * dz) > radiusSq) continue;
                        for (int dy = -pebbleNearWaterVerticalRangeTier; dy <= pebbleNearWaterVerticalRangeTier; ++dy) {
                            const glm::ivec3 probe(centerCell.x + dx, centerCell.y + dy, centerCell.z + dz);
                            if (getBlockAt(voxelWorld, probe) != static_cast<uint32_t>(waterPrototypeID)) {
                                continue;
                            }
                            if (getBlockAt(voxelWorld, probe + glm::ivec3(0, 1, 0)) == 0u) {
                                return true;
                            }
                        }
                    }
                }
                return false;
            };

            for (int tierZ = minZ; tierZ <= maxZ; ++tierZ) {
                for (int tierX = minX; tierX <= maxX; ++tierX) {
                    const int worldX = tierX * sectionScale;
                    const int worldZ = tierZ * sectionScale;
                    const bool islandQuadrants =
                        (worldCtx.expanse.islandRadius > 0.0f) && worldCtx.expanse.secondaryBiomeEnabled;
                    const uint32_t seed = hash2D(worldX, worldZ);
                    const int biomeID = ExpanseBiomeSystemLogic::ResolveBiome(
                        worldCtx,
                        static_cast<float>(worldX),
                        static_cast<float>(worldZ)
                    );
                    if (isWithinJungleVolcano(worldCtx.expanse, biomeID, worldX, worldZ)) {
                        continue;
                    }

                    if (spec.lilypadPatchEnabled && hasLilypadPrototype && waterPrototypeID >= 0) {
                        glm::ivec3 lilypadCell(0);
                        bool foundLilypadCell = false;
                        // Find the highest exposed water cell in this section column.
                        for (int tierY = maxY - 1; tierY >= minY; --tierY) {
                            const glm::ivec3 waterCell(tierX, tierY, tierZ);
                            if (getBlockAt(voxelWorld, waterCell) != static_cast<uint32_t>(waterPrototypeID)) {
                                continue;
                            }
                            const glm::ivec3 candidateCell(tierX, tierY + 1, tierZ);
                            if (!cellBelongsToSection(candidateCell, sectionCoord, sectionSize)) {
                                continue;
                            }
                            if (getBlockAt(voxelWorld, candidateCell) != 0u) {
                                continue;
                            }
                            lilypadCell = candidateCell;
                            foundLilypadCell = true;
                            break;
                        }
                        if (foundLilypadCell) {
                            const int lilypadWorldY = lilypadCell.y * sectionScale;
                            if (lilypadWorldY <= -99) {
                                // Depth rivers use dedicated 2x2 lilypad generation in terrain pass.
                                continue;
                            }
                            const int lilyChance = std::max(0, std::min(100, spec.lilypadPatchPercent));
                            if (lilyChance > 0) {
                                // Lilypads spawn in coarse "veins" (patchy lanes) instead of uniform per-column scatter.
                                // This keeps water surfaces from being covered too evenly.
                                constexpr int kLilypadVeinCellSizeBlocks = 12;
                                constexpr int kLilypadVeinPercentA = 16;
                                constexpr int kLilypadVeinPercentB = 9;
                                constexpr int kLilypadVeinChanceMultiplier = 3;
                                auto floorDiv = [](int v, int d) -> int {
                                    if (d <= 0) return 0;
                                    if (v >= 0) return v / d;
                                    return -(((-v) + d - 1) / d);
                                };
                                const int veinCellX = floorDiv(worldX, kLilypadVeinCellSizeBlocks);
                                const int veinCellZ = floorDiv(worldZ, kLilypadVeinCellSizeBlocks);
                                const uint32_t veinSeedA = hash2D(veinCellX + 9013, veinCellZ + 1733);
                                const uint32_t veinSeedB = hash2D(veinCellX + 4103, veinCellZ + 1297);
                                const bool inLilypadVein =
                                    (static_cast<int>(veinSeedA % 100u) < kLilypadVeinPercentA)
                                    || (static_cast<int>(veinSeedB % 100u) < kLilypadVeinPercentB);
                                if (!inLilypadVein) {
                                    continue;
                                }

                                const uint32_t lilySeed = hash3D(
                                    worldX + 229,
                                    lilypadCell.y * sectionScale + 311,
                                    worldZ + 419
                                );
                                const int lilyVeinChance = std::min(100, lilyChance * kLilypadVeinChanceMultiplier);
                                if (static_cast<int>(lilySeed % 100u) < lilyVeinChance) {
                                    int patchID = ((lilySeed >> 13u) & 1u) == 0u
                                        ? lilypadPatchPrototypeX
                                        : lilypadPatchPrototypeZ;
                                    if (patchID < 0) {
                                        patchID = (lilypadPatchPrototypeX >= 0) ? lilypadPatchPrototypeX : lilypadPatchPrototypeZ;
                                    }
                                    if (patchID >= 0) {
                                        voxelWorld.setBlock(lilypadCell,
                                            static_cast<uint32_t>(patchID),
                                            packColor(glm::vec3(1.0f)),
                                            false
                                        );
                                        modified = true;
                                    }
                                }
                            }
                        }
                    }

                    bool spawnFlower = false;
                    if (spec.flowerEnabled && flowerPrototypeID >= 0 && spec.flowerSpawnModulo > 0) {
                        spawnFlower = (seed % static_cast<uint32_t>(spec.flowerSpawnModulo)) == 0u;
                    }

                    bool spawnGrass = false;
                    if (spec.grassEnabled && hasAnyGrassOrCoverPrototype && spec.grassSpawnModulo > 0) {
                        spawnGrass = ((seed >> 1u) % static_cast<uint32_t>(spec.grassSpawnModulo)) == 0u;
                    }

                    if (spec.temperateOnly) {
                        const bool flowerBiomeAllowed = (biomeID == 0 || biomeID == 1);
                        if (!flowerBiomeAllowed) {
                            spawnFlower = false;
                        }
                    }

                    auto rareRollMatchesIndex = [&](int rareIndex) -> bool {
                        if (rareIndex < 0 || rareIndex >= rareFlowerPrototypeCount) return false;
                        const int protoID = rareFlowerPrototypeIDs[static_cast<size_t>(rareIndex)];
                        if (protoID < 0) return false;
                        if (protoID == blueRareFlowerPrototypeID) {
                            if (biomeID == 2 || biomeID == 3) return false; // desert / jungle
                        }
                        const int salt = 97 * (rareIndex + 1);
                        const uint32_t rareSeed = hash2D(worldX + salt, worldZ - salt * 3);
                        int spawnModulo = rareFlowerSpawnModulo;
                        if (protoID == blueRareFlowerPrototypeID) {
                            spawnModulo = std::max(1, rareFlowerSpawnModulo * kYoungClematisExtraRarityMultiplier);
                        }
                        return (rareSeed % static_cast<uint32_t>(spawnModulo)) == 0u;
                    };

                    bool spawnAnyRareFlower = false;
                    if (spec.flowerEnabled && hasAnyRareFlower) {
                        for (int i = 0; i < rareFlowerPrototypeCount; ++i) {
                            if (rareRollMatchesIndex(i)) {
                                spawnAnyRareFlower = true;
                                break;
                            }
                        }
                    }

                    const bool spawnMiniPine =
                        hasMiniPine
                        && biomeID == 0
                        && ((seed % static_cast<uint32_t>(miniPineModulo)) == 0u);
                    const bool spawnMiniPineTriple =
                        hasMiniPineTriple
                        && biomeID == 0
                        && ((hash2D(worldX + 613, worldZ - 947) % static_cast<uint32_t>(miniPineTripleModulo)) == 0u);
                    const bool spawnFlaxDouble =
                        hasFlaxDouble
                        && ((hash2D(worldX - 1423, worldZ + 2719) % static_cast<uint32_t>(tallDoublePlantModulo)) == 0u);
                    const bool spawnHempDouble =
                        hasHempDouble
                        && ((hash2D(worldX + 3307, worldZ - 811) % static_cast<uint32_t>(tallDoublePlantModulo)) == 0u);
                    const bool spawnSucculent =
                        spec.flowerEnabled
                        && hasAnySucculentPrototype
                        && biomeID == 2
                        && ((hash2D(worldX - 2089, worldZ + 1103) % static_cast<uint32_t>(rareFlowerSpawnModulo)) == 0u);
                    constexpr int kJungleOrangeUnderLeafPercent = 12;
                    const bool spawnJungleOrangeUnderLeaf =
                        spec.flowerEnabled
                        && jungleOrangeUnderLeafPrototypeID >= 0
                        && biomeID == 3
                        && (static_cast<int>(
                            hash2D(worldX + 557, worldZ - 983) % 100u
                        ) < kJungleOrangeUnderLeafPercent);
                    if (!spawnFlower
                        && !spawnGrass
                        && !spawnMiniPine
                        && !spawnMiniPineTriple
                        && !spawnFlaxDouble
                        && !spawnHempDouble
                        && !spawnSucculent
                        && !spawnJungleOrangeUnderLeaf
                        && !spawnAnyRareFlower) continue;

                    float terrainHeight = 0.0f;
                    const bool isLand = ExpanseBiomeSystemLogic::SampleTerrain(
                        worldCtx,
                        static_cast<float>(worldX),
                        static_cast<float>(worldZ),
                        terrainHeight
                    );

                    const int groundWorldY = static_cast<int>(std::floor(terrainHeight));
                    const int groundTierY = floorDivInt(groundWorldY, sectionScale);
                    const glm::ivec3 placeCell(tierX, groundTierY + 1, tierZ);
                    if (!cellBelongsToSection(placeCell, sectionCoord, sectionSize)) continue;

                    const glm::ivec3 groundCell(tierX, groundTierY, tierZ);
                    const uint32_t groundID = getBlockAt(voxelWorld, groundCell);
                    if (groundID == 0u) {
                        continue;
                    }

                    if (!isLand) continue;

                    if (!isFoliageGroundPrototypeID(prototypes, groundID, waterPrototypeID)) continue;

                    const uint32_t placeCellID = getBlockAt(voxelWorld, placeCell);

                    if (spawnJungleOrangeUnderLeaf) {
                        const int canopyRadius = std::max(0, spec.stickCanopySearchRadius / sectionScale);
                        const int canopyHeight = std::max(2, spec.stickCanopySearchHeight / sectionScale);
                        glm::ivec3 hangingCell(0);
                        if (findLeafHangingCellNear(
                                prototypes,
                                voxelWorld,
                                sectionTier,
                                sectionCoord,
                                sectionSize,
                                tierX,
                                groundTierY,
                                tierZ,
                                canopyRadius,
                                canopyHeight,
                                hangingCell)) {
                            voxelWorld.setBlock(hangingCell,
                                static_cast<uint32_t>(jungleOrangeUnderLeafPrototypeID),
                                packColor(glm::vec3(1.0f)),
                                false
                            );
                            modified = true;
                            continue;
                        }
                    }

                    if (placeCellID != 0u) continue;

                    if (spawnMiniPineTriple) {
                        const glm::ivec3 middleCell = placeCell + glm::ivec3(0, 1, 0);
                        const glm::ivec3 topCell = placeCell + glm::ivec3(0, 2, 0);
                        if (!cellBelongsToSection(middleCell, sectionCoord, sectionSize)) continue;
                        if (!cellBelongsToSection(topCell, sectionCoord, sectionSize)) continue;
                        if (getBlockAt(voxelWorld, middleCell) != 0u) continue;
                        if (getBlockAt(voxelWorld, topCell) != 0u) continue;
                        voxelWorld.setBlock(placeCell,
                            static_cast<uint32_t>(miniPineTripleBottomPrototypeID),
                            packColor(glm::vec3(1.0f)),
                            false
                        );
                        voxelWorld.setBlock(middleCell,
                            static_cast<uint32_t>(miniPineTripleMiddlePrototypeID),
                            packColor(glm::vec3(1.0f)),
                            false
                        );
                        voxelWorld.setBlock(topCell,
                            static_cast<uint32_t>(miniPineTripleTopPrototypeID),
                            packColor(glm::vec3(1.0f)),
                            false
                        );
                        modified = true;
                        continue;
                    }

                    if (spawnMiniPine) {
                        glm::ivec3 topCell = placeCell + glm::ivec3(0, 1, 0);
                        if (!cellBelongsToSection(topCell, sectionCoord, sectionSize)) continue;
                        if (getBlockAt(voxelWorld, topCell) != 0u) continue;
                        voxelWorld.setBlock(placeCell,
                            static_cast<uint32_t>(miniPineBottomPrototypeID),
                            packColor(glm::vec3(1.0f)),
                            false
                        );
                        voxelWorld.setBlock(topCell,
                            static_cast<uint32_t>(miniPineTopPrototypeID),
                            packColor(glm::vec3(1.0f)),
                            false
                        );
                        modified = true;
                        continue;
                    }

                    if (spawnFlaxDouble || spawnHempDouble) {
                        int bottomID = -1;
                        int topID = -1;
                        if (spawnFlaxDouble && spawnHempDouble) {
                            const bool chooseFlax = ((seed >> 7u) & 1u) == 0u;
                            bottomID = chooseFlax ? flaxDoubleBottomPrototypeID : hempDoubleBottomPrototypeID;
                            topID = chooseFlax ? flaxDoubleTopPrototypeID : hempDoubleTopPrototypeID;
                        } else if (spawnFlaxDouble) {
                            bottomID = flaxDoubleBottomPrototypeID;
                            topID = flaxDoubleTopPrototypeID;
                        } else {
                            bottomID = hempDoubleBottomPrototypeID;
                            topID = hempDoubleTopPrototypeID;
                        }
                        if (bottomID >= 0 && topID >= 0) {
                            glm::ivec3 topCell = placeCell + glm::ivec3(0, 1, 0);
                            if (!cellBelongsToSection(topCell, sectionCoord, sectionSize)) continue;
                            if (getBlockAt(voxelWorld, topCell) != 0u) continue;
                            voxelWorld.setBlock(placeCell,
                                static_cast<uint32_t>(bottomID),
                                packColor(glm::vec3(1.0f)),
                                false
                            );
                            voxelWorld.setBlock(topCell,
                                static_cast<uint32_t>(topID),
                                packColor(glm::vec3(1.0f)),
                                false
                            );
                            modified = true;
                            continue;
                        }
                    }

                    if (spawnSucculent) {
                        int succulentID = succulentPrototypeID;
                        if (succulentPrototypeID >= 0 && succulentPrototypeVariantID >= 0) {
                            const uint32_t succulentSeed = hash2D(worldX + 2903, worldZ - 1901);
                            succulentID = ((succulentSeed >> 9u) & 1u) == 0u
                                ? succulentPrototypeID
                                : succulentPrototypeVariantID;
                        } else if (succulentID < 0) {
                            succulentID = succulentPrototypeVariantID;
                        }
                        if (succulentID >= 0) {
                            voxelWorld.setBlock(placeCell,
                                static_cast<uint32_t>(succulentID),
                                packColor(glm::vec3(1.0f)),
                                false
                            );
                            modified = true;
                            continue;
                        }
                    }

                    if (!spawnFlower && spawnAnyRareFlower) {
                        std::array<int, kRareFlowerPoolMax> triggeredRareFlowerIDs{};
                        int triggeredRareFlowerCount = 0;
                        for (int i = 0; i < rareFlowerPrototypeCount; ++i) {
                            if (!rareRollMatchesIndex(i)) continue;
                            const int protoID = rareFlowerPrototypeIDs[static_cast<size_t>(i)];
                            if (protoID < 0) continue;
                            triggeredRareFlowerIDs[static_cast<size_t>(triggeredRareFlowerCount)] = protoID;
                            triggeredRareFlowerCount += 1;
                            if (triggeredRareFlowerCount >= static_cast<int>(triggeredRareFlowerIDs.size())) break;
                        }
                        const int rareFlowerID = chooseRareFlowerPrototypeIDForSeed(
                            triggeredRareFlowerIDs,
                            triggeredRareFlowerCount,
                            seed >> 4u
                        );
                        if (rareFlowerID >= 0) {
                            voxelWorld.setBlock(placeCell,
                                static_cast<uint32_t>(rareFlowerID),
                                packColor(glm::vec3(1.0f)),
                                false
                            );
                            modified = true;
                            continue;
                        }
                    }

                    if (spawnFlower) {
                        voxelWorld.setBlock(placeCell,
                            static_cast<uint32_t>(flowerPrototypeID),
                            flowerColorForCell(worldX, worldZ),
                            false
                        );
                        modified = true;
                    } else if (spawnGrass) {
                        const std::string& groundName = prototypes[static_cast<size_t>(groundID)].name;
                        const bool isWinterBareBiome = (biomeID == 4);
                        bool spawnStick = false;
                        uint32_t stickSeed = 0u;
                        if (spec.stickEnabled && hasAnyStickPrototype && isGrassSurfacePrototypeName(groundName)) {
                            const int stickChance = std::max(0, std::min(100, spec.stickSpawnPercent));
                            if (stickChance > 0) {
                                stickSeed = hash3D(worldX, groundWorldY, worldZ);
                                if (static_cast<int>(stickSeed % 100u) < stickChance) {
                                    if (isWinterBareBiome) {
                                        spawnStick = true;
                                    } else {
                                        const int canopyRadius = std::max(0, spec.stickCanopySearchRadius / sectionScale);
                                        const int canopyHeight = std::max(2, spec.stickCanopySearchHeight / sectionScale);
                                        spawnStick = hasLeafCanopyNear(
                                            voxelWorld,
                                            sectionTier,
                                            leafPrototypeID,
                                            tierX,
                                            groundTierY,
                                            tierZ,
                                            canopyRadius,
                                            canopyHeight
                                        );
                                    }
                                }
                            }
                        }
                        if (spawnStick) {
                            int biomeStickIDX = stickPrototypeIDX;
                            int biomeStickIDZ = stickPrototypeIDZ;
                            if (isWinterBareBiome) {
                                if (winterStickPrototypeIDX >= 0) biomeStickIDX = winterStickPrototypeIDX;
                                if (winterStickPrototypeIDZ >= 0) biomeStickIDZ = winterStickPrototypeIDZ;
                            }
                            const bool useBareBranch =
                                isWinterBareBiome
                                && (bareBranchPrototypeX >= 0 || bareBranchPrototypeZ >= 0)
                                && (static_cast<int>((stickSeed >> 8u) % 100u) < 35);
                            int stickID = -1;
                            if (useBareBranch) {
                                stickID = ((seed >> 9u) & 1u) == 0u ? bareBranchPrototypeX : bareBranchPrototypeZ;
                                if (stickID < 0) {
                                    stickID = (bareBranchPrototypeX >= 0) ? bareBranchPrototypeX : bareBranchPrototypeZ;
                                }
                            } else {
                                stickID = ((seed >> 9u) & 1u) == 0u ? biomeStickIDX : biomeStickIDZ;
                                if (stickID < 0) stickID = (biomeStickIDX >= 0) ? biomeStickIDX : biomeStickIDZ;
                            }
                            if (stickID >= 0) {
                                voxelWorld.setBlock(placeCell, static_cast<uint32_t>(stickID), stickColorForCell(worldX, worldZ), false);
                                modified = true;
                            }
                        } else {
                            int biomeCoverIDX = grassCoverPrototypeIDX;
                            int biomeCoverIDZ = grassCoverPrototypeIDZ;
                            if (biomeID == 1) {
                                if (grassCoverPrototypeMeadowIDX >= 0) biomeCoverIDX = grassCoverPrototypeMeadowIDX;
                                if (grassCoverPrototypeMeadowIDZ >= 0) biomeCoverIDZ = grassCoverPrototypeMeadowIDZ;
                            } else if (islandQuadrants && biomeID == 3) {
                                if (grassCoverPrototypeBiome3IDX >= 0) biomeCoverIDX = grassCoverPrototypeBiome3IDX;
                                if (grassCoverPrototypeBiome3IDZ >= 0) biomeCoverIDZ = grassCoverPrototypeBiome3IDZ;
                            } else if (biomeID == 4) {
                                if (grassCoverPrototypeBiome4IDX >= 0) biomeCoverIDX = grassCoverPrototypeBiome4IDX;
                                if (grassCoverPrototypeBiome4IDZ >= 0) biomeCoverIDZ = grassCoverPrototypeBiome4IDZ;
                            } else if (biomeID == 2) {
                                if (grassCoverPrototypeBiome2IDX >= 0) biomeCoverIDX = grassCoverPrototypeBiome2IDX;
                                if (grassCoverPrototypeBiome2IDZ >= 0) biomeCoverIDZ = grassCoverPrototypeBiome2IDZ;
                            }

                            bool placedGrassCover = false;
                            const bool coverGroundAllowed =
                                isGrassSurfacePrototypeName(groundName)
                                || (biomeID == 2 && isDesertGroundPrototypeName(groundName));
                            if (spec.grassCoverEnabled
                                && biomeID != 2
                                && (biomeCoverIDX >= 0 || biomeCoverIDZ >= 0)) {
                                // Prefer exact texture match to the supporting top block before biome fallback.
                                if (isForestFloorSurfacePrototypeName(groundName)) {
                                    if (grassCoverPrototypeForestIDX >= 0) biomeCoverIDX = grassCoverPrototypeForestIDX;
                                    if (grassCoverPrototypeForestIDZ >= 0) biomeCoverIDZ = grassCoverPrototypeForestIDZ;
                                } else if (isMeadowGrassSurfacePrototypeName(groundName)) {
                                    if (grassCoverPrototypeMeadowIDX >= 0) biomeCoverIDX = grassCoverPrototypeMeadowIDX;
                                    if (grassCoverPrototypeMeadowIDZ >= 0) biomeCoverIDZ = grassCoverPrototypeMeadowIDZ;
                                } else if (isJungleGrassSurfacePrototypeName(groundName)) {
                                    if (grassCoverPrototypeBiome3IDX >= 0) biomeCoverIDX = grassCoverPrototypeBiome3IDX;
                                    if (grassCoverPrototypeBiome3IDZ >= 0) biomeCoverIDZ = grassCoverPrototypeBiome3IDZ;
                                } else if (isBareWinterSurfacePrototypeName(groundName)) {
                                    if (grassCoverPrototypeBiome4IDX >= 0) biomeCoverIDX = grassCoverPrototypeBiome4IDX;
                                    if (grassCoverPrototypeBiome4IDZ >= 0) biomeCoverIDZ = grassCoverPrototypeBiome4IDZ;
                                } else if (isDesertGroundPrototypeName(groundName)) {
                                    if (grassCoverPrototypeBiome2IDX >= 0) biomeCoverIDX = grassCoverPrototypeBiome2IDX;
                                    if (grassCoverPrototypeBiome2IDZ >= 0) biomeCoverIDZ = grassCoverPrototypeBiome2IDZ;
                                }
                                if (coverGroundAllowed) {
                                    const int coverChance = std::max(0, std::min(100, spec.grassCoverPercent));
                                    if (coverChance > 0) {
                                        const uint32_t coverSeed = hash3D(worldX + 913, groundWorldY + 37, worldZ - 211);
                                        if (static_cast<int>(coverSeed % 100u) < coverChance) {
                                            int coverID = ((seed >> 13u) & 1u) == 0u ? biomeCoverIDX : biomeCoverIDZ;
                                            if (coverID < 0) coverID = (biomeCoverIDX >= 0) ? biomeCoverIDX : biomeCoverIDZ;
                                            if (coverID >= 0) {
                                                const int supportTopTile = ::RenderInitSystemLogic::FaceTileIndexFor(
                                                    &worldCtx,
                                                    prototypes[static_cast<size_t>(groundID)],
                                                    2
                                                );
                                                voxelWorld.setBlock(placeCell,
                                                    static_cast<uint32_t>(coverID),
                                                    withGrassCoverSnapshotTile(grassColorForCell(worldX, worldZ), supportTopTile),
                                                    false
                                                );
                                                modified = true;
                                                placedGrassCover = true;
                                            }
                                        }
                                    }
                                }
                            }
                            if (placedGrassCover) continue;

                            bool placedPebblePatch = false;
                            if (spec.pebblePatchEnabled
                                && biomeID != 2
                                && coverGroundAllowed
                                && (pebblePatchPrototypeX >= 0 || pebblePatchPrototypeZ >= 0)) {
                                const int pebbleChance = std::max(0, std::min(100, spec.pebblePatchPercent));
                                if (pebbleChance > 0) {
                                    const uint32_t pebbleSeed = hash3D(worldX + 1579, groundWorldY + 223, worldZ - 673);
                                    if (static_cast<int>(pebbleSeed % 100u) < pebbleChance) {
                                        if (!isNearSurfaceWater(placeCell)) continue;
                                        int patchID = ((pebbleSeed >> 13u) & 1u) == 0u ? pebblePatchPrototypeX : pebblePatchPrototypeZ;
                                        if (patchID < 0) patchID = (pebblePatchPrototypeX >= 0) ? pebblePatchPrototypeX : pebblePatchPrototypeZ;
                                        if (patchID >= 0) {
                                            voxelWorld.setBlock(placeCell,
                                                static_cast<uint32_t>(patchID),
                                                packColor(glm::vec3(1.0f)),
                                                false
                                            );
                                            modified = true;
                                            placedPebblePatch = true;
                                        }
                                    }
                                }
                            }
                            if (placedPebblePatch) continue;

                            bool placedAutumnLeafPatch = false;
                            int leafPatchPrototypeX = -1;
                            int leafPatchPrototypeZ = -1;
                            if (biomeID == 0) {
                                leafPatchPrototypeX = autumnLeafPatchPrototypeX;
                                leafPatchPrototypeZ = autumnLeafPatchPrototypeZ;
                            } else if (biomeID == 4) {
                                leafPatchPrototypeX = deadLeafPatchPrototypeX;
                                leafPatchPrototypeZ = deadLeafPatchPrototypeZ;
                            }
                            if (spec.autumnLeafPatchEnabled
                                && coverGroundAllowed
                                && (leafPatchPrototypeX >= 0 || leafPatchPrototypeZ >= 0)) {
                                const int leafChance = std::max(0, std::min(100, spec.autumnLeafPatchPercent));
                                if (leafChance > 0) {
                                    const uint32_t leafSeed = hash3D(worldX + 229, groundWorldY + 311, worldZ + 419);
                                    if (static_cast<int>(leafSeed % 100u) < leafChance) {
                                        int patchID = ((leafSeed >> 13u) & 1u) == 0u ? leafPatchPrototypeX : leafPatchPrototypeZ;
                                        if (patchID < 0) patchID = (leafPatchPrototypeX >= 0) ? leafPatchPrototypeX : leafPatchPrototypeZ;
                                        if (patchID >= 0) {
                                            voxelWorld.setBlock(placeCell,
                                                static_cast<uint32_t>(patchID),
                                                packColor(glm::vec3(1.0f)),
                                                false
                                            );
                                            modified = true;
                                            placedAutumnLeafPatch = true;
                                        }
                                    }
                                }
                            }
                            if (placedAutumnLeafPatch) continue;

                            if (isGrassGrowthBlockedSurfacePrototypeName(groundName)) {
                                continue;
                            }

                            const int clampedGrassTuftPercent = std::max(0, std::min(100, spec.grassTuftPercent));
                            if (clampedGrassTuftPercent <= 0) {
                                continue;
                            }
                            if (clampedGrassTuftPercent < 100) {
                                const uint32_t grassTuftSeed = hash3D(worldX + 1847, groundWorldY + 97, worldZ - 563);
                                if (static_cast<int>(grassTuftSeed % 100u) >= clampedGrassTuftPercent) {
                                    continue;
                                }
                            }

                            int biomeTallGrassID = grassPrototypeID;
                            int biomeShortGrassID = shortGrassPrototypeID;
                            if (biomeID == 1) {
                                if (grassPrototypeBiome1ID >= 0) biomeTallGrassID = grassPrototypeBiome1ID;
                                if (shortGrassPrototypeBiome1ID >= 0) biomeShortGrassID = shortGrassPrototypeBiome1ID;
                            } else if (islandQuadrants && biomeID == 3) {
                                if (grassPrototypeBiome3ID >= 0) biomeTallGrassID = grassPrototypeBiome3ID;
                                biomeShortGrassID = -1; // no short grass in jungle
                            } else if (biomeID == 4) {
                                biomeShortGrassID = -1;
                                if (grassPrototypeBiome4Count > 0) {
                                    const int grassIndex = static_cast<int>((seed >> 14u) % static_cast<uint32_t>(grassPrototypeBiome4Count));
                                    biomeTallGrassID = grassPrototypeBiome4IDs[static_cast<size_t>(grassIndex)];
                                } else if (grassPrototypeBiome3ID >= 0) {
                                    biomeTallGrassID = grassPrototypeBiome3ID;
                                }
                            } else if (biomeID == 2) {
                                biomeTallGrassID = -1;
                                biomeShortGrassID = -1;
                            }

                            int grassID = biomeTallGrassID;
                            if (biomeShortGrassID >= 0) {
                                const int clampedShortPercent = std::max(0, std::min(100, spec.shortGrassPercent));
                                if (grassID < 0 || static_cast<int>((seed >> 11u) % 100u) < clampedShortPercent) {
                                    grassID = biomeShortGrassID;
                                }
                            }
                            if (grassID < 0) continue;
                            voxelWorld.setBlock(placeCell, static_cast<uint32_t>(grassID), grassColorForCell(worldX, worldZ), false);
                            modified = true;
                        }
                    }
                }
            }
        }
        void writeDesertCactusToSection(const std::vector<Entity>& prototypes,
                                        const WorldContext& worldCtx,
                                        VoxelWorldContext& voxelWorld,
                                        int sectionTier,
                                        const glm::ivec3& sectionCoord,
                                        int sectionSize,
                                        int sectionScale,
                                        int cactusPrototypeAID,
                                        int cactusPrototypeBID,
                                        int cactusPrototypeAXID,
                                        int cactusPrototypeAZID,
                                        int cactusPrototypeBXID,
                                        int cactusPrototypeBZID,
                                        int cactusPrototypeAJunctionXID,
                                        int cactusPrototypeAJunctionZID,
                                        int cactusPrototypeBJunctionXID,
                                        int cactusPrototypeBJunctionZID,
                                        int waterPrototypeID,
                                        int spawnModulo,
                                        bool& unresolvedDependencies,
                                        bool& modified) {
            (void)unresolvedDependencies;
            if (spawnModulo <= 0) return;
            if (cactusPrototypeAID < 0 && cactusPrototypeBID < 0) return;
            constexpr int kCactusTrunkHeight = 5;
            const uint32_t cactusColor = packColor(glm::vec3(0.30f, 0.68f, 0.26f));
            const std::array<glm::ivec2, 4> kArmDirs = {
                glm::ivec2(1, 0),
                glm::ivec2(-1, 0),
                glm::ivec2(0, 1),
                glm::ivec2(0, -1)
            };

            const int minX = sectionCoord.x * sectionSize;
            const int minZ = sectionCoord.z * sectionSize;
            const int maxX = minX + sectionSize - 1;
            const int maxZ = minZ + sectionSize - 1;

            for (int tierZ = minZ; tierZ <= maxZ; ++tierZ) {
                for (int tierX = minX; tierX <= maxX; ++tierX) {
                    const int worldX = tierX * sectionScale;
                    const int worldZ = tierZ * sectionScale;
                    if (ExpanseBiomeSystemLogic::ResolveBiome(
                            worldCtx,
                            static_cast<float>(worldX),
                            static_cast<float>(worldZ)) != 2) {
                        continue;
                    }

                    const uint32_t seed = hash2D(worldX + 907, worldZ - 127);
                    if ((seed % static_cast<uint32_t>(spawnModulo)) != 0u) continue;

                    float terrainHeight = 0.0f;
                    const bool isLand = ExpanseBiomeSystemLogic::SampleTerrain(
                        worldCtx,
                        static_cast<float>(worldX),
                        static_cast<float>(worldZ),
                        terrainHeight
                    );
                    if (!isLand) continue;

                    const int groundWorldY = static_cast<int>(std::floor(terrainHeight));
                    const int groundTierY = floorDivInt(groundWorldY, sectionScale);
                    const glm::ivec3 placeCell(tierX, groundTierY + 1, tierZ);
                    if (!cellBelongsToSection(placeCell, sectionCoord, sectionSize)) continue;
                    if (getBlockAt(voxelWorld, placeCell) != 0u) continue;

                    const glm::ivec3 groundCell(tierX, groundTierY, tierZ);
                    const uint32_t groundID = getBlockAt(voxelWorld, groundCell);
                    if (groundID == 0u) {
                        continue;
                    }
                    if (!isFoliageGroundPrototypeID(prototypes, groundID, waterPrototypeID)) continue;
                    if (groundID >= prototypes.size()) continue;
                    if (!isDesertGroundPrototypeName(prototypes[static_cast<size_t>(groundID)].name)) continue;

                    const bool useFamilyB = (cactusPrototypeBID >= 0)
                        && (cactusPrototypeAID < 0 || (((seed >> 8u) & 1u) == 1u));
                    int cactusTrunkID = useFamilyB ? cactusPrototypeBID : cactusPrototypeAID;
                    if (cactusTrunkID < 0) {
                        cactusTrunkID = (cactusPrototypeAID >= 0) ? cactusPrototypeAID : cactusPrototypeBID;
                    }
                    if (cactusTrunkID < 0) continue;

                    int cactusArmXID = useFamilyB ? cactusPrototypeBXID : cactusPrototypeAXID;
                    int cactusArmZID = useFamilyB ? cactusPrototypeBZID : cactusPrototypeAZID;
                    int cactusJunctionXID = useFamilyB ? cactusPrototypeBJunctionXID : cactusPrototypeAJunctionXID;
                    int cactusJunctionZID = useFamilyB ? cactusPrototypeBJunctionZID : cactusPrototypeAJunctionZID;
                    if (cactusArmXID < 0) cactusArmXID = cactusTrunkID;
                    if (cactusArmZID < 0) cactusArmZID = cactusTrunkID;
                    if (cactusJunctionXID < 0) cactusJunctionXID = cactusTrunkID;
                    if (cactusJunctionZID < 0) cactusJunctionZID = cactusTrunkID;

                    bool canPlaceTrunk = true;
                    for (int i = 1; i <= kCactusTrunkHeight; ++i) {
                        const glm::ivec3 trunkCell(tierX, groundTierY + i, tierZ);
                        if (!cellBelongsToSection(trunkCell, sectionCoord, sectionSize)
                            || getBlockAt(voxelWorld, trunkCell) != 0u) {
                            canPlaceTrunk = false;
                            break;
                        }
                    }
                    if (!canPlaceTrunk) continue;

                    for (int i = 1; i <= kCactusTrunkHeight; ++i) {
                        const glm::ivec3 trunkCell(tierX, groundTierY + i, tierZ);
                        voxelWorld.setBlock(trunkCell,
                            static_cast<uint32_t>(cactusTrunkID),
                            cactusColor,
                            false
                        );
                    }
                    modified = true;

                    auto placeArm = [&](int directionIndex, int trunkOffsetY) {
                        if (trunkOffsetY <= 0 || trunkOffsetY >= kCactusTrunkHeight) return;
                        const glm::ivec2 dir = kArmDirs[static_cast<size_t>(directionIndex & 3)];
                        const glm::ivec3 trunkCell(tierX, groundTierY + trunkOffsetY, tierZ);
                        const glm::ivec3 armCell(tierX + dir.x, groundTierY + trunkOffsetY, tierZ + dir.y);
                        if (!cellBelongsToSection(armCell, sectionCoord, sectionSize)) return;
                        if (getBlockAt(voxelWorld, armCell) != 0u) return;

                        const int junctionPrototypeID = (dir.x != 0) ? cactusJunctionXID : cactusJunctionZID;
                        voxelWorld.setBlock(trunkCell,
                            static_cast<uint32_t>(junctionPrototypeID),
                            cactusColor,
                            false
                        );
                        modified = true;

                        const int armPrototypeID = (dir.x != 0) ? cactusArmXID : cactusArmZID;
                        voxelWorld.setBlock(armCell,
                            static_cast<uint32_t>(armPrototypeID),
                            cactusColor,
                            false
                        );
                        modified = true;

                        const glm::ivec3 tipCell(armCell.x, armCell.y + 1, armCell.z);
                        if (!cellBelongsToSection(tipCell, sectionCoord, sectionSize)) return;
                        if (getBlockAt(voxelWorld, tipCell) != 0u) return;
                        voxelWorld.setBlock(tipCell,
                            static_cast<uint32_t>(cactusTrunkID),
                            cactusColor,
                            false
                        );
                        modified = true;
                    };

                    const int firstArmDir = static_cast<int>((seed >> 12u) & 3u);
                    const int firstArmOffsetY = 2 + static_cast<int>((seed >> 14u) & 1u);
                    placeArm(firstArmDir, firstArmOffsetY);

                    const bool hasSecondArm = (((seed >> 16u) & 1u) == 1u);
                    if (hasSecondArm) {
                        int secondArmOffsetY = 3 + static_cast<int>((seed >> 18u) & 1u);
                        if (secondArmOffsetY == firstArmOffsetY) {
                            secondArmOffsetY = std::min(kCactusTrunkHeight - 1, firstArmOffsetY + 1);
                        }
                        const int secondArmDir = (firstArmDir + 2) & 3;
                        placeArm(secondArmDir, secondArmOffsetY);
                    }
                }
            }
        }
