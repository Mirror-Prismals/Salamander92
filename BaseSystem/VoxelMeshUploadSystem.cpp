#pragma once

#include "Host/PlatformInput.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace RenderInitSystemLogic {
    RenderBehavior BehaviorForPrototype(const Entity& proto);
    void DestroyVoxelFaceRenderBuffers(VoxelFaceRenderBuffers& buffers, IRenderBackend& renderBackend);
    void DestroyChunkRenderBuffers(ChunkRenderBuffers& buffers, IRenderBackend& renderBackend);
    int getRegistryInt(const BaseSystem& baseSystem, const std::string& key, int fallback);
    bool getRegistryBool(const BaseSystem& baseSystem, const std::string& key, bool fallback);
    float getRegistryFloat(const BaseSystem& baseSystem, const std::string& key, float fallback);
    bool shouldRenderVoxelSection(const BaseSystem& baseSystem,
                                  const VoxelSection& section,
                                  const glm::vec3& cameraPos);
}
namespace VoxelMeshInitSystemLogic {
    glm::vec3 UnpackColor(uint32_t packed);
    int FloorDivInt(int value, int divisor);
}
namespace VoxelMeshUploadSystemLogic {
    namespace {
        bool startsWith(const std::string& value, const char* prefix) {
            if (!prefix) return false;
            const size_t len = std::char_traits<char>::length(prefix);
            return value.size() >= len && value.compare(0, len, prefix) == 0;
        }

        bool endsWith(const std::string& value, const char* suffix) {
            if (!suffix) return false;
            const size_t len = std::char_traits<char>::length(suffix);
            return value.size() >= len && value.compare(value.size() - len, len, suffix) == 0;
        }

        bool isWaterSlopeAlpha(float alpha) {
            return alpha <= -23.5f && alpha > -33.5f;
        }

        bool isMaskedFoliageTaggedFace(float alpha) {
            // Leaf blocks and card foliage are authored with negative sentinel alpha tags.
            // They need both a depth-writing opaque slice and a blended translucent slice.
            if (alpha < 0.0f && alpha > -3.5f) return true;
            if (alpha <= -9.5f && alpha > -13.5f) return true;
            return false;
        }

        bool shouldRenderInAlphaPass(float alpha) {
            if (isWaterSlopeAlpha(alpha)) return true;
            return (alpha > 0.0f && alpha < 0.999f);
        }

        bool isLeafPrototypeName(const std::string& name) {
            return name == "Leaf" || startsWith(name, "LeafJungle");
        }

        bool isFlowerPrototypeName(const std::string& name) {
            return startsWith(name, "Flower");
        }

        bool isTallGrassPrototypeName(const std::string& name) {
            return startsWith(name, "GrassTuft") && !startsWith(name, "GrassTuftShort");
        }

        bool isShortGrassPrototypeName(const std::string& name) {
            return startsWith(name, "GrassTuftShort");
        }

        bool isCavePotPrototypeName(const std::string& name) {
            return name == "StonePebbleCavePotTexX" || name == "StonePebbleCavePotTexZ";
        }

        bool isGrassCoverXName(const std::string& name) {
            return name == "GrassCoverTexX"
                || (startsWith(name, "GrassCover") && endsWith(name, "TexX"));
        }

        bool isGrassCoverZName(const std::string& name) {
            return name == "GrassCoverTexZ"
                || (startsWith(name, "GrassCover") && endsWith(name, "TexZ"));
        }

        bool isStickXName(const std::string& name) {
            return name == "StickTexX"
                || name == "StickWinterTexX";
        }

        bool isStickZName(const std::string& name) {
            return name == "StickTexZ"
                || name == "StickWinterTexZ";
        }

        bool isStickPrototypeName(const std::string& name) {
            return isStickXName(name) || isStickZName(name);
        }

        bool isStonePebbleXName(const std::string& name) {
            if (isCavePotPrototypeName(name)) return false;
            return name == "StonePebbleTexX"
                || (startsWith(name, "StonePebble") && endsWith(name, "TexX"));
        }

        bool isStonePebbleZName(const std::string& name) {
            if (isCavePotPrototypeName(name)) return false;
            return name == "StonePebbleTexZ"
                || (startsWith(name, "StonePebble") && endsWith(name, "TexZ"));
        }

        bool isPetalPileName(const std::string& name) {
            if (startsWith(name, "StonePebblePetalsBook")) return false;
            return startsWith(name, "StonePebblePetals")
                || startsWith(name, "StonePebblePatch")
                || startsWith(name, "StonePebbleLeaf")
                || startsWith(name, "StonePebbleLilypad")
                || startsWith(name, "StonePebbleSandDollar");
        }

