#pragma once

namespace MiniVoxelParticleSystemLogic {
namespace {
    constexpr std::array<glm::vec3, 6> kMiniVoxelFaceNormals = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(-1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, -1.0f)
    };

    struct MiningParticleAtlasCache {
        bool valid = false;
        RenderHandle atlasTexture = 0;
        int atlasWidth = 0;
        int atlasHeight = 0;
        int tilesPerRow = 0;
        int tilesPerCol = 0;
        glm::ivec2 tileSize = glm::ivec2(24, 24);
        std::vector<unsigned char> atlasPixels;
    };

    MiningParticleAtlasCache& miningParticleAtlasCache() {
        static MiningParticleAtlasCache cache;
        return cache;
    }

    bool ensureMiningParticleAtlasCacheLoaded(const BaseSystem& baseSystem) {
        MiningParticleAtlasCache& cache = miningParticleAtlasCache();
        if (!baseSystem.renderer || !baseSystem.renderBackend) {
            cache.valid = false;
            cache.atlasPixels.clear();
            return false;
        }

        const RendererContext& renderer = *baseSystem.renderer;
        if (renderer.atlasTexture == 0
            || renderer.atlasTextureSize.x <= 0
            || renderer.atlasTextureSize.y <= 0
            || renderer.atlasTilesPerRow <= 0
            || renderer.atlasTilesPerCol <= 0
            || renderer.atlasTileSize.x <= 0
            || renderer.atlasTileSize.y <= 0) {
            cache.valid = false;
            cache.atlasPixels.clear();
            return false;
        }

        const bool refresh = !cache.valid
            || cache.atlasTexture != renderer.atlasTexture
            || cache.atlasWidth != renderer.atlasTextureSize.x
            || cache.atlasHeight != renderer.atlasTextureSize.y
            || cache.tilesPerRow != renderer.atlasTilesPerRow
            || cache.tilesPerCol != renderer.atlasTilesPerCol
            || cache.tileSize != renderer.atlasTileSize;
        if (!refresh) return true;

        const size_t pixelCount = static_cast<size_t>(renderer.atlasTextureSize.x)
            * static_cast<size_t>(renderer.atlasTextureSize.y) * 4u;
        if (pixelCount == 0u) {
            cache.valid = false;
            cache.atlasPixels.clear();
            return false;
        }

        const bool readbackOk = baseSystem.renderBackend->readTexture2DRgba(
            renderer.atlasTexture,
            renderer.atlasTextureSize.x,
            renderer.atlasTextureSize.y,
            cache.atlasPixels
        );
        if (!readbackOk) {
            cache.valid = false;
            cache.atlasPixels.clear();
            return false;
        }

        cache.atlasTexture = renderer.atlasTexture;
        cache.atlasWidth = renderer.atlasTextureSize.x;
        cache.atlasHeight = renderer.atlasTextureSize.y;
        cache.tilesPerRow = renderer.atlasTilesPerRow;
        cache.tilesPerCol = renderer.atlasTilesPerCol;
        cache.tileSize = renderer.atlasTileSize;
        cache.valid = true;
        return true;
    }

    int chooseFaceIndexFromNormal(const glm::vec3& normal) {
        int bestFace = 2; // +Y fallback
        float bestDot = -std::numeric_limits<float>::infinity();
        for (int face = 0; face < static_cast<int>(kMiniVoxelFaceNormals.size()); ++face) {
            const float d = glm::dot(normal, kMiniVoxelFaceNormals[face]);
            if (d > bestDot) {
                bestDot = d;
                bestFace = face;
            }
        }
        return bestFace;
    }

    int firstValidTileIndex(const std::array<int, 6>& tileIndices) {
        for (int tile : tileIndices) {
            if (tile >= 0) return tile;
        }
        return -1;
    }

