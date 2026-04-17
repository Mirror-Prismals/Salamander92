#pragma once
#include "../Host.h"
#include "Host/PlatformInput.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace HostLogic { EntityInstance CreateInstance(BaseSystem& baseSystem, int prototypeID, glm::vec3 position, glm::vec3 color); }
namespace BlockSelectionSystemLogic {
    void RemoveBlockFromCache(BaseSystem& baseSystem, const std::vector<Entity>& prototypes, int worldIndex, const glm::vec3& position);
    bool HasBlockAt(BaseSystem& baseSystem, const std::vector<Entity>& prototypes, int worldIndex, const glm::vec3& position);
    void AddBlockToCache(BaseSystem& baseSystem, std::vector<Entity>& prototypes, int worldIndex, const glm::vec3& position, int prototypeID);
}
namespace StructureCaptureSystemLogic { void NotifyBlockChanged(BaseSystem& baseSystem, int worldIndex, const glm::vec3& position); }
namespace RayTracedAudioSystemLogic { void InvalidateSourceCache(BaseSystem& baseSystem); }
namespace ChucKSystemLogic { void StopNoiseShred(BaseSystem& baseSystem); }
namespace AudioSystemLogic { bool TriggerGameplaySfx(BaseSystem& baseSystem, const std::string& cueName, float gain); }
namespace VoxelMeshingSystemLogic { void RequestPriorityVoxelRemesh(BaseSystem& baseSystem, std::vector<Entity>& prototypes, const glm::ivec3& worldCell); }
namespace TreeGenerationSystemLogic { void NotifyPineLogRemoved(const glm::ivec3& worldCell, int removedPrototypeID); }
namespace GemSystemLogic {
    void SpawnGemDropFromOre(BaseSystem& baseSystem, std::vector<Entity>& prototypes, int removedPrototypeID, const glm::vec3& blockPos, const glm::vec3& playerForward);
    bool TryPickupGemFromRay(BaseSystem& baseSystem, const glm::vec3& rayOrigin, const glm::vec3& rayDirection, float maxDistance, GemDropState* outDrop);
    void PlaceGemDrop(BaseSystem& baseSystem, GemDropState&& heldDrop, const glm::vec3& position);
}
namespace OreMiningSystemLogic {
    bool IsMiningActive(const BaseSystem& baseSystem);
    bool StartOreMiningFromBlock(BaseSystem& baseSystem,
                                 std::vector<Entity>& prototypes,
                                 int worldIndex,
                                 const glm::ivec3& cell,
                                 int targetPrototypeID,
                                 const glm::vec3& blockPos,
                                 const glm::vec3& playerForward);
}
namespace GroundCraftingSystemLogic { bool IsRitualActive(const BaseSystem& baseSystem); }
namespace GemChiselSystemLogic {
    bool IsChiselActive(const BaseSystem& baseSystem);
    bool StartGemChiselAtCell(BaseSystem& baseSystem, const glm::ivec3& cell, int worldIndex);
}
namespace BookSystemLogic { bool IsBookPrototypeName(const std::string& name); }

namespace BlockChargeSystemLogic {

    namespace {
        constexpr float CHARGE_TIME_PICKUP = 0.25f;
        constexpr float CHARGE_TIME_DESTROY = 0.25f;
        constexpr float POSITION_EPSILON = 0.05f;
        constexpr int HATCHET_MATERIAL_STONE = 0;
        constexpr int HATCHET_MATERIAL_RUBY = 1;
        constexpr int HATCHET_MATERIAL_AMETHYST = 2;
        constexpr int HATCHET_MATERIAL_FLOURITE = 3;
        constexpr int HATCHET_MATERIAL_SILVER = 4;
        constexpr int HATCHET_MATERIAL_COUNT = 5;
        constexpr int BLOCK_DAMAGE_SHADER_MAX = 64;
        constexpr int CHALK_TILE_CORNER = 186;
        constexpr int CHALK_TILE_CROSS = 187;
        constexpr int CHALK_TILE_DOT = 188;
        constexpr int CHALK_TILE_END = 189;
        constexpr int CHALK_TILE_STRAIGHT = 190;
        constexpr int CHALK_TILE_T = 191;
        constexpr int SURFACE_STONE_PILE_MIN = 1;
        constexpr int SURFACE_STONE_PILE_MAX = 8;

        struct ThrownHeldBlockRuntime {
            bool active = false;
            int worldIndex = -1;
            int prototypeID = -1;
            glm::vec3 color = glm::vec3(1.0f);
            uint32_t packedColor = 0u;
            bool hasSourceCell = false;
            glm::ivec3 sourceCell = glm::ivec3(0);
            glm::vec3 position = glm::vec3(0.0f);
            glm::vec3 velocity = glm::vec3(0.0f);
            int instanceID = -1;
            float age = 0.0f;
        };

        struct BlockDamageKey {
            int worldIndex = -1;
            glm::ivec3 cell = glm::ivec3(0);
            bool operator==(const BlockDamageKey& other) const {
                return worldIndex == other.worldIndex && cell == other.cell;
            }
        };

        struct BlockDamageKeyHash {
            std::size_t operator()(const BlockDamageKey& key) const noexcept {
                std::size_t h = std::hash<int>()(key.worldIndex);
                h ^= (std::hash<int>()(key.cell.x) + 0x9e3779b9u + (h << 6) + (h >> 2));
                h ^= (std::hash<int>()(key.cell.y) + 0x9e3779b9u + (h << 6) + (h >> 2));
                h ^= (std::hash<int>()(key.cell.z) + 0x9e3779b9u + (h << 6) + (h >> 2));
                return h;
            }
        };

