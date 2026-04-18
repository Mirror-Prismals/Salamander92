#pragma once

#include "Host/PlatformInput.h"
#include "Structures/BinaryGreedyMesher.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
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
    namespace BinaryGreedyMesher = SalamanderBinaryGreedyMesher;
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

        struct BinaryGreedyMaterialKey {
            uint32_t prototypeID = 0;
            uint32_t packedColor = 0;
            bool opaqueOnly = false;

            bool operator==(const BinaryGreedyMaterialKey& other) const noexcept {
                return prototypeID == other.prototypeID
                    && packedColor == other.packedColor
                    && opaqueOnly == other.opaqueOnly;
            }
        };

        struct BinaryGreedyMaterialKeyHash {
            std::size_t operator()(const BinaryGreedyMaterialKey& key) const noexcept {
                std::size_t h = std::hash<uint32_t>()(key.prototypeID);
                h ^= std::hash<uint32_t>()(key.packedColor) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
                h ^= std::hash<int>()(key.opaqueOnly ? 1 : 0) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
                return h;
            }
        };

        struct BinaryGreedyTypeInfo {
            uint32_t prototypeID = 0;
            uint32_t packedColor = 0;
            bool opaqueOnly = false;
        };

        struct BinaryGreedyScratch {
            std::vector<uint8_t> voxels;
            std::vector<BinaryGreedyMesher::Quad> quads;
            BinaryGreedyMesher::MeshData meshData{};

            void ensureCapacity() {
                if (voxels.size() != static_cast<size_t>(BinaryGreedyMesher::kPaddedChunkVolume)) {
                    voxels.assign(static_cast<size_t>(BinaryGreedyMesher::kPaddedChunkVolume), 0);
                } else {
                    std::fill(voxels.begin(), voxels.end(), static_cast<uint8_t>(0));
                }
                quads.clear();
                meshData.quads = &quads;
                meshData.quadCount = 0;
                for (int i = 0; i < 6; ++i) {
                    meshData.faceQuadBegin[i] = 0;
                    meshData.faceQuadLength[i] = 0;
                }
            }
        };

        int binaryGreedyVoxelIndex(int x, int y, int z) {
            return z
                + x * BinaryGreedyMesher::kPaddedChunkSize
                + y * BinaryGreedyMesher::kPaddedChunkArea;
        }

        bool isBinaryGreedyRenderableName(const std::string& name) {
            if (name == "Water" || name == "TransparentWave") return false;
            if (isLeafPrototypeName(name)) return false;
            if (isTallGrassPrototypeName(name)) return false;
            if (isShortGrassPrototypeName(name)) return false;
            if (isFlowerPrototypeName(name)) return false;
            if (isLeafFanPlantPrototypeName(name)) return false;
            if (isCavePotPrototypeName(name)) return false;
            if (isStickPrototypeName(name)) return false;
            if (isGrassCoverXName(name) || isGrassCoverZName(name)) return false;
            if (isStonePebbleXName(name) || isStonePebbleZName(name)) return false;
            if (isPetalPileName(name)) return false;
            return true;
        }

        std::vector<VoxelMeshingPrototypeTraits> buildPrototypeRenderTraits(const BaseSystem& baseSystem,
                                                                            const std::vector<Entity>& prototypes) {
            std::vector<VoxelMeshingPrototypeTraits> traits(prototypes.size());
            for (size_t i = 1; i < prototypes.size(); ++i) {
                const Entity& proto = prototypes[i];
                VoxelMeshingPrototypeTraits& out = traits[i];
                out.renderableBlock = proto.isRenderable && proto.isBlock;
                if (!out.renderableBlock) continue;

                out.opaqueBlock = proto.isOpaque;
                const std::string& name = proto.name;
                out.leaf = isLeafPrototypeName(name);
                out.tallGrass = isTallGrassPrototypeName(name);
                out.shortGrass = isShortGrassPrototypeName(name);
                out.flower = isFlowerPrototypeName(name);
                out.cavePot = isCavePotPrototypeName(name);
                out.leafFanPlant = isLeafFanPlantPrototypeName(name);
                out.stickX = isStickXName(name);
                out.stickZ = isStickZName(name);
                out.stick = out.stickX || out.stickZ;
                out.grassCoverX = isGrassCoverXName(name);
                out.grassCoverZ = isGrassCoverZName(name);
                out.stonePebbleX = isStonePebbleXName(name);
                out.stonePebbleZ = isStonePebbleZName(name);
                out.surfaceStonePebble = isSurfaceStonePebbleName(name);
                out.petalPile = isPetalPileName(name);
                out.water = (name == "Water");
                out.transparentWave = (name == "TransparentWave");
                out.binaryGreedyRenderable = out.opaqueBlock && isBinaryGreedyRenderableName(name);
                for (int faceType = 0; faceType < 6; ++faceType) {
                    out.faceTiles[static_cast<size_t>(faceType)] =
                        ::RenderInitSystemLogic::FaceTileIndexFor(baseSystem.world.get(), proto, faceType);
                }
            }
            return traits;
        }

        uint8_t decodeQuadCoord(uint64_t quad, int shift) {
            return static_cast<uint8_t>((quad >> shift) & 63ull);
        }

        int paddedSnapshotIndex(const VoxelMeshingSnapshot& snapshot, int x, int y, int z) {
            const int paddedSize = snapshot.sectionSize + 2;
            return (x + 1)
                + (y + 1) * paddedSize
                + (z + 1) * paddedSize * paddedSize;
        }

        uint32_t snapshotBlockAt(const VoxelMeshingSnapshot& snapshot, int x, int y, int z) {
            if (x < -1 || x > snapshot.sectionSize
                || y < -1 || y > snapshot.sectionSize
                || z < -1 || z > snapshot.sectionSize) {
                return 0;
            }
            const int idx = paddedSnapshotIndex(snapshot, x, y, z);
            if (idx < 0 || idx >= static_cast<int>(snapshot.paddedIds.size())) return 0;
            return snapshot.paddedIds[static_cast<size_t>(idx)];
        }

        uint32_t snapshotColorAt(const VoxelMeshingSnapshot& snapshot, int x, int y, int z) {
            if (x < -1 || x > snapshot.sectionSize
                || y < -1 || y > snapshot.sectionSize
                || z < -1 || z > snapshot.sectionSize) {
                return 0;
            }
            const int idx = paddedSnapshotIndex(snapshot, x, y, z);
            if (idx < 0 || idx >= static_cast<int>(snapshot.paddedColors.size())) return 0;
            return snapshot.paddedColors[static_cast<size_t>(idx)];
        }

        uint8_t snapshotCombinedLightAt(const VoxelMeshingSnapshot& snapshot, int x, int y, int z) {
            if (x < -1 || x > snapshot.sectionSize
                || y < -1 || y > snapshot.sectionSize
                || z < -1 || z > snapshot.sectionSize) {
                return static_cast<uint8_t>(0);
            }
            const int idx = paddedSnapshotIndex(snapshot, x, y, z);
            if (idx < 0 || idx >= static_cast<int>(snapshot.paddedCombinedLight.size())) {
                return static_cast<uint8_t>(0);
            }
            return snapshot.paddedCombinedLight[static_cast<size_t>(idx)];
        }

        float lightFactorForLevel(const VoxelMeshingSnapshot& snapshot, uint8_t level) {
            if (!snapshot.voxelLightingEnabled) return 1.0f;
            const float normalized = glm::clamp(static_cast<float>(level) / 15.0f, 0.0f, 1.0f);
            const float curve = std::pow(normalized, glm::clamp(snapshot.voxelLightingGamma, 0.25f, 4.0f));
            const float factor = snapshot.voxelLightingMinBrightness
                + (1.0f - snapshot.voxelLightingMinBrightness) * curve;
            return 1.0f + (factor - 1.0f) * snapshot.voxelLightingStrength;
        }

        bool snapshotBlockOccludesAmbientLight(const VoxelMeshingSnapshot& snapshot,
                                               const std::vector<VoxelMeshingPrototypeTraits>& prototypeTraits,
                                               const glm::ivec3& localCell) {
            const uint32_t blockId = snapshotBlockAt(snapshot, localCell.x, localCell.y, localCell.z);
            if (blockId == 0 || blockId >= prototypeTraits.size()) return false;
            return prototypeTraits[blockId].opaqueBlock;
        }

        struct FaceCornerSides {
            glm::ivec3 sideA;
            glm::ivec3 sideB;
        };

        const std::array<glm::ivec3, 6>& FaceLightingNormals() {
            static const std::array<glm::ivec3, 6> kNormals = {
                glm::ivec3(1, 0, 0),
                glm::ivec3(-1, 0, 0),
                glm::ivec3(0, 1, 0),
                glm::ivec3(0, -1, 0),
                glm::ivec3(0, 0, 1),
                glm::ivec3(0, 0, -1)
            };
            return kNormals;
        }

        const std::array<std::array<FaceCornerSides, 4>, 6>& FaceCornerLightingSides() {
            static const std::array<std::array<FaceCornerSides, 4>, 6> kSides = {{
                {{
                    {glm::ivec3(0, -1, 0), glm::ivec3(0, 0, -1)},
                    {glm::ivec3(0, -1, 0), glm::ivec3(0, 0, 1)},
                    {glm::ivec3(0, 1, 0), glm::ivec3(0, 0, 1)},
                    {glm::ivec3(0, 1, 0), glm::ivec3(0, 0, -1)}
                }},
                {{
                    {glm::ivec3(0, -1, 0), glm::ivec3(0, 0, 1)},
                    {glm::ivec3(0, -1, 0), glm::ivec3(0, 0, -1)},
                    {glm::ivec3(0, 1, 0), glm::ivec3(0, 0, -1)},
                    {glm::ivec3(0, 1, 0), glm::ivec3(0, 0, 1)}
                }},
                {{
                    {glm::ivec3(1, 0, 0), glm::ivec3(0, 0, 1)},
                    {glm::ivec3(-1, 0, 0), glm::ivec3(0, 0, 1)},
                    {glm::ivec3(-1, 0, 0), glm::ivec3(0, 0, -1)},
                    {glm::ivec3(1, 0, 0), glm::ivec3(0, 0, -1)}
                }},
                {{
                    {glm::ivec3(1, 0, 0), glm::ivec3(0, 0, -1)},
                    {glm::ivec3(-1, 0, 0), glm::ivec3(0, 0, -1)},
                    {glm::ivec3(-1, 0, 0), glm::ivec3(0, 0, 1)},
                    {glm::ivec3(1, 0, 0), glm::ivec3(0, 0, 1)}
                }},
                {{
                    {glm::ivec3(-1, 0, 0), glm::ivec3(0, -1, 0)},
                    {glm::ivec3(1, 0, 0), glm::ivec3(0, -1, 0)},
                    {glm::ivec3(1, 0, 0), glm::ivec3(0, 1, 0)},
                    {glm::ivec3(-1, 0, 0), glm::ivec3(0, 1, 0)}
                }},
                {{
                    {glm::ivec3(1, 0, 0), glm::ivec3(0, -1, 0)},
                    {glm::ivec3(-1, 0, 0), glm::ivec3(0, -1, 0)},
                    {glm::ivec3(-1, 0, 0), glm::ivec3(0, 1, 0)},
                    {glm::ivec3(1, 0, 0), glm::ivec3(0, 1, 0)}
                }}
            }};
            return kSides;
        }

        glm::ivec3 faceCornerAnchorLocal(const glm::ivec3& faceMinLocal,
                                         int faceType,
                                         int spanA,
                                         int spanB,
                                         int cornerIndex) {
            const int aMax = std::max(0, spanA - 1);
            const int bMax = std::max(0, spanB - 1);
            switch (faceType) {
                case 0:
                    switch (cornerIndex) {
                        case 0: return faceMinLocal;
                        case 1: return faceMinLocal + glm::ivec3(0, 0, bMax);
                        case 2: return faceMinLocal + glm::ivec3(0, aMax, bMax);
                        default: return faceMinLocal + glm::ivec3(0, aMax, 0);
                    }
                case 1:
                    switch (cornerIndex) {
                        case 0: return faceMinLocal + glm::ivec3(0, 0, bMax);
                        case 1: return faceMinLocal;
                        case 2: return faceMinLocal + glm::ivec3(0, aMax, 0);
                        default: return faceMinLocal + glm::ivec3(0, aMax, bMax);
                    }
                case 2:
                    switch (cornerIndex) {
                        case 0: return faceMinLocal + glm::ivec3(aMax, 0, bMax);
                        case 1: return faceMinLocal + glm::ivec3(0, 0, bMax);
                        case 2: return faceMinLocal;
                        default: return faceMinLocal + glm::ivec3(aMax, 0, 0);
                    }
                case 3:
                    switch (cornerIndex) {
                        case 0: return faceMinLocal + glm::ivec3(aMax, 0, 0);
                        case 1: return faceMinLocal;
                        case 2: return faceMinLocal + glm::ivec3(0, 0, bMax);
                        default: return faceMinLocal + glm::ivec3(aMax, 0, bMax);
                    }
                case 4:
                    switch (cornerIndex) {
                        case 0: return faceMinLocal;
                        case 1: return faceMinLocal + glm::ivec3(aMax, 0, 0);
                        case 2: return faceMinLocal + glm::ivec3(aMax, bMax, 0);
                        default: return faceMinLocal + glm::ivec3(0, bMax, 0);
                    }
                case 5:
                    switch (cornerIndex) {
                        case 0: return faceMinLocal + glm::ivec3(aMax, 0, 0);
                        case 1: return faceMinLocal;
                        case 2: return faceMinLocal + glm::ivec3(0, bMax, 0);
                        default: return faceMinLocal + glm::ivec3(aMax, bMax, 0);
                    }
                default:
                    return faceMinLocal;
            }
        }

        glm::vec4 computeFaceCornerLighting(const VoxelMeshingSnapshot& snapshot,
                                            const std::vector<VoxelMeshingPrototypeTraits>& prototypeTraits,
                                            const glm::ivec3& faceMinLocal,
                                            int faceType,
                                            int spanA,
                                            int spanB,
                                            bool applyStructuralAo) {
            const auto& normals = FaceLightingNormals();
            const auto& cornerSides = FaceCornerLightingSides();
            static const std::array<float, 4> kAoFactors = {1.0f, 0.86f, 0.72f, 0.60f};
            glm::vec4 outAo(1.0f);

            for (int corner = 0; corner < 4; ++corner) {
                const glm::ivec3 anchor = faceCornerAnchorLocal(faceMinLocal, faceType, spanA, spanB, corner);
                const FaceCornerSides& sides = cornerSides[static_cast<size_t>(faceType)][static_cast<size_t>(corner)];

                float lightFactor = 1.0f;
                if (snapshot.voxelLightingEnabled) {
                    const glm::ivec3 outside = anchor + normals[static_cast<size_t>(faceType)];
                    float lightSum = 0.0f;
                    lightSum += lightFactorForLevel(
                        snapshot,
                        snapshotCombinedLightAt(snapshot, outside.x, outside.y, outside.z)
                    );
                    lightSum += lightFactorForLevel(
                        snapshot,
                        snapshotCombinedLightAt(snapshot,
                                                outside.x + sides.sideA.x,
                                                outside.y + sides.sideA.y,
                                                outside.z + sides.sideA.z)
                    );
                    lightSum += lightFactorForLevel(
                        snapshot,
                        snapshotCombinedLightAt(snapshot,
                                                outside.x + sides.sideB.x,
                                                outside.y + sides.sideB.y,
                                                outside.z + sides.sideB.z)
                    );
                    lightSum += lightFactorForLevel(
                        snapshot,
                        snapshotCombinedLightAt(snapshot,
                                                outside.x + sides.sideA.x + sides.sideB.x,
                                                outside.y + sides.sideA.y + sides.sideB.y,
                                                outside.z + sides.sideA.z + sides.sideB.z)
                    );
                    lightFactor = lightSum * 0.25f;
                }

                float aoFactor = 1.0f;
                if (applyStructuralAo) {
                    const bool sideAOccluded = snapshotBlockOccludesAmbientLight(
                        snapshot,
                        prototypeTraits,
                        anchor + sides.sideA
                    );
                    const bool sideBOccluded = snapshotBlockOccludesAmbientLight(
                        snapshot,
                        prototypeTraits,
                        anchor + sides.sideB
                    );
                    const bool cornerOccluded = snapshotBlockOccludesAmbientLight(
                        snapshot,
                        prototypeTraits,
                        anchor + sides.sideA + sides.sideB
                    );
                    const int occlusionLevel = (sideAOccluded && sideBOccluded)
                        ? 3
                        : (sideAOccluded ? 1 : 0) + (sideBOccluded ? 1 : 0) + (cornerOccluded ? 1 : 0);
                    aoFactor = kAoFactors[static_cast<size_t>(occlusionLevel)];
                }

                outAo[corner] = glm::clamp(lightFactor * aoFactor, 0.0f, 1.0f);
            }

            return outAo;
        }

        bool appendBinaryGreedyQuad(FaceInstanceRenderData& outFace,
                                    int mirrorFaceType,
                                    const glm::ivec3& worldMin,
                                    int spanA,
                                    int spanB) {
            if (spanA <= 0 || spanB <= 0) return false;

            if (mirrorFaceType == 2 || mirrorFaceType == 3) {
                outFace.position = glm::vec3(
                    static_cast<float>(worldMin.x) + 0.5f * static_cast<float>(spanA - 1),
                    static_cast<float>(worldMin.y) + (mirrorFaceType == 2 ? 0.5f : -0.5f),
                    static_cast<float>(worldMin.z) + 0.5f * static_cast<float>(spanB - 1)
                );
                outFace.scale = glm::vec2(static_cast<float>(spanA), static_cast<float>(spanB));
            } else if (mirrorFaceType == 0 || mirrorFaceType == 1) {
                outFace.position = glm::vec3(
                    static_cast<float>(worldMin.x) + (mirrorFaceType == 0 ? 0.5f : -0.5f),
                    static_cast<float>(worldMin.y) + 0.5f * static_cast<float>(spanA - 1),
                    static_cast<float>(worldMin.z) + 0.5f * static_cast<float>(spanB - 1)
                );
                outFace.scale = glm::vec2(static_cast<float>(spanB), static_cast<float>(spanA));
            } else {
                outFace.position = glm::vec3(
                    static_cast<float>(worldMin.x) + 0.5f * static_cast<float>(spanA - 1),
                    static_cast<float>(worldMin.y) + 0.5f * static_cast<float>(spanB - 1),
                    static_cast<float>(worldMin.z) + (mirrorFaceType == 4 ? 0.5f : -0.5f)
                );
                outFace.scale = glm::vec2(static_cast<float>(spanA), static_cast<float>(spanB));
            }

            outFace.uvScale = outFace.scale;
            outFace.alpha = 1.0f;
            return true;
        }

        bool appendBinaryGreedySectionFaces(const VoxelMeshingSnapshot& snapshot,
                                            const std::vector<VoxelMeshingPrototypeTraits>& prototypeTraits,
                                            std::array<std::vector<FaceInstanceRenderData>, 6>& opaqueFaces) {
            thread_local BinaryGreedyScratch scratch;
            constexpr int kInteriorSize = BinaryGreedyMesher::kChunkSize;

            const int sectionSize = snapshot.sectionSize;
            if (sectionSize <= 0) return true;

            for (int tileZ = 0; tileZ < sectionSize; tileZ += kInteriorSize) {
                for (int tileY = 0; tileY < sectionSize; tileY += kInteriorSize) {
                    for (int tileX = 0; tileX < sectionSize; tileX += kInteriorSize) {
                        scratch.ensureCapacity();
                        std::vector<BinaryGreedyTypeInfo> typeInfos;
                        typeInfos.reserve(64);
                        typeInfos.push_back({});
                        std::unordered_map<BinaryGreedyMaterialKey, uint8_t, BinaryGreedyMaterialKeyHash> typeMap;
                        typeMap.reserve(64);

                        auto assignType = [&](uint32_t prototypeID,
                                              uint32_t packedColor,
                                              bool opaqueOnly,
                                              uint8_t& outType) -> bool {
                            BinaryGreedyMaterialKey key{prototypeID, packedColor, opaqueOnly};
                            auto it = typeMap.find(key);
                            if (it != typeMap.end()) {
                                outType = it->second;
                                return true;
                            }
                            if (typeInfos.size() >= 255u) {
                                return false;
                            }
                            outType = static_cast<uint8_t>(typeInfos.size());
                            typeInfos.push_back({prototypeID, packedColor, opaqueOnly});
                            typeMap.emplace(key, outType);
                            return true;
                        };

                        for (int py = 0; py < BinaryGreedyMesher::kPaddedChunkSize; ++py) {
                            for (int px = 0; px < BinaryGreedyMesher::kPaddedChunkSize; ++px) {
                                for (int pz = 0; pz < BinaryGreedyMesher::kPaddedChunkSize; ++pz) {
                                    const bool interior = (px > 0 && px < BinaryGreedyMesher::kPaddedChunkSize - 1
                                                        && py > 0 && py < BinaryGreedyMesher::kPaddedChunkSize - 1
                                                        && pz > 0 && pz < BinaryGreedyMesher::kPaddedChunkSize - 1);
                                    const glm::ivec3 tileLocal(px - 1, py - 1, pz - 1);
                                    const glm::ivec3 sectionLocal = glm::ivec3(tileX, tileY, tileZ) + tileLocal;
                                    uint32_t blockId = 0;
                                    uint32_t packedColor = 0;
                                    if (!interior
                                        || (sectionLocal.x >= 0 && sectionLocal.x < sectionSize
                                            && sectionLocal.y >= 0 && sectionLocal.y < sectionSize
                                            && sectionLocal.z >= 0 && sectionLocal.z < sectionSize)) {
                                        blockId = snapshotBlockAt(
                                            snapshot,
                                            sectionLocal.x,
                                            sectionLocal.y,
                                            sectionLocal.z
                                        );
                                        if (blockId != 0) {
                                            packedColor = snapshotColorAt(
                                                snapshot,
                                                sectionLocal.x,
                                                sectionLocal.y,
                                                sectionLocal.z
                                            );
                                        }
                                    }

                                    uint8_t type = 0;
                                    if (blockId < prototypeTraits.size()
                                        && prototypeTraits[blockId].binaryGreedyRenderable) {
                                        if (!assignType(blockId, packedColor, false, type)) {
                                            return false;
                                        }
                                    } else if (blockId < prototypeTraits.size()
                                               && prototypeTraits[blockId].opaqueBlock) {
                                        if (!assignType(blockId, packedColor, true, type)) {
                                            return false;
                                        }
                                    }

                                    scratch.voxels[static_cast<size_t>(binaryGreedyVoxelIndex(px, py, pz))] = type;
                                }
                            }
                        }

                        BinaryGreedyMesher::mesh(scratch.voxels.data(), scratch.meshData);

                        for (int binaryFace = 0; binaryFace < 6; ++binaryFace) {
                            const int faceBegin = scratch.meshData.faceQuadBegin[binaryFace];
                            const int faceLength = scratch.meshData.faceQuadLength[binaryFace];
                            for (int quadIndex = 0; quadIndex < faceLength; ++quadIndex) {
                                const BinaryGreedyMesher::Quad& quad =
                                    scratch.quads[static_cast<size_t>(faceBegin + quadIndex)];
                                const uint8_t type = quad.type;
                                if (type == 0 || type >= typeInfos.size()) continue;
                                const BinaryGreedyTypeInfo& info = typeInfos[static_cast<size_t>(type)];
                                if (info.opaqueOnly) continue;
                                if (info.prototypeID == 0 || info.prototypeID >= prototypeTraits.size()) continue;
                                const VoxelMeshingPrototypeTraits& protoTraits = prototypeTraits[info.prototypeID];

                                int mirrorFaceType = 0;
                                int spanA = 1;
                                int spanB = 1;
                                glm::ivec3 worldMin = snapshot.sectionBase;
                                switch (quad.face) {
                                    case BinaryGreedyMesher::Face::PosX:
                                        mirrorFaceType = 0;
                                        worldMin += glm::ivec3(tileX + quad.x, tileY + quad.y, tileZ + quad.z);
                                        spanA = quad.width;
                                        spanB = quad.height;
                                        break;
                                    case BinaryGreedyMesher::Face::NegX:
                                        mirrorFaceType = 1;
                                        worldMin += glm::ivec3(tileX + quad.x, tileY + quad.y, tileZ + quad.z);
                                        spanA = quad.width;
                                        spanB = quad.height;
                                        break;
                                    case BinaryGreedyMesher::Face::PosY:
                                        mirrorFaceType = 2;
                                        worldMin += glm::ivec3(tileX + quad.x, tileY + quad.y, tileZ + quad.z);
                                        spanA = quad.width;
                                        spanB = quad.height;
                                        break;
                                    case BinaryGreedyMesher::Face::NegY:
                                        mirrorFaceType = 3;
                                        worldMin += glm::ivec3(tileX + quad.x, tileY + quad.y, tileZ + quad.z);
                                        spanA = quad.width;
                                        spanB = quad.height;
                                        break;
                                    case BinaryGreedyMesher::Face::PosZ:
                                        mirrorFaceType = 4;
                                        worldMin += glm::ivec3(tileX + quad.x, tileY + quad.y, tileZ + quad.z);
                                        spanA = quad.width;
                                        spanB = quad.height;
                                        break;
                                    case BinaryGreedyMesher::Face::NegZ:
                                        mirrorFaceType = 5;
                                        worldMin += glm::ivec3(tileX + quad.x, tileY + quad.y, tileZ + quad.z);
                                        spanA = quad.width;
                                        spanB = quad.height;
                                        break;
                                }

                                FaceInstanceRenderData face{};
                                face.tileIndex = protoTraits.faceTiles[static_cast<size_t>(mirrorFaceType)];
                                face.color = (face.tileIndex >= 0)
                                    ? glm::vec3(1.0f)
                                    : VoxelMeshInitSystemLogic::UnpackColor(info.packedColor);

                                if (!appendBinaryGreedyQuad(face, mirrorFaceType, worldMin, spanA, spanB)) continue;
                                face.ao = computeFaceCornerLighting(
                                    snapshot,
                                    prototypeTraits,
                                    worldMin - snapshot.sectionBase,
                                    mirrorFaceType,
                                    spanA,
                                    spanB,
                                    true
                                );
                                opaqueFaces[static_cast<size_t>(mirrorFaceType)].push_back(face);
                            }
                        }
                    }
                }
            }

            return true;
        }

        bool UploadPreparedVoxelSectionMesh(const PreparedVoxelSectionMesh& preparedMesh,
                                            ChunkRenderBuffers& buffers,
                                            RendererContext& renderer,
                                            IRenderBackend& renderBackend) {
            ::RenderInitSystemLogic::DestroyChunkRenderBuffers(buffers, renderBackend);
            buffers.counts.fill(0);
            buffers.usesTexturedFaceBuffers = preparedMesh.usesTexturedFaceBuffers;

            for (int faceType = 0; faceType < 6; ++faceType) {
                const auto& opaque = preparedMesh.opaqueFaces[static_cast<size_t>(faceType)];
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

                const auto& alpha = preparedMesh.alphaFaces[static_cast<size_t>(faceType)];
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
            buffers.builtWithFaceCulling = preparedMesh.builtWithFaceCulling;
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

    std::vector<VoxelMeshingPrototypeTraits> BuildVoxelMeshingPrototypeTraits(const BaseSystem& baseSystem,
                                                                              const std::vector<Entity>& prototypes) {
        return buildPrototypeRenderTraits(baseSystem, prototypes);
    }

    bool PrepareVoxelSectionMesh(const VoxelMeshingSnapshot& snapshot,
                                 const std::vector<VoxelMeshingPrototypeTraits>& prototypeTraits,
                                 PreparedVoxelSectionMesh& outMesh) {
        outMesh = {};
        outMesh.dirtyTicket = snapshot.dirtyTicket;
        outMesh.usesTexturedFaceBuffers = true;
        if (snapshot.sectionSize <= 0 || snapshot.nonAirCount <= 0) {
            return true;
        }

        static const std::array<glm::ivec3, 6> kFaceNormals = {
            glm::ivec3(1, 0, 0),
            glm::ivec3(-1, 0, 0),
            glm::ivec3(0, 1, 0),
            glm::ivec3(0, -1, 0),
            glm::ivec3(0, 0, 1),
            glm::ivec3(0, 0, -1)
        };
        static const std::array<int, 4> kPlantFaces = {0, 1, 4, 5};
        static const VoxelMeshingPrototypeTraits kEmptyTraits{};
        auto traitsFor = [&](uint32_t id) -> const VoxelMeshingPrototypeTraits& {
            if (id < prototypeTraits.size()) return prototypeTraits[id];
            return kEmptyTraits;
        };

        auto pushFace = [&](int faceType, const FaceInstanceRenderData& face) {
            if (isMaskedFoliageTaggedFace(face.alpha)) {
                outMesh.opaqueFaces[static_cast<size_t>(faceType)].push_back(face);
                outMesh.alphaFaces[static_cast<size_t>(faceType)].push_back(face);
                return;
            }
            if (shouldRenderInAlphaPass(face.alpha)) {
                outMesh.alphaFaces[static_cast<size_t>(faceType)].push_back(face);
            } else {
                outMesh.opaqueFaces[static_cast<size_t>(faceType)].push_back(face);
            }
        };

        const bool binaryGreedyBuilt = snapshot.binaryGreedyEnabled
            && appendBinaryGreedySectionFaces(snapshot, prototypeTraits, outMesh.opaqueFaces);

        for (int z = 0; z < snapshot.sectionSize; ++z) {
            for (int y = 0; y < snapshot.sectionSize; ++y) {
                for (int x = 0; x < snapshot.sectionSize; ++x) {
                    const uint32_t id = snapshotBlockAt(snapshot, x, y, z);
                    const VoxelMeshingPrototypeTraits& traits = traitsFor(id);
                    if (!traits.renderableBlock) continue;
                    if (binaryGreedyBuilt && traits.binaryGreedyRenderable) continue;

                    const bool isLeaf = traits.leaf;
                    const bool isTallGrass = traits.tallGrass;
                    const bool isShortGrass = traits.shortGrass;
                    const bool isFlower = traits.flower;
                    const bool isCavePot = traits.cavePot;
                    const bool isPlantCard = isTallGrass || isShortGrass || isFlower || isCavePot;
                    const bool isLeafFanPlant = traits.leafFanPlant;
                    const bool isStick = traits.stick;
                    const bool isGrassCover = traits.grassCoverX || traits.grassCoverZ;
                    const bool isStonePebble = traits.stonePebbleX || traits.stonePebbleZ;
                    const bool isSurfaceStonePebble = traits.surfaceStonePebble;
                    const bool isPetalPile = traits.petalPile;
                    const bool isNarrowStonePebble = isStonePebble && !isPetalPile;
                    const bool isGroundCoverDecal = isGrassCover || isPetalPile;
                    const glm::ivec3 worldCell = snapshot.sectionBase + glm::ivec3(x, y, z);
                    const uint32_t packedColorRaw = snapshotColorAt(snapshot, x, y, z);
                    const glm::vec3 packedColor = VoxelMeshInitSystemLogic::UnpackColor(packedColorRaw);

                    if (isPlantCard) {
                        const int plantTile = traits.faceTiles[2];
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
                            face.ao = computeFaceCornerLighting(
                                snapshot,
                                prototypeTraits,
                                glm::ivec3(x, y, z),
                                faceType,
                                1,
                                1,
                                false
                            );
                            face.scale = plantScale;
                            face.uvScale = glm::vec2(1.0f);
                            pushFace(faceType, face);
                        }
                        continue;
                    }

                    for (int faceType = 0; faceType < 6; ++faceType) {
                        const glm::ivec3 neighborLocal = glm::ivec3(x, y, z) + kFaceNormals[static_cast<size_t>(faceType)];
                        const uint32_t neighborId = snapshotBlockAt(snapshot, neighborLocal.x, neighborLocal.y, neighborLocal.z);
                        if (traitsFor(neighborId).opaqueBlock) continue;
                        if (isLeaf && traitsFor(neighborId).leaf) continue;

                        FaceInstanceRenderData face{};
                        glm::vec3 normal = glm::vec3(kFaceNormals[static_cast<size_t>(faceType)]);
                        face.tileIndex = traits.faceTiles[static_cast<size_t>(faceType)];
                        if (isGrassCover) {
                            const int snapshotTile = decodeGrassCoverSnapshotTile(packedColorRaw);
                            if (snapshotTile >= 0) {
                                face.tileIndex = snapshotTile;
                            } else {
                                const uint32_t supportId = snapshotBlockAt(snapshot, x, y - 1, z);
                                const int supportTopTile = traitsFor(supportId).faceTiles[2];
                                if (supportTopTile >= 0) {
                                    face.tileIndex = supportTopTile;
                                }
                            }
                        }
                        face.color = (face.tileIndex >= 0) ? glm::vec3(1.0f) : packedColor;
                        face.alpha = isLeaf ? -1.0f : 1.0f;
                        if (isGrassCover) {
                            face.alpha = -14.0f;
                        } else if (traits.water) {
                            face.alpha = 0.08f;
                        } else if (traits.transparentWave) {
                            face.alpha = 0.06f;
                        }
                        if (traits.water || traits.transparentWave) {
                            face.ao = glm::vec4(1.0f);
                        } else {
                            face.ao = computeFaceCornerLighting(
                                snapshot,
                                prototypeTraits,
                                glm::ivec3(x, y, z),
                                faceType,
                                1,
                                1,
                                true
                            );
                        }
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
                                halfExtents = traits.stickX
                                    ? glm::vec3(kHalf12, kHalf1, kHalf1)
                                    : glm::vec3(kHalf1, kHalf1, kHalf12);
                            } else {
                                constexpr float kHalf2 = 2.0f / 48.0f;
                                constexpr float kHalf6 = 6.0f / 48.0f;
                                halfExtents = traits.stonePebbleX
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

        outMesh.builtWithFaceCulling = false;
        return true;
    }

    void UpdateVoxelMeshUpload(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float, PlatformWindowHandle) {
        if (!baseSystem.renderer || !baseSystem.player || !baseSystem.renderBackend) return;
        RendererContext& renderer = *baseSystem.renderer;
        IRenderBackend& renderBackend = *baseSystem.renderBackend;
        glm::vec3 playerPos = baseSystem.player->cameraPosition;
        (void)prototypes;
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

            std::vector<VoxelSectionKey> stalePrepared;
            std::vector<VoxelSectionKey> clearDirty;
            for (const auto& [key, prepared] : voxelRender.preparedMeshes) {
                (void)prepared;
                auto it = voxelWorld.sections.find(key);
                if (it == voxelWorld.sections.end() || it->second.nonAirCount <= 0) {
                    stalePrepared.push_back(key);
                    clearDirty.push_back(key);
                    continue;
                }
                if (!isRenderableSection(key)) {
                    stalePrepared.push_back(key);
                }
            }
            for (const auto& key : stalePrepared) {
                voxelRender.preparedMeshes.erase(key);
                voxelRender.renderBuffersDirty.erase(key);
            }
            for (const auto& key : clearDirty) {
                voxelWorld.clearSectionDirty(key);
            }
            clearDirty.clear();

            std::vector<VoxelSectionKey> staleUploadKeys;
            for (const auto& key : voxelRender.renderBuffersDirty) {
                auto it = voxelWorld.sections.find(key);
                if (it == voxelWorld.sections.end() || it->second.nonAirCount <= 0) {
                    staleUploadKeys.push_back(key);
                    clearDirty.push_back(key);
                    continue;
                }
                if (!isRenderableSection(key)) {
                    staleUploadKeys.push_back(key);
                }
            }
            for (const auto& key : staleUploadKeys) {
                voxelRender.renderBuffersDirty.erase(key);
            }
            for (const auto& key : clearDirty) {
                voxelWorld.clearSectionDirty(key);
            }

            if (!voxelRender.renderBuffersDirty.empty()) {
                auto start = std::chrono::steady_clock::now();
                size_t uploadCount = 0;
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
                const size_t readyUploadBacklog = voxelRender.renderBuffersDirty.size();
                const bool uploadBacklogActive =
                    readyUploadBacklog > static_cast<size_t>(std::max(1, uploadMaxSections));
                const bool bootstrapActive = bootstrapEnabled
                    && (
                        (sectionCount >= static_cast<size_t>(bootstrapMinSections)
                            && static_cast<double>(meshCount)
                                < static_cast<double>(sectionCount) * static_cast<double>(bootstrapMeshCoverageTarget))
                        || uploadBacklogActive
                    );
                if (bootstrapActive) {
                    const int bootstrapSectionsCap = std::max(
                        1,
                        ::RenderInitSystemLogic::getRegistryInt(
                            baseSystem,
                            "voxelUploadBootstrapMaxSectionsPerFrame",
                            6
                        )
                    );
                    const float bootstrapMsCap = std::max(
                        0.1f,
                        ::RenderInitSystemLogic::getRegistryFloat(
                            baseSystem,
                            "voxelUploadBootstrapMaxMsPerFrame",
                            6.0f
                        )
                    );
                    uploadMaxSections = std::max(
                        uploadMaxSections,
                        uploadBacklogActive
                            ? std::min(static_cast<int>(readyUploadBacklog), bootstrapSectionsCap)
                            : bootstrapSectionsCap
                    );
                    uploadMaxMs = std::max(
                        uploadMaxMs,
                        bootstrapMsCap
                    );
                }

                struct Candidate {
                    VoxelSectionKey key;
                    float dist2 = 0.0f;
                    bool visible = false;
                };
                std::vector<Candidate> candidates;
                candidates.reserve(voxelRender.renderBuffersDirty.size());
                for (const auto& key : voxelRender.renderBuffersDirty) {
                    auto it = voxelWorld.sections.find(key);
                    if (it == voxelWorld.sections.end()) continue;
                    if (!isRenderableSection(key)) continue;
                    candidates.push_back({
                        key,
                        sectionDist2ToCamera(it->second, playerPos),
                        ::RenderInitSystemLogic::shouldRenderVoxelSection(baseSystem, it->second, playerPos)
                    });
                }
                std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
                    if (a.visible != b.visible) return a.visible && !b.visible;
                    if (a.dist2 != b.dist2) return a.dist2 < b.dist2;
                    if (a.key.coord.x != b.key.coord.x) return a.key.coord.x < b.key.coord.x;
                    if (a.key.coord.y != b.key.coord.y) return a.key.coord.y < b.key.coord.y;
                    return a.key.coord.z < b.key.coord.z;
                });

                for (const Candidate& c : candidates) {
                    if (static_cast<int>(uploadCount) >= uploadMaxSections) break;
                    const auto now = std::chrono::steady_clock::now();
                    const double elapsedMs = std::chrono::duration<double, std::milli>(now - start).count();
                    if (elapsedMs >= static_cast<double>(uploadMaxMs)) break;

                    auto it = voxelWorld.sections.find(c.key);
                    if (it == voxelWorld.sections.end()
                        || it->second.nonAirCount <= 0
                        || !isRenderableSection(c.key)) {
                        voxelRender.preparedMeshes.erase(c.key);
                        voxelRender.renderBuffersDirty.erase(c.key);
                        if (it == voxelWorld.sections.end() || it->second.nonAirCount <= 0) {
                            voxelWorld.clearSectionDirty(c.key);
                        }
                        continue;
                    }

                    auto preparedIt = voxelRender.preparedMeshes.find(c.key);
                    if (preparedIt == voxelRender.preparedMeshes.end()) {
                        continue;
                    }

                    const uint64_t currentDirtyTicket = voxelWorld.getSectionDirtyTicket(c.key);
                    if (currentDirtyTicket == 0) {
                        voxelRender.preparedMeshes.erase(preparedIt);
                        voxelRender.renderBuffersDirty.erase(c.key);
                        continue;
                    }

                    if (preparedIt->second.dirtyTicket != currentDirtyTicket) {
                        voxelRender.preparedMeshes.erase(preparedIt);
                        continue;
                    }

                    if (UploadPreparedVoxelSectionMesh(
                            preparedIt->second,
                            voxelRender.renderBuffers[c.key],
                            renderer,
                            renderBackend)) {
                        voxelRender.preparedMeshes.erase(c.key);
                        voxelRender.renderBuffersDirty.erase(c.key);
                        voxelWorld.clearSectionDirty(c.key);
                        ++uploadCount;
                    }
                }
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start
                ).count();
                if (debugVoxelMeshingPerf) {
                    std::cout << "RenderSystem: uploaded " << uploadCount
                              << " prepared voxel section buffer(s) in "
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