    glm::vec3 sampleAtlasTileColor(const MiningParticleAtlasCache& cache,
                                   int tileIndex,
                                   float u,
                                   float v,
                                   const glm::vec3& fallback) {
        if (!cache.valid || cache.atlasPixels.empty()) return fallback;
        if (tileIndex < 0
            || cache.tilesPerRow <= 0
            || cache.tilesPerCol <= 0
            || cache.tileSize.x <= 0
            || cache.tileSize.y <= 0) {
            return fallback;
        }

        const int tileCount = cache.tilesPerRow * cache.tilesPerCol;
        if (tileIndex >= tileCount) return fallback;

        float uu = u - std::floor(u);
        float vv = v - std::floor(v);
        if (uu < 0.0f) uu += 1.0f;
        if (vv < 0.0f) vv += 1.0f;

        const int localX = std::clamp(static_cast<int>(std::floor(uu * static_cast<float>(cache.tileSize.x))), 0, cache.tileSize.x - 1);
        const int localY = std::clamp(static_cast<int>(std::floor(vv * static_cast<float>(cache.tileSize.y))), 0, cache.tileSize.y - 1);
        const int tileX = (tileIndex % cache.tilesPerRow) * cache.tileSize.x;
        const int tileRowTop = tileIndex / cache.tilesPerRow;
        const int tileRowBottom = cache.tilesPerCol - 1 - tileRowTop;
        const int tileY = tileRowBottom * cache.tileSize.y;
        const int px = tileX + localX;
        const int py = tileY + (cache.tileSize.y - 1 - localY);
        if (px < 0 || px >= cache.atlasWidth || py < 0 || py >= cache.atlasHeight) return fallback;

        const size_t idx = static_cast<size_t>((py * cache.atlasWidth + px) * 4);
        if (idx + 3u >= cache.atlasPixels.size()) return fallback;

        const float alpha = static_cast<float>(cache.atlasPixels[idx + 3u]) / 255.0f;
        if (alpha <= 0.001f) return fallback;

        const float r = static_cast<float>(cache.atlasPixels[idx + 0u]) / 255.0f;
        const float g = static_cast<float>(cache.atlasPixels[idx + 1u]) / 255.0f;
        const float b = static_cast<float>(cache.atlasPixels[idx + 2u]) / 255.0f;
        return glm::vec3(r, g, b);
    }

    bool appendOpaqueAtlasTileColors(const MiningParticleAtlasCache& cache,
                                     int tileIndex,
                                     std::unordered_set<uint32_t>& seenColors,
                                     std::vector<glm::vec3>& outColors) {
        if (!cache.valid || cache.atlasPixels.empty()) return false;
        if (tileIndex < 0
            || cache.tilesPerRow <= 0
            || cache.tilesPerCol <= 0
            || cache.tileSize.x <= 0
            || cache.tileSize.y <= 0) {
            return false;
        }

        const int tileCount = cache.tilesPerRow * cache.tilesPerCol;
        if (tileIndex >= tileCount) return false;

        const int tileX = (tileIndex % cache.tilesPerRow) * cache.tileSize.x;
        const int tileRowTop = tileIndex / cache.tilesPerRow;
        const int tileRowBottom = cache.tilesPerCol - 1 - tileRowTop;
        const int tileY = tileRowBottom * cache.tileSize.y;
        if (tileX < 0
            || tileY < 0
            || tileX + cache.tileSize.x > cache.atlasWidth
            || tileY + cache.tileSize.y > cache.atlasHeight) {
            return false;
        }

        outColors.reserve(outColors.size() + static_cast<size_t>(cache.tileSize.x * cache.tileSize.y));
        for (int localY = 0; localY < cache.tileSize.y; ++localY) {
            for (int localX = 0; localX < cache.tileSize.x; ++localX) {
                const int px = tileX + localX;
                const int py = tileY + (cache.tileSize.y - 1 - localY);
                const size_t idx = static_cast<size_t>((py * cache.atlasWidth + px) * 4);
                if (idx + 3u >= cache.atlasPixels.size()) continue;
                const float alpha = static_cast<float>(cache.atlasPixels[idx + 3u]) / 255.0f;
                if (alpha <= 0.001f) continue;
                const unsigned char r8 = cache.atlasPixels[idx + 0u];
                const unsigned char g8 = cache.atlasPixels[idx + 1u];
                const unsigned char b8 = cache.atlasPixels[idx + 2u];
                const uint32_t packed = (static_cast<uint32_t>(r8) << 16u)
                    | (static_cast<uint32_t>(g8) << 8u)
                    | static_cast<uint32_t>(b8);
                if (!seenColors.insert(packed).second) continue;
                outColors.emplace_back(
                    static_cast<float>(r8) / 255.0f,
                    static_cast<float>(g8) / 255.0f,
                    static_cast<float>(b8) / 255.0f
                );
            }
        }
        return true;
    }