        struct BlockDamageEntry {
            int prototypeID = -1;
            int hits = 0;
            int requiredHits = 8;
            double lastHitTime = 0.0;
        };

        std::unordered_map<BlockDamageKey, BlockDamageEntry, BlockDamageKeyHash>& blockDamageMap() {
            static std::unordered_map<BlockDamageKey, BlockDamageEntry, BlockDamageKeyHash> s_map;
            return s_map;
        }

        float readRegistryFloat(const BaseSystem& baseSystem, const char* key, float fallback) {
            if (!baseSystem.registry) return fallback;
            auto it = baseSystem.registry->find(key);
            if (it == baseSystem.registry->end() || !std::holds_alternative<std::string>(it->second)) return fallback;
            try {
                return std::stof(std::get<std::string>(it->second));
            } catch (...) {
                return fallback;
            }
        }

        int readRegistryInt(const BaseSystem& baseSystem, const char* key, int fallback) {
            if (!baseSystem.registry) return fallback;
            auto it = baseSystem.registry->find(key);
            if (it == baseSystem.registry->end() || !std::holds_alternative<std::string>(it->second)) return fallback;
            try {
                return std::stoi(std::get<std::string>(it->second));
            } catch (...) {
                return fallback;
            }
        }

        bool readRegistryBool(const BaseSystem& baseSystem, const char* key, bool fallback) {
            if (!baseSystem.registry) return fallback;
            auto it = baseSystem.registry->find(key);
            if (it == baseSystem.registry->end()) return fallback;
            if (std::holds_alternative<bool>(it->second)) return std::get<bool>(it->second);
            if (!std::holds_alternative<std::string>(it->second)) return fallback;
            std::string v = std::get<std::string>(it->second);
            std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
            if (v == "0" || v == "false" || v == "no" || v == "off") return false;
            return fallback;
        }

        glm::vec3 cameraEyePosition(const BaseSystem& baseSystem, const PlayerContext& player) {
            if (baseSystem.gamemode == "survival") {
                return player.cameraPosition + glm::vec3(0.0f, 0.6f, 0.0f);
            }
            return player.cameraPosition;
        }

        glm::vec3 cameraForwardDirection(const PlayerContext& player) {
            glm::vec3 front(0.0f);
            front.x = std::cos(glm::radians(player.cameraYaw)) * std::cos(glm::radians(player.cameraPitch));
            front.y = std::sin(glm::radians(player.cameraPitch));
            front.z = std::sin(glm::radians(player.cameraYaw)) * std::cos(glm::radians(player.cameraPitch));
            if (glm::length(front) < 0.0001f) {
                return glm::vec3(0.0f, 0.0f, -1.0f);
            }
            return glm::normalize(front);
        }

        bool isPickupHandMode(BuildModeType mode) {
            return mode == BuildModeType::Pickup || mode == BuildModeType::PickupLeft;
        }

        void saveActiveHeldBlockForMode(PlayerContext& player, BuildModeType mode) {
            if (mode == BuildModeType::Pickup) {
                player.rightHandHoldingBlock = player.isHoldingBlock;
                player.rightHandHeldPrototypeID = player.heldPrototypeID;
                player.rightHandHeldBlockColor = player.heldBlockColor;
                player.rightHandHeldPackedColor = player.heldPackedColor;
                player.rightHandHeldHasSourceCell = player.heldHasSourceCell;
                player.rightHandHeldSourceCell = player.heldSourceCell;
            } else if (mode == BuildModeType::PickupLeft) {
                player.leftHandHoldingBlock = player.isHoldingBlock;
                player.leftHandHeldPrototypeID = player.heldPrototypeID;
                player.leftHandHeldBlockColor = player.heldBlockColor;
                player.leftHandHeldPackedColor = player.heldPackedColor;
                player.leftHandHeldHasSourceCell = player.heldHasSourceCell;
                player.leftHandHeldSourceCell = player.heldSourceCell;
            }
        }

        void loadActiveHeldBlockForMode(PlayerContext& player, BuildModeType mode) {
            if (mode == BuildModeType::Pickup) {
                player.isHoldingBlock = player.rightHandHoldingBlock;
                player.heldPrototypeID = player.rightHandHeldPrototypeID;
                player.heldBlockColor = player.rightHandHeldBlockColor;
                player.heldPackedColor = player.rightHandHeldPackedColor;
                player.heldHasSourceCell = player.rightHandHeldHasSourceCell;
                player.heldSourceCell = player.rightHandHeldSourceCell;
            } else if (mode == BuildModeType::PickupLeft) {
                player.isHoldingBlock = player.leftHandHoldingBlock;
                player.heldPrototypeID = player.leftHandHeldPrototypeID;
                player.heldBlockColor = player.leftHandHeldBlockColor;
                player.heldPackedColor = player.leftHandHeldPackedColor;
                player.heldHasSourceCell = player.leftHandHeldHasSourceCell;
                player.heldSourceCell = player.leftHandHeldSourceCell;
            }
        }

        struct ScopeExit {
            std::function<void()> fn;
            ~ScopeExit() {
                if (fn) fn();
            }
        };

        bool isRemovableGameplayBlock(const Entity& proto) {
            if (!proto.isBlock) return false;
            if (proto.name == "VoidPortalBlockTex") return false;
            // Terrain voxels are often chunkable but flagged immutable (e.g. ScaffoldBlock).
            // Allow those for gameplay pickup/destroy while keeping non-chunkable immutables protected.
            if (proto.isMutable) return true;
            return proto.isChunkable;
        }