        bool isSurfaceStonePebbleName(const std::string& name) {
            return name == "StonePebbleTexX" || name == "StonePebbleTexZ"
                || name == "StonePebbleRubyTexX" || name == "StonePebbleRubyTexZ"
                || name == "StonePebbleAmethystTexX" || name == "StonePebbleAmethystTexZ"
                || name == "StonePebbleFlouriteTexX" || name == "StonePebbleFlouriteTexZ"
                || name == "StonePebbleSilverTexX" || name == "StonePebbleSilverTexZ";
        }

        bool isLeafFanPlantPrototypeName(const std::string& name) {
            return name == "GrassTuftLeafFanOak"
                || name == "GrassTuftLeafFanPine";
        }

        struct NarrowHalfExtents {
            float x;
            float y;
            float z;
        };

        constexpr int kSurfaceStonePileMin = 1;
        constexpr int kSurfaceStonePileMax = 8;

        uint32_t hashCell3D(int x, int y, int z) {
            uint32_t h = static_cast<uint32_t>(x) * 73856093u;
            h ^= static_cast<uint32_t>(y) * 19349663u;
            h ^= static_cast<uint32_t>(z) * 83492791u;
            h ^= (h >> 13u);
            h *= 1274126177u;
            h ^= (h >> 16u);
            return h;
        }

        struct StonePebblePilePieces {
            int count = 0;
            std::array<glm::vec2, kSurfaceStonePileMax> offsets{};
            std::array<NarrowHalfExtents, kSurfaceStonePileMax> halfExtents{};
        };

        StonePebblePilePieces stonePebblePilePiecesForCell(const glm::ivec3& cell, int requestedCount) {
            StonePebblePilePieces out;
            out.count = std::clamp(requestedCount, kSurfaceStonePileMin, kSurfaceStonePileMax);
            int placed = 0;
            constexpr float kPlacementPad = 1.0f / 96.0f;
            for (int i = 0; i < out.count; ++i) {
                const uint32_t sizeHash = hashCell3D(
                    cell.x + i * 83,
                    cell.y - i * 47,
                    cell.z + i * 59
                );
                NarrowHalfExtents ext;
                ext.x = (2.0f + static_cast<float>(sizeHash & 3u)) / 48.0f;
                ext.z = (2.0f + static_cast<float>((sizeHash >> 2u) & 3u)) / 48.0f;
                ext.y = (1.0f + static_cast<float>((sizeHash >> 4u) % 3u)) / 48.0f;
                if (((sizeHash >> 6u) & 1u) != 0u) {
                    std::swap(ext.x, ext.z);
                }
                if (out.count == 1) {
                    ext.x *= 1.35f;
                    ext.z *= 1.35f;
                    ext.y *= 1.20f;
                }

                bool placedThis = false;
                for (int attempt = 0; attempt < 12; ++attempt) {
                    const uint32_t h = hashCell3D(
                        cell.x + i * 37 + attempt * 11,
                        cell.y + i * 19 - attempt * 7,
                        cell.z - i * 53 + attempt * 13
                    );
                    float ox = (static_cast<float>((h >> 8u) & 0xffu) / 255.0f - 0.5f) * 0.72f;
                    float oz = (static_cast<float>((h >> 16u) & 0xffu) / 255.0f - 0.5f) * 0.72f;
                    ox = std::clamp(ox, -0.5f + ext.x + kPlacementPad, 0.5f - ext.x - kPlacementPad);
                    oz = std::clamp(oz, -0.5f + ext.z + kPlacementPad, 0.5f - ext.z - kPlacementPad);

                    bool overlaps = false;
                    for (int j = 0; j < placed; ++j) {
                        const glm::vec2 prevOffset = out.offsets[static_cast<size_t>(j)];
                        const NarrowHalfExtents prevExt = out.halfExtents[static_cast<size_t>(j)];
                        if (std::abs(ox - prevOffset.x) < (ext.x + prevExt.x + kPlacementPad)
                            && std::abs(oz - prevOffset.y) < (ext.z + prevExt.z + kPlacementPad)) {
                            overlaps = true;
                            break;
                        }
                    }
                    if (overlaps) continue;

                    out.offsets[static_cast<size_t>(placed)] = glm::vec2(ox, oz);
                    out.halfExtents[static_cast<size_t>(placed)] = ext;
                    ++placed;
                    placedThis = true;
                    break;
                }

                if (!placedThis) {
                    const uint32_t h = hashCell3D(
                        cell.x - i * 71,
                        cell.y + i * 43,
                        cell.z + i * 29
                    );
                    const float angle = (static_cast<float>(h & 1023u) / 1023.0f) * 6.2831853f + static_cast<float>(i) * 0.71f;
                    const float radius = 0.09f + 0.03f * static_cast<float>(i % 4);
                    float ox = std::cos(angle) * radius;
                    float oz = std::sin(angle) * radius;
                    ox = std::clamp(ox, -0.5f + ext.x + kPlacementPad, 0.5f - ext.x - kPlacementPad);
                    oz = std::clamp(oz, -0.5f + ext.z + kPlacementPad, 0.5f - ext.z - kPlacementPad);
                    out.offsets[static_cast<size_t>(placed)] = glm::vec2(ox, oz);
                    out.halfExtents[static_cast<size_t>(placed)] = ext;
                    ++placed;
                }
            }

            out.count = std::max(placed, 1);
            return out;
        }

