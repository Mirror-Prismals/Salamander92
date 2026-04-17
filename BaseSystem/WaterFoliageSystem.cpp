        void writeWaterFoliageToSection(const std::vector<Entity>& prototypes,
                                        VoxelWorldContext& voxelWorld,
                                        int sectionTier,
                                        const glm::ivec3& sectionCoord,
                                        int sectionSize,
                                        int sectionScale,
                                        int kelpPrototypeID,
                                        int kelpTileIndex,
                                        int seaUrchinPrototypeIDX,
                                        int seaUrchinPrototypeIDZ,
                                        int sandDollarPrototypeIDX,
                                        int sandDollarPrototypeIDZ,
                                        int waterPrototypeID,
                                        const FoliageSpec& spec,
                                        bool& modified) {
            if (!spec.enabled || !spec.waterFoliageEnabled) return;
            if (waterPrototypeID < 0) return;
            const bool hasKelp = kelpPrototypeID >= 0 && kelpTileIndex >= 0;
            const bool hasSeaUrchinX = seaUrchinPrototypeIDX >= 0;
            const bool hasSeaUrchinZ = seaUrchinPrototypeIDZ >= 0;
            const bool hasSeaUrchin = hasSeaUrchinX || hasSeaUrchinZ;
            const bool hasSandDollarX = sandDollarPrototypeIDX >= 0;
            const bool hasSandDollarZ = sandDollarPrototypeIDZ >= 0;
            const bool hasSandDollar = hasSandDollarX || hasSandDollarZ;
            if (!hasKelp && !hasSeaUrchin && !hasSandDollar) return;

            const int kelpChance = std::max(0, std::min(100, spec.kelpSpawnPercent));
            const int seaUrchinChance = std::max(0, std::min(100, spec.seaUrchinSpawnPercent));
            const int sandDollarChance = std::max(0, std::min(100, spec.sandDollarSpawnPercent));
            const int seaUrchinSpawnModulo = std::max(
                1,
                spec.flowerSpawnModulo * kBlueFlowerRareChance * kYoungClematisExtraRarityMultiplier
            );
            if (kelpChance <= 0 && seaUrchinChance <= 0 && sandDollarChance <= 0) return;
            constexpr int kDepthFoliageTopY = -99;

            const int minX = sectionCoord.x * sectionSize;
            const int minY = sectionCoord.y * sectionSize;
            const int minZ = sectionCoord.z * sectionSize;
            const int maxX = minX + sectionSize - 1;
            const int maxY = minY + sectionSize - 1;
            const int maxZ = minZ + sectionSize - 1;
            auto inSection = [&](const glm::ivec3& cell) {
                return cell.x >= minX && cell.x <= maxX
                    && cell.y >= minY && cell.y <= maxY
                    && cell.z >= minZ && cell.z <= maxZ;
            };
            auto isWaterCellInSection = [&](const glm::ivec3& cell) {
                return inSection(cell)
                    && getBlockAt(voxelWorld, cell) == static_cast<uint32_t>(waterPrototypeID);
            };
            auto clearEmbeddedTile = [&](uint32_t packedColor) {
                // Strip legacy high-byte payloads while preserving supported water wave class metadata.
                const uint8_t waveClass = waterWaveClassFromPackedColor(packedColor);
                const uint8_t encoded = static_cast<uint8_t>((waveClass & 0x0fu) << 4u);
                const uint32_t rgb = packedColor & 0x00ffffffu;
                return rgb | (static_cast<uint32_t>(encoded) << 24u);
            };
            auto nearbyWaterColor = [&](const glm::ivec3& cell) {
                static const std::array<glm::ivec3, 6> kNeighborSteps = {
                    glm::ivec3(0, -1, 0),
                    glm::ivec3(0, 1, 0),
                    glm::ivec3(1, 0, 0),
                    glm::ivec3(-1, 0, 0),
                    glm::ivec3(0, 0, 1),
                    glm::ivec3(0, 0, -1)
                };
                for (const glm::ivec3& step : kNeighborSteps) {
                    const glm::ivec3 probe = cell + step;
                    if (!inSection(probe)) continue;
                    if (getBlockAt(voxelWorld, probe) != static_cast<uint32_t>(waterPrototypeID)) continue;
                    const uint32_t probeColor = clearEmbeddedTile(getColorAt(voxelWorld, probe));
                    if (probeColor != 0u) return probeColor;
                }
                const uint32_t selfColor = clearEmbeddedTile(getColorAt(voxelWorld, cell));
                if (selfColor != 0u) return selfColor;
                return withWaterFoliageMarker(packColor(glm::vec3(0.05f, 0.2f, 0.5f)), kWaterFoliageMarkerNone);
            };
            auto isWaterFoliageSupportedCellInSection = [&](const glm::ivec3& cell) {
                if (!isWaterCellInSection(cell)) return false;
                if (!isWaterCellInSection(cell + glm::ivec3(0, 1, 0))) return false;
                const glm::ivec3 below = cell + glm::ivec3(0, -1, 0);
                if (!inSection(below)) return false;
                const uint32_t belowId = getBlockAt(voxelWorld, below);
                return isFoliageGroundPrototypeID(prototypes, belowId, waterPrototypeID);
            };

            // Migrate legacy aquatic-foliage blocks and clear invalid/legacy water markers.
            for (int tierY = minY; tierY <= maxY; ++tierY) {
                for (int tierZ = minZ; tierZ <= maxZ; ++tierZ) {
                    for (int tierX = minX; tierX <= maxX; ++tierX) {
                        const glm::ivec3 cell(tierX, tierY, tierZ);
                        const uint32_t blockID = getBlockAt(voxelWorld, cell);
                        if (blockID == static_cast<uint32_t>(waterPrototypeID)) {
                            const uint32_t packedColor = getColorAt(voxelWorld, cell);
                            uint8_t marker = waterFoliageMarkerFromPackedColor(packedColor);
                            const int legacyEmbeddedTile = decodeGrassCoverSnapshotTile(packedColor);
                            if (marker == kWaterFoliageMarkerNone
                                && hasKelp
                                && legacyEmbeddedTile == kelpTileIndex) {
                                marker = kWaterFoliageMarkerKelp;
                            }
                            if (marker != kWaterFoliageMarkerNone) {
                                const glm::ivec3 belowCell = cell + glm::ivec3(0, -1, 0);
                                const bool validGround = inSection(belowCell)
                                    && isFoliageGroundPrototypeID(
                                        prototypes,
                                        getBlockAt(voxelWorld, belowCell),
                                        waterPrototypeID
                                    );
                                const bool markerSupported =
                                    (marker == kWaterFoliageMarkerKelp && hasKelp)
                                    || (marker == kWaterFoliageMarkerSeaUrchinX && hasSeaUrchinX)
                                    || (marker == kWaterFoliageMarkerSeaUrchinZ && hasSeaUrchinZ)
                                    || (marker == kWaterFoliageMarkerSandDollarX && hasSandDollarX)
                                    || (marker == kWaterFoliageMarkerSandDollarZ && hasSandDollarZ);
                                const bool keepMarker = markerSupported
                                    && validGround
                                    && isWaterFoliageSupportedCellInSection(cell);
                                if (keepMarker) {
                                    if (waterFoliageMarkerFromPackedColor(packedColor) != marker) {
                                        voxelWorld.setBlock(cell,
                                            static_cast<uint32_t>(waterPrototypeID),
                                            withWaterFoliageMarker(clearEmbeddedTile(packedColor), marker),
                                            false
                                        );
                                        modified = true;
                                    }
                                    continue;
                                }
                                voxelWorld.setBlock(cell,
                                    static_cast<uint32_t>(waterPrototypeID),
                                    clearEmbeddedTile(packedColor),
                                    false
                                );
                                modified = true;
                            }
                            continue;
                        }

                        if ((hasKelp && blockID == static_cast<uint32_t>(kelpPrototypeID))
                            || (hasSeaUrchinX && blockID == static_cast<uint32_t>(seaUrchinPrototypeIDX))
                            || (hasSeaUrchinZ && blockID == static_cast<uint32_t>(seaUrchinPrototypeIDZ))
                            || (hasSandDollarX && blockID == static_cast<uint32_t>(sandDollarPrototypeIDX))
                            || (hasSandDollarZ && blockID == static_cast<uint32_t>(sandDollarPrototypeIDZ))) {
                            voxelWorld.setBlock(cell,
                                static_cast<uint32_t>(waterPrototypeID),
                                nearbyWaterColor(cell),
                                false
                            );
                            modified = true;
                        }
                    }
                }
            }

            for (int tierZ = minZ + 1; tierZ <= maxZ - 1; ++tierZ) {
                for (int tierX = minX + 1; tierX <= maxX - 1; ++tierX) {
                    int baseGroundTierY = std::numeric_limits<int>::min();
                    for (int tierY = maxY - 1; tierY >= minY; --tierY) {
                        const glm::ivec3 groundCell(tierX, tierY, tierZ);
                        const uint32_t groundID = getBlockAt(voxelWorld, groundCell);
                        if (groundID == 0u) continue;
                        if (!isFoliageGroundPrototypeID(prototypes, groundID, waterPrototypeID)) continue;

                        const glm::ivec3 placeCell(tierX, tierY + 1, tierZ);
                        if (!inSection(placeCell)) continue;
                        if (!isWaterFoliageSupportedCellInSection(placeCell)) continue;
                        baseGroundTierY = tierY;
                        break;
                    }
                    if (baseGroundTierY == std::numeric_limits<int>::min()) continue;

                    const int worldX = tierX * sectionScale;
                    const int worldY = (baseGroundTierY + 1) * sectionScale;
                    const int worldZ = tierZ * sectionScale;
                    const uint32_t seed = hash3D(worldX + 173, worldY + 991, worldZ - 557);

                    bool placed = false;
                    const bool canPlaceUrchin = hasSeaUrchin && seaUrchinChance > 0
                        && ((seed % static_cast<uint32_t>(seaUrchinSpawnModulo)) == 0u);
                    const bool canPlaceSandDollar = hasSandDollar
                        && sandDollarChance > 0
                        && (worldY <= kDepthFoliageTopY)
                        && (static_cast<int>((seed >> 20u) % 100u) < sandDollarChance);
                    const bool canPlaceKelp = hasKelp && kelpChance > 0
                        && static_cast<int>((seed >> 8u) % 100u) < kelpChance;
                    const glm::ivec3 kelpCell(tierX, baseGroundTierY + 1, tierZ);

                    if (canPlaceSandDollar
                        && (!canPlaceKelp || ((seed >> 16u) & 1u) == 0u)
                        && (!canPlaceUrchin || ((seed >> 17u) & 1u) == 0u)) {
                        uint8_t marker = (((seed >> 5u) & 1u) == 0u)
                            ? kWaterFoliageMarkerSandDollarX
                            : kWaterFoliageMarkerSandDollarZ;
                        if (marker == kWaterFoliageMarkerSandDollarX && !hasSandDollarX) {
                            marker = hasSandDollarZ ? kWaterFoliageMarkerSandDollarZ : kWaterFoliageMarkerNone;
                        } else if (marker == kWaterFoliageMarkerSandDollarZ && !hasSandDollarZ) {
                            marker = hasSandDollarX ? kWaterFoliageMarkerSandDollarX : kWaterFoliageMarkerNone;
                        }
                        if (marker != kWaterFoliageMarkerNone) {
                            const uint32_t baseWaterColor = nearbyWaterColor(kelpCell);
                            voxelWorld.setBlock(kelpCell,
                                static_cast<uint32_t>(waterPrototypeID),
                                withWaterFoliageMarker(baseWaterColor, marker),
                                false
                            );
                            modified = true;
                            placed = true;
                        }
                    } else if (canPlaceUrchin && (!canPlaceKelp || ((seed >> 16u) & 1u) == 0u)) {
                        uint8_t marker = (((seed >> 4u) & 1u) == 0u)
                            ? kWaterFoliageMarkerSeaUrchinX
                            : kWaterFoliageMarkerSeaUrchinZ;
                        if (marker == kWaterFoliageMarkerSeaUrchinX && !hasSeaUrchinX) {
                            marker = hasSeaUrchinZ ? kWaterFoliageMarkerSeaUrchinZ : kWaterFoliageMarkerNone;
                        } else if (marker == kWaterFoliageMarkerSeaUrchinZ && !hasSeaUrchinZ) {
                            marker = hasSeaUrchinX ? kWaterFoliageMarkerSeaUrchinX : kWaterFoliageMarkerNone;
                        }
                        if (marker != kWaterFoliageMarkerNone) {
                            const uint32_t baseWaterColor = nearbyWaterColor(kelpCell);
                            voxelWorld.setBlock(kelpCell,
                                static_cast<uint32_t>(waterPrototypeID),
                                withWaterFoliageMarker(baseWaterColor, marker),
                                false
                            );
                            modified = true;
                            placed = true;
                        }
                    }

                    if (!placed && canPlaceKelp) {
                        if (inSection(kelpCell) && isWaterFoliageSupportedCellInSection(kelpCell)) {
                            const uint32_t baseWaterColor = nearbyWaterColor(kelpCell);
                            voxelWorld.setBlock(kelpCell,
                                static_cast<uint32_t>(waterPrototypeID),
                                withWaterFoliageMarker(baseWaterColor, kWaterFoliageMarkerKelp),
                                false
                            );
                            modified = true;
                        }
                    } else if (!placed && isWaterCellInSection(kelpCell)) {
                        const uint32_t packedColor = getColorAt(voxelWorld, kelpCell);
                        const uint8_t marker = waterFoliageMarkerFromPackedColor(packedColor);
                        const int legacyEmbeddedTile = decodeGrassCoverSnapshotTile(packedColor);
                        if (marker != kWaterFoliageMarkerNone
                            || (hasKelp && legacyEmbeddedTile == kelpTileIndex)) {
                            voxelWorld.setBlock(kelpCell,
                                static_cast<uint32_t>(waterPrototypeID),
                                clearEmbeddedTile(packedColor),
                                false
                            );
                            modified = true;
                        }
                    }
                }
            }
        }