        bool isComputerPrototypeID(const std::vector<Entity>& prototypes, int prototypeID) {
            return prototypeID >= 0
                && prototypeID < static_cast<int>(prototypes.size())
                && prototypes[static_cast<size_t>(prototypeID)].name == "Computer";
        }

        bool isWallStonePrototypeName(const std::string& name) {
            return name == "WallStoneTexPosX"
                || name == "WallStoneTexNegX"
                || name == "WallStoneTexPosZ"
                || name == "WallStoneTexNegZ";
        }

        int resolveLeafPrototypeID(const std::vector<Entity>& prototypes) {
            for (const auto& proto : prototypes) {
                if (proto.name == "Leaf") return proto.prototypeID;
            }
            return -1;
        }

        bool isStickPrototypeName(const std::string& name) {
            return name == "StickTexX" || name == "StickTexZ";
        }

        bool isChalkStickPrototypeName(const std::string& name) {
            return name == "StonePebbleChalkTexX"
                || name == "StonePebbleChalkTexZ";
        }

        bool isChalkDrawToolPrototypeName(const std::string& name) {
            return isChalkStickPrototypeName(name)
                || name == "ChalkBlockTex";
        }

        bool isChalkDustPrototypeName(const std::string& name) {
            return name == "GrassCoverChalkTexX"
                || name == "GrassCoverChalkTexZ";
        }

        bool isVerticalLogPrototypeName(const std::string& name) {
            return name == "FirLog1Tex"
                || name == "FirLog2Tex"
                || name == "SpruceLog1Tex"
                || name == "SpruceLog2Tex"
                || name == "SpruceLog3Tex"
                || name == "SpruceLog4Tex"
                || name == "OakLogTex";
        }

        bool isCavePotPrototypeName(const std::string& name) {
            return name == "StonePebbleCavePotTexX"
                || name == "StonePebbleCavePotTexZ";
        }

        bool isFlowerPrototypeName(const std::string& name) {
            return name == "Flower";
        }

        bool isNaturalSurfaceStonePrototypeName(const std::string& name) {
            return name == "StonePebbleTexX" || name == "StonePebbleTexZ";
        }

        bool isSurfaceStonePrototypeName(const std::string& name) {
            return isNaturalSurfaceStonePrototypeName(name)
                || name == "StonePebbleRubyTexX" || name == "StonePebbleRubyTexZ"
                || name == "StonePebbleAmethystTexX" || name == "StonePebbleAmethystTexZ"
                || name == "StonePebbleFlouriteTexX" || name == "StonePebbleFlouriteTexZ"
                || name == "StonePebbleSilverTexX" || name == "StonePebbleSilverTexZ";
        }

        bool isBlueprintPrototypeName(const std::string& name) {
            return name.rfind("GrassCoverBlueprint", 0) == 0;
        }

        int oreKindForPrototypeName(const std::string& name) {
            if (name == "RubyOreTex") return 0;
            if (name == "AmethystOreTex") return 1;
            if (name == "FlouriteOreTex" || name == "FluoriteOreTex") return 2;
            if (name == "SilverOreTex") return 3;
            return -1;
        }

        bool isOrePrototypeID(const std::vector<Entity>& prototypes, int prototypeID) {
            if (prototypeID < 0 || prototypeID >= static_cast<int>(prototypes.size())) return false;
            return oreKindForPrototypeName(prototypes[static_cast<size_t>(prototypeID)].name) >= 0;
        }

        int hatchetMaterialFromGemKind(int gemKind) {
            if (gemKind == 0) return HATCHET_MATERIAL_RUBY;
            if (gemKind == 1) return HATCHET_MATERIAL_AMETHYST;
            if (gemKind == 2) return HATCHET_MATERIAL_FLOURITE;
            if (gemKind == 3) return HATCHET_MATERIAL_SILVER;
            return HATCHET_MATERIAL_STONE;
        }

        int hatchetMaterialFromStonePrototypeName(const std::string& name, bool* outRecognized = nullptr) {
            if (outRecognized) *outRecognized = true;
            if (name == "StonePebbleTexX" || name == "StonePebbleTexZ") return HATCHET_MATERIAL_STONE;
            if (name == "StonePebbleRubyTexX" || name == "StonePebbleRubyTexZ") return HATCHET_MATERIAL_RUBY;
            if (name == "StonePebbleAmethystTexX" || name == "StonePebbleAmethystTexZ") return HATCHET_MATERIAL_AMETHYST;
            if (name == "StonePebbleFlouriteTexX" || name == "StonePebbleFlouriteTexZ") return HATCHET_MATERIAL_FLOURITE;
            if (name == "StonePebbleSilverTexX" || name == "StonePebbleSilverTexZ") return HATCHET_MATERIAL_SILVER;
            if (outRecognized) *outRecognized = false;
            return HATCHET_MATERIAL_STONE;
        }

        glm::vec3 hatchetMaterialColor(int material) {
            switch (material) {
                case HATCHET_MATERIAL_RUBY: return glm::vec3(0.86f, 0.18f, 0.20f);
                case HATCHET_MATERIAL_AMETHYST: return glm::vec3(0.64f, 0.48f, 0.88f);
                case HATCHET_MATERIAL_FLOURITE: return glm::vec3(0.38f, 0.67f, 0.96f);
                case HATCHET_MATERIAL_SILVER: return glm::vec3(0.92f, 0.93f, 0.95f);
                case HATCHET_MATERIAL_STONE:
                default: return glm::vec3(1.0f);
            }
        }