        struct GrassCoverDots {
            int count = 0;
            std::array<glm::vec2, 48> offsets{};
        };

        GrassCoverDots grassCoverDotsForCell(const glm::ivec3& cell) {
            GrassCoverDots out;
            constexpr int kMinDots = 36;
            constexpr int kMaxDots = 48;
            constexpr int kGridCellsPerAxis = 24;
            constexpr float kGridUnit = 1.0f / 24.0f;
            const uint32_t seed = hashCell3D(cell.x + 913, cell.y + 37, cell.z - 211);
            out.count = kMinDots + static_cast<int>(seed % static_cast<uint32_t>(kMaxDots - kMinDots + 1));
            for (int i = 0; i < out.count; ++i) {
                const uint32_t h = hashCell3D(
                    cell.x + i * 31,
                    cell.y - i * 17,
                    cell.z + i * 13
                );
                const int oxSlot = static_cast<int>(h % static_cast<uint32_t>(kGridCellsPerAxis));
                const int ozSlot = static_cast<int>((h >> 8u) % static_cast<uint32_t>(kGridCellsPerAxis));
                const float ox = (static_cast<float>(oxSlot) + 0.5f) * kGridUnit - 0.5f;
                const float oz = (static_cast<float>(ozSlot) + 0.5f) * kGridUnit - 0.5f;
                out.offsets[static_cast<size_t>(i)] = glm::vec2(ox, oz);
            }
            return out;
        }

        int decodeGrassCoverSnapshotTile(uint32_t packedColor) {
            const int encoded = static_cast<int>((packedColor >> 24) & 0xffu);
            if (encoded <= 0) return -1;
            const int marker = encoded & 0x0f;
            const int waveClass = (encoded >> 4) & 0x0f;
            if (marker <= 5 && waveClass <= 4) return -1;
            return encoded - 1;
        }

        int decodeSurfaceStonePileCount(uint32_t packedColor) {
            const int encoded = static_cast<int>((packedColor >> 24) & 0xffu);
            if (encoded <= 0) return kSurfaceStonePileMin;
            return std::clamp(encoded, kSurfaceStonePileMin, kSurfaceStonePileMax);
        }

        const std::vector<VertexAttribLayout>& FaceVertexLayout() {
            static const std::vector<VertexAttribLayout> kLayout = {
                {0u, 3, VertexAttribType::Float, false, static_cast<unsigned int>(8 * sizeof(float)), 0u, 0u},
                {1u, 3, VertexAttribType::Float, false, static_cast<unsigned int>(8 * sizeof(float)), static_cast<size_t>(3 * sizeof(float)), 0u},
                {2u, 2, VertexAttribType::Float, false, static_cast<unsigned int>(8 * sizeof(float)), static_cast<size_t>(6 * sizeof(float)), 0u},
            };
            return kLayout;
        }

        const std::vector<VertexAttribLayout>& FaceInstanceLayout() {
            static const std::vector<VertexAttribLayout> kLayout = {
                {3u, 3, VertexAttribType::Float, false, static_cast<unsigned int>(sizeof(FaceInstanceRenderData)), offsetof(FaceInstanceRenderData, position), 1u},
                {4u, 3, VertexAttribType::Float, false, static_cast<unsigned int>(sizeof(FaceInstanceRenderData)), offsetof(FaceInstanceRenderData, color), 1u},
                {5u, 1, VertexAttribType::Int,   false, static_cast<unsigned int>(sizeof(FaceInstanceRenderData)), offsetof(FaceInstanceRenderData, tileIndex), 1u},
                {6u, 1, VertexAttribType::Float, false, static_cast<unsigned int>(sizeof(FaceInstanceRenderData)), offsetof(FaceInstanceRenderData, alpha), 1u},
                {7u, 4, VertexAttribType::Float, false, static_cast<unsigned int>(sizeof(FaceInstanceRenderData)), offsetof(FaceInstanceRenderData, ao), 1u},
                {8u, 2, VertexAttribType::Float, false, static_cast<unsigned int>(sizeof(FaceInstanceRenderData)), offsetof(FaceInstanceRenderData, scale), 1u},
                {9u, 2, VertexAttribType::Float, false, static_cast<unsigned int>(sizeof(FaceInstanceRenderData)), offsetof(FaceInstanceRenderData, uvScale), 1u},
            };
            return kLayout;
        }