    bool gatherOpaqueAtlasFaceColors(const MiningParticleAtlasCache& cache,
                                     const std::array<int, 6>& tileIndices,
                                     std::vector<glm::vec3>& outColors) {
        outColors.clear();
        std::unordered_set<uint32_t> seenColors;
        seenColors.reserve(24u * 24u * tileIndices.size());
        for (int tile : tileIndices) {
            (void)appendOpaqueAtlasTileColors(cache, tile, seenColors, outColors);
        }
        return !outColors.empty();
    }

    uint32_t hash3D(int x, int y, int z) {
        uint32_t ux = static_cast<uint32_t>(x) * 73856093u;
        uint32_t uy = static_cast<uint32_t>(y) * 19349663u;
        uint32_t uz = static_cast<uint32_t>(z) * 83492791u;
        uint32_t h = ux ^ uy ^ uz;
        h ^= (h >> 13);
        h *= 1274126177u;
        h ^= (h >> 16);
        return h;
    }

    inline float nextMiniVoxelRange(uint32_t& state, float minValue, float maxValue) {
        state = state * 1664525u + 1013904223u;
        const float normalized = static_cast<float>(state & 0x00ffffffu) / 16777216.0f;
        return glm::mix(minValue, maxValue, normalized);
    }

    glm::ivec3 pointToVoxelCell(const glm::vec3& p) {
        return glm::ivec3(
            static_cast<int>(std::floor(p.x + 0.5f)),
            static_cast<int>(std::floor(p.y + 0.5f)),
            static_cast<int>(std::floor(p.z + 0.5f))
        );
    }

    bool pointInsideSolidVoxel(const BaseSystem& baseSystem, const glm::vec3& p) {
        if (!baseSystem.voxelWorld || !baseSystem.voxelWorld->enabled) return false;
        const glm::ivec3 cell = pointToVoxelCell(p);
        return baseSystem.voxelWorld->getBlockWorld(cell) != 0u;
    }

    glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback) {
        const float len = glm::length(v);
        if (len <= 1e-5f) return fallback;
        return v / len;
    }

    void addMiniVoxelFace(std::array<std::vector<FaceInstanceRenderData>, 6>& faceInstances,
                          int faceType,
                          const MiniVoxelParticle& particle,
                          float sizeScale,
                          float alpha) {
        FaceInstanceRenderData face;
        face.position = particle.position + kMiniVoxelFaceNormals[faceType] * (sizeScale * 0.5f);
        face.color = particle.color;
        face.tileIndex = particle.tileIndices[faceType];
        face.alpha = alpha;
        face.ao = glm::vec4(1.0f);
        face.scale = glm::vec2(sizeScale, sizeScale);
        face.uvScale = glm::vec2(1.0f);
        faceInstances[faceType].push_back(face);
    }
} // namespace

