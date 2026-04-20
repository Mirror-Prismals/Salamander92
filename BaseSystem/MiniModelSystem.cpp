#pragma once

#include "Host/PlatformInput.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "../stb_image.h"

namespace MiniModelSystemLogic {

namespace {
    namespace fs = std::filesystem;

    constexpr int kGridPerBlock = 24;
    constexpr float kMicroUnit = 1.0f / 24.0f;
    constexpr float kRayStep = 0.5f / 24.0f;
    constexpr float kPreviewAlpha = 0.62f;
    constexpr float kShellAlpha = 0.14f;
    constexpr float kShellOffset = 0.0025f;
    constexpr float kSelectionEpsilon = 0.05f;

    struct BenchRayHit {
        bool valid = false;
        bool occupied = false;
        bool benchSurface = false;
        int workbenchIndex = -1;
        glm::ivec3 cell = glm::ivec3(0);
        glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 point = glm::vec3(0.0f);
        float distance = std::numeric_limits<float>::max();
    };

    struct AabbRayRange {
        bool hit = false;
        bool startedInside = false;
        float entryT = 0.0f;
        float exitT = 0.0f;
        glm::vec3 entryNormal = glm::vec3(0.0f);
        glm::vec3 exitNormal = glm::vec3(0.0f);
    };