        int detectHatchetMaterialFromColor(const glm::vec3& color) {
            int bestMaterial = HATCHET_MATERIAL_STONE;
            float bestDist2 = std::numeric_limits<float>::max();
            for (int m = HATCHET_MATERIAL_RUBY; m < HATCHET_MATERIAL_COUNT; ++m) {
                const glm::vec3 target = hatchetMaterialColor(m);
                const glm::vec3 d = color - target;
                const float dist2 = glm::dot(d, d);
                if (dist2 < bestDist2) {
                    bestDist2 = dist2;
                    bestMaterial = m;
                }
            }
            // Keep natural grey stones as base material unless they are clearly gem-tinted.
            const float matchThreshold2 = 0.16f * 0.16f;
            if (bestDist2 <= matchThreshold2) return bestMaterial;
            return HATCHET_MATERIAL_STONE;
        }

        int sumHatchetInventory(const PlayerContext& player) {
            int total = 0;
            for (int i = 0; i < HATCHET_MATERIAL_COUNT; ++i) {
                total += std::max(0, player.hatchetInventoryByMaterial[static_cast<size_t>(i)]);
            }
            return total;
        }

        int firstAvailableHatchetMaterial(const PlayerContext& player) {
            for (int i = 0; i < HATCHET_MATERIAL_COUNT; ++i) {
                if (player.hatchetInventoryByMaterial[static_cast<size_t>(i)] > 0) return i;
            }
            return HATCHET_MATERIAL_STONE;
        }

        int resolveSurfaceStonePrototypeID(const std::vector<Entity>& prototypes, int preferredPrototypeID = -1) {
            if (preferredPrototypeID >= 0 && preferredPrototypeID < static_cast<int>(prototypes.size())) {
                if (isSurfaceStonePrototypeName(prototypes[static_cast<size_t>(preferredPrototypeID)].name)) {
                    return preferredPrototypeID;
                }
            }

            int fallbackZ = -1;
            for (const auto& proto : prototypes) {
                if (!proto.isBlock) continue;
                if (proto.name == "StonePebbleTexX") return proto.prototypeID;
                if (fallbackZ < 0 && proto.name == "StonePebbleTexZ") fallbackZ = proto.prototypeID;
            }
            return fallbackZ;
        }

        int resolveGemIngotPrototypeID(const std::vector<Entity>& prototypes, int material) {
            const char* preferredX = nullptr;
            const char* fallbackZ = nullptr;
            switch (material) {
                case HATCHET_MATERIAL_RUBY:
                    preferredX = "StonePebbleRubyTexX";
                    fallbackZ = "StonePebbleRubyTexZ";
                    break;
                case HATCHET_MATERIAL_AMETHYST:
                    preferredX = "StonePebbleAmethystTexX";
                    fallbackZ = "StonePebbleAmethystTexZ";
                    break;
                case HATCHET_MATERIAL_FLOURITE:
                    preferredX = "StonePebbleFlouriteTexX";
                    fallbackZ = "StonePebbleFlouriteTexZ";
                    break;
                case HATCHET_MATERIAL_SILVER:
                    preferredX = "StonePebbleSilverTexX";
                    fallbackZ = "StonePebbleSilverTexZ";
                    break;
                default:
                    return resolveSurfaceStonePrototypeID(prototypes);
            }

            int fallbackId = -1;
            for (const auto& proto : prototypes) {
                if (!proto.isBlock) continue;
                if (preferredX && proto.name == preferredX) return proto.prototypeID;
                if (fallbackId < 0 && fallbackZ && proto.name == fallbackZ) fallbackId = proto.prototypeID;
            }
            if (fallbackId >= 0) return fallbackId;
            return resolveSurfaceStonePrototypeID(prototypes);
        }

        int resolveMappedAtlasTileIndex(const WorldContext& world, const char* textureKey) {
            if (!textureKey) return -1;
            auto it = world.atlasMappings.find(textureKey);
            if (it == world.atlasMappings.end()) return -1;
            if (it->second.all >= 0) return it->second.all;
            if (it->second.side >= 0) return it->second.side;
            if (it->second.top >= 0) return it->second.top;
            if (it->second.bottom >= 0) return it->second.bottom;
            return -1;
        }

        bool extractAtlasTilePixels(const std::vector<unsigned char>& atlasPixels,
                                    const glm::ivec2& atlasSize,
                                    int tileIndex,
                                    const glm::ivec2& tileSize,
                                    int tilesPerRow,
                                    int tilesPerCol,
                                    std::vector<unsigned char>& outPixels) {
            outPixels.clear();
            if (atlasPixels.empty()
                || atlasSize.x <= 0
                || atlasSize.y <= 0
                || tileIndex < 0
                || tileSize.x <= 0
                || tileSize.y <= 0
                || tilesPerRow <= 0
                || tilesPerCol <= 0) {
                return false;
            }

            const int tileX = (tileIndex % tilesPerRow) * tileSize.x;
            const int tileRowFromTop = (tileIndex / tilesPerRow);
            const int tileRowFromBottom = (tilesPerCol - 1 - tileRowFromTop);
            const int tileY = tileRowFromBottom * tileSize.y;
            if (tileX < 0
                || tileY < 0
                || tileX + tileSize.x > atlasSize.x
                || tileY + tileSize.y > atlasSize.y) {
                return false;
            }

            outPixels.resize(static_cast<size_t>(tileSize.x * tileSize.y * 4), 0u);
            for (int y = 0; y < tileSize.y; ++y) {
                const size_t src = static_cast<size_t>(((tileY + y) * atlasSize.x + tileX) * 4);
                const size_t dst = static_cast<size_t>((y * tileSize.x) * 4);
                std::copy_n(&atlasPixels[src], static_cast<size_t>(tileSize.x * 4), &outPixels[dst]);
            }
            return true;
        }