void SpawnFromBlock(BaseSystem& baseSystem,
                    const std::vector<Entity>& prototypes,
                    int worldIndex,
                    const glm::ivec3& cell,
                    int prototypeID,
                    const glm::vec3& color,
                    const glm::vec3& hitPosition,
                    const glm::vec3& hitNormal) {
    if (!baseSystem.renderer || !baseSystem.world) return;
    if (prototypeID < 0 || prototypeID >= static_cast<int>(prototypes.size())) return;
    RendererContext& renderer = *baseSystem.renderer;
    const WorldContext* worldCtx = baseSystem.world.get();
    if (!worldCtx) return;
    const Entity& proto = prototypes[static_cast<size_t>(prototypeID)];
    std::array<int, 6> tileIndices;
    for (int face = 0; face < 6; ++face) {
        tileIndices[face] = RenderInitSystemLogic::FaceTileIndexFor(worldCtx, proto, face);
    }
    uint32_t rng = hash3D(cell.x, cell.y, cell.z) ^ static_cast<uint32_t>(baseSystem.frameIndex * 31u);
    const int count = glm::clamp(
        RenderInitSystemLogic::getRegistryInt(baseSystem, "MiningParticleCount", 12),
        4,
        64
    );
    const float lifetime = glm::clamp(
        RenderInitSystemLogic::getRegistryFloat(baseSystem, "MiningParticleLifetimeSeconds", 0.55f),
        0.1f,
        3.0f
    );
    const float baseSize = 1.0f / 24.0f;
    const glm::vec3 fallbackCellCenter = glm::vec3(cell) + glm::vec3(0.5f);
    const glm::vec3 baseNormal = (glm::length(hitNormal) > 0.001f)
        ? glm::normalize(hitNormal)
        : glm::vec3(0.0f, 1.0f, 0.0f);
    const int preferredFace = chooseFaceIndexFromNormal(baseNormal);
    int colorSampleTile = (preferredFace >= 0 && preferredFace < static_cast<int>(tileIndices.size()))
        ? tileIndices[preferredFace]
        : firstValidTileIndex(tileIndices);
    std::array<int, 6> solidColorTileIndices;
    solidColorTileIndices.fill(-1);
    const bool atlasSampleReady = ensureMiningParticleAtlasCacheLoaded(baseSystem);
    const MiningParticleAtlasCache& atlasCache = miningParticleAtlasCache();
    const glm::vec3 fallbackParticleColor = (glm::length(color) > 0.001f) ? color : glm::vec3(1.0f);
    std::vector<glm::vec3> opaqueTileColors;
    if (atlasSampleReady) {
        gatherOpaqueAtlasFaceColors(atlasCache, tileIndices, opaqueTileColors);
    }
    const glm::vec3 tangentA = (std::abs(baseNormal.y) < 0.99f)
        ? glm::normalize(glm::cross(baseNormal, glm::vec3(0.0f, 1.0f, 0.0f)))
        : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 tangentB = glm::normalize(glm::cross(baseNormal, tangentA));
    const glm::vec3 spawnBase = (glm::length(hitPosition - glm::vec3(cell)) < 3.0f)
        ? (hitPosition + baseNormal * 0.02f)
        : fallbackCellCenter;
    for (int i = 0; i < count; ++i) {
        MiniVoxelParticle particle;
        // Start on a thin shell around the impact point, biased outward from the hit face.
        const float radialA = nextMiniVoxelRange(rng, -1.0f, 1.0f);
        const float radialB = nextMiniVoxelRange(rng, -1.0f, 1.0f);
        const float radialN = nextMiniVoxelRange(rng, 0.25f, 1.0f);
        glm::vec3 radial = safeNormalize(
            tangentA * radialA + tangentB * radialB + baseNormal * radialN,
            baseNormal
        );
        if (glm::dot(radial, baseNormal) < 0.0f) {
            radial = safeNormalize(radial + baseNormal * 0.75f, baseNormal);
        }
        const float shellRadius = nextMiniVoxelRange(rng, 0.03f, 0.16f);
        particle.position = spawnBase
            + radial * shellRadius
            + baseNormal * 0.015f;

        // If initial shell point is still inside solid geometry, push it out.
        if (pointInsideSolidVoxel(baseSystem, particle.position)) {
            for (int escape = 0; escape < 6 && pointInsideSolidVoxel(baseSystem, particle.position); ++escape) {
                particle.position += baseNormal * 0.06f;
            }
        }

        const float speed = nextMiniVoxelRange(rng, 0.55f, 2.1f);
        const float spreadA = nextMiniVoxelRange(rng, -0.45f, 0.45f);
        const float spreadB = nextMiniVoxelRange(rng, -0.45f, 0.45f);
        particle.velocity = radial * speed
            + tangentA * spreadA
            + tangentB * spreadB
            + baseNormal * nextMiniVoxelRange(rng, 0.2f, 0.9f);
        particle.tileIndices = solidColorTileIndices;
        particle.lifetime = lifetime;
        particle.baseSize = baseSize;
        if (!opaqueTileColors.empty()) {
            const float pick = nextMiniVoxelRange(rng, 0.0f, static_cast<float>(opaqueTileColors.size()));
            const size_t randomIndex = std::min(
                opaqueTileColors.size() - 1u,
                static_cast<size_t>(pick)
            );
            const size_t colorIndex = (randomIndex + static_cast<size_t>(i)) % opaqueTileColors.size();
            particle.color = opaqueTileColors[colorIndex] * fallbackParticleColor;
        } else if (atlasSampleReady && colorSampleTile >= 0) {
            const float sampleU = nextMiniVoxelRange(rng, 0.05f, 0.95f);
            const float sampleV = nextMiniVoxelRange(rng, 0.05f, 0.95f);
            particle.color = sampleAtlasTileColor(atlasCache, colorSampleTile, sampleU, sampleV, fallbackParticleColor) * fallbackParticleColor;
        } else {
            particle.color = fallbackParticleColor;
        }
        renderer.miniVoxelParticles.push_back(particle);
    }
}