        const std::vector<VertexAttribLayout>& BranchInstanceLayout() {
            static const std::vector<VertexAttribLayout> kLayout = {
                {3u, 3, VertexAttribType::Float, false, static_cast<unsigned int>(sizeof(BranchInstanceData)), offsetof(BranchInstanceData, position), 1u},
                {4u, 3, VertexAttribType::Float, false, static_cast<unsigned int>(sizeof(BranchInstanceData)), offsetof(BranchInstanceData, rotation), 1u},
                {5u, 3, VertexAttribType::Float, false, static_cast<unsigned int>(sizeof(BranchInstanceData)), offsetof(BranchInstanceData, color), 1u},
            };
            return kLayout;
        }

        const std::vector<VertexAttribLayout>& BlockInstanceLayout() {
            static const std::vector<VertexAttribLayout> kLayout = {
                {3u, 3, VertexAttribType::Float, false, static_cast<unsigned int>(sizeof(InstanceData)), offsetof(InstanceData, position), 1u},
                {4u, 3, VertexAttribType::Float, false, static_cast<unsigned int>(sizeof(InstanceData)), offsetof(InstanceData, color), 1u},
                {5u, 3, VertexAttribType::Float, false, static_cast<unsigned int>(sizeof(InstanceData)), offsetof(InstanceData, color), 1u},
            };
            return kLayout;
        }

