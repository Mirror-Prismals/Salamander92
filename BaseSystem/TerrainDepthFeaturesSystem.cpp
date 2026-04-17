#pragma once

namespace TerrainSystemLogic {

    namespace {
        struct WaterfallCascadeSeed {
            glm::ivec3 cell = glm::ivec3(0);
            int totalFallDistance = 0;
            bool hasPendingPool = false;
            glm::ivec3 pendingPoolImpactCell = glm::ivec3(0);
            glm::ivec3 pendingPoolLastWaterCell = glm::ivec3(0);
            uint32_t pendingPoolWaterColor = 0u;
        };

        struct TerrainDepthFeatureJobState {
            int phase = 0;
            std::vector<glm::ivec3> pendingChalkPlacements;
            int waterfallContinuationCursor = 0;
            int waterfallSourceCursor = 0;
            int chalkCursor = 0;
            std::vector<WaterfallCascadeSeed> waterfallCascadeQueue;
            size_t waterfallCascadeQueueHead = 0;
            int depthDecorSubphase = 0;
            int depthDecorLayerCursor = 0;
            int depthDecorBeamStartsPlaced = 0;
            int depthLavaContinuationCursor = 0;
            int depthLavaSourceCursor = 0;
            int obsidianLayerCursor = 0;
        };

        static std::unordered_map<VoxelSectionKey, TerrainDepthFeatureJobState, VoxelSectionKeyHash> g_depthFeatureJobs;