    bool getRegistryBool(const BaseSystem& baseSystem, const std::string& key, bool fallback) {
        if (!baseSystem.registry) return fallback;
        auto it = baseSystem.registry->find(key);
        if (it == baseSystem.registry->end()) return fallback;
        if (std::holds_alternative<bool>(it->second)) return std::get<bool>(it->second);
        if (!std::holds_alternative<std::string>(it->second)) return fallback;
        std::string raw = std::get<std::string>(it->second);
        std::transform(raw.begin(), raw.end(), raw.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (raw == "1" || raw == "true" || raw == "yes" || raw == "on") return true;
        if (raw == "0" || raw == "false" || raw == "no" || raw == "off") return false;
        return fallback;
    }

    std::string getRegistryString(const BaseSystem& baseSystem,
                                  const std::string& key,
                                  const std::string& fallback = "") {
        if (!baseSystem.registry) return fallback;
        auto it = baseSystem.registry->find(key);
        if (it == baseSystem.registry->end()) return fallback;
        if (!std::holds_alternative<std::string>(it->second)) return fallback;
        return std::get<std::string>(it->second);
    }

    bool isDevLevel(const BaseSystem& baseSystem) {
        const std::string levelKey = getRegistryString(baseSystem, "level", "");
        return levelKey == "dev" || levelKey == "dev_level";
    }

    bool isMiniModelModeActive(const BaseSystem& baseSystem) {
        return baseSystem.player && baseSystem.player->buildMode == BuildModeType::MiniModel;
    }

    glm::ivec3 gridSizeForWorkbench(const MiniModelWorkbenchDef& def) {
        return glm::max(def.sizeBlocks, glm::ivec3(1)) * kGridPerBlock;
    }

    int linearIndex(const glm::ivec3& cell, const glm::ivec3& dims) {
        return (cell.y * dims.x + cell.x) * dims.z + cell.z;
    }

    bool inBounds(const glm::ivec3& cell, const glm::ivec3& dims) {
        return cell.x >= 0 && cell.y >= 0 && cell.z >= 0
            && cell.x < dims.x && cell.y < dims.y && cell.z < dims.z;
    }

    glm::vec3 workbenchWorldMin(const MiniModelWorkbenchDef& def) {
        return glm::vec3(def.minCorner) - glm::vec3(0.5f);
    }

    glm::vec3 workbenchWorldMax(const MiniModelWorkbenchDef& def) {
        return workbenchWorldMin(def) + glm::vec3(glm::max(def.sizeBlocks, glm::ivec3(1)));
    }

    glm::vec3 workbenchWorldCenter(const MiniModelWorkbenchDef& def) {
        return workbenchWorldMin(def) + 0.5f * glm::vec3(glm::max(def.sizeBlocks, glm::ivec3(1)));
    }

    uint32_t packColorOpaque(const glm::vec3& color) {
        auto clampByte = [](float v) -> uint32_t {
            int iv = static_cast<int>(std::round(v * 255.0f));
            iv = std::clamp(iv, 0, 255);
            return static_cast<uint32_t>(iv);
        };
        const uint32_t r = clampByte(color.r);
        const uint32_t g = clampByte(color.g);
        const uint32_t b = clampByte(color.b);
        return 0xff000000u | (r << 16u) | (g << 8u) | b;
    }

    glm::vec3 unpackColorOpaque(uint32_t packed) {
        if (packed == 0u) return glm::vec3(1.0f);
        return glm::vec3(
            static_cast<float>((packed >> 16u) & 0xffu) / 255.0f,
            static_cast<float>((packed >> 8u) & 0xffu) / 255.0f,
            static_cast<float>(packed & 0xffu) / 255.0f
        );
    }

    glm::vec3 axisNormalFromVector(const glm::vec3& v) {
        if (std::abs(v.x) >= std::abs(v.y) && std::abs(v.x) >= std::abs(v.z)) {
            return glm::vec3(v.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
        }
        if (std::abs(v.y) >= std::abs(v.x) && std::abs(v.y) >= std::abs(v.z)) {
            return glm::vec3(0.0f, v.y >= 0.0f ? 1.0f : -1.0f, 0.0f);
        }
        return glm::vec3(0.0f, 0.0f, v.z >= 0.0f ? 1.0f : -1.0f);
    }

    int faceTypeFromNormal(const glm::vec3& normal) {
        const glm::vec3 n = axisNormalFromVector(normal);
        if (n.x > 0.5f) return 0;
        if (n.x < -0.5f) return 1;
        if (n.y > 0.5f) return 2;
        if (n.y < -0.5f) return 3;
        if (n.z > 0.5f) return 4;
        return 5;
    }

    glm::vec3 normalFromFaceType(int faceType) {
        switch (faceType) {
            case 0: return glm::vec3(1.0f, 0.0f, 0.0f);
            case 1: return glm::vec3(-1.0f, 0.0f, 0.0f);
            case 2: return glm::vec3(0.0f, 1.0f, 0.0f);
            case 3: return glm::vec3(0.0f, -1.0f, 0.0f);
            case 4: return glm::vec3(0.0f, 0.0f, 1.0f);
            default: return glm::vec3(0.0f, 0.0f, -1.0f);
        }
    }

    glm::vec2 faceScaleForBounds(int faceType, const glm::vec3& boundsBlocks) {
        if (faceType == 0 || faceType == 1) return glm::vec2(boundsBlocks.z, boundsBlocks.y);
        if (faceType == 2 || faceType == 3) return glm::vec2(boundsBlocks.x, boundsBlocks.z);
        return glm::vec2(boundsBlocks.x, boundsBlocks.y);
    }

    void appendFace(std::array<std::vector<FaceInstanceRenderData>, 6>& outFaces,
                    int faceType,
                    const glm::vec3& position,
                    const glm::vec3& color,
                    float alpha,
                    const glm::vec2& scale) {
        if (faceType < 0 || faceType >= static_cast<int>(outFaces.size())) return;
        FaceInstanceRenderData face;
        face.position = position;
        face.color = color;
        face.tileIndex = -1;
        face.alpha = alpha;
        face.ao = glm::vec4(1.0f);
        face.scale = scale;
        face.uvScale = glm::vec2(1.0f);
        outFaces[static_cast<size_t>(faceType)].push_back(face);
    }

    void faceAxes(const glm::vec3& normal, glm::vec3& outU, glm::vec3& outV) {
        const glm::vec3 n = axisNormalFromVector(normal);
        if (n.x > 0.5f) {
            outU = glm::vec3(0.0f, 0.0f, 1.0f);
            outV = glm::vec3(0.0f, 1.0f, 0.0f);
            return;
        }
        if (n.x < -0.5f) {
            outU = glm::vec3(0.0f, 0.0f, -1.0f);
            outV = glm::vec3(0.0f, 1.0f, 0.0f);
            return;
        }
        if (n.y > 0.5f) {
            outU = glm::vec3(1.0f, 0.0f, 0.0f);
            outV = glm::vec3(0.0f, 0.0f, -1.0f);
            return;
        }
        if (n.y < -0.5f) {
            outU = glm::vec3(1.0f, 0.0f, 0.0f);
            outV = glm::vec3(0.0f, 0.0f, 1.0f);
            return;
        }
        if (n.z > 0.5f) {
            outU = glm::vec3(-1.0f, 0.0f, 0.0f);
            outV = glm::vec3(0.0f, 1.0f, 0.0f);
            return;
        }
        outU = glm::vec3(1.0f, 0.0f, 0.0f);
        outV = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    bool computeFacePixelFromHit(const glm::vec3& cellCenter,
                                 const glm::vec3& normal,
                                 const glm::vec3& hitPos,
                                 int& outX,
                                 int& outY) {
        glm::vec3 axisU(1.0f, 0.0f, 0.0f);
        glm::vec3 axisV(0.0f, 1.0f, 0.0f);
        faceAxes(normal, axisU, axisV);
        const glm::vec3 local = hitPos - cellCenter;
        float u = glm::dot(local, axisU) + 0.5f;
        float v = glm::dot(local, axisV) + 0.5f;
        u = glm::clamp(u, 0.0f, 0.999999f);
        v = glm::clamp(v, 0.0f, 0.999999f);
        outX = std::clamp(static_cast<int>(std::floor(u * 24.0f)), 0, 23);
        outY = std::clamp(static_cast<int>(std::floor(v * 24.0f)), 0, 23);
        return true;
    }

    bool rayAabbRange(const glm::vec3& origin,
                      const glm::vec3& dir,
                      const glm::vec3& minBounds,
                      const glm::vec3& maxBounds,
                      AabbRayRange& outRange) {
        outRange = AabbRayRange{};
        float tMin = 0.0f;
        float tMax = std::numeric_limits<float>::max();
        glm::vec3 entryNormal(0.0f);
        glm::vec3 exitNormal(0.0f);
        bool startedInside = true;

        for (int axis = 0; axis < 3; ++axis) {
            const float originAxis = origin[axis];
            const float dirAxis = dir[axis];
            const float minAxis = minBounds[axis];
            const float maxAxis = maxBounds[axis];
            if (originAxis < minAxis || originAxis > maxAxis) {
                startedInside = false;
            }

            if (std::abs(dirAxis) < 1e-6f) {
                if (originAxis < minAxis || originAxis > maxAxis) return false;
                continue;
            }

            float t1 = (minAxis - originAxis) / dirAxis;
            float t2 = (maxAxis - originAxis) / dirAxis;
            glm::vec3 normal1(0.0f);
            glm::vec3 normal2(0.0f);
            normal1[axis] = -1.0f;
            normal2[axis] = 1.0f;
            if (t1 > t2) {
                std::swap(t1, t2);
                std::swap(normal1, normal2);
            }
            if (t1 > tMin) {
                tMin = t1;
                entryNormal = normal1;
            }
            if (t2 < tMax) {
                tMax = t2;
                exitNormal = normal2;
            }
            if (tMin > tMax) return false;
        }

        if (tMax < 0.0f) return false;

        outRange.hit = true;
        outRange.startedInside = startedInside;
        outRange.entryT = std::max(0.0f, tMin);
        outRange.exitT = tMax;
        outRange.entryNormal = entryNormal;
        outRange.exitNormal = exitNormal;
        return true;
    }

    bool findBlockInstanceAt(const LevelContext& level,
                             const std::vector<Entity>& prototypes,
                             int worldIndex,
                             const glm::vec3& position,
                             const EntityInstance*& outInst,
                             const Entity*& outProto) {
        outInst = nullptr;
        outProto = nullptr;
        if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) return false;
        const Entity& world = level.worlds[static_cast<size_t>(worldIndex)];
        for (const EntityInstance& inst : world.instances) {
            if (glm::distance(inst.position, position) > kSelectionEpsilon) continue;
            if (inst.prototypeID < 0 || inst.prototypeID >= static_cast<int>(prototypes.size())) continue;
            const Entity& proto = prototypes[static_cast<size_t>(inst.prototypeID)];
            if (!proto.isBlock) continue;
            outInst = &inst;
            outProto = &proto;
            return true;
        }
        return false;
    }

    glm::ivec3 pointToWorkbenchCell(const MiniModelWorkbenchDef& def, const glm::vec3& point) {
        const glm::vec3 local = (point - workbenchWorldMin(def)) * static_cast<float>(kGridPerBlock);
        return glm::ivec3(
            static_cast<int>(std::floor(local.x)),
            static_cast<int>(std::floor(local.y)),
            static_cast<int>(std::floor(local.z))
        );
    }

    bool cellOccupied(const MiniModelWorkbenchState& bench, const glm::ivec3& cell) {
        const glm::ivec3 dims = gridSizeForWorkbench(bench.def);
        if (!inBounds(cell, dims)) return false;
        const int idx = linearIndex(cell, dims);
        return idx >= 0
            && idx < static_cast<int>(bench.cells.size())
            && bench.cells[static_cast<size_t>(idx)] != 0u;
    }

    uint32_t getCellPacked(const MiniModelWorkbenchState& bench, const glm::ivec3& cell) {
        const glm::ivec3 dims = gridSizeForWorkbench(bench.def);
        if (!inBounds(cell, dims)) return 0u;
        const int idx = linearIndex(cell, dims);
        if (idx < 0 || idx >= static_cast<int>(bench.cells.size())) return 0u;
        return bench.cells[static_cast<size_t>(idx)];
    }

    void setCellPacked(MiniModelWorkbenchState& bench, const glm::ivec3& cell, uint32_t packed) {
        const glm::ivec3 dims = gridSizeForWorkbench(bench.def);
        if (!inBounds(cell, dims)) return;
        const int idx = linearIndex(cell, dims);
        if (idx < 0 || idx >= static_cast<int>(bench.cells.size())) return;
        bench.cells[static_cast<size_t>(idx)] = packed;
        bench.dirty = true;
        bench.facesDirty = true;
    }

    glm::vec3 localCenterForCell(const glm::ivec3& cell, const glm::ivec3& dims) {
        const glm::vec3 dimsWorld = glm::vec3(dims) * kMicroUnit;
        return -0.5f * dimsWorld
            + glm::vec3(cell) * kMicroUnit
            + glm::vec3(0.5f * kMicroUnit);
    }

    template <typename OccupancyFn, typename ColorFn>
    void bakeFaceBatches(const glm::ivec3& dims,
                         OccupancyFn&& isOccupied,
                         ColorFn&& readPacked,
                         std::array<std::vector<FaceInstanceRenderData>, 6>& outFaces) {
        for (auto& batch : outFaces) batch.clear();
        const std::array<glm::ivec3, 6> kOffsets = {
            glm::ivec3(1, 0, 0),
            glm::ivec3(-1, 0, 0),
            glm::ivec3(0, 1, 0),
            glm::ivec3(0, -1, 0),
            glm::ivec3(0, 0, 1),
            glm::ivec3(0, 0, -1)
        };
        for (int y = 0; y < dims.y; ++y) {
            for (int x = 0; x < dims.x; ++x) {
                for (int z = 0; z < dims.z; ++z) {
                    const glm::ivec3 cell(x, y, z);
                    if (!isOccupied(cell)) continue;
                    const uint32_t packed = readPacked(cell);
                    const glm::vec3 color = unpackColorOpaque(packed);
                    const glm::vec3 localCenter = localCenterForCell(cell, dims);
                    for (int faceType = 0; faceType < 6; ++faceType) {
                        const glm::ivec3 neighbor = cell + kOffsets[static_cast<size_t>(faceType)];
                        if (inBounds(neighbor, dims) && isOccupied(neighbor)) continue;
                        FaceInstanceRenderData face;
                        face.position = localCenter + normalFromFaceType(faceType) * (0.5f * kMicroUnit);
                        face.color = color;
                        face.tileIndex = -1;
                        face.alpha = 1.0f;
                        face.ao = glm::vec4(1.0f);
                        face.scale = glm::vec2(kMicroUnit);
                        face.uvScale = glm::vec2(1.0f);
                        outFaces[static_cast<size_t>(faceType)].push_back(face);
                    }
                }
            }
        }
    }

    void ensureWorkbenchInitialized(MiniModelWorkbenchState& bench) {
        if (bench.initialized) return;
        const glm::ivec3 dims = gridSizeForWorkbench(bench.def);
        const size_t total = static_cast<size_t>(dims.x) * static_cast<size_t>(dims.y) * static_cast<size_t>(dims.z);
        bench.cells.assign(total, 0u);
        fs::path outPath(bench.def.outputPath);
        if (!bench.def.outputPath.empty() && fs::exists(outPath) && fs::is_regular_file(outPath)) {
            std::ifstream file(outPath);
            if (file.is_open()) {
                json data;
                try {
                    data = json::parse(file);
                } catch (...) {
                    data = json();
                }
                if (data.value("format", "") == "mini_palette_rle_v1") {
                    glm::ivec3 fileGridSize = dims;
                    if (data.contains("gridSize") && data["gridSize"].is_array() && data["gridSize"].size() == 3) {
                        fileGridSize = glm::ivec3(
                            data["gridSize"][0].get<int>(),
                            data["gridSize"][1].get<int>(),
                            data["gridSize"][2].get<int>()
                        );
                    }
                    if (fileGridSize == dims) {
                        std::vector<uint32_t> palettePacked;
                        if (data.contains("palette") && data["palette"].is_array()) {
                            for (const auto& entry : data["palette"]) {
                                glm::vec3 color(1.0f);
                                if (entry.contains("color") && entry["color"].is_array() && entry["color"].size() == 3) {
                                    color = glm::vec3(
                                        entry["color"][0].get<float>(),
                                        entry["color"][1].get<float>(),
                                        entry["color"][2].get<float>()
                                    );
                                }
                                palettePacked.push_back(packColorOpaque(color));
                            }
                        }
                        size_t cursor = 0;
                        if (data.contains("runs") && data["runs"].is_array()) {
                            for (const auto& run : data["runs"]) {
                                const int paletteIndex = run.value("palette", -1);
                                const int count = std::max(0, run.value("count", 0));
                                for (int i = 0; i < count && cursor < bench.cells.size(); ++i, ++cursor) {
                                    if (paletteIndex >= 0 && paletteIndex < static_cast<int>(palettePacked.size())) {
                                        bench.cells[cursor] = palettePacked[static_cast<size_t>(paletteIndex)];
                                    }
                                }
                                if (cursor >= bench.cells.size()) break;
                            }
                        }
                    }
                }
            }
        }
        bench.initialized = true;
        bench.dirty = false;
        bench.facesDirty = true;
    }

    void rebuildWorkbenchFaces(MiniModelWorkbenchState& bench) {
        ensureWorkbenchInitialized(bench);
        const glm::ivec3 dims = gridSizeForWorkbench(bench.def);
        bakeFaceBatches(
            dims,
            [&](const glm::ivec3& cell) { return cellOccupied(bench, cell); },
            [&](const glm::ivec3& cell) { return getCellPacked(bench, cell); },
            bench.localFaces
        );
        bench.facesDirty = false;
    }

    bool ensureAtlasPixelsLoaded(BaseSystem& baseSystem) {
        if (!baseSystem.miniModel) return false;
        MiniModelContext& ctx = *baseSystem.miniModel;
        const std::string atlasPath = getRegistryString(baseSystem, "AtlasTexturePath", "Procedures/assets/atlas_v10.png");
        if (ctx.atlasPixelsLoaded && ctx.atlasPixelsPath == atlasPath && !ctx.atlasPixels.empty()) {
            return true;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* pixels = stbi_load(atlasPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels && atlasPath != "Procedures/assets/atlas_v10.png") {
            pixels = stbi_load("Procedures/assets/atlas_v10.png", &width, &height, &channels, STBI_rgb_alpha);
        }
        if (!pixels || width <= 0 || height <= 0) {
            if (pixels) stbi_image_free(pixels);
            ctx.atlasPixelsLoaded = false;
            ctx.atlasPixels.clear();
            ctx.atlasPixelsSize = glm::ivec2(0);
            return false;
        }

        ctx.atlasPixels.assign(
            pixels,
            pixels + static_cast<size_t>(width * height * 4)
        );
        stbi_image_free(pixels);
        ctx.atlasPixelsLoaded = true;
        ctx.atlasPixelsPath = atlasPath;
        ctx.atlasPixelsSize = glm::ivec2(width, height);
        return true;
    }

    bool sampleAtlasTileColor(BaseSystem& baseSystem,
                              int tileIndex,
                              int pixelX,
                              int pixelY,
                              glm::vec3& outColor) {
        if (!baseSystem.world || !baseSystem.miniModel) return false;
        if (!ensureAtlasPixelsLoaded(baseSystem)) return false;
        MiniModelContext& ctx = *baseSystem.miniModel;
        const WorldContext& world = *baseSystem.world;
        const glm::ivec2 tileSize = world.atlasTileSize;
        if (tileSize.x <= 0 || tileSize.y <= 0
            || world.atlasTilesPerRow <= 0 || world.atlasTilesPerCol <= 0
            || tileIndex < 0) return false;

        pixelX = std::clamp(pixelX, 0, tileSize.x - 1);
        pixelY = std::clamp(pixelY, 0, tileSize.y - 1);
        const int tileX = (tileIndex % world.atlasTilesPerRow) * tileSize.x;
        const int tileRowFromTop = tileIndex / world.atlasTilesPerRow;
        const int tileRowFromBottom = world.atlasTilesPerCol - 1 - tileRowFromTop;
        const int tileY = tileRowFromBottom * tileSize.y;
        if (tileX < 0 || tileY < 0
            || tileX + tileSize.x > ctx.atlasPixelsSize.x
            || tileY + tileSize.y > ctx.atlasPixelsSize.y) {
            return false;
        }

        const int sampleX = tileX + pixelX;
        const int sampleY = tileY + pixelY;
        const size_t idx = static_cast<size_t>((sampleY * ctx.atlasPixelsSize.x + sampleX) * 4);
        if (idx + 3 >= ctx.atlasPixels.size()) return false;
        const float alpha = static_cast<float>(ctx.atlasPixels[idx + 3]) / 255.0f;
        if (alpha <= 0.01f) return false;
        outColor = glm::vec3(
            static_cast<float>(ctx.atlasPixels[idx + 0]) / 255.0f,
            static_cast<float>(ctx.atlasPixels[idx + 1]) / 255.0f,
            static_cast<float>(ctx.atlasPixels[idx + 2]) / 255.0f
        );
        return true;
    }

    bool sampleColorFromRegularBlock(BaseSystem& baseSystem,
                                     std::vector<Entity>& prototypes,
                                     glm::vec3& outColor) {
        if (!baseSystem.player || !baseSystem.level || !baseSystem.world) return false;
        PlayerContext& player = *baseSystem.player;
        LevelContext& level = *baseSystem.level;
        if (!player.hasBlockTarget || player.targetedWorldIndex < 0) return false;

        const Entity* targetProto = nullptr;
        glm::vec3 rawColor(1.0f);
        if (baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
            const glm::ivec3 targetCell = glm::ivec3(glm::round(player.targetedBlockPosition));
            const uint32_t blockID = baseSystem.voxelWorld->getBlockWorld(targetCell);
            if (blockID > 0u && blockID < prototypes.size()) {
                targetProto = &prototypes[blockID];
                rawColor = unpackColorOpaque(baseSystem.voxelWorld->getColorWorld(targetCell));
            }
        }

        const EntityInstance* targetInst = nullptr;
        if (!targetProto) {
            findBlockInstanceAt(level, prototypes, player.targetedWorldIndex, player.targetedBlockPosition, targetInst, targetProto);
            if (targetInst) rawColor = targetInst->color;
        }

        if (!targetProto) return false;
        if (!targetProto->useTexture) {
            outColor = rawColor;
            return true;
        }

        const int faceType = faceTypeFromNormal(player.targetedBlockNormal);
        const int tileIndex = RenderInitSystemLogic::FaceTileIndexFor(baseSystem.world.get(), *targetProto, faceType);
        if (tileIndex < 0) {
            outColor = rawColor;
            return true;
        }

        int pixelX = 0;
        int pixelY = 0;
        if (!computeFacePixelFromHit(player.targetedBlockPosition, player.targetedBlockNormal, player.targetedBlockHitPosition, pixelX, pixelY)) {
            return false;
        }
        if (!sampleAtlasTileColor(baseSystem, tileIndex, pixelX, pixelY, outColor)) {
            outColor = rawColor;
            return true;
        }
        return true;
    }

    bool loadWorkbenchConfig(MiniModelContext& ctx, const fs::path& path) {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        json data;
        try {
            data = json::parse(file);
        } catch (...) {
            return false;
        }
        if (!data.value("enabled", true)) return true;

        MiniModelWorkbenchState bench;
        bench.def.id = data.value("id", path.stem().string());
        bench.def.worldName = data.value("world", "");
        if (data.contains("bounds") && data["bounds"].is_object()) {
            const json& bounds = data["bounds"];
            if (bounds.contains("min") && bounds["min"].is_array() && bounds["min"].size() == 3) {
                bench.def.minCorner = glm::ivec3(
                    bounds["min"][0].get<int>(),
                    bounds["min"][1].get<int>(),
                    bounds["min"][2].get<int>()
                );
            }
            if (bounds.contains("size") && bounds["size"].is_array() && bounds["size"].size() == 3) {
                bench.def.sizeBlocks = glm::ivec3(
                    bounds["size"][0].get<int>(),
                    bounds["size"][1].get<int>(),
                    bounds["size"][2].get<int>()
                );
            }
        }
        bench.def.sizeBlocks = glm::max(bench.def.sizeBlocks, glm::ivec3(1));
        bench.def.outputPath = data.value("output", "");
        if (bench.def.id.empty() || bench.def.worldName.empty() || bench.def.outputPath.empty()) {
            return false;
        }
        ctx.workbenches.push_back(std::move(bench));
        return true;
    }

    void ensureWorkbenchConfigLoaded(BaseSystem& baseSystem) {
        if (!baseSystem.level || !baseSystem.miniModel) return;
        MiniModelContext& ctx = *baseSystem.miniModel;
        if (ctx.level == baseSystem.level.get() && ctx.configLoaded) return;

        const glm::vec3 preservedColor = ctx.activeColor;
        ctx = MiniModelContext{};
        ctx.level = baseSystem.level.get();
        ctx.activeColor = preservedColor;

        const fs::path dir("Procedures/MiniModels");
        if (fs::exists(dir)) {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
                (void)loadWorkbenchConfig(ctx, entry.path());
            }
        }
        ctx.configLoaded = true;
    }

    void updateWorkbenchWorldIndices(LevelContext& level, MiniModelContext& ctx) {
        for (MiniModelWorkbenchState& bench : ctx.workbenches) {
            bench.def.worldIndex = -1;
            for (size_t i = 0; i < level.worlds.size(); ++i) {
                if (level.worlds[i].name == bench.def.worldName) {
                    bench.def.worldIndex = static_cast<int>(i);
                    break;
                }
            }
        }
    }

    void invalidateAssetsForOutput(MiniModelContext& ctx,
                                   const std::string& outputPath,
                                   const std::string& assetId) {
        for (auto it = ctx.assets.begin(); it != ctx.assets.end();) {
            const MiniModelAssetRecord& asset = it->second;
            if (asset.sourcePath == outputPath || (!assetId.empty() && asset.id == assetId)) {
                it = ctx.assets.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool exportWorkbenchAsset(MiniModelContext& ctx, MiniModelWorkbenchState& bench) {
        ensureWorkbenchInitialized(bench);
        const glm::ivec3 dims = gridSizeForWorkbench(bench.def);
        const size_t total = bench.cells.size();
        std::vector<int> stream;
        stream.reserve(total);
        std::vector<glm::vec3> palette;
        std::vector<uint32_t> palettePacked;

        auto paletteIndexFor = [&](uint32_t packed) -> int {
            for (size_t i = 0; i < palettePacked.size(); ++i) {
                if (palettePacked[i] == packed) return static_cast<int>(i);
            }
            palettePacked.push_back(packed);
            palette.push_back(unpackColorOpaque(packed));
            return static_cast<int>(palettePacked.size() - 1);
        };

        for (uint32_t packed : bench.cells) {
            if (packed == 0u) {
                stream.push_back(-1);
            } else {
                stream.push_back(paletteIndexFor(packed));
            }
        }

        json output;
        output["format"] = "mini_palette_rle_v1";
        output["id"] = bench.def.id;
        output["boundsBlocks"] = {
            bench.def.sizeBlocks.x,
            bench.def.sizeBlocks.y,
            bench.def.sizeBlocks.z
        };
        output["gridPerBlock"] = kGridPerBlock;
        output["gridSize"] = { dims.x, dims.y, dims.z };

        json paletteJson = json::array();
        for (const glm::vec3& color : palette) {
            paletteJson.push_back({
                {"color", { color.x, color.y, color.z }}
            });
        }
        output["palette"] = paletteJson;

        json runs = json::array();
        if (!stream.empty()) {
            int current = stream[0];
            int count = 1;
            for (size_t i = 1; i < stream.size(); ++i) {
                if (stream[i] == current) {
                    ++count;
                } else {
                    runs.push_back({{"palette", current}, {"count", count}});
                    current = stream[i];
                    count = 1;
                }
            }
            runs.push_back({{"palette", current}, {"count", count}});
        }
        output["runs"] = runs;

        fs::path outPath(bench.def.outputPath);
        if (outPath.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(outPath.parent_path(), ec);
        }
        std::ofstream file(outPath);
        if (!file.is_open()) return false;
        file << output.dump(2);
        file.close();
        bench.dirty = false;
        invalidateAssetsForOutput(ctx, outPath.string(), bench.def.id);
        return true;
    }

    bool resolveMiniModelAssetPath(const std::string& key, std::string& outPath) {
        std::vector<fs::path> candidates;
        if (!key.empty()) {
            candidates.emplace_back(key);
            if (key.size() >= 5 && key.compare(key.size() - 5, 5, ".json") == 0) {
                candidates.emplace_back(fs::path("MiniModels") / key);
            } else {
                candidates.emplace_back(fs::path("MiniModels") / (key + ".json"));
                candidates.emplace_back(fs::path("MiniModels") / key);
            }
        }
        std::error_code ec;
        for (const fs::path& path : candidates) {
            if (fs::exists(path, ec) && !ec && fs::is_regular_file(path, ec) && !ec) {
                outPath = path.string();
                return true;
            }
            ec.clear();
        }
        return false;
    }

    bool loadAssetRecord(BaseSystem& baseSystem,
                         const std::string& assetKey,
                         MiniModelAssetRecord& outAsset) {
        std::string assetPath;
        if (!resolveMiniModelAssetPath(assetKey, assetPath)) return false;

        std::ifstream file(assetPath);
        if (!file.is_open()) return false;
        json data;
        try {
            data = json::parse(file);
        } catch (...) {
            return false;
        }
        if (data.value("format", "") != "mini_palette_rle_v1") return false;

        MiniModelAssetRecord asset;
        asset.id = data.value("id", assetKey);
        asset.sourcePath = assetPath;
        if (data.contains("boundsBlocks") && data["boundsBlocks"].is_array() && data["boundsBlocks"].size() == 3) {
            asset.sizeBlocks = glm::ivec3(
                data["boundsBlocks"][0].get<int>(),
                data["boundsBlocks"][1].get<int>(),
                data["boundsBlocks"][2].get<int>()
            );
        }
        if (data.contains("gridPerBlock")) asset.gridPerBlock = std::max(1, data["gridPerBlock"].get<int>());
        if (data.contains("gridSize") && data["gridSize"].is_array() && data["gridSize"].size() == 3) {
            asset.gridSize = glm::ivec3(
                data["gridSize"][0].get<int>(),
                data["gridSize"][1].get<int>(),
                data["gridSize"][2].get<int>()
            );
        } else {
            asset.gridSize = glm::max(asset.sizeBlocks, glm::ivec3(1)) * asset.gridPerBlock;
        }

        std::vector<uint32_t> palettePacked;
        if (data.contains("palette") && data["palette"].is_array()) {
            for (const auto& entry : data["palette"]) {
                glm::vec3 color(1.0f);
                if (entry.contains("color") && entry["color"].is_array() && entry["color"].size() == 3) {
                    color = glm::vec3(
                        entry["color"][0].get<float>(),
                        entry["color"][1].get<float>(),
                        entry["color"][2].get<float>()
                    );
                }
                palettePacked.push_back(packColorOpaque(color));
            }
        }

        const size_t total = static_cast<size_t>(asset.gridSize.x)
            * static_cast<size_t>(asset.gridSize.y)
            * static_cast<size_t>(asset.gridSize.z);
        std::vector<uint32_t> dense(total, 0u);
        size_t cursor = 0;
        if (data.contains("runs") && data["runs"].is_array()) {
            for (const auto& run : data["runs"]) {
                const int paletteIndex = run.value("palette", -1);
                const int count = std::max(0, run.value("count", 0));
                for (int i = 0; i < count && cursor < dense.size(); ++i, ++cursor) {
                    if (paletteIndex >= 0 && paletteIndex < static_cast<int>(palettePacked.size())) {
                        dense[cursor] = palettePacked[static_cast<size_t>(paletteIndex)];
                    }
                }
                if (cursor >= dense.size()) break;
            }
        }

        bakeFaceBatches(
            asset.gridSize,
            [&](const glm::ivec3& cell) {
                if (!inBounds(cell, asset.gridSize)) return false;
                return dense[static_cast<size_t>(linearIndex(cell, asset.gridSize))] != 0u;
            },
            [&](const glm::ivec3& cell) {
                return dense[static_cast<size_t>(linearIndex(cell, asset.gridSize))];
            },
            asset.localFaces
        );

        asset.loaded = true;
        asset.loadFailed = false;
        outAsset = std::move(asset);
        return true;
    }

    void ensureReferencedAssetsLoaded(BaseSystem& baseSystem, std::vector<Entity>& prototypes) {
        if (!baseSystem.miniModel) return;
        MiniModelContext& ctx = *baseSystem.miniModel;
        for (const Entity& proto : prototypes) {
            if (proto.miniModelAsset.empty()) continue;
            auto it = ctx.assets.find(proto.miniModelAsset);
            if (it != ctx.assets.end() && (it->second.loaded || it->second.loadFailed)) continue;
            MiniModelAssetRecord record;
            if (loadAssetRecord(baseSystem, proto.miniModelAsset, record)) {
                ctx.assets[proto.miniModelAsset] = std::move(record);
            } else {
                MiniModelAssetRecord failed;
                failed.id = proto.miniModelAsset;
                failed.loadFailed = true;
                ctx.assets[proto.miniModelAsset] = std::move(failed);
            }
        }
    }

    void appendTranslatedFaces(const std::array<std::vector<FaceInstanceRenderData>, 6>& localFaces,
                               const glm::vec3& worldCenter,
                               float scale,
                               std::array<std::vector<FaceInstanceRenderData>, 6>& outFaces) {
        for (size_t faceType = 0; faceType < localFaces.size(); ++faceType) {
            const auto& src = localFaces[faceType];
            auto& dst = outFaces[faceType];
            dst.reserve(dst.size() + src.size());
            for (const FaceInstanceRenderData& local : src) {
                FaceInstanceRenderData face = local;
                face.position = worldCenter + local.position * scale;
                face.scale *= scale;
                dst.push_back(face);
            }
        }
    }

    void buildShellFaces(const MiniModelWorkbenchState& bench,
                         bool active,
                         std::array<std::vector<FaceInstanceRenderData>, 6>& outFaces) {
        const glm::vec3 center = workbenchWorldCenter(bench.def);
        const glm::vec3 dims = glm::vec3(glm::max(bench.def.sizeBlocks, glm::ivec3(1)));
        const glm::vec3 shellColor = active
            ? glm::vec3(0.72f, 0.86f, 1.0f)
            : glm::vec3(0.45f, 0.55f, 0.62f);
        for (int faceType = 0; faceType < 6; ++faceType) {
            const glm::vec3 normal = normalFromFaceType(faceType);
            float halfExtent = 0.5f;
            if (faceType == 0 || faceType == 1) halfExtent = dims.x * 0.5f;
            else if (faceType == 2 || faceType == 3) halfExtent = dims.y * 0.5f;
            else halfExtent = dims.z * 0.5f;
            appendFace(
                outFaces,
                faceType,
                center + normal * (halfExtent + kShellOffset),
                shellColor,
                kShellAlpha,
                faceScaleForBounds(faceType, dims)
            );
        }
    }

    void setPreviewForCell(MiniModelContext& ctx,
                           const MiniModelWorkbenchState& bench,
                           const glm::ivec3& cell,
                           bool canPlace,
                           const glm::vec3& color) {
        for (auto& batch : ctx.preview.faces) batch.clear();
        ctx.preview.active = true;
        ctx.preview.canPlace = canPlace;
        ctx.preview.workbenchIndex = ctx.activeWorkbenchIndex;
        ctx.preview.cell = cell;
        ctx.preview.color = color;
        const glm::ivec3 dims = gridSizeForWorkbench(bench.def);
        const glm::vec3 localCenter = localCenterForCell(cell, dims);
        const glm::vec3 worldCenter = workbenchWorldCenter(bench.def);
        for (int faceType = 0; faceType < 6; ++faceType) {
            appendFace(
                ctx.preview.faces,
                faceType,
                worldCenter + localCenter + normalFromFaceType(faceType) * (0.5f * kMicroUnit),
                canPlace ? color : glm::vec3(1.0f, 0.20f, 0.18f),
                kPreviewAlpha,
                glm::vec2(kMicroUnit)
            );
        }
    }

    BenchRayHit raycastWorkbench(const BaseSystem& baseSystem,
                                 const MiniModelContext& ctx,
                                 const glm::vec3& origin,
                                 const glm::vec3& dir) {
        BenchRayHit best;
        for (size_t benchIndex = 0; benchIndex < ctx.workbenches.size(); ++benchIndex) {
            const MiniModelWorkbenchState& bench = ctx.workbenches[benchIndex];
            if (!bench.initialized || bench.def.worldIndex < 0) continue;
            const glm::ivec3 dims = gridSizeForWorkbench(bench.def);
            const glm::vec3 minBounds = workbenchWorldMin(bench.def);
            const glm::vec3 maxBounds = workbenchWorldMax(bench.def);
            AabbRayRange range;
            if (!rayAabbRange(origin, dir, minBounds, maxBounds, range)) continue;
            const float endT = std::max(range.entryT, range.exitT);
            if (endT <= 0.0f) continue;

            BenchRayHit candidate;
            candidate.valid = true;
            candidate.workbenchIndex = static_cast<int>(benchIndex);
            candidate.distance = range.startedInside ? range.exitT : range.entryT;
            candidate.point = origin + dir * candidate.distance;
            candidate.normal = range.startedInside ? range.exitNormal : range.entryNormal;
            candidate.benchSurface = true;

            glm::ivec3 prevCell(std::numeric_limits<int>::min());
            for (float t = std::max(0.0f, range.entryT); t <= range.exitT; t += kRayStep) {
                const glm::vec3 point = origin + dir * t;
                const glm::ivec3 cell = pointToWorkbenchCell(bench.def, point);
                if (!inBounds(cell, dims)) continue;
                if (cell == prevCell) continue;
                if (cellOccupied(bench, cell)) {
                    candidate.distance = t;
                    candidate.point = point;
                    candidate.cell = cell;
                    candidate.occupied = true;
                    candidate.benchSurface = false;
                    if (prevCell.x == std::numeric_limits<int>::min()) {
                        candidate.normal = range.entryNormal;
                    } else {
                        candidate.normal = glm::vec3(prevCell - cell);
                        candidate.normal = axisNormalFromVector(candidate.normal);
                    }
                    break;
                }
                prevCell = cell;
            }

            if (!candidate.occupied) {
                const glm::vec3 placePoint = candidate.point - candidate.normal * (0.5f * kRayStep);
                const glm::ivec3 surfaceCell = pointToWorkbenchCell(bench.def, placePoint);
                if (inBounds(surfaceCell, dims)) {
                    candidate.cell = surfaceCell;
                }
            }

            if (!best.valid || candidate.distance < best.distance) {
                best = candidate;
            }
        }
        return best;
    }

    void clearPreview(MiniModelContext& ctx) {
        ctx.preview = MiniModelPreviewState{};
    }

    void updateHudPreview(BaseSystem& baseSystem) {
        if (!baseSystem.player || !baseSystem.hud || !isMiniModelModeActive(baseSystem)) return;
        HUDContext& hud = *baseSystem.hud;
        const glm::vec3 previewColor = baseSystem.miniModel
            ? baseSystem.miniModel->activeColor
            : glm::vec3(1.0f);
        hud.buildModeActive = true;
        hud.buildModeType = static_cast<int>(BuildModeType::MiniModel);
        hud.buildPreviewColor = previewColor;
        hud.buildPreviewTileIndex = -1;
        hud.chargeValue = 1.0f;
        hud.chargeReady = true;
        hud.showCharge = true;
        hud.displayTimer = std::max(hud.displayTimer, 2.0f);
    }
} // namespace

void UpdateMiniModels(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float, PlatformWindowHandle) {
    if (!baseSystem.level || !baseSystem.player || !baseSystem.miniModel) return;
    MiniModelContext& ctx = *baseSystem.miniModel;
    ensureWorkbenchConfigLoaded(baseSystem);
    updateWorkbenchWorldIndices(*baseSystem.level, ctx);
    ensureReferencedAssetsLoaded(baseSystem, prototypes);

    if (!isDevLevel(baseSystem) && baseSystem.player->buildMode == BuildModeType::MiniModel) {
        baseSystem.player->buildMode = BuildModeType::Pickup;
    }

    for (MiniModelWorkbenchState& bench : ctx.workbenches) {
        ensureWorkbenchInitialized(bench);
        if (bench.facesDirty) rebuildWorkbenchFaces(bench);
        if (bench.dirty) {
            (void)exportWorkbenchAsset(ctx, bench);
        }
    }

    if (!isDevLevel(baseSystem) || !isMiniModelModeActive(baseSystem)) {
        clearPreview(ctx);
        return;
    }
    if (baseSystem.ui && baseSystem.ui->active) {
        clearPreview(ctx);
        return;
    }

    PlayerContext& player = *baseSystem.player;
    glm::vec3 rayDir(
        std::cos(glm::radians(player.cameraYaw)) * std::cos(glm::radians(player.cameraPitch)),
        std::sin(glm::radians(player.cameraPitch)),
        std::sin(glm::radians(player.cameraYaw)) * std::cos(glm::radians(player.cameraPitch))
    );
    if (glm::length(rayDir) <= 1e-5f) rayDir = glm::vec3(0.0f, 0.0f, -1.0f);
    else rayDir = glm::normalize(rayDir);

    const BenchRayHit hit = raycastWorkbench(baseSystem, ctx, player.cameraPosition, rayDir);
    ctx.activeWorkbenchIndex = hit.workbenchIndex;
    clearPreview(ctx);

    if (player.middleMousePressed) {
        glm::vec3 sampledColor(1.0f);
        bool sampled = sampleColorFromRegularBlock(baseSystem, prototypes, sampledColor);
        if (!sampled && hit.valid && hit.occupied && hit.workbenchIndex >= 0
            && hit.workbenchIndex < static_cast<int>(ctx.workbenches.size())) {
            sampledColor = unpackColorOpaque(getCellPacked(
                ctx.workbenches[static_cast<size_t>(hit.workbenchIndex)],
                hit.cell
            ));
            sampled = true;
        }
        if (sampled) ctx.activeColor = sampledColor;
    }

    if (hit.valid && hit.workbenchIndex >= 0 && hit.workbenchIndex < static_cast<int>(ctx.workbenches.size())) {
        MiniModelWorkbenchState& bench = ctx.workbenches[static_cast<size_t>(hit.workbenchIndex)];
        const glm::ivec3 dims = gridSizeForWorkbench(bench.def);

        if (hit.occupied) {
            glm::ivec3 placeCell = hit.cell + glm::ivec3(glm::round(hit.normal));
            if (inBounds(placeCell, dims) && !cellOccupied(bench, placeCell)) {
                setPreviewForCell(ctx, bench, placeCell, true, ctx.activeColor);
                if (player.leftMousePressed) {
                    setCellPacked(bench, placeCell, packColorOpaque(ctx.activeColor));
                    rebuildWorkbenchFaces(bench);
                    (void)exportWorkbenchAsset(ctx, bench);
                }
            } else {
                setPreviewForCell(ctx, bench, hit.cell, false, ctx.activeColor);
            }
            if (player.rightMousePressed && cellOccupied(bench, hit.cell)) {
                setCellPacked(bench, hit.cell, 0u);
                rebuildWorkbenchFaces(bench);
                (void)exportWorkbenchAsset(ctx, bench);
            }
        } else if (hit.benchSurface && inBounds(hit.cell, dims)) {
            const bool empty = !cellOccupied(bench, hit.cell);
            setPreviewForCell(ctx, bench, hit.cell, empty, ctx.activeColor);
            if (empty && player.leftMousePressed) {
                setCellPacked(bench, hit.cell, packColorOpaque(ctx.activeColor));
                rebuildWorkbenchFaces(bench);
                (void)exportWorkbenchAsset(ctx, bench);
            }
        }
    }

    updateHudPreview(baseSystem);
}

bool AppendRenderableFacesForInstance(const BaseSystem& baseSystem,
                                      const Entity& proto,
                                      const EntityInstance& instance,
                                      std::array<std::vector<FaceInstanceRenderData>, 6>& outFaces) {
    if (proto.miniModelAsset.empty() || !baseSystem.miniModel) return false;
    const MiniModelContext& ctx = *baseSystem.miniModel;
    auto it = ctx.assets.find(proto.miniModelAsset);
    if (it == ctx.assets.end() || !it->second.loaded) return false;
    const float scale = std::max(0.001f, proto.miniModelScale);
    appendTranslatedFaces(
        it->second.localFaces,
        instance.position + proto.miniModelOffset,
        scale,
        outFaces
    );
    return true;
}

void AppendWorkbenchFaces(const BaseSystem& baseSystem,
                          std::array<std::vector<FaceInstanceRenderData>, 6>& outFaces) {
    if (!baseSystem.miniModel || !baseSystem.level) return;
    const MiniModelContext& ctx = *baseSystem.miniModel;
    for (size_t i = 0; i < ctx.workbenches.size(); ++i) {
        const MiniModelWorkbenchState& bench = ctx.workbenches[i];
        if (!bench.initialized || bench.def.worldIndex < 0) continue;
        appendTranslatedFaces(bench.localFaces, workbenchWorldCenter(bench.def), 1.0f, outFaces);
        buildShellFaces(bench, static_cast<int>(i) == ctx.activeWorkbenchIndex, outFaces);
    }
    if (ctx.preview.active) {
        for (size_t faceType = 0; faceType < ctx.preview.faces.size(); ++faceType) {
            const auto& src = ctx.preview.faces[faceType];
            auto& dst = outFaces[faceType];
            dst.insert(dst.end(), src.begin(), src.end());
        }
    }
}

} // namespace MiniModelSystemLogic