        bool BuildVoxelRenderBuffers(BaseSystem& baseSystem,
                                     std::vector<Entity>& prototypes,
                                     const VoxelSectionKey& sectionKey,
                                     bool faceCullingInitialized,
                                     IRenderBackend& renderBackend) {
            if (!baseSystem.voxelWorld || !baseSystem.voxelRender || !baseSystem.renderer) return false;
            VoxelWorldContext& voxelWorld = *baseSystem.voxelWorld;
            VoxelRenderContext& voxelRender = *baseSystem.voxelRender;
            RendererContext& renderer = *baseSystem.renderer;
            auto secIt = voxelWorld.sections.find(sectionKey);
            if (secIt == voxelWorld.sections.end()) return false;
            const VoxelSection& section = secIt->second;
            if (section.nonAirCount <= 0) return true;
            ChunkRenderBuffers& buffers = voxelRender.renderBuffers[sectionKey];

            ::RenderInitSystemLogic::DestroyChunkRenderBuffers(buffers, renderBackend);
            buffers.counts.fill(0);
            buffers.usesTexturedFaceBuffers = true;

            static const std::array<glm::ivec3, 6> kFaceNormals = {
                glm::ivec3(1, 0, 0),
                glm::ivec3(-1, 0, 0),
                glm::ivec3(0, 1, 0),
                glm::ivec3(0, -1, 0),
                glm::ivec3(0, 0, 1),
                glm::ivec3(0, 0, -1)
            };

            auto isRenderableBlock = [&](uint32_t id) -> bool {
                if (id == 0 || id >= prototypes.size()) return false;
                const Entity& p = prototypes[id];
                return p.isRenderable && p.isBlock;
            };

            auto isOpaqueBlock = [&](uint32_t id) -> bool {
                if (!isRenderableBlock(id)) return false;
                return prototypes[id].isOpaque;
            };

            auto isLeafBlock = [&](uint32_t blockId) -> bool {
                if (!isRenderableBlock(blockId)) return false;
                return isLeafPrototypeName(prototypes[blockId].name);
            };

            std::array<std::vector<FaceInstanceRenderData>, 6> opaqueFaces;
            std::array<std::vector<FaceInstanceRenderData>, 6> alphaFaces;
            const glm::ivec3 base = section.coord * section.size;
            static const std::array<int, 4> kPlantFaces = {0, 1, 4, 5};

            auto pushFace = [&](int faceType, const FaceInstanceRenderData& face) {
                if (isMaskedFoliageTaggedFace(face.alpha)) {
                    opaqueFaces[static_cast<size_t>(faceType)].push_back(face);
                    alphaFaces[static_cast<size_t>(faceType)].push_back(face);
                    return;
                }
                if (shouldRenderInAlphaPass(face.alpha)) {
                    alphaFaces[static_cast<size_t>(faceType)].push_back(face);
                } else {
                    opaqueFaces[static_cast<size_t>(faceType)].push_back(face);
                }
            };

            for (int z = 0; z < section.size; ++z) {
                for (int y = 0; y < section.size; ++y) {
                    for (int x = 0; x < section.size; ++x) {
                        const int idx = x + y * section.size + z * section.size * section.size;
                        if (idx < 0 || idx >= static_cast<int>(section.ids.size())) continue;

                        const uint32_t id = section.ids[idx];
                        if (!isRenderableBlock(id)) continue;
                        const Entity& proto = prototypes[id];
                        const std::string& name = proto.name;
                        const bool isLeaf = isLeafPrototypeName(name);
                        const bool isTallGrass = isTallGrassPrototypeName(name);
                        const bool isShortGrass = isShortGrassPrototypeName(name);
                        const bool isFlower = isFlowerPrototypeName(name);
                        const bool isCavePot = isCavePotPrototypeName(name);
                        const bool isPlantCard = isTallGrass || isShortGrass || isFlower || isCavePot;
                        const bool isLeafFanPlant = isLeafFanPlantPrototypeName(name);
                        const bool isStick = isStickPrototypeName(name);
                        const bool isGrassCover = isGrassCoverXName(name) || isGrassCoverZName(name);
                        const bool isStonePebble = isStonePebbleXName(name) || isStonePebbleZName(name);
                        const bool isSurfaceStonePebble = isSurfaceStonePebbleName(name);
                        const bool isPetalPile = isPetalPileName(name);
                        const bool isNarrowStonePebble = isStonePebble && !isPetalPile;
                        const bool isGroundCoverDecal = isGrassCover || isPetalPile;
                        const glm::ivec3 worldCell = base + glm::ivec3(x, y, z);
                        const uint32_t packedColorRaw = section.colors[idx];
                        const glm::vec3 packedColor = VoxelMeshInitSystemLogic::UnpackColor(section.colors[idx]);

                        if (isPlantCard) {
                            const int plantTile = ::RenderInitSystemLogic::FaceTileIndexFor(baseSystem.world.get(), proto, 2);
                            float plantAlpha = -2.0f;
                            if (isFlower) plantAlpha = -3.0f;
                            else if (isShortGrass) plantAlpha = -2.3f;
                            else if (isCavePot) plantAlpha = -10.0f;

                            const glm::vec3 tint = (plantTile >= 0) ? glm::vec3(1.0f) : packedColor;
                            glm::vec2 plantScale(1.0f);
                            bool keepPlantBottomAnchored = true;
                            if (isLeafFanPlant) {
                                plantScale = glm::vec2(3.0f, 3.0f);
                                keepPlantBottomAnchored = false;
                            }
                            if (isFlower && plantTile < 0) {
                                plantScale = glm::vec2(0.86f, 0.92f);
                            }

                            for (int faceType : kPlantFaces) {
                                FaceInstanceRenderData face{};
                                face.position = glm::vec3(worldCell);
                                if (keepPlantBottomAnchored) {
                                    face.position.y += (plantScale.y - 1.0f) * 0.5f;
                                }
                                face.tileIndex = plantTile;
                                face.color = tint;
                                face.alpha = plantAlpha;
                                face.ao = glm::vec4(1.0f);
                                face.scale = plantScale;
                                face.uvScale = glm::vec2(1.0f);
                                pushFace(faceType, face);
                            }
                            continue;
                        }

                        for (int faceType = 0; faceType < 6; ++faceType) {
                            const glm::ivec3 neighborCell = worldCell + kFaceNormals[static_cast<size_t>(faceType)];
                            const uint32_t neighborId = voxelWorld.getBlockWorld(neighborCell);
                            if (isOpaqueBlock(neighborId)) continue;
                            if (isLeaf && isLeafBlock(neighborId)) continue;

                            FaceInstanceRenderData face{};
                            glm::vec3 normal = glm::vec3(kFaceNormals[static_cast<size_t>(faceType)]);
                            face.tileIndex = ::RenderInitSystemLogic::FaceTileIndexFor(baseSystem.world.get(), proto, faceType);
                            if (isGrassCover) {
                                const int snapshotTile = decodeGrassCoverSnapshotTile(packedColorRaw);
                                if (snapshotTile >= 0) {
                                    face.tileIndex = snapshotTile;
                                } else {
                                    const glm::ivec3 supportCell = worldCell + glm::ivec3(0, -1, 0);
                                    const uint32_t supportId = voxelWorld.getBlockWorld(supportCell);
                                    if (supportId > 0 && supportId < static_cast<uint32_t>(prototypes.size())) {
                                        const Entity& supportProto = prototypes[static_cast<size_t>(supportId)];
                                        const int supportTopTile = ::RenderInitSystemLogic::FaceTileIndexFor(
                                            baseSystem.world.get(),
                                            supportProto,
                                            2
                                        );
                                        if (supportTopTile >= 0) {
                                            face.tileIndex = supportTopTile;
                                        }
                                    }
                                }
                            }
                            face.color = (face.tileIndex >= 0)
                                ? glm::vec3(1.0f)
                                : packedColor;
                            face.alpha = isLeaf ? -1.0f : 1.0f;
                            if (isGrassCover) {
                                face.alpha = -14.0f;
                            } else if (proto.name == "Water") {
                                face.alpha = 0.08f;
                            } else if (proto.name == "TransparentWave") {
                                face.alpha = 0.06f;
                            }
                            face.ao = glm::vec4(1.0f);
                            auto faceHalfExtentFor = [&](const glm::vec3& extents) -> float {
                                if (faceType == 0 || faceType == 1) return extents.x;
                                if (faceType == 2 || faceType == 3) return extents.y;
                                return extents.z;
                            };
                            auto faceScaleFor = [&](const glm::vec3& extents) -> glm::vec2 {
                                if (faceType == 0 || faceType == 1) {
                                    return glm::vec2(extents.z * 2.0f, extents.y * 2.0f);
                                }
                                if (faceType == 2 || faceType == 3) {
                                    return glm::vec2(extents.x * 2.0f, extents.z * 2.0f);
                                }
                                return glm::vec2(extents.x * 2.0f, extents.y * 2.0f);
                            };
                            auto setScaledFaceGeometry = [&](FaceInstanceRenderData& target,
                                                             const glm::vec3& center,
                                                             const glm::vec3& extents) {
                                target.position = center + normal * faceHalfExtentFor(extents);
                                target.scale = faceScaleFor(extents);
                                target.uvScale = target.scale;
                            };

                            if (isSurfaceStonePebble) {
                                const int pileCount = decodeSurfaceStonePileCount(packedColorRaw);
                                const StonePebblePilePieces pile = stonePebblePilePiecesForCell(worldCell, pileCount);
                                for (int piece = 0; piece < pile.count; ++piece) {
                                    const glm::vec2 pieceOffset = pile.offsets[static_cast<size_t>(piece)];
                                    const NarrowHalfExtents pieceExt = pile.halfExtents[static_cast<size_t>(piece)];
                                    const glm::vec3 pieceHalfExtents(pieceExt.x, pieceExt.y, pieceExt.z);
                                    glm::vec3 pieceCenter = glm::vec3(worldCell);
                                    pieceCenter.x += pieceOffset.x;
                                    pieceCenter.z += pieceOffset.y;
                                    pieceCenter.y += (-0.5f + pieceExt.y + 0.01f);
                                    FaceInstanceRenderData pieceFace = face;
                                    setScaledFaceGeometry(pieceFace, pieceCenter, pieceHalfExtents);
                                    pushFace(faceType, pieceFace);
                                }
                                continue;
                            }

                            if (isStick || isNarrowStonePebble) {
                                glm::vec3 halfExtents(0.5f);
                                if (isStick) {
                                    constexpr float kHalf1 = 1.0f / 48.0f;
                                    constexpr float kHalf12 = 12.0f / 48.0f;
                                    halfExtents = isStickXName(name)
                                        ? glm::vec3(kHalf12, kHalf1, kHalf1)
                                        : glm::vec3(kHalf1, kHalf1, kHalf12);
                                } else {
                                    constexpr float kHalf2 = 2.0f / 48.0f;
                                    constexpr float kHalf6 = 6.0f / 48.0f;
                                    halfExtents = isStonePebbleXName(name)
                                        ? glm::vec3(kHalf6, kHalf2, kHalf2)
                                        : glm::vec3(kHalf2, kHalf2, kHalf6);
                                }
                                glm::vec3 narrowCenter = glm::vec3(worldCell);
                                narrowCenter.y += (-0.5f + halfExtents.y + 0.01f);
                                setScaledFaceGeometry(face, narrowCenter, halfExtents);
                                pushFace(faceType, face);
                                continue;
                            }

                            glm::vec3 halfExtents(0.5f);
                            if (isGrassCover) {
                                constexpr float kDotHalf = 1.0f / 48.0f;
                                halfExtents = glm::vec3(kDotHalf, kDotHalf, kDotHalf);
                            } else if (isPetalPile) {
                                halfExtents = glm::vec3(0.5f, 1.0f / 48.0f, 0.5f);
                            }

                            float halfExtent = faceHalfExtentFor(halfExtents);
                            glm::vec3 faceAnchor = glm::vec3(worldCell);
                            if (isGroundCoverDecal) {
                                // Ground-cover decals live in the empty cell above the support block,
                                // but their thin geometry should sit on the support surface itself.
                                faceAnchor.y -= 0.5f;
                            }
                            face.position = faceAnchor + normal * halfExtent;
                            face.scale = isGroundCoverDecal ? faceScaleFor(halfExtents) : glm::vec2(1.0f);
                            face.uvScale = face.scale;
                            if (isGrassCover) {
                                const GrassCoverDots dots = grassCoverDotsForCell(worldCell);
                                for (int dot = 0; dot < dots.count; ++dot) {
                                    FaceInstanceRenderData dotFace = face;
                                    const glm::vec2 offset = dots.offsets[static_cast<size_t>(dot)];
                                    dotFace.position.x += offset.x;
                                    dotFace.position.z += offset.y;
                                    pushFace(faceType, dotFace);
                                }
                            } else {
                                pushFace(faceType, face);
                            }
                        }
                    }
                }
            }

            for (int faceType = 0; faceType < 6; ++faceType) {
                const auto& opaque = opaqueFaces[static_cast<size_t>(faceType)];
                if (!opaque.empty()) {
                    renderBackend.ensureVertexArray(buffers.faceBuffers.opaqueVaos[static_cast<size_t>(faceType)]);
                    renderBackend.ensureArrayBuffer(buffers.faceBuffers.opaqueVBOs[static_cast<size_t>(faceType)]);
                    renderBackend.uploadArrayBufferData(
                        buffers.faceBuffers.opaqueVBOs[static_cast<size_t>(faceType)],
                        opaque.data(),
                        opaque.size() * sizeof(FaceInstanceRenderData),
                        false
                    );
                    renderBackend.configureVertexArray(
                        buffers.faceBuffers.opaqueVaos[static_cast<size_t>(faceType)],
                        renderer.faceVBO,
                        FaceVertexLayout(),
                        buffers.faceBuffers.opaqueVBOs[static_cast<size_t>(faceType)],
                        FaceInstanceLayout()
                    );
                    buffers.faceBuffers.opaqueCounts[static_cast<size_t>(faceType)] = static_cast<int>(opaque.size());
                }

                const auto& alpha = alphaFaces[static_cast<size_t>(faceType)];
                if (!alpha.empty()) {
                    renderBackend.ensureVertexArray(buffers.faceBuffers.alphaVaos[static_cast<size_t>(faceType)]);
                    renderBackend.ensureArrayBuffer(buffers.faceBuffers.alphaVBOs[static_cast<size_t>(faceType)]);
                    renderBackend.uploadArrayBufferData(
                        buffers.faceBuffers.alphaVBOs[static_cast<size_t>(faceType)],
                        alpha.data(),
                        alpha.size() * sizeof(FaceInstanceRenderData),
                        false
                    );
                    renderBackend.configureVertexArray(
                        buffers.faceBuffers.alphaVaos[static_cast<size_t>(faceType)],
                        renderer.faceVBO,
                        FaceVertexLayout(),
                        buffers.faceBuffers.alphaVBOs[static_cast<size_t>(faceType)],
                        FaceInstanceLayout()
                    );
                    buffers.faceBuffers.alphaCounts[static_cast<size_t>(faceType)] = static_cast<int>(alpha.size());
                }
            }

            renderBackend.unbindVertexArray();
            buffers.builtWithFaceCulling = faceCullingInitialized;
            return true;
        }