        bool ApplyTerrainDepthFeaturePasses(const TerrainDepthFeaturePassContext& ctx,
                                            const std::vector<glm::ivec3>& pendingChalkPlacements,
                                            bool& wroteAny) {
            BaseSystem& baseSystem = ctx.baseSystem;
            std::vector<Entity>& prototypes = ctx.prototypes;
            VoxelWorldContext& voxelWorld = ctx.voxelWorld;
            const glm::ivec3& sectionCoord = ctx.sectionCoord;
            const int size = ctx.size;
            const bool outCompleted = ctx.outCompleted;
            const bool isExpanseLevel = ctx.isExpanseLevel;
            const bool unifiedDepthsEnabled = ctx.unifiedDepthsEnabled;
            const int unifiedDepthsTopY = ctx.unifiedDepthsTopY;
            const int unifiedDepthsMinY = ctx.unifiedDepthsMinY;
            const bool waterfallEnabled = ctx.waterfallEnabled;
            const int waterfallMaxDrop = ctx.waterfallMaxDrop;
            const int waterfallCascadeBudget = ctx.waterfallCascadeBudget;
            const int waterSurfaceY = ctx.waterSurfaceY;
            const bool chalkEnabled = ctx.chalkEnabled;
            const Entity* chalkProto = ctx.chalkProto;
            const int chalkStickSpawnPercent = ctx.chalkStickSpawnPercent;
            const int chalkSeed = ctx.chalkSeed;
            const Entity* chalkStickProtoX = ctx.chalkStickProtoX;
            const Entity* chalkStickProtoZ = ctx.chalkStickProtoZ;
            const glm::vec3& chalkColor = ctx.chalkColor;
            const bool depthLavaFloorEnabled = ctx.depthLavaFloorEnabled;
            const std::array<const Entity*, 9>& depthLavaTileProtos = ctx.depthLavaTileProtos;
            const glm::vec3& lavaColor = ctx.lavaColor;
            const Entity* obsidianProto = ctx.obsidianProto;
            const Entity* waterProto = ctx.waterProto;
            const Entity* waterSlopeProtoPosX = ctx.waterSlopeProtoPosX;
            const Entity* waterSlopeProtoNegX = ctx.waterSlopeProtoNegX;
            const Entity* waterSlopeProtoPosZ = ctx.waterSlopeProtoPosZ;
            const Entity* waterSlopeProtoNegZ = ctx.waterSlopeProtoNegZ;
            const Entity* waterSlopeCornerProtoPosXPosZ = ctx.waterSlopeCornerProtoPosXPosZ;
            const Entity* waterSlopeCornerProtoPosXNegZ = ctx.waterSlopeCornerProtoPosXNegZ;
            const Entity* waterSlopeCornerProtoNegXPosZ = ctx.waterSlopeCornerProtoNegXPosZ;
            const Entity* waterSlopeCornerProtoNegXNegZ = ctx.waterSlopeCornerProtoNegXNegZ;
            const Entity* depthStoneProto = ctx.depthStoneProto;
            const Entity* stoneProto = ctx.stoneProto;
            const Entity* depthPurpleDirtProto = ctx.depthPurpleDirtProto;
            const Entity* depthRustBeamProto = ctx.depthRustBeamProto;
            const std::array<const Entity*, 4>& depthBigLilypadProtosX = ctx.depthBigLilypadProtosX;
            const std::array<const Entity*, 4>& depthBigLilypadProtosZ = ctx.depthBigLilypadProtosZ;
            const Entity* depthMossWallProtoPosX = ctx.depthMossWallProtoPosX;
            const Entity* depthMossWallProtoNegX = ctx.depthMossWallProtoNegX;
            const Entity* depthMossWallProtoPosZ = ctx.depthMossWallProtoPosZ;
            const Entity* depthMossWallProtoNegZ = ctx.depthMossWallProtoNegZ;
            const Entity* depthCrystalProto = ctx.depthCrystalProto;
            const Entity* depthCrystalBlueProto = ctx.depthCrystalBlueProto;
            const Entity* depthCrystalBlueBigProto = ctx.depthCrystalBlueBigProto;
            const Entity* depthCrystalMagentaBigProto = ctx.depthCrystalMagentaBigProto;
            const int depthPurplePatchPercent = ctx.depthPurplePatchPercent;
            const int depthRustBeamPercent = ctx.depthRustBeamPercent;
            const int depthRustBeamSeed = ctx.depthRustBeamSeed;
            const int depthMossPercent = ctx.depthMossPercent;
            const int depthBigLilypadPercent = ctx.depthBigLilypadPercent;
            const int depthRiverCrystalPercent = ctx.depthRiverCrystalPercent;
            const int depthRiverCrystalBankSearchDown = ctx.depthRiverCrystalBankSearchDown;
            const int depthRiverCrystalBankSearchRadius = ctx.depthRiverCrystalBankSearchRadius;
            const uint32_t packedWaterColorUnknown = ctx.packedWaterColorUnknown;

            const VoxelSectionKey sectionKey{sectionCoord};
            if (!outCompleted) {
                g_depthFeatureJobs.erase(sectionKey);
                return true;
            }

            if (g_depthFeatureJobs.size() > 4096u) {
                for (auto it = g_depthFeatureJobs.begin(); it != g_depthFeatureJobs.end();) {
                    if (voxelWorld.sections.count(it->first) == 0) {
                        it = g_depthFeatureJobs.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            auto [jobIt, inserted] = g_depthFeatureJobs.try_emplace(sectionKey);
            TerrainDepthFeatureJobState& jobState = jobIt->second;
            if (inserted && !pendingChalkPlacements.empty()) {
                jobState.pendingChalkPlacements = pendingChalkPlacements;
            }
            const int phasesPerUpdate = std::max(
                1,
                getRegistryInt(baseSystem, "TerrainDepthFeaturePhasesPerUpdate", 1)
            );
            const std::vector<glm::ivec3>& chalkPlacements = jobState.pendingChalkPlacements;
            auto phaseEnabled = [&](int phase) -> bool {
                switch (phase) {
                    case 0:
                        return isExpanseLevel && unifiedDepthsEnabled;
                    case 1:
                        {
                            const int sectionMinY = sectionCoord.y * size;
                            const int sectionMaxY = sectionMinY + size - 1;
                            const int waterfallBandUp = std::max(
                                0,
                                getRegistryInt(baseSystem, "WaterfallFeatureBandUp", 48)
                            );
                            const int waterfallBandDown = std::max(
                                0,
                                getRegistryInt(baseSystem, "WaterfallFeatureBandDown", 96)
                            );
                            const int minRelevantY = waterSurfaceY - waterfallBandDown;
                            const int maxRelevantY = waterSurfaceY + waterfallBandUp;
                            if (sectionMaxY < minRelevantY || sectionMinY > maxRelevantY) {
                                return false;
                            }

                            const float waterfallMaxDistance = std::max(
                                0.0f,
                                getRegistryFloat(baseSystem, "WaterfallFeatureMaxDistance", 192.0f)
                            );
                            if (waterfallMaxDistance > 0.0f && baseSystem.player) {
                                const float sectionCenterX = (static_cast<float>(sectionCoord.x) + 0.5f) * static_cast<float>(size);
                                const float sectionCenterZ = (static_cast<float>(sectionCoord.z) + 0.5f) * static_cast<float>(size);
                                const glm::vec3 camPos = baseSystem.player->cameraPosition;
                                const float dx = sectionCenterX - camPos.x;
                                const float dz = sectionCenterZ - camPos.z;
                                if ((dx * dx + dz * dz) > (waterfallMaxDistance * waterfallMaxDistance)) {
                                    return false;
                                }
                            }

                            return waterfallEnabled
                            || (chalkEnabled
                                && chalkProto
                                && chalkProto->prototypeID > 0
                                && !chalkPlacements.empty());
                        }
                    case 2:
                        return isExpanseLevel
                            && unifiedDepthsEnabled
                            && depthLavaFloorEnabled
                            && !depthLavaTileProtos.empty();
                    case 3:
                        return obsidianProto
                            && obsidianProto->prototypeID > 0
                            && waterProto
                            && waterProto->prototypeID > 0;
                    default:
                        return false;
                }
            };
            while (jobState.phase < 4 && !phaseEnabled(jobState.phase)) {
                jobState.phase += 1;
            }
            for (int phaseStep = 0; phaseStep < phasesPerUpdate && jobState.phase < 4; ++phaseStep) {
                bool phaseComplete = true;
                const bool runDepthDecorPhase = (jobState.phase == 0);
                const bool runWaterfallPhase = (jobState.phase == 1);
                const bool runLavaCascadePhase = (jobState.phase == 2);
                const bool runObsidianPhase = (jobState.phase == 3);

                if (runDepthDecorPhase && isExpanseLevel && unifiedDepthsEnabled) {
                const int sectionMinX = sectionCoord.x * size;
                const int sectionMaxX = sectionMinX + size - 1;
                const int sectionMinYDepth = sectionCoord.y * size;
                const int sectionMaxYDepth = sectionMinYDepth + size - 1;
                const int sectionMinZ = sectionCoord.z * size;
                const int sectionMaxZ = sectionMinZ + size - 1;
                const uint32_t depthStoneId = static_cast<uint32_t>((depthStoneProto ? depthStoneProto : stoneProto)->prototypeID);
                const uint32_t fallbackStoneId = static_cast<uint32_t>(stoneProto->prototypeID);
                const uint32_t depthPurpleDirtId = depthPurpleDirtProto ? static_cast<uint32_t>(depthPurpleDirtProto->prototypeID) : 0u;
                const uint32_t depthRustBeamId = depthRustBeamProto ? static_cast<uint32_t>(depthRustBeamProto->prototypeID) : 0u;
                const uint32_t waterId = static_cast<uint32_t>(waterProto->prototypeID);
                auto inCurrentSection = [&](const glm::ivec3& cell) {
                    return cell.x >= sectionMinX && cell.x <= sectionMaxX
                        && cell.y >= sectionMinYDepth && cell.y <= sectionMaxYDepth
                        && cell.z >= sectionMinZ && cell.z <= sectionMaxZ;
                };
                auto isSolidSupport = [&](uint32_t id) {
                    if (id == 0u) return false;
                    if (id >= static_cast<uint32_t>(prototypes.size())) return false;
                    return prototypes[static_cast<size_t>(id)].isSolid;
                };
                auto isDepthRiverWaterCell = [&](const glm::ivec3& cell) {
                    const uint32_t id = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, cell);
                    if (id != waterId) return false;
                    if (cell.y > unifiedDepthsTopY) return false;
                    const uint32_t packed = VoxelMeshInitSystemLogic::GetVoxelColorAt(voxelWorld, cell);
                    return waterWaveClassFromPackedColor(packed) == kWaterWaveClassRiver;
                };
                auto isAnyWaterCell = [&](const glm::ivec3& cell) {
                    const uint32_t id = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, cell);
                    return id == waterId;
                };
                auto isDepthLavaId = [&](uint32_t id) {
                    if (id == 0u) return false;
                    for (const Entity* lavaProto : depthLavaTileProtos) {
                        if (lavaProto && lavaProto->prototypeID > 0
                            && id == static_cast<uint32_t>(lavaProto->prototypeID)) {
                            return true;
                        }
                    }
                    return false;
                };
                auto hasNearbyDepthLava = [&](const glm::ivec3& cell, int radiusXZ, int radiusY) {
                    for (int dy = -radiusY; dy <= radiusY; ++dy) {
                        const int y = cell.y + dy;
                        if (y > unifiedDepthsTopY || y <= unifiedDepthsMinY) continue;
                        for (int dz = -radiusXZ; dz <= radiusXZ; ++dz) {
                            for (int dx = -radiusXZ; dx <= radiusXZ; ++dx) {
                                if (dx == 0 && dy == 0 && dz == 0) continue;
                                const glm::ivec3 n(cell.x + dx, y, cell.z + dz);
                                const uint32_t id = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, n);
                                if (isDepthLavaId(id)) {
                                    return true;
                                }
                            }
                        }
                    }
                    return false;
                };

                const int depthDecorLayersPerUpdate = std::max(
                    1,
                    getRegistryInt(baseSystem, "DepthDecorLayersPerUpdate", 2)
                );
                auto clampLayerCursor = [&](int maxLayerExclusive) {
                    if (maxLayerExclusive <= 0) return 0;
                    return std::max(0, std::min(jobState.depthDecorLayerCursor, maxLayerExclusive));
                };
                auto advanceDepthDecorSubphase = [&](int nextSubphase) {
                    jobState.depthDecorSubphase = nextSubphase;
                    jobState.depthDecorLayerCursor = 0;
                };
                if (jobState.depthDecorSubphase < 0 || jobState.depthDecorSubphase > 5) {
                    jobState.depthDecorSubphase = 0;
                    jobState.depthDecorLayerCursor = 0;
                    jobState.depthDecorBeamStartsPlaced = 0;
                }

                switch (jobState.depthDecorSubphase) {
                    case 0: {
                        if (!(depthPurpleDirtProto && depthPurplePatchPercent > 0)) {
                            advanceDepthDecorSubphase(1);
                            phaseComplete = false;
                            break;
                        }
                        constexpr int kDepthPurplePatchCellSize = 12;
                        constexpr int kDepthPurplePatchMinRadius = 2;
                        constexpr int kDepthPurplePatchMaxRadius = 5;
                        constexpr int kDepthPurplePatchSeed = 911;
                        const int startLayer = clampLayerCursor(size);
                        const int endLayer = std::min(size, startLayer + depthDecorLayersPerUpdate);
                        for (int localY = startLayer; localY < endLayer; ++localY) {
                            const int y = sectionMinYDepth + localY;
                            if (y > unifiedDepthsTopY || y <= unifiedDepthsMinY) continue;
                            for (int z = sectionMinZ; z <= sectionMaxZ; ++z) {
                                for (int x = sectionMinX; x <= sectionMaxX; ++x) {
                                    const glm::ivec3 cell(x, y, z);
                                    const uint32_t id = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, cell);
                                    if (id != depthStoneId && id != fallbackStoneId) continue;
                                    const uint32_t aboveId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, cell + glm::ivec3(0, 1, 0));
                                    if (aboveId != 0u) continue;
                                    const uint32_t belowId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, cell + glm::ivec3(0, -1, 0));
                                    if (!isSolidSupport(belowId)) continue;
                                    const int patchCellX = floorDivInt(x, kDepthPurplePatchCellSize);
                                    const int patchCellZ = floorDivInt(z, kDepthPurplePatchCellSize);
                                    bool inPatch = false;
                                    for (int oz = -1; oz <= 1 && !inPatch; ++oz) {
                                        for (int ox = -1; ox <= 1 && !inPatch; ++ox) {
                                            const int cellX = patchCellX + ox;
                                            const int cellZ = patchCellZ + oz;
                                            const uint32_t spawnSeed = hash2DInt(
                                                cellX + kDepthPurplePatchSeed * 37,
                                                cellZ - kDepthPurplePatchSeed * 53
                                            );
                                            if (static_cast<int>(spawnSeed % 100u) >= depthPurplePatchPercent) continue;
                                            const uint32_t centerSeed = hash2DInt(
                                                cellX * 131 + kDepthPurplePatchSeed,
                                                cellZ * 173 - kDepthPurplePatchSeed
                                            );
                                            const int centerBaseX = cellX * kDepthPurplePatchCellSize;
                                            const int centerBaseZ = cellZ * kDepthPurplePatchCellSize;
                                            const int centerX = centerBaseX
                                                + static_cast<int>((centerSeed & 0xffu) * static_cast<uint32_t>(kDepthPurplePatchCellSize) / 256u);
                                            const int centerZ = centerBaseZ
                                                + static_cast<int>(((centerSeed >> 8u) & 0xffu) * static_cast<uint32_t>(kDepthPurplePatchCellSize) / 256u);
                                            const int radiusRange = std::max(1, kDepthPurplePatchMaxRadius - kDepthPurplePatchMinRadius + 1);
                                            const int radius = kDepthPurplePatchMinRadius
                                                + static_cast<int>((centerSeed >> 16u) % static_cast<uint32_t>(radiusRange));
                                            const int dx = x - centerX;
                                            const int dz = z - centerZ;
                                            if ((dx * dx + dz * dz) <= (radius * radius)) {
                                                inPatch = true;
                                            }
                                        }
                                    }
                                    if (!inPatch) continue;
                                    voxelWorld.setBlock(cell, depthPurpleDirtId, packColor(glm::vec3(0.36f, 0.23f, 0.45f)), false);
                                    wroteAny = true;
                                }
                            }
                        }
                        jobState.depthDecorLayerCursor = endLayer;
                        if (endLayer >= size) {
                            advanceDepthDecorSubphase(1);
                        }
                        phaseComplete = false;
                        break;
                    }
                    case 1: {
                        if (!(depthRustBeamProto && depthRustBeamPercent > 0)) {
                            advanceDepthDecorSubphase(2);
                            phaseComplete = false;
                            break;
                        }
                        static const std::array<glm::ivec3, 4> kBeamDirs = {
                            glm::ivec3(1, 0, 0),
                            glm::ivec3(-1, 0, 0),
                            glm::ivec3(0, 0, 1),
                            glm::ivec3(0, 0, -1)
                        };
                        const int maxBeamStarts = 24;
                        const int startLayer = clampLayerCursor(size);
                        const int endLayer = std::min(size, startLayer + depthDecorLayersPerUpdate);
                        for (int localY = startLayer;
                             localY < endLayer && jobState.depthDecorBeamStartsPlaced < maxBeamStarts;
                             ++localY) {
                            const int y = sectionMinYDepth + localY;
                            if (y > unifiedDepthsTopY || y <= unifiedDepthsMinY) continue;
                            for (int z = sectionMinZ; z <= sectionMaxZ && jobState.depthDecorBeamStartsPlaced < maxBeamStarts; ++z) {
                                for (int x = sectionMinX; x <= sectionMaxX && jobState.depthDecorBeamStartsPlaced < maxBeamStarts; ++x) {
                                    const glm::ivec3 startCell(x, y, z);
                                    const uint32_t id = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, startCell);
                                    if (id != depthStoneId && id != fallbackStoneId && id != depthPurpleDirtId) continue;
                                    const uint32_t seed = hash3DInt(x + depthRustBeamSeed * 11, y - depthRustBeamSeed * 7, z + depthRustBeamSeed * 13);
                                    if (static_cast<int>(seed % 100u) >= depthRustBeamPercent) continue;
                                    if (!hasNearbyDepthLava(startCell, 8, 6)) continue;
                                    const int dirIndex = static_cast<int>((seed >> 8u) & 3u);
                                    const glm::ivec3 dir = kBeamDirs[static_cast<size_t>(dirIndex)];
                                    const int beamLength = 3 + static_cast<int>((seed >> 12u) % 5u);
                                    bool placedAny = false;
                                    for (int step = 0; step < beamLength; ++step) {
                                        const glm::ivec3 beamCell = startCell + dir * step;
                                        if (!inCurrentSection(beamCell)) break;
                                        const uint32_t beamId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, beamCell);
                                        if (beamId != depthStoneId
                                            && beamId != fallbackStoneId
                                            && beamId != depthPurpleDirtId
                                            && beamId != depthRustBeamId) {
                                            break;
                                        }
                                        voxelWorld.setBlock(beamCell, depthRustBeamId, packColor(glm::vec3(0.50f, 0.34f, 0.26f)), false);
                                        placedAny = true;
                                        wroteAny = true;
                                    }
                                    if (placedAny) {
                                        jobState.depthDecorBeamStartsPlaced += 1;
                                    }
                                }
                            }
                        }
                        jobState.depthDecorLayerCursor = endLayer;
                        if (jobState.depthDecorBeamStartsPlaced >= maxBeamStarts || endLayer >= size) {
                            advanceDepthDecorSubphase(2);
                        }
                        phaseComplete = false;
                        break;
                    }
                    case 2: {
                        const bool hasBigLilypadX =
                            depthBigLilypadProtosX[0] && depthBigLilypadProtosX[1] && depthBigLilypadProtosX[2] && depthBigLilypadProtosX[3];
                        const bool hasBigLilypadZ =
                            depthBigLilypadProtosZ[0] && depthBigLilypadProtosZ[1] && depthBigLilypadProtosZ[2] && depthBigLilypadProtosZ[3];
                        if (!((hasBigLilypadX || hasBigLilypadZ) && depthBigLilypadPercent > 0)) {
                            advanceDepthDecorSubphase(3);
                            phaseComplete = false;
                            break;
                        }
                        const int layerCount = std::max(0, size - 1);
                        const int startLayer = clampLayerCursor(layerCount);
                        const int endLayer = std::min(layerCount, startLayer + depthDecorLayersPerUpdate);
                        for (int localY = startLayer; localY < endLayer; ++localY) {
                            const int y = sectionMinYDepth + localY;
                            if (y > unifiedDepthsTopY || y <= unifiedDepthsMinY) continue;
                            for (int z = sectionMinZ; z <= sectionMaxZ - 1; ++z) {
                                for (int x = sectionMinX; x <= sectionMaxX - 1; ++x) {
                                    const glm::ivec3 waterNW(x, y, z);
                                    const glm::ivec3 waterNE(x + 1, y, z);
                                    const glm::ivec3 waterSW(x, y, z + 1);
                                    const glm::ivec3 waterSE(x + 1, y, z + 1);
                                    if (!isDepthRiverWaterCell(waterNW)
                                        || !isDepthRiverWaterCell(waterNE)
                                        || !isDepthRiverWaterCell(waterSW)
                                        || !isDepthRiverWaterCell(waterSE)) {
                                        continue;
                                    }
                                    const glm::ivec3 topNW = waterNW + glm::ivec3(0, 1, 0);
                                    const glm::ivec3 topNE = waterNE + glm::ivec3(0, 1, 0);
                                    const glm::ivec3 topSW = waterSW + glm::ivec3(0, 1, 0);
                                    const glm::ivec3 topSE = waterSE + glm::ivec3(0, 1, 0);
                                    if (!inCurrentSection(topNW) || !inCurrentSection(topNE) || !inCurrentSection(topSW) || !inCurrentSection(topSE)) continue;
                                    if (VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, topNW) != 0u
                                        || VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, topNE) != 0u
                                        || VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, topSW) != 0u
                                        || VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, topSE) != 0u) {
                                        continue;
                                    }
                                    const uint32_t seed = hash3DInt(x + 901, y - 617, z + 433);
                                    if (static_cast<int>(seed % 100u) >= depthBigLilypadPercent) continue;
                                    const auto& protoSet = hasBigLilypadZ ? depthBigLilypadProtosZ : depthBigLilypadProtosX;
                                    voxelWorld.setBlock(topNW, static_cast<uint32_t>(protoSet[3]->prototypeID), packColor(glm::vec3(1.0f)), false);
                                    voxelWorld.setBlock(topNE, static_cast<uint32_t>(protoSet[2]->prototypeID), packColor(glm::vec3(1.0f)), false);
                                    voxelWorld.setBlock(topSW, static_cast<uint32_t>(protoSet[1]->prototypeID), packColor(glm::vec3(1.0f)), false);
                                    voxelWorld.setBlock(topSE, static_cast<uint32_t>(protoSet[0]->prototypeID), packColor(glm::vec3(1.0f)), false);
                                    wroteAny = true;
                                }
                            }
                        }
                        jobState.depthDecorLayerCursor = endLayer;
                        if (endLayer >= layerCount) {
                            advanceDepthDecorSubphase(3);
                        }
                        phaseComplete = false;
                        break;
                    }
                    case 3: {
                        if (!(depthRiverCrystalPercent > 0)) {
                            advanceDepthDecorSubphase(4);
                            phaseComplete = false;
                            break;
                        }
                        struct CrystalChoice {
                            uint32_t prototypeID = 0u;
                            int weight = 0;
                        };
                        std::vector<CrystalChoice> crystalChoices;
                        crystalChoices.reserve(4);
                        auto addCrystalChoice = [&](const Entity* proto, int weight) {
                            if (!proto || proto->prototypeID <= 0 || weight <= 0) return;
                            crystalChoices.push_back(CrystalChoice{
                                static_cast<uint32_t>(proto->prototypeID),
                                weight
                            });
                        };
                        addCrystalChoice(depthCrystalProto, 36);
                        addCrystalChoice(depthCrystalBlueProto, 30);
                        addCrystalChoice(depthCrystalBlueBigProto, 18);
                        addCrystalChoice(depthCrystalMagentaBigProto, 16);
                        if (crystalChoices.empty()) {
                            advanceDepthDecorSubphase(4);
                            phaseComplete = false;
                            break;
                        }
                        int totalCrystalWeight = 0;
                        for (const CrystalChoice& choice : crystalChoices) totalCrystalWeight += choice.weight;
                        const std::array<glm::ivec3, 4> kSideDirs = {
                            glm::ivec3(1, 0, 0),
                            glm::ivec3(-1, 0, 0),
                            glm::ivec3(0, 0, 1),
                            glm::ivec3(0, 0, -1)
                        };
                        const int startLayer = clampLayerCursor(size);
                        const int endLayer = std::min(size, startLayer + depthDecorLayersPerUpdate);
                        for (int localY = startLayer; localY < endLayer; ++localY) {
                            const int y = sectionMinYDepth + localY;
                            if (y > unifiedDepthsTopY || y <= unifiedDepthsMinY) continue;
                            for (int z = sectionMinZ; z <= sectionMaxZ; ++z) {
                                for (int x = sectionMinX; x <= sectionMaxX; ++x) {
                                    const glm::ivec3 bankCell(x, y, z);
                                    const uint32_t bankId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, bankCell);
                                    if (!isSolidSupport(bankId)) continue;
                                    if (bankId == waterId || isDepthLavaId(bankId)) continue;
                                    const glm::ivec3 placeCell = bankCell + glm::ivec3(0, 1, 0);
                                    if (!inCurrentSection(placeCell)) continue;
                                    if (VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, placeCell) != 0u) continue;
                                    const glm::ivec3 placeAboveCell = placeCell + glm::ivec3(0, 1, 0);
                                    if (!inCurrentSection(placeAboveCell)) continue;
                                    if (VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, placeAboveCell) != 0u) continue;
                                    if (isAnyWaterCell(placeCell) || isAnyWaterCell(placeAboveCell)) continue;
                                    bool sideTouchesWaterAtPlacementHeight = false;
                                    for (const glm::ivec3& side : kSideDirs) {
                                        if (isAnyWaterCell(placeCell + side) || isAnyWaterCell(placeAboveCell + side)) {
                                            sideTouchesWaterAtPlacementHeight = true;
                                            break;
                                        }
                                    }
                                    if (sideTouchesWaterAtPlacementHeight) continue;
                                    bool touchesDepthRiver = false;
                                    for (int dz = -depthRiverCrystalBankSearchRadius; dz <= depthRiverCrystalBankSearchRadius && !touchesDepthRiver; ++dz) {
                                        for (int dx = -depthRiverCrystalBankSearchRadius; dx <= depthRiverCrystalBankSearchRadius && !touchesDepthRiver; ++dx) {
                                            for (int down = 0; down <= depthRiverCrystalBankSearchDown; ++down) {
                                                const glm::ivec3 depthOffset(dx, -down, dz);
                                                if (isDepthRiverWaterCell(bankCell + depthOffset)
                                                    || isDepthRiverWaterCell(placeCell + depthOffset)) {
                                                    touchesDepthRiver = true;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                    if (!touchesDepthRiver) continue;
                                    const uint32_t spawnSeed = hash3DInt(x + 1249, y - 811, z + 947);
                                    if (static_cast<int>(spawnSeed % 100u) >= depthRiverCrystalPercent) continue;
                                    int pick = static_cast<int>((spawnSeed >> 8u) % static_cast<uint32_t>(totalCrystalWeight));
                                    uint32_t crystalId = crystalChoices.front().prototypeID;
                                    for (const CrystalChoice& choice : crystalChoices) {
                                        if (pick < choice.weight) {
                                            crystalId = choice.prototypeID;
                                            break;
                                        }
                                        pick -= choice.weight;
                                    }
                                    voxelWorld.setBlock(placeCell, crystalId, packColor(glm::vec3(1.0f)), false);
                                    wroteAny = true;
                                }
                            }
                        }
                        jobState.depthDecorLayerCursor = endLayer;
                        if (endLayer >= size) {
                            advanceDepthDecorSubphase(4);
                        }
                        phaseComplete = false;
                        break;
                    }
                    case 4: {
                        if (!(depthMossPercent > 0
                            && depthMossWallProtoPosX && depthMossWallProtoNegX
                            && depthMossWallProtoPosZ && depthMossWallProtoNegZ)) {
                            jobState.depthDecorSubphase = 5;
                            phaseComplete = false;
                            break;
                        }
                        static const std::array<glm::ivec3, 4> kSideDirs = {
                            glm::ivec3(1, 0, 0),
                            glm::ivec3(-1, 0, 0),
                            glm::ivec3(0, 0, 1),
                            glm::ivec3(0, 0, -1)
                        };
                        const int startLayer = clampLayerCursor(size);
                        const int endLayer = std::min(size, startLayer + depthDecorLayersPerUpdate);
                        for (int localY = startLayer; localY < endLayer; ++localY) {
                            const int y = sectionMinYDepth + localY;
                            if (y > unifiedDepthsTopY || y <= unifiedDepthsMinY) continue;
                            for (int z = sectionMinZ; z <= sectionMaxZ; ++z) {
                                for (int x = sectionMinX; x <= sectionMaxX; ++x) {
                                    const glm::ivec3 cell(x, y, z);
                                    if (VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, cell) != 0u) continue;
                                    bool nearRiverWater = false;
                                    for (const glm::ivec3& d : kSideDirs) {
                                        if (isDepthRiverWaterCell(cell + d)) {
                                            nearRiverWater = true;
                                            break;
                                        }
                                    }
                                    if (!nearRiverWater) continue;
                                    const uint32_t seed = hash3DInt(x + 701, y - 359, z + 1013);
                                    if (static_cast<int>(seed % 100u) >= depthMossPercent) continue;
                                    std::array<int, 4> mossCandidates = {-1, -1, -1, -1};
                                    int candidateCount = 0;
                                    auto trySupport = [&](const glm::ivec3& offset, const Entity* mossProto) {
                                        if (!mossProto) return;
                                        const uint32_t supportId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, cell + offset);
                                        if (!isSolidSupport(supportId)) return;
                                        mossCandidates[static_cast<size_t>(candidateCount)] = mossProto->prototypeID;
                                        candidateCount += 1;
                                    };
                                    trySupport(glm::ivec3(1, 0, 0), depthMossWallProtoPosX);
                                    trySupport(glm::ivec3(-1, 0, 0), depthMossWallProtoNegX);
                                    trySupport(glm::ivec3(0, 0, 1), depthMossWallProtoPosZ);
                                    trySupport(glm::ivec3(0, 0, -1), depthMossWallProtoNegZ);
                                    if (candidateCount <= 0) continue;
                                    const int pick = static_cast<int>((seed >> 9u) % static_cast<uint32_t>(candidateCount));
                                    const int mossId = mossCandidates[static_cast<size_t>(pick)];
                                    if (mossId < 0) continue;
                                    voxelWorld.setBlock(cell, static_cast<uint32_t>(mossId), packColor(glm::vec3(0.56f, 0.72f, 0.48f)), false);
                                    wroteAny = true;
                                }
                            }
                        }
                        jobState.depthDecorLayerCursor = endLayer;
                        if (endLayer >= size) {
                            jobState.depthDecorSubphase = 5;
                        }
                        phaseComplete = false;
                        break;
                    }
                    default:
                        break;
                }

                if (jobState.depthDecorSubphase >= 5) {
                    jobState.depthDecorSubphase = 0;
                    jobState.depthDecorLayerCursor = 0;
                    jobState.depthDecorBeamStartsPlaced = 0;
                    phaseComplete = true;
                } else {
                    phaseComplete = false;
                }
            }
                if (runWaterfallPhase && waterfallEnabled) {
                const bool hasWaterSlopeProtos =
                    (waterSlopeProtoPosX && waterSlopeProtoPosX->prototypeID > 0)
                    || (waterSlopeProtoNegX && waterSlopeProtoNegX->prototypeID > 0)
                    || (waterSlopeProtoPosZ && waterSlopeProtoPosZ->prototypeID > 0)
                    || (waterSlopeProtoNegZ && waterSlopeProtoNegZ->prototypeID > 0)
                    || (waterSlopeCornerProtoPosXPosZ && waterSlopeCornerProtoPosXPosZ->prototypeID > 0)
                    || (waterSlopeCornerProtoPosXNegZ && waterSlopeCornerProtoPosXNegZ->prototypeID > 0)
                    || (waterSlopeCornerProtoNegXPosZ && waterSlopeCornerProtoNegXPosZ->prototypeID > 0)
                    || (waterSlopeCornerProtoNegXNegZ && waterSlopeCornerProtoNegXNegZ->prototypeID > 0);
                if (hasWaterSlopeProtos) {
                    const int sectionMinX = sectionCoord.x * size;
                    const int sectionMaxX = sectionMinX + size - 1;
                    const int sectionMinY = sectionCoord.y * size;
                    const int sectionMaxY = sectionMinY + size - 1;
                    const int sectionMinZ = sectionCoord.z * size;
                    const int sectionMaxZ = sectionMinZ + size - 1;
                    const int waterfallCascadeMaxDrop = std::max(
                        8,
                        std::min(
                            waterfallMaxDrop,
                            std::max(8, getRegistryInt(baseSystem, "WaterfallCascadeMaxDropClamp", 96))
                        )
                    );
                    auto slopePrototypeForExposedAir = [&](const glm::ivec3& dir) -> int {
                        // Exposed air marks the downhill side. Match runtime slope semantics:
                        // PosX/PosZ slope names are downhill toward -X/-Z respectively.
                        if (dir.x > 0) return waterSlopeProtoNegX ? waterSlopeProtoNegX->prototypeID : -1;
                        if (dir.x < 0) return waterSlopeProtoPosX ? waterSlopeProtoPosX->prototypeID : -1;
                        if (dir.z > 0) return waterSlopeProtoNegZ ? waterSlopeProtoNegZ->prototypeID : -1;
                        if (dir.z < 0) return waterSlopeProtoPosZ ? waterSlopeProtoPosZ->prototypeID : -1;
                        return -1;
                    };
                    auto cornerSlopePrototypeForAirPair = [&](const glm::ivec3& dirA, const glm::ivec3& dirB) -> int {
                        const int sumX = dirA.x + dirB.x;
                        const int sumZ = dirA.z + dirB.z;
                        if (sumX > 0 && sumZ > 0) {
                            return waterSlopeCornerProtoNegXNegZ ? waterSlopeCornerProtoNegXNegZ->prototypeID : -1;
                        }
                        if (sumX > 0 && sumZ < 0) {
                            return waterSlopeCornerProtoNegXPosZ ? waterSlopeCornerProtoNegXPosZ->prototypeID : -1;
                        }
                        if (sumX < 0 && sumZ > 0) {
                            return waterSlopeCornerProtoPosXNegZ ? waterSlopeCornerProtoPosXNegZ->prototypeID : -1;
                        }
                        if (sumX < 0 && sumZ < 0) {
                            return waterSlopeCornerProtoPosXPosZ ? waterSlopeCornerProtoPosXPosZ->prototypeID : -1;
                        }
                        return -1;
                    };
                    auto inCurrentSectionXZ = [&](const glm::ivec3& cell) {
                        return cell.x >= sectionMinX && cell.x <= sectionMaxX
                            && cell.z >= sectionMinZ && cell.z <= sectionMaxZ;
                    };
                    auto stopsWaterfallDrop = [&](uint32_t id) {
                        if (id == 0u) return false;
                        if (id >= static_cast<uint32_t>(prototypes.size())) return true;
                        return prototypes[static_cast<size_t>(id)].isSolid;
                    };
                    auto isWaterOrWaterSlopeId = [&](uint32_t id) {
                        if (id == static_cast<uint32_t>(waterProto->prototypeID)) return true;
                        if (waterSlopeProtoPosX && id == static_cast<uint32_t>(waterSlopeProtoPosX->prototypeID)) return true;
                        if (waterSlopeProtoNegX && id == static_cast<uint32_t>(waterSlopeProtoNegX->prototypeID)) return true;
                        if (waterSlopeProtoPosZ && id == static_cast<uint32_t>(waterSlopeProtoPosZ->prototypeID)) return true;
                        if (waterSlopeProtoNegZ && id == static_cast<uint32_t>(waterSlopeProtoNegZ->prototypeID)) return true;
                        if (waterSlopeCornerProtoPosXPosZ && id == static_cast<uint32_t>(waterSlopeCornerProtoPosXPosZ->prototypeID)) return true;
                        if (waterSlopeCornerProtoPosXNegZ && id == static_cast<uint32_t>(waterSlopeCornerProtoPosXNegZ->prototypeID)) return true;
                        if (waterSlopeCornerProtoNegXPosZ && id == static_cast<uint32_t>(waterSlopeCornerProtoNegXPosZ->prototypeID)) return true;
                        if (waterSlopeCornerProtoNegXNegZ && id == static_cast<uint32_t>(waterSlopeCornerProtoNegXNegZ->prototypeID)) return true;
                        return false;
                    };

                    auto carveWaterfallImpactPool = [&](const glm::ivec3& solidImpactCell,
                                                        const glm::ivec3& lastWaterCell,
                                                        uint32_t waterColor,
                                                        int totalFallDistance,
                                                        std::vector<glm::ivec3>* outCascadeSources) {
                        if (totalFallDistance <= 5) return false;
                        const int extraFall = totalFallDistance - 5;
                        const int poolRadius = std::max(1, std::min(8, 1 + extraFall / 10));
                        const int poolDepthBase = std::max(1, std::min(5, 1 + extraFall / 14));
                        // Start pool one block into terrain (at impact-solid height),
                        // not at the previous water surface cell.
                        const int surfaceY = std::min(lastWaterCell.y - 1, solidImpactCell.y);
                        bool carvedAny = false;
                        std::vector<glm::ivec3> carvedCells;
                        carvedCells.reserve(static_cast<size_t>((poolRadius * 2 + 1) * (poolRadius * 2 + 1)));
                        for (int dz = -poolRadius; dz <= poolRadius; ++dz) {
                            for (int dx = -poolRadius; dx <= poolRadius; ++dx) {
                                const float dist = std::sqrt(static_cast<float>(dx * dx + dz * dz));
                                if (dist > static_cast<float>(poolRadius) + 0.25f) continue;
                                const int localDepth = std::max(
                                    1,
                                    poolDepthBase - static_cast<int>(std::floor(dist * 0.85f))
                                );
                                for (int depthStep = 0; depthStep < localDepth; ++depthStep) {
                                    const glm::ivec3 carveCell(
                                        solidImpactCell.x + dx,
                                        surfaceY - depthStep,
                                        solidImpactCell.z + dz
                                    );
                                    const uint32_t carveId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, carveCell);
                                    if (isWaterOrWaterSlopeId(carveId)) continue;
                                    bool replaceCell = false;
                                    if (carveId == 0u) {
                                        replaceCell = true;
                                    } else if (carveId < static_cast<uint32_t>(prototypes.size())) {
                                        replaceCell = prototypes[static_cast<size_t>(carveId)].isSolid;
                                    }
                                    if (!replaceCell) continue;
                                    voxelWorld.setBlock(carveCell,
                                        static_cast<uint32_t>(waterProto->prototypeID),
                                        waterColor,
                                        false
                                    );
                                    wroteAny = true;
                                    carvedAny = true;
                                    carvedCells.push_back(carveCell);
                                }
                            }
                        }
                        if (carvedAny && outCascadeSources) {
                            static const std::array<glm::ivec3, 4> kPoolSideDirs = {
                                glm::ivec3(1, 0, 0),
                                glm::ivec3(-1, 0, 0),
                                glm::ivec3(0, 0, 1),
                                glm::ivec3(0, 0, -1)
                            };
                            std::unordered_set<uint64_t> seen;
                            seen.reserve(carvedCells.size());
                            for (const glm::ivec3& cell : carvedCells) {
                                if (!inCurrentSectionXZ(cell)) continue;
                                if (cell.y < sectionMinY || cell.y > sectionMaxY) continue;
                                const uint32_t id = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, cell);
                                if (id != static_cast<uint32_t>(waterProto->prototypeID)) continue;
                                bool hasAirSide = false;
                                for (const glm::ivec3& dir : kPoolSideDirs) {
                                    const glm::ivec3 sideCell = cell + dir;
                                    if (!inCurrentSectionXZ(sideCell)) continue;
                                    if (sideCell.y < sectionMinY || sideCell.y > sectionMaxY) continue;
                                    const uint32_t sideId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, sideCell);
                                    if (sideId == 0u && slopePrototypeForExposedAir(dir) > 0) {
                                        hasAirSide = true;
                                        break;
                                    }
                                }
                                if (!hasAirSide) continue;
                                const uint32_t h0 = hash3DInt(cell.x + 991, cell.y - 1543, cell.z + 2713);
                                const uint32_t h1 = hash3DInt(cell.x - 3119, cell.y + 733, cell.z - 4723);
                                const uint64_t key = (static_cast<uint64_t>(h0) << 32u) | static_cast<uint64_t>(h1);
                                if (!seen.insert(key).second) continue;
                                outCascadeSources->push_back(cell);
                            }
                        }
                        return carvedAny;
                    };

                    static const std::array<glm::ivec3, 4> kSideDirs = {
                        glm::ivec3(1, 0, 0),
                        glm::ivec3(-1, 0, 0),
                        glm::ivec3(0, 0, 1),
                        glm::ivec3(0, 0, -1)
                    };
                    auto makeCascadeSourceKey = [&](const glm::ivec3& cell) -> uint64_t {
                        const uint32_t h0 = hash3DInt(cell.x + 1021, cell.y - 4093, cell.z + 7993);
                        const uint32_t h1 = hash3DInt(cell.x - 5153, cell.y + 1237, cell.z - 6949);
                        return (static_cast<uint64_t>(h0) << 32u) | static_cast<uint64_t>(h1);
                    };
                    std::unordered_map<uint64_t, int> cascadeBestFallForSource;
                    cascadeBestFallForSource.reserve(static_cast<size_t>(size * size));
                    const int waterfallCascadeBudgetPerSource = std::max(
                        4,
                        std::min(
                            waterfallCascadeBudget,
                            getRegistryInt(baseSystem, "WaterfallCascadeBudgetPerSource", 48)
                        )
                    );
                    auto runCascadeFromSource = [&](const glm::ivec3& initialSource,
                                                    uint32_t fallbackColor,
                                                    int initialFallDistance,
                                                    bool hasPendingPool,
                                                    const glm::ivec3& pendingPoolImpactCell,
                                                    const glm::ivec3& pendingPoolLastWaterCell,
                                                    uint32_t pendingPoolWaterColor) {
                        std::vector<WaterfallCascadeSeed> queue;
                        queue.reserve(32);
                        queue.push_back({
                            initialSource,
                            std::max(0, initialFallDistance),
                            hasPendingPool,
                            pendingPoolImpactCell,
                            pendingPoolLastWaterCell,
                            pendingPoolWaterColor
                        });
                        size_t head = 0;
                        int budgetRemaining = waterfallCascadeBudgetPerSource;
                        while (head < queue.size() && budgetRemaining > 0) {
                            const WaterfallCascadeSeed sourceSeed = queue[head++];
                            const glm::ivec3 sourceCell = sourceSeed.cell;
                            const int sourceTotalFallDistance = std::max(0, sourceSeed.totalFallDistance);
                            budgetRemaining -= 1;
                            if (!inCurrentSectionXZ(sourceCell)) continue;
                            if (sourceCell.y < sectionMinY || sourceCell.y > sectionMaxY) continue;
                            const uint64_t sourceKey = makeCascadeSourceKey(sourceCell);
                            auto bestIt = cascadeBestFallForSource.find(sourceKey);
                            if (bestIt != cascadeBestFallForSource.end() && bestIt->second >= sourceTotalFallDistance) continue;
                            cascadeBestFallForSource[sourceKey] = sourceTotalFallDistance;
                            const uint32_t sourceId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, sourceCell);
                            if (sourceId != static_cast<uint32_t>(waterProto->prototypeID)) {
                                if (sourceSeed.hasPendingPool) {
                                    const uint32_t poolColor = (sourceSeed.pendingPoolWaterColor & 0x00ffffffu) != 0u
                                        ? sourceSeed.pendingPoolWaterColor
                                        : fallbackColor;
                                    std::vector<glm::ivec3> poolCascadeSources;
                                    (void)carveWaterfallImpactPool(
                                        sourceSeed.pendingPoolImpactCell,
                                        sourceSeed.pendingPoolLastWaterCell,
                                        poolColor,
                                        sourceTotalFallDistance,
                                        &poolCascadeSources
                                    );
                                    for (const glm::ivec3& poolSource : poolCascadeSources) {
                                        queue.push_back({
                                            poolSource,
                                            sourceTotalFallDistance,
                                            false,
                                            glm::ivec3(0),
                                            glm::ivec3(0),
                                            0u
                                        });
                                    }
                                }
                                continue;
                            }
                            uint32_t sourceWaterColor = VoxelMeshInitSystemLogic::GetVoxelColorAt(voxelWorld, sourceCell);
                            if ((sourceWaterColor & 0x00ffffffu) == 0u) sourceWaterColor = fallbackColor;
                            bool emittedCascade = false;
                            auto emitCascadeSlope = [&](const glm::ivec3& slopeCell, int slopePrototypeID) -> bool {
                                if (!inCurrentSectionXZ(slopeCell)) return false;
                                if (slopeCell.y < sectionMinY || slopeCell.y > sectionMaxY) return false;
                                if (slopePrototypeID <= 0) return false;
                                if (VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, slopeCell) != 0u) return false;

                                glm::ivec3 dropCell = slopeCell + glm::ivec3(0, -1, 0);
                                glm::ivec3 terminationCell = sourceCell;
                                bool terminatedBySolid = false;
                                int placedCount = 0;
                                int traversedCount = 0;
                                for (int dropStep = 0; dropStep < waterfallCascadeMaxDrop && dropCell.y >= sectionMinY; ++dropStep) {
                                    const uint32_t dropId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, dropCell);
                                    if (isWaterOrWaterSlopeId(dropId)) {
                                        terminationCell = dropCell;
                                        traversedCount += 1;
                                        dropCell.y -= 1;
                                        continue;
                                    }
                                    if (stopsWaterfallDrop(dropId)) {
                                        terminatedBySolid = true;
                                        break;
                                    }
                                    voxelWorld.setBlock(dropCell,
                                        static_cast<uint32_t>(waterProto->prototypeID),
                                        sourceWaterColor,
                                        false
                                    );
                                    wroteAny = true;
                                    placedCount += 1;
                                    terminationCell = dropCell;
                                    traversedCount += 1;
                                    dropCell.y -= 1;
                                }

                                if (placedCount <= 0) return false;

                                const int totalFallDistance = sourceTotalFallDistance + traversedCount;

                                voxelWorld.setBlock(slopeCell,
                                    static_cast<uint32_t>(slopePrototypeID),
                                    sourceWaterColor,
                                    false
                                );
                                wroteAny = true;

                                if (terminatedBySolid
                                    && terminationCell.y >= sectionMinY && terminationCell.y <= sectionMaxY
                                    && inCurrentSectionXZ(terminationCell)) {
                                    queue.push_back({
                                        terminationCell,
                                        totalFallDistance,
                                        true,
                                        dropCell,
                                        terminationCell,
                                        sourceWaterColor
                                    });
                                }
                                return true;
                            };

                            std::array<glm::ivec3, 2> cornerAirDirs{
                                glm::ivec3(0),
                                glm::ivec3(0)
                            };
                            int cornerAirCount = 0;
                            int cornerSolidCount = 0;
                            for (const glm::ivec3& dir : kSideDirs) {
                                const glm::ivec3 sideCell = sourceCell + dir;
                                if (!inCurrentSectionXZ(sideCell)) continue;
                                if (sideCell.y < sectionMinY || sideCell.y > sectionMaxY) continue;
                                const uint32_t sideId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, sideCell);
                                if (sideId == 0u && slopePrototypeForExposedAir(dir) > 0) {
                                    if (cornerAirCount < 2) cornerAirDirs[static_cast<size_t>(cornerAirCount)] = dir;
                                    cornerAirCount += 1;
                                } else if (stopsWaterfallDrop(sideId)) {
                                    cornerSolidCount += 1;
                                }
                            }

                            bool usedCornerSlope = false;
                            if (cornerAirCount == 2 && cornerSolidCount == 2) {
                                const glm::ivec3 dirA = cornerAirDirs[0];
                                const glm::ivec3 dirB = cornerAirDirs[1];
                                const int dot = dirA.x * dirB.x + dirA.z * dirB.z;
                                if (dot == 0) {
                                    const int cornerSlopePrototypeID = cornerSlopePrototypeForAirPair(dirA, dirB);
                                    if (cornerSlopePrototypeID > 0) {
                                        const glm::ivec3 cornerCell = sourceCell + dirA + dirB;
                                        usedCornerSlope = emitCascadeSlope(cornerCell, cornerSlopePrototypeID);
                                    }
                                }
                            }

                            emittedCascade = emittedCascade || usedCornerSlope;

                            if (!usedCornerSlope) {
                                for (const glm::ivec3& dir : kSideDirs) {
                                    const glm::ivec3 slopeCell = sourceCell + dir;
                                    const int slopePrototypeID = slopePrototypeForExposedAir(dir);
                                    if (emitCascadeSlope(slopeCell, slopePrototypeID)) {
                                        emittedCascade = true;
                                    }
                                }
                            }

                            if (sourceSeed.hasPendingPool && !emittedCascade) {
                                const uint32_t poolColor = (sourceSeed.pendingPoolWaterColor & 0x00ffffffu) != 0u
                                    ? sourceSeed.pendingPoolWaterColor
                                    : sourceWaterColor;
                                std::vector<glm::ivec3> poolCascadeSources;
                                (void)carveWaterfallImpactPool(
                                    sourceSeed.pendingPoolImpactCell,
                                    sourceSeed.pendingPoolLastWaterCell,
                                    poolColor,
                                    sourceTotalFallDistance,
                                    &poolCascadeSources
                                );
                                for (const glm::ivec3& poolSource : poolCascadeSources) {
                                    queue.push_back({
                                        poolSource,
                                        sourceTotalFallDistance,
                                        false,
                                        glm::ivec3(0),
                                        glm::ivec3(0),
                                        0u
                                    });
                                }
                            }
                        }
                    };

                    const int waterfallContinuationCellsPerUpdate = std::max(
                        1,
                        getRegistryInt(baseSystem, "WaterfallContinuationCellsPerUpdate", 32)
                    );
                    const int waterfallSourceCellsPerUpdate = std::max(
                        1,
                        getRegistryInt(baseSystem, "WaterfallSourceCellsPerUpdate", 96)
                    );
                    const int continuationCellCount = size * size;
                    const int sourceCellCount = size * size * size;
                    jobState.waterfallContinuationCursor = std::max(0, std::min(jobState.waterfallContinuationCursor, continuationCellCount));
                    jobState.waterfallSourceCursor = std::max(0, std::min(jobState.waterfallSourceCursor, sourceCellCount));

                    if (jobState.waterfallCascadeQueueHead > jobState.waterfallCascadeQueue.size()) {
                        jobState.waterfallCascadeQueueHead = 0;
                        jobState.waterfallCascadeQueue.clear();
                    }
                    auto pendingCascadeSeedCount = [&]() -> size_t {
                        return (jobState.waterfallCascadeQueueHead >= jobState.waterfallCascadeQueue.size())
                            ? 0u
                            : (jobState.waterfallCascadeQueue.size() - jobState.waterfallCascadeQueueHead);
                    };
                    const int waterfallCascadeSeedsPerUpdate = std::max(
                        1,
                        getRegistryInt(baseSystem, "WaterfallCascadeSeedsPerUpdate", 16)
                    );
                    const size_t waterfallCascadeQueueSoftCap = static_cast<size_t>(std::max(
                        waterfallCascadeSeedsPerUpdate,
                        getRegistryInt(baseSystem, "WaterfallCascadeQueueSoftCap", 512)
                    ));
                    auto canQueueMoreCascadeSeeds = [&]() -> bool {
                        return pendingCascadeSeedCount() < waterfallCascadeQueueSoftCap;
                    };
                    auto enqueueCascadeSeed = [&](const WaterfallCascadeSeed& seed) {
                        if (!canQueueMoreCascadeSeeds()) return false;
                        jobState.waterfallCascadeQueue.push_back(seed);
                        return true;
                    };

                    // Continue waterfalls entering from the section above in slices.
                    int continuationProcessed = 0;
                    while (jobState.waterfallContinuationCursor < continuationCellCount
                        && continuationProcessed < waterfallContinuationCellsPerUpdate
                        && canQueueMoreCascadeSeeds()) {
                        const int idx = jobState.waterfallContinuationCursor;
                        const int localX = idx % size;
                        const int localZ = idx / size;
                        const int worldX = sectionMinX + localX;
                        const int worldZ = sectionMinZ + localZ;
                        jobState.waterfallContinuationCursor += 1;
                        continuationProcessed += 1;

                        const glm::ivec3 aboveCell(worldX, sectionMaxY + 1, worldZ);
                        const uint32_t aboveId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, aboveCell);
                        if (!isWaterOrWaterSlopeId(aboveId)) continue;
                        uint32_t carryWaterColor = VoxelMeshInitSystemLogic::GetVoxelColorAt(voxelWorld, aboveCell);
                        if ((carryWaterColor & 0x00ffffffu) == 0u) {
                            carryWaterColor = packedWaterColorUnknown;
                        }

                        glm::ivec3 dropCell(worldX, sectionMaxY, worldZ);
                        glm::ivec3 terminationCell(worldX, sectionMaxY, worldZ);
                        bool touchedSectionWater = false;
                        bool terminatedBySolid = false;
                        int traversedCount = 0;
                        for (int dropStep = 0; dropStep < waterfallCascadeMaxDrop && dropCell.y >= sectionMinY; ++dropStep) {
                            const uint32_t dropId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, dropCell);
                            if (isWaterOrWaterSlopeId(dropId)) {
                                terminationCell = dropCell;
                                touchedSectionWater = true;
                                traversedCount += 1;
                                dropCell.y -= 1;
                                continue;
                            }
                            if (stopsWaterfallDrop(dropId)) {
                                terminatedBySolid = true;
                                break;
                            }
                            voxelWorld.setBlock(dropCell,
                                static_cast<uint32_t>(waterProto->prototypeID),
                                carryWaterColor,
                                false
                            );
                            wroteAny = true;
                            terminationCell = dropCell;
                            touchedSectionWater = true;
                            traversedCount += 1;
                            dropCell.y -= 1;
                        }

                        if (terminatedBySolid && touchedSectionWater
                            && terminationCell.y >= sectionMinY && terminationCell.y <= sectionMaxY) {
                            (void)enqueueCascadeSeed({
                                terminationCell,
                                std::max(0, traversedCount),
                                true,
                                dropCell,
                                terminationCell,
                                carryWaterColor
                            });
                        }
                    }

                    // Scan top-exposed water sources in slices.
                    int sourceProcessed = 0;
                    while (jobState.waterfallSourceCursor < sourceCellCount
                        && sourceProcessed < waterfallSourceCellsPerUpdate
                        && canQueueMoreCascadeSeeds()) {
                        const int idx = jobState.waterfallSourceCursor;
                        const int localX = idx % size;
                        const int yz = idx / size;
                        const int localZ = yz % size;
                        const int localY = yz / size;
                        const glm::ivec3 sourceCell(
                            sectionMinX + localX,
                            sectionMinY + localY,
                            sectionMinZ + localZ
                        );
                        jobState.waterfallSourceCursor += 1;
                        sourceProcessed += 1;

                        const uint32_t sourceId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, sourceCell);
                        if (sourceId != static_cast<uint32_t>(waterProto->prototypeID)) continue;
                        const uint32_t aboveId = VoxelMeshInitSystemLogic::GetVoxelIdAt(
                            voxelWorld,
                            sourceCell + glm::ivec3(0, 1, 0)
                        );
                        const bool topExposed = !isWaterOrWaterSlopeId(aboveId);
                        if (!topExposed) continue;
                        (void)enqueueCascadeSeed({
                            sourceCell,
                            0,
                            false,
                            glm::ivec3(0),
                            glm::ivec3(0),
                            0u
                        });
                    }

                    int cascadeSeedsProcessed = 0;
                    while (jobState.waterfallCascadeQueueHead < jobState.waterfallCascadeQueue.size()
                        && cascadeSeedsProcessed < waterfallCascadeSeedsPerUpdate) {
                        const WaterfallCascadeSeed seed = jobState.waterfallCascadeQueue[jobState.waterfallCascadeQueueHead++];
                        uint32_t seedWaterColor = VoxelMeshInitSystemLogic::GetVoxelColorAt(voxelWorld, seed.cell);
                        if ((seedWaterColor & 0x00ffffffu) == 0u) {
                            seedWaterColor = (seed.pendingPoolWaterColor & 0x00ffffffu) != 0u
                                ? seed.pendingPoolWaterColor
                                : packedWaterColorUnknown;
                        }
                        runCascadeFromSource(
                            seed.cell,
                            seedWaterColor,
                            seed.totalFallDistance,
                            seed.hasPendingPool,
                            seed.pendingPoolImpactCell,
                            seed.pendingPoolLastWaterCell,
                            seed.pendingPoolWaterColor
                        );
                        cascadeSeedsProcessed += 1;
                    }
                    if (jobState.waterfallCascadeQueueHead > 0
                        && (jobState.waterfallCascadeQueueHead * 2 >= jobState.waterfallCascadeQueue.size()
                            || jobState.waterfallCascadeQueueHead == jobState.waterfallCascadeQueue.size())) {
                        jobState.waterfallCascadeQueue.erase(
                            jobState.waterfallCascadeQueue.begin(),
                            jobState.waterfallCascadeQueue.begin() + static_cast<long>(jobState.waterfallCascadeQueueHead)
                        );
                        jobState.waterfallCascadeQueueHead = 0;
                    }

                    const bool continuationDone = (jobState.waterfallContinuationCursor >= continuationCellCount);
                    const bool sourceDone = (jobState.waterfallSourceCursor >= sourceCellCount);
                    const bool cascadeQueueDrained = (pendingCascadeSeedCount() == 0u);
                    if (!continuationDone || !sourceDone || !cascadeQueueDrained) {
                        phaseComplete = false;
                    }
                }
            }
                if (runWaterfallPhase
                && chalkEnabled
                && chalkProto
                && chalkProto->prototypeID > 0
                && !chalkPlacements.empty()) {
                const int sectionMinX = sectionCoord.x * size;
                const int sectionMaxX = sectionMinX + size - 1;
                const int sectionMinY = sectionCoord.y * size;
                const int sectionMaxY = sectionMinY + size - 1;
                const int sectionMinZ = sectionCoord.z * size;
                const int sectionMaxZ = sectionMinZ + size - 1;

                auto inCurrentSection = [&](const glm::ivec3& cell) {
                    return cell.x >= sectionMinX && cell.x <= sectionMaxX
                        && cell.y >= sectionMinY && cell.y <= sectionMaxY
                        && cell.z >= sectionMinZ && cell.z <= sectionMaxZ;
                };
                auto isWaterSlopeId = [&](uint32_t id) {
                    if (id == 0u) return false;
                    if (waterSlopeProtoPosX && id == static_cast<uint32_t>(waterSlopeProtoPosX->prototypeID)) return true;
                    if (waterSlopeProtoNegX && id == static_cast<uint32_t>(waterSlopeProtoNegX->prototypeID)) return true;
                    if (waterSlopeProtoPosZ && id == static_cast<uint32_t>(waterSlopeProtoPosZ->prototypeID)) return true;
                    if (waterSlopeProtoNegZ && id == static_cast<uint32_t>(waterSlopeProtoNegZ->prototypeID)) return true;
                    if (waterSlopeCornerProtoPosXPosZ && id == static_cast<uint32_t>(waterSlopeCornerProtoPosXPosZ->prototypeID)) return true;
                    if (waterSlopeCornerProtoPosXNegZ && id == static_cast<uint32_t>(waterSlopeCornerProtoPosXNegZ->prototypeID)) return true;
                    if (waterSlopeCornerProtoNegXPosZ && id == static_cast<uint32_t>(waterSlopeCornerProtoNegXPosZ->prototypeID)) return true;
                    if (waterSlopeCornerProtoNegXNegZ && id == static_cast<uint32_t>(waterSlopeCornerProtoNegXNegZ->prototypeID)) return true;
                    return false;
                };
                auto isWaterOrSlopeId = [&](uint32_t id) {
                    if (id == static_cast<uint32_t>(waterProto->prototypeID)) return true;
                    return isWaterSlopeId(id);
                };
                static const std::array<glm::ivec3, 4> kHorizontalDirs = {
                    glm::ivec3(1, 0, 0),
                    glm::ivec3(-1, 0, 0),
                    glm::ivec3(0, 0, 1),
                    glm::ivec3(0, 0, -1)
                };
                auto isFallingWaterCell = [&](const glm::ivec3& cell) {
                    const uint32_t id = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, cell);
                    if (isWaterSlopeId(id)) return true;
                    if (id != static_cast<uint32_t>(waterProto->prototypeID)) return false;
                    const uint32_t belowId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, cell + glm::ivec3(0, -1, 0));
                    if (!isWaterOrSlopeId(belowId)) return false;
                    for (const glm::ivec3& dir : kHorizontalDirs) {
                        const uint32_t sideId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, cell + dir);
                        if (sideId == 0u) return true;
                    }
                    return false;
                };
                auto hasNearbyFallingWater = [&](const glm::ivec3& groundCell) {
                    constexpr int kSearchRadiusXZ = 2;
                    constexpr int kSearchMinDY = -6;
                    constexpr int kSearchMaxDY = 6;
                    for (int dy = kSearchMinDY; dy <= kSearchMaxDY; ++dy) {
                        for (int dz = -kSearchRadiusXZ; dz <= kSearchRadiusXZ; ++dz) {
                            for (int dx = -kSearchRadiusXZ; dx <= kSearchRadiusXZ; ++dx) {
                                const glm::ivec3 sampleCell = groundCell + glm::ivec3(dx, dy, dz);
                                if (!inCurrentSection(sampleCell)) continue;
                                if (isFallingWaterCell(sampleCell)) return true;
                            }
                        }
                    }
                    return false;
                };

                const int chalkPlacementsPerUpdate = std::max(
                    1,
                    getRegistryInt(baseSystem, "ChalkPlacementsPerUpdate", 128)
                );
                const int chalkPlacementCount = static_cast<int>(chalkPlacements.size());
                jobState.chalkCursor = std::max(0, std::min(jobState.chalkCursor, chalkPlacementCount));
                int chalkProcessed = 0;
                while (jobState.chalkCursor < chalkPlacementCount
                    && chalkProcessed < chalkPlacementsPerUpdate) {
                    const glm::ivec3& chalkCell = chalkPlacements[static_cast<size_t>(jobState.chalkCursor)];
                    jobState.chalkCursor += 1;
                    chalkProcessed += 1;
                    if (!inCurrentSection(chalkCell)) continue;
                    const glm::ivec3 aboveCell = chalkCell + glm::ivec3(0, 1, 0);
                    const uint32_t currentId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, chalkCell);
                    const uint32_t aboveId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, aboveCell);
                    if (currentId == 0u || currentId == static_cast<uint32_t>(waterProto->prototypeID)) continue;
                    if (aboveId != 0u) continue;
                    if (!hasNearbyFallingWater(chalkCell)) continue;

                    voxelWorld.setBlock(chalkCell,
                        static_cast<uint32_t>(chalkProto->prototypeID),
                        packColor(chalkColor),
                        false
                    );
                    wroteAny = true;

                    if (chalkStickSpawnPercent <= 0) continue;
                    if (!inCurrentSection(aboveCell)) continue;
                    if (VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, aboveCell) != 0u) continue;
                    const uint32_t stickSeed = hash3DInt(
                        chalkCell.x + chalkSeed * 11,
                        aboveCell.y + chalkSeed * 13,
                        chalkCell.z - chalkSeed * 17
                    );
                    if (static_cast<int>((stickSeed >> 3u) % 100u) >= chalkStickSpawnPercent) continue;
                    int stickID = (((stickSeed >> 9u) & 1u) == 0u)
                        ? (chalkStickProtoX ? chalkStickProtoX->prototypeID : -1)
                        : (chalkStickProtoZ ? chalkStickProtoZ->prototypeID : -1);
                    if (stickID < 0) {
                        stickID = chalkStickProtoX ? chalkStickProtoX->prototypeID
                            : (chalkStickProtoZ ? chalkStickProtoZ->prototypeID : -1);
                    }
                    if (stickID < 0) continue;
                    voxelWorld.setBlock(aboveCell,
                        static_cast<uint32_t>(stickID),
                        packColor(chalkColor),
                        false
                    );
                    wroteAny = true;
                }
                if (jobState.chalkCursor < chalkPlacementCount) {
                    phaseComplete = false;
                }
            }
                if (runLavaCascadePhase
                && isExpanseLevel
                && unifiedDepthsEnabled
                && depthLavaFloorEnabled
                && !depthLavaTileProtos.empty()) {
                const bool depthLavaCascadeEnabled = getRegistryBool(baseSystem, "DepthLavaCascadeEnabled", true);
                if (depthLavaCascadeEnabled) {
                    const int depthLavaCascadeMaxDrop = std::max(
                        8,
                        getRegistryInt(baseSystem, "DepthLavaCascadeMaxDrop", 300)
                    );
                    const int sectionMinX = sectionCoord.x * size;
                    const int sectionMaxX = sectionMinX + size - 1;
                    const int sectionMinY = sectionCoord.y * size;
                    const int sectionMaxY = sectionMinY + size - 1;
                    const int sectionMinZ = sectionCoord.z * size;
                    const int sectionMaxZ = sectionMinZ + size - 1;
                    const uint32_t packedDepthLavaColor = packColor(lavaColor);

                    auto inCurrentSectionXZ = [&](const glm::ivec3& cell) {
                        return cell.x >= sectionMinX && cell.x <= sectionMaxX
                            && cell.z >= sectionMinZ && cell.z <= sectionMaxZ;
                    };
                    auto isDepthLavaId = [&](uint32_t id) {
                        if (id == 0u) return false;
                        for (const Entity* lavaProto : depthLavaTileProtos) {
                            if (lavaProto && lavaProto->prototypeID > 0
                                && id == static_cast<uint32_t>(lavaProto->prototypeID)) {
                                return true;
                            }
                        }
                        return false;
                    };
                    auto canReplaceWithDepthLava = [&](uint32_t id) {
                        if (id == 0u) return true;
                        if (isDepthLavaId(id)) return false;
                        if (id >= static_cast<uint32_t>(prototypes.size())) return false;
                        return prototypes[static_cast<size_t>(id)].isBlock;
                    };
                    auto depthLavaTileIdFor = [&](int wx, int wz) -> uint32_t {
                        const int tx = positiveMod(wx, 3);
                        const int tz = positiveMod(wz, 3);
                        const int tileIdx = tz * 3 + tx;
                        const Entity* lavaTileProto = depthLavaTileProtos[static_cast<size_t>(tileIdx)];
                        if (lavaTileProto && lavaTileProto->prototypeID > 0) {
                            return static_cast<uint32_t>(lavaTileProto->prototypeID);
                        }
                        return static_cast<uint32_t>(waterProto->prototypeID);
                    };
                    auto dropDepthLavaFromSource = [&](const glm::ivec3& sourceCell, uint32_t fallbackColor) {
                        if (!inCurrentSectionXZ(sourceCell)) return;
                        if (sourceCell.y < sectionMinY || sourceCell.y > sectionMaxY) return;
                        glm::ivec3 dropCell = sourceCell + glm::ivec3(0, -1, 0);
                        uint32_t useColor = (fallbackColor & 0x00ffffffu) != 0u
                            ? fallbackColor
                            : packedDepthLavaColor;
                        for (int dropStep = 0; dropStep < depthLavaCascadeMaxDrop && dropCell.y >= sectionMinY; ++dropStep) {
                            if (dropCell.y <= unifiedDepthsMinY) break;
                            if (dropCell.y >= unifiedDepthsTopY) {
                                dropCell.y -= 1;
                                continue;
                            }
                            const uint32_t dropId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, dropCell);
                            if (isDepthLavaId(dropId)) {
                                dropCell.y -= 1;
                                continue;
                            }
                            if (!canReplaceWithDepthLava(dropId)) {
                                break;
                            }
                            voxelWorld.setBlock(dropCell,
                                depthLavaTileIdFor(dropCell.x, dropCell.z),
                                useColor,
                                false
                            );
                            wroteAny = true;
                            dropCell.y -= 1;
                        }
                    };

                    const int depthLavaContinuationColumnsPerUpdate = std::max(
                        1,
                        getRegistryInt(baseSystem, "DepthLavaContinuationColumnsPerUpdate", 128)
                    );
                    const int depthLavaSourceColumnsPerUpdate = std::max(
                        1,
                        getRegistryInt(baseSystem, "DepthLavaSourceColumnsPerUpdate", 128)
                    );
                    const int columnCount = size * size;
                    jobState.depthLavaContinuationCursor = std::max(
                        0,
                        std::min(jobState.depthLavaContinuationCursor, columnCount)
                    );
                    jobState.depthLavaSourceCursor = std::max(
                        0,
                        std::min(jobState.depthLavaSourceCursor, columnCount)
                    );

                    int continuationProcessed = 0;
                    while (jobState.depthLavaContinuationCursor < columnCount
                        && continuationProcessed < depthLavaContinuationColumnsPerUpdate) {
                        const int idx = jobState.depthLavaContinuationCursor;
                        const int localX = idx % size;
                        const int localZ = idx / size;
                        const int worldX = sectionMinX + localX;
                        const int worldZ = sectionMinZ + localZ;
                        jobState.depthLavaContinuationCursor += 1;
                        continuationProcessed += 1;

                        const glm::ivec3 aboveCell(worldX, sectionMaxY + 1, worldZ);
                        const uint32_t aboveId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, aboveCell);
                        if (!isDepthLavaId(aboveId)) continue;
                        uint32_t carryColor = VoxelMeshInitSystemLogic::GetVoxelColorAt(voxelWorld, aboveCell);
                        if ((carryColor & 0x00ffffffu) == 0u) carryColor = packedDepthLavaColor;
                        dropDepthLavaFromSource(glm::ivec3(worldX, sectionMaxY, worldZ), carryColor);
                    }

                    int sourceProcessed = 0;
                    while (jobState.depthLavaSourceCursor < columnCount
                        && sourceProcessed < depthLavaSourceColumnsPerUpdate) {
                        const int idx = jobState.depthLavaSourceCursor;
                        const int localX = idx % size;
                        const int localZ = idx / size;
                        const int worldX = sectionMinX + localX;
                        const int worldZ = sectionMinZ + localZ;
                        jobState.depthLavaSourceCursor += 1;
                        sourceProcessed += 1;

                        for (int worldY = sectionMaxY; worldY >= sectionMinY; --worldY) {
                            if (worldY <= unifiedDepthsMinY || worldY >= unifiedDepthsTopY) continue;
                            const glm::ivec3 sourceCell(worldX, worldY, worldZ);
                            const uint32_t sourceId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, sourceCell);
                            if (!isDepthLavaId(sourceId)) continue;
                            uint32_t sourceColor = VoxelMeshInitSystemLogic::GetVoxelColorAt(voxelWorld, sourceCell);
                            if ((sourceColor & 0x00ffffffu) == 0u) sourceColor = packedDepthLavaColor;
                            dropDepthLavaFromSource(sourceCell, sourceColor);
                            break;
                        }
                    }

                    const bool continuationDone = (jobState.depthLavaContinuationCursor >= columnCount);
                    const bool sourceDone = (jobState.depthLavaSourceCursor >= columnCount);
                    if (!continuationDone || !sourceDone) {
                        phaseComplete = false;
                    } else {
                        jobState.depthLavaContinuationCursor = 0;
                        jobState.depthLavaSourceCursor = 0;
                    }
                }
            }
                if (runObsidianPhase
                && obsidianProto
                && obsidianProto->prototypeID > 0
                && waterProto
                && waterProto->prototypeID > 0) {
                const int sectionMinX = sectionCoord.x * size;
                const int sectionMaxX = sectionMinX + size - 1;
                const int sectionMinY = sectionCoord.y * size;
                const int sectionMaxY = sectionMinY + size - 1;
                const int sectionMinZ = sectionCoord.z * size;
                const int sectionMaxZ = sectionMinZ + size - 1;

                auto isWaterLikeId = [&](uint32_t id) {
                    if (id == 0u) return false;
                    if (id == static_cast<uint32_t>(waterProto->prototypeID)) return true;
                    if (waterSlopeProtoPosX && id == static_cast<uint32_t>(waterSlopeProtoPosX->prototypeID)) return true;
                    if (waterSlopeProtoNegX && id == static_cast<uint32_t>(waterSlopeProtoNegX->prototypeID)) return true;
                    if (waterSlopeProtoPosZ && id == static_cast<uint32_t>(waterSlopeProtoPosZ->prototypeID)) return true;
                    if (waterSlopeProtoNegZ && id == static_cast<uint32_t>(waterSlopeProtoNegZ->prototypeID)) return true;
                    if (waterSlopeCornerProtoPosXPosZ && id == static_cast<uint32_t>(waterSlopeCornerProtoPosXPosZ->prototypeID)) return true;
                    if (waterSlopeCornerProtoPosXNegZ && id == static_cast<uint32_t>(waterSlopeCornerProtoPosXNegZ->prototypeID)) return true;
                    if (waterSlopeCornerProtoNegXPosZ && id == static_cast<uint32_t>(waterSlopeCornerProtoNegXPosZ->prototypeID)) return true;
                    if (waterSlopeCornerProtoNegXNegZ && id == static_cast<uint32_t>(waterSlopeCornerProtoNegXNegZ->prototypeID)) return true;
                    return false;
                };
                auto isLavaLikeId = [&](uint32_t id) {
                    if (id == 0u || id >= static_cast<uint32_t>(prototypes.size())) return false;
                    const std::string& name = prototypes[static_cast<size_t>(id)].name;
                    return name == "LavaBlockTex"
                        || name == "Lava"
                        || name.rfind("DepthLavaTile", 0) == 0;
                };
                static const std::array<glm::ivec3, 6> kNeighborDirs = {
                    glm::ivec3(1, 0, 0),
                    glm::ivec3(-1, 0, 0),
                    glm::ivec3(0, 1, 0),
                    glm::ivec3(0, -1, 0),
                    glm::ivec3(0, 0, 1),
                    glm::ivec3(0, 0, -1)
                };

                const int obsidianLayersPerUpdate = std::max(
                    1,
                    getRegistryInt(baseSystem, "ObsidianReactionLayersPerUpdate", 2)
                );
                jobState.obsidianLayerCursor = std::max(0, std::min(jobState.obsidianLayerCursor, size));
                const int startLayer = jobState.obsidianLayerCursor;
                const int endLayer = std::min(size, startLayer + obsidianLayersPerUpdate);
                std::vector<glm::ivec3> toObsidian;
                toObsidian.reserve(static_cast<size_t>(size * size));
                for (int localY = startLayer; localY < endLayer; ++localY) {
                    const int worldY = sectionMinY + localY;
                    for (int worldZ = sectionMinZ; worldZ <= sectionMaxZ; ++worldZ) {
                        for (int worldX = sectionMinX; worldX <= sectionMaxX; ++worldX) {
                            const glm::ivec3 cell(worldX, worldY, worldZ);
                            const uint32_t id = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, cell);
                            const bool waterLike = isWaterLikeId(id);
                            const bool lavaLike = isLavaLikeId(id);
                            if (!waterLike && !lavaLike) continue;
                            bool adjacentOpposite = false;
                            for (const glm::ivec3& d : kNeighborDirs) {
                                const uint32_t neighborId = VoxelMeshInitSystemLogic::GetVoxelIdAt(voxelWorld, cell + d);
                                if ((waterLike && isLavaLikeId(neighborId))
                                    || (lavaLike && isWaterLikeId(neighborId))) {
                                    adjacentOpposite = true;
                                    break;
                                }
                            }
                            if (adjacentOpposite) {
                                toObsidian.push_back(cell);
                            }
                        }
                    }
                }

                for (const glm::ivec3& cell : toObsidian) {
                    voxelWorld.setBlock(cell,
                        static_cast<uint32_t>(obsidianProto->prototypeID),
                        packColor(glm::vec3(1.0f)),
                        false
                    );
                    wroteAny = true;
                }

                jobState.obsidianLayerCursor = endLayer;
                if (endLayer < size) {
                    phaseComplete = false;
                } else {
                    jobState.obsidianLayerCursor = 0;
                }
            }

                if (!phaseComplete) {
                    break;
                }
                jobState.phase += 1;
                while (jobState.phase < 4 && !phaseEnabled(jobState.phase)) {
                    jobState.phase += 1;
                }
            }

            const bool finished = (jobState.phase >= 4);
            if (finished) {
                g_depthFeatureJobs.erase(sectionKey);
            }
            return finished;
        }
    }

}