        bool buildStencilHeadVoxelsFromTile(const std::vector<unsigned char>& stencilTile,
                                            int width,
                                            int height,
                                            std::vector<glm::ivec3>& outVoxels) {
            outVoxels.clear();
            if (width <= 0 || height <= 0) return false;
            if (stencilTile.size() < static_cast<size_t>(width * height * 4)) return false;

            int minStencilAlpha = 255;
            int maxStencilAlpha = 0;
            for (size_t i = 0; i + 3 < stencilTile.size(); i += 4) {
                const int a = static_cast<int>(stencilTile[i + 3]);
                minStencilAlpha = std::min(minStencilAlpha, a);
                maxStencilAlpha = std::max(maxStencilAlpha, a);
            }
            const bool useStencilAlphaMask = (maxStencilAlpha - minStencilAlpha) > 16 && maxStencilAlpha > 0;
            auto readLumaAt = [&](int x, int y) -> int {
                const int sx = std::clamp(x, 0, std::max(0, width - 1));
                const int sy = std::clamp(y, 0, std::max(0, height - 1));
                const size_t idx = static_cast<size_t>((sy * width + sx) * 4);
                return (static_cast<int>(stencilTile[idx + 0])
                    + static_cast<int>(stencilTile[idx + 1])
                    + static_cast<int>(stencilTile[idx + 2])) / 3;
            };
            const int backgroundLuma = (
                readLumaAt(0, 0)
                + readLumaAt(width - 1, 0)
                + readLumaAt(0, height - 1)
                + readLumaAt(width - 1, height - 1)
            ) / 4;

            const int halfW = width / 2;
            const int halfH = height / 2;
            outVoxels.reserve(static_cast<size_t>(width * height));
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    const size_t idx = static_cast<size_t>((y * width + x) * 4);
                    int overlayAlpha = 0;
                    if (useStencilAlphaMask) {
                        overlayAlpha = static_cast<int>(stencilTile[idx + 3]);
                    } else {
                        const int stencilLuma = (static_cast<int>(stencilTile[idx + 0])
                            + static_cast<int>(stencilTile[idx + 1])
                            + static_cast<int>(stencilTile[idx + 2])) / 3;
                        const int lumaDelta = std::abs(stencilLuma - backgroundLuma);
                        overlayAlpha = std::clamp(lumaDelta * 2, 0, 255);
                    }
                    if (overlayAlpha < 20) continue;

                    const int localX = x - halfW;
                    const int localY = (height - 1 - y) - halfH;
                    outVoxels.emplace_back(localX, localY, 0);
                }
            }
            return !outVoxels.empty();
        }

        bool buildPickaxeHeadVoxelsFromStencil(BaseSystem& baseSystem,
                                               std::vector<glm::ivec3>& outVoxels) {
            outVoxels.clear();
            if (!baseSystem.world || !baseSystem.renderer || !baseSystem.renderBackend) return false;
            const WorldContext& world = *baseSystem.world;
            const RendererContext& renderer = *baseSystem.renderer;
            if (renderer.atlasTexture == 0
                || renderer.atlasTextureSize.x <= 0
                || renderer.atlasTextureSize.y <= 0
                || renderer.atlasTilesPerRow <= 0
                || renderer.atlasTilesPerCol <= 0
                || renderer.atlasTileSize.x <= 0
                || renderer.atlasTileSize.y <= 0) {
                return false;
            }

            const int stencilTile = resolveMappedAtlasTileIndex(world, "24x24PickaxeStencil");
            if (stencilTile < 0) return false;

            struct PickaxeHeadStencilCache {
                RenderHandle atlasTexture = 0;
                int atlasWidth = 0;
                int atlasHeight = 0;
                int tilesPerRow = 0;
                int tilesPerCol = 0;
                glm::ivec2 tileSize = glm::ivec2(0);
                int stencilTile = -1;
                std::vector<glm::ivec3> voxels;
            };
            static PickaxeHeadStencilCache s_cache;

            const bool cacheValid = !s_cache.voxels.empty()
                && s_cache.atlasTexture == renderer.atlasTexture
                && s_cache.atlasWidth == renderer.atlasTextureSize.x
                && s_cache.atlasHeight == renderer.atlasTextureSize.y
                && s_cache.tilesPerRow == renderer.atlasTilesPerRow
                && s_cache.tilesPerCol == renderer.atlasTilesPerCol
                && s_cache.tileSize == renderer.atlasTileSize
                && s_cache.stencilTile == stencilTile;
            if (cacheValid) {
                outVoxels = s_cache.voxels;
                return true;
            }

            std::vector<unsigned char> atlasPixels;
            if (!baseSystem.renderBackend->readTexture2DRgba(
                    renderer.atlasTexture,
                    renderer.atlasTextureSize.x,
                    renderer.atlasTextureSize.y,
                    atlasPixels)) {
                return false;
            }

            std::vector<unsigned char> stencilPixels;
            if (!extractAtlasTilePixels(
                    atlasPixels,
                    renderer.atlasTextureSize,
                    stencilTile,
                    renderer.atlasTileSize,
                    renderer.atlasTilesPerRow,
                    renderer.atlasTilesPerCol,
                    stencilPixels)) {
                return false;
            }

            std::vector<glm::ivec3> builtVoxels;
            if (!buildStencilHeadVoxelsFromTile(
                    stencilPixels,
                    renderer.atlasTileSize.x,
                    renderer.atlasTileSize.y,
                    builtVoxels)) {
                return false;
            }

            s_cache.atlasTexture = renderer.atlasTexture;
            s_cache.atlasWidth = renderer.atlasTextureSize.x;
            s_cache.atlasHeight = renderer.atlasTextureSize.y;
            s_cache.tilesPerRow = renderer.atlasTilesPerRow;
            s_cache.tilesPerCol = renderer.atlasTilesPerCol;
            s_cache.tileSize = renderer.atlasTileSize;
            s_cache.stencilTile = stencilTile;
            s_cache.voxels = builtVoxels;

            outVoxels = s_cache.voxels;
            return !outVoxels.empty();
        }

        int64_t packInt2(int x, int y) {
            return (static_cast<int64_t>(x) << 32)
                ^ static_cast<uint32_t>(y);
        }

        glm::ivec2 unpackInt2(int64_t packed) {
            return glm::ivec2(
                static_cast<int>(packed >> 32),
                static_cast<int>(static_cast<uint32_t>(packed & 0xffffffffu))
            );
        }

        bool isGemValidPickaxeHeadShape(BaseSystem& baseSystem,
                                        const GemDropState& drop,
                                        const std::vector<glm::ivec3>& stencilHeadVoxels) {
            if (drop.voxelCells.empty() || stencilHeadVoxels.empty()) return false;

            std::unordered_set<int64_t> stencil2D;
            stencil2D.reserve(stencilHeadVoxels.size() * 2u + 1u);
            glm::ivec2 sMin(std::numeric_limits<int>::max());
            glm::ivec2 sMax(std::numeric_limits<int>::min());
            for (const glm::ivec3& cell : stencilHeadVoxels) {
                const glm::ivec2 p(cell.x, cell.y);
                stencil2D.insert(packInt2(p.x, p.y));
                sMin = glm::min(sMin, p);
                sMax = glm::max(sMax, p);
            }
            if (stencil2D.empty()) return false;

            const std::array<std::pair<int, int>, 3> projections = {{
                {0, 1}, // x,y
                {0, 2}, // x,z
                {1, 2}  // y,z
            }};

            int bestOverlap = -1;
            int bestOutside = std::numeric_limits<int>::max();
            int bestGemCount = 0;
            for (const auto& axes : projections) {
                const int axisU = axes.first;
                const int axisV = axes.second;
                std::unordered_set<int64_t> gem2D;
                gem2D.reserve(drop.voxelCells.size() * 2u + 1u);
                glm::ivec2 gMin(std::numeric_limits<int>::max());
                glm::ivec2 gMax(std::numeric_limits<int>::min());

                for (const glm::ivec3& cell : drop.voxelCells) {
                    const int coords[3] = {cell.x, cell.y, cell.z};
                    const glm::ivec2 p(coords[axisU], coords[axisV]);
                    gem2D.insert(packInt2(p.x, p.y));
                    gMin = glm::min(gMin, p);
                    gMax = glm::max(gMax, p);
                }
                if (gem2D.empty()) continue;

                const int dxMin = std::max(-96, sMin.x - gMax.x - 2);
                const int dxMax = std::min(96, sMax.x - gMin.x + 2);
                const int dyMin = std::max(-96, sMin.y - gMax.y - 2);
                const int dyMax = std::min(96, sMax.y - gMin.y + 2);

                int localBestOverlap = -1;
                int localBestOutside = std::numeric_limits<int>::max();
                for (int dy = dyMin; dy <= dyMax; ++dy) {
                    for (int dx = dxMin; dx <= dxMax; ++dx) {
                        int overlap = 0;
                        for (int64_t packed : gem2D) {
                            const glm::ivec2 p = unpackInt2(packed);
                            if (stencil2D.find(packInt2(p.x + dx, p.y + dy)) != stencil2D.end()) {
                                ++overlap;
                            }
                        }
                        const int outside = static_cast<int>(gem2D.size()) - overlap;
                        if (overlap > localBestOverlap
                            || (overlap == localBestOverlap && outside < localBestOutside)) {
                            localBestOverlap = overlap;
                            localBestOutside = outside;
                        }
                    }
                }

                if (localBestOverlap > bestOverlap
                    || (localBestOverlap == bestOverlap && localBestOutside < bestOutside)) {
                    bestOverlap = localBestOverlap;
                    bestOutside = localBestOutside;
                    bestGemCount = static_cast<int>(gem2D.size());
                }
            }

            if (bestOverlap <= 0 || bestGemCount <= 0) return false;
            const int stencilCount = static_cast<int>(stencil2D.size());
            const float coverage = static_cast<float>(bestOverlap) / static_cast<float>(std::max(1, stencilCount));
            const float outsideRatio = static_cast<float>(bestOutside) / static_cast<float>(std::max(1, bestGemCount));
            const float minCoverage = glm::clamp(
                readRegistryFloat(baseSystem, "GemPickaxeHeadMinStencilCoverage", 0.60f),
                0.1f,
                1.0f
            );
            const float maxOutsideRatio = glm::clamp(
                readRegistryFloat(baseSystem, "GemPickaxeHeadMaxOutsideRatio", 0.25f),
                0.0f,
                1.0f
            );
            const int minAbsoluteOverlap = std::max(
                12,
                static_cast<int>(std::floor(static_cast<float>(stencilCount) * 0.30f))
            );

            return coverage >= minCoverage
                && outsideRatio <= maxOutsideRatio
                && bestOverlap >= minAbsoluteOverlap;
        }

        int detectFlowerPetalVariantFromColor(const glm::vec3& color) {
            static const std::array<glm::vec3, 6> kFlowerPalette = {
                glm::vec3(0.97f, 0.48f, 0.72f), // pink
                glm::vec3(0.94f, 0.72f, 0.27f), // marigold
                glm::vec3(0.99f, 0.95f, 0.85f), // cream
                glm::vec3(0.86f, 0.61f, 0.96f), // lilac
                glm::vec3(0.96f, 0.43f, 0.35f), // coral
                glm::vec3(0.95f, 0.83f, 0.28f)  // gold
            };
            int best = 0;
            float bestDist2 = std::numeric_limits<float>::max();
            for (int i = 0; i < static_cast<int>(kFlowerPalette.size()); ++i) {
                const glm::vec3 d = color - kFlowerPalette[static_cast<size_t>(i)];
                const float dist2 = glm::dot(d, d);
                if (dist2 < bestDist2) {
                    bestDist2 = dist2;
                    best = i;
                }
            }
            return best;
        }

        int resolveFlowerPetalsPrototypeID(const std::vector<Entity>& prototypes, int variant, bool preferX) {
            struct Pair { const char* x = nullptr; const char* z = nullptr; };
            static const std::array<Pair, 6> kPairs = {{
                {"StonePebblePetalsPinkTexX", "StonePebblePetalsPinkTexZ"},
                {"StonePebblePetalsMarigoldTexX", "StonePebblePetalsMarigoldTexZ"},
                {"StonePebblePetalsCreamTexX", "StonePebblePetalsCreamTexZ"},
                {"StonePebblePetalsLilacTexX", "StonePebblePetalsLilacTexZ"},
                {"StonePebblePetalsCoralTexX", "StonePebblePetalsCoralTexZ"},
                {"StonePebblePetalsGoldTexX", "StonePebblePetalsGoldTexZ"}
            }};
            if (variant < 0 || variant >= static_cast<int>(kPairs.size())) variant = 0;
            const Pair& pair = kPairs[static_cast<size_t>(variant)];
            const char* primary = preferX ? pair.x : pair.z;
            const char* secondary = preferX ? pair.z : pair.x;
            int fallback = -1;
            for (const auto& proto : prototypes) {
                if (!proto.isBlock) continue;
                if (primary && proto.name == primary) return proto.prototypeID;
                if (fallback < 0 && secondary && proto.name == secondary) fallback = proto.prototypeID;
            }
            return fallback;
        }

        glm::vec3 normalizeOrDefault(const glm::vec3& v, const glm::vec3& fallback) {
            if (glm::length(v) < 1e-4f) return fallback;
            return glm::normalize(v);
        }

        glm::vec3 projectDirectionOnSurface(const glm::vec3& direction,
                                            const glm::vec3& surfaceNormal) {
            glm::vec3 n = normalizeOrDefault(surfaceNormal, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::vec3 projected = direction - n * glm::dot(direction, n);
            if (glm::length(projected) < 1e-4f) {
                projected = glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f));
                if (glm::length(projected) < 1e-4f) {
                    projected = glm::cross(n, glm::vec3(1.0f, 0.0f, 0.0f));
                }
            }
            return normalizeOrDefault(projected, glm::vec3(1.0f, 0.0f, 0.0f));
        }

        bool isWallStonePrototypeID(const std::vector<Entity>& prototypes, int prototypeID) {
            if (prototypeID < 0 || prototypeID >= static_cast<int>(prototypes.size())) return false;
            return isWallStonePrototypeName(prototypes[static_cast<size_t>(prototypeID)].name);
        }

        bool isLeafPrototypeID(const std::vector<Entity>& prototypes, int prototypeID, int leafPrototypeID) {
            if (prototypeID < 0 || prototypeID >= static_cast<int>(prototypes.size())) return false;
            if (leafPrototypeID >= 0 && prototypeID == leafPrototypeID) return true;
            return prototypes[static_cast<size_t>(prototypeID)].name == "Leaf";
        }

        bool isBoulderingAnchorPrototypeID(const std::vector<Entity>& prototypes,
                                           int prototypeID,
                                           bool allowLeafAnchors,
                                           int leafPrototypeID) {
            if (isWallStonePrototypeID(prototypes, prototypeID)) return true;
            if (allowLeafAnchors && isLeafPrototypeID(prototypes, prototypeID, leafPrototypeID)) return true;
            return false;
        }

        int resolveTargetPrototypeID(const BaseSystem& baseSystem,
                                     const LevelContext& level,
                                     const std::vector<Entity>& prototypes,
                                     const PlayerContext& player,
                                     glm::ivec3* outCell = nullptr,
                                     bool* outFromVoxel = nullptr) {
            if (!player.hasBlockTarget) return -1;
            const glm::ivec3 targetCell = glm::ivec3(glm::round(player.targetedBlockPosition));
            if (baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                const uint32_t id = baseSystem.voxelWorld->getBlockWorld(targetCell);
                if (id > 0 && id < prototypes.size()) {
                    if (outCell) *outCell = targetCell;
                    if (outFromVoxel) *outFromVoxel = true;
                    return static_cast<int>(id);
                }
            }

            int worldIndex = player.targetedWorldIndex;
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) return -1;
            const Entity& world = level.worlds[static_cast<size_t>(worldIndex)];
            for (const auto& inst : world.instances) {
                if (glm::distance(inst.position, player.targetedBlockPosition) > POSITION_EPSILON) continue;
                if (inst.prototypeID < 0 || inst.prototypeID >= static_cast<int>(prototypes.size())) continue;
                if (outCell) *outCell = glm::ivec3(glm::round(inst.position));
                if (outFromVoxel) *outFromVoxel = false;
                return inst.prototypeID;
            }
            return -1;
        }

        bool findBlueprintInstanceAtTarget(const LevelContext& level,
                                           const std::vector<Entity>& prototypes,
                                           const PlayerContext& player,
                                           glm::ivec3* outCell) {
            int worldIndex = player.targetedWorldIndex;
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) return false;
            const Entity& world = level.worlds[static_cast<size_t>(worldIndex)];
            for (const auto& inst : world.instances) {
                if (glm::distance(inst.position, player.targetedBlockPosition) > POSITION_EPSILON) continue;
                if (inst.prototypeID < 0 || inst.prototypeID >= static_cast<int>(prototypes.size())) continue;
                const Entity& proto = prototypes[static_cast<size_t>(inst.prototypeID)];
                if (!isBlueprintPrototypeName(proto.name)) continue;
                if (outCell) *outCell = glm::ivec3(glm::round(inst.position));
                return true;
            }
            return false;
        }

        float snapToTwentyFourth(float v) {
            return std::round(v * 24.0f) / 24.0f;
        }

        bool computeGemPlacementWithinCell(const BaseSystem& baseSystem,
                                           const GemDropState& drop,
                                           const glm::ivec3& cell,
                                           const glm::vec3& hitPos,
                                           glm::vec3* outPos) {
            if (!outPos) return false;

            const float renderScale = glm::clamp(readRegistryFloat(baseSystem, "GemDropVisualScale", 1.0f), 0.1f, 100.0f);
            const float gemScale = std::max(0.05f, renderScale);
            constexpr float kMiniVoxelSize = 1.0f / 24.0f;
            const float half = (kMiniVoxelSize * 0.5f) * gemScale;

            float minX = -0.499f;
            float maxX = 0.499f;
            float minZ = -0.499f;
            float maxZ = 0.499f;
            if (!drop.voxelCells.empty()) {
                minX = std::numeric_limits<float>::max();
                maxX = -std::numeric_limits<float>::max();
                minZ = std::numeric_limits<float>::max();
                maxZ = -std::numeric_limits<float>::max();
                for (const glm::ivec3& voxel : drop.voxelCells) {
                    const float centerX = static_cast<float>(voxel.x) * (kMiniVoxelSize * gemScale);
                    const float centerZ = static_cast<float>(voxel.z) * (kMiniVoxelSize * gemScale);
                    minX = std::min(minX, centerX - half);
                    maxX = std::max(maxX, centerX + half);
                    minZ = std::min(minZ, centerZ - half);
                    maxZ = std::max(maxZ, centerZ + half);
                }
            }

            float minOffsetX = -0.5f - minX;
            float maxOffsetX = 0.5f - maxX;
            float minOffsetZ = -0.5f - minZ;
            float maxOffsetZ = 0.5f - maxZ;
            if (minOffsetX > maxOffsetX) {
                // If a raw gem footprint is larger than one block, still allow in-block
                // 1/24 placement control on blueprint surfaces.
                minOffsetX = -11.0f / 24.0f;
                maxOffsetX =  11.0f / 24.0f;
            }
            if (minOffsetZ > maxOffsetZ) {
                minOffsetZ = -11.0f / 24.0f;
                maxOffsetZ =  11.0f / 24.0f;
            }

            float localX = snapToTwentyFourth(hitPos.x - static_cast<float>(cell.x));
            float localZ = snapToTwentyFourth(hitPos.z - static_cast<float>(cell.z));
            localX = glm::clamp(localX, minOffsetX, maxOffsetX);
            localZ = glm::clamp(localZ, minOffsetZ, maxOffsetZ);
            localX = glm::clamp(snapToTwentyFourth(localX), minOffsetX, maxOffsetX);
            localZ = glm::clamp(snapToTwentyFourth(localZ), minOffsetZ, maxOffsetZ);

            *outPos = glm::vec3(
                static_cast<float>(cell.x) + localX,
                static_cast<float>(cell.y),
                static_cast<float>(cell.z) + localZ
            );
            return true;
        }

        bool hasBoulderingAnchorAtCell(const BaseSystem& baseSystem,
                                       const LevelContext& level,
                                       const std::vector<Entity>& prototypes,
                                       const glm::ivec3& cell,
                                       int worldIndexHint,
                                       bool allowLeafAnchors,
                                       int leafPrototypeID) {
            if (baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                const uint32_t id = baseSystem.voxelWorld->getBlockWorld(cell);
                if (id > 0
                    && id < prototypes.size()
                    && isBoulderingAnchorPrototypeID(
                        prototypes,
                        static_cast<int>(id),
                        allowLeafAnchors,
                        leafPrototypeID)) {
                    return true;
                }
            }
            if (worldIndexHint < 0 || worldIndexHint >= static_cast<int>(level.worlds.size())) return false;
            const Entity& world = level.worlds[static_cast<size_t>(worldIndexHint)];
            const glm::vec3 cellPos = glm::vec3(cell);
            for (const auto& inst : world.instances) {
                if (glm::distance(inst.position, cellPos) > POSITION_EPSILON) continue;
                if (isBoulderingAnchorPrototypeID(prototypes, inst.prototypeID, allowLeafAnchors, leafPrototypeID)) return true;
            }
            return false;
        }