        float sectionDist2ToCamera(const VoxelSection& section, const glm::vec3& cameraPos) {
            const int scale = 1;
            const float span = static_cast<float>(section.size * scale);
            const glm::vec3 center(
                (static_cast<float>(section.coord.x) + 0.5f) * span,
                (static_cast<float>(section.coord.y) + 0.5f) * span,
                (static_cast<float>(section.coord.z) + 0.5f) * span
            );
            const glm::vec3 delta = center - cameraPos;
            return glm::dot(delta, delta);
        }

        float keyDist2ToCamera(const VoxelWorldContext& voxelWorld,
                               const VoxelSectionKey& key,
                               const glm::vec3& cameraPos) {
            int size = voxelWorld.sectionSize;
            if (size < 1) size = 1;
            const int scale = 1;
            const float span = static_cast<float>(size * scale);
            const glm::vec3 center(
                (static_cast<float>(key.coord.x) + 0.5f) * span,
                (static_cast<float>(key.coord.y) + 0.5f) * span,
                (static_cast<float>(key.coord.z) + 0.5f) * span
            );
            const glm::vec3 delta = center - cameraPos;
            return glm::dot(delta, delta);
        }
    }

    void UpdateVoxelMeshUpload(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float, PlatformWindowHandle) {
        if (!baseSystem.renderer || !baseSystem.player || !baseSystem.renderBackend) return;
        RendererContext& renderer = *baseSystem.renderer;
        IRenderBackend& renderBackend = *baseSystem.renderBackend;
        glm::vec3 playerPos = baseSystem.player->cameraPosition;
        bool useVoxelRendering = baseSystem.voxelWorld
            && baseSystem.voxelWorld->enabled
            && baseSystem.voxelRender;
        const bool debugVoxelMeshingPerf = ::RenderInitSystemLogic::getRegistryBool(baseSystem, "DebugVoxelMeshingPerf", false);

        if (useVoxelRendering) {
            VoxelWorldContext& voxelWorld = *baseSystem.voxelWorld;
            VoxelRenderContext& voxelRender = *baseSystem.voxelRender;
            auto isRenderableSection = [&](const VoxelSectionKey& key) -> bool {
                const VoxelChunkLifecycleState* state = voxelWorld.findChunkState(key);
                if (!state) return true;
                return state->isRenderable();
            };
            std::vector<VoxelSectionKey> staleSections;
            for (const auto& [key, _] : voxelRender.renderBuffers) {
                auto it = voxelWorld.sections.find(key);
                if (it == voxelWorld.sections.end()
                    || it->second.nonAirCount <= 0
                    || !isRenderableSection(key)) {
                    staleSections.push_back(key);
                }
            }
            for (const auto& key : staleSections) {
                ::RenderInitSystemLogic::DestroyChunkRenderBuffers(voxelRender.renderBuffers[key], renderBackend);
                voxelRender.renderBuffers.erase(key);
            }

            for (const auto& key : voxelWorld.dirtySections) {
                auto it = voxelWorld.sections.find(key);
                if (it == voxelWorld.sections.end()) continue;
                if (!isRenderableSection(key)) continue;
                voxelRender.renderBuffersDirty.insert(key);
            }

            if (!voxelRender.renderBuffersDirty.empty()) {
                auto start = std::chrono::steady_clock::now();
                size_t buildCount = 0;
                int uploadMaxSections = std::max(
                    1,
                    ::RenderInitSystemLogic::getRegistryInt(baseSystem, "voxelUploadMaxSectionsPerFrame", 4)
                );
                float uploadMaxMs = std::max(
                    0.1f,
                    ::RenderInitSystemLogic::getRegistryFloat(baseSystem, "voxelUploadMaxMsPerFrame", 4.0f)
                );
                const bool bootstrapEnabled = ::RenderInitSystemLogic::getRegistryBool(
                    baseSystem,
                    "voxelUploadBootstrapEnabled",
                    true
                );
                const int bootstrapMinSections = std::max(
                    1,
                    ::RenderInitSystemLogic::getRegistryInt(baseSystem, "voxelUploadBootstrapMinSections", 32)
                );
                const float bootstrapMeshCoverageTarget = std::clamp(
                    ::RenderInitSystemLogic::getRegistryFloat(
                        baseSystem,
                        "voxelUploadBootstrapMeshCoverageTarget",
                        0.60f
                    ),
                    0.05f,
                    1.0f
                );
                const size_t sectionCount = voxelWorld.sections.size();
                const size_t meshCount = voxelRender.renderBuffers.size();
                const bool bootstrapActive = bootstrapEnabled
                    && sectionCount >= static_cast<size_t>(bootstrapMinSections)
                    && static_cast<double>(meshCount)
                        < static_cast<double>(sectionCount) * static_cast<double>(bootstrapMeshCoverageTarget);
                if (bootstrapActive) {
                    uploadMaxSections = std::max(
                        uploadMaxSections,
                        std::max(
                            1,
                            ::RenderInitSystemLogic::getRegistryInt(
                                baseSystem,
                                "voxelUploadBootstrapMaxSectionsPerFrame",
                                6
                            )
                        )
                    );
                    uploadMaxMs = std::max(
                        uploadMaxMs,
                        std::max(
                            0.1f,
                            ::RenderInitSystemLogic::getRegistryFloat(
                                baseSystem,
                                "voxelUploadBootstrapMaxMsPerFrame",
                                6.0f
                            )
                        )
                    );
                }

                struct Candidate {
                    VoxelSectionKey key;
                    float dist2 = 0.0f;
                };
                std::vector<Candidate> candidates;
                candidates.reserve(voxelRender.renderBuffersDirty.size());
                for (const auto& key : voxelRender.renderBuffersDirty) {
                    auto it = voxelWorld.sections.find(key);
                    if (it == voxelWorld.sections.end()) continue;
                    if (!isRenderableSection(key)) continue;
                    candidates.push_back({key, sectionDist2ToCamera(it->second, playerPos)});
                }
                std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
                    if (a.dist2 != b.dist2) return a.dist2 < b.dist2;
                    if (a.key.coord.x != b.key.coord.x) return a.key.coord.x < b.key.coord.x;
                    if (a.key.coord.y != b.key.coord.y) return a.key.coord.y < b.key.coord.y;
                    return a.key.coord.z < b.key.coord.z;
                });

                for (const Candidate& c : candidates) {
                    if (static_cast<int>(buildCount) >= uploadMaxSections) break;
                    const auto now = std::chrono::steady_clock::now();
                    const double elapsedMs = std::chrono::duration<double, std::milli>(now - start).count();
                    if (elapsedMs >= static_cast<double>(uploadMaxMs)) break;

                    auto it = voxelWorld.sections.find(c.key);
                    if (it == voxelWorld.sections.end()
                        || it->second.nonAirCount <= 0
                        || !isRenderableSection(c.key)) {
                        voxelRender.renderBuffersDirty.erase(c.key);
                        continue;
                    }
                    if (BuildVoxelRenderBuffers(baseSystem, prototypes, c.key, false, renderBackend)) {
                        voxelRender.renderBuffersDirty.erase(c.key);
                        voxelWorld.dirtySections.erase(c.key);
                        ++buildCount;
                    }
                }
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start
                ).count();
                if (debugVoxelMeshingPerf) {
                    std::cout << "RenderSystem: rebuilt " << buildCount
                              << " voxel section buffer(s) in "
                              << elapsedMs << " ms"
                              << " (pending " << voxelRender.renderBuffersDirty.size()
                              << ", cap " << uploadMaxSections
                              << ", budget " << uploadMaxMs << "ms"
                              << ", bootstrap " << (bootstrapActive ? "on" : "off")
                              << ")." << std::endl;
                }
            }
        }
    }
}