void UpdateParticles(BaseSystem& baseSystem,
                     float dt,
                     std::array<std::vector<FaceInstanceRenderData>, 6>& faceInstances) {
    if (!baseSystem.renderer) return;
    if (dt <= 0.0f) return;
    RendererContext& renderer = *baseSystem.renderer;
    if (renderer.miniVoxelParticles.empty()) return;
    const float gravity = -std::abs(RenderInitSystemLogic::getRegistryFloat(baseSystem, "MiningParticleGravity", 18.0f));
    size_t writeIndex = 0;
    for (size_t i = 0; i < renderer.miniVoxelParticles.size(); ++i) {
        MiniVoxelParticle& particle = renderer.miniVoxelParticles[i];
        const glm::vec3 prevPos = particle.position;
        particle.velocity.y += gravity * dt;
        const glm::vec3 step = particle.velocity * dt;
        glm::vec3 resolvedPos = prevPos;

        // Axis-separated collision keeps particles from tunneling through solid voxels.
        glm::vec3 candidate = resolvedPos;
        candidate.x += step.x;
        if (!pointInsideSolidVoxel(baseSystem, candidate)) {
            resolvedPos.x = candidate.x;
        } else {
            particle.velocity.x *= -0.25f;
        }

        candidate = resolvedPos;
        candidate.y += step.y;
        if (!pointInsideSolidVoxel(baseSystem, candidate)) {
            resolvedPos.y = candidate.y;
        } else {
            if (particle.velocity.y < 0.0f) {
                particle.velocity.x *= 0.72f;
                particle.velocity.z *= 0.72f;
            }
            particle.velocity.y *= -0.2f;
        }

        candidate = resolvedPos;
        candidate.z += step.z;
        if (!pointInsideSolidVoxel(baseSystem, candidate)) {
            resolvedPos.z = candidate.z;
        } else {
            particle.velocity.z *= -0.25f;
        }

        if (pointInsideSolidVoxel(baseSystem, resolvedPos)) {
            resolvedPos = prevPos;
            particle.velocity *= 0.25f;
        }

        particle.position = resolvedPos;
        particle.age += dt;
        if (particle.age >= particle.lifetime) continue;
        const float alpha = 1.0f;
        const float scale = particle.baseSize;
        for (int faceType = 0; faceType < 6; ++faceType) {
            addMiniVoxelFace(faceInstances, faceType, particle, scale, alpha);
        }
        renderer.miniVoxelParticles[writeIndex++] = particle;
    }
    if (writeIndex < renderer.miniVoxelParticles.size()) {
        renderer.miniVoxelParticles.resize(writeIndex);
    }
}
} // namespace MiniVoxelParticleSystemLogic
