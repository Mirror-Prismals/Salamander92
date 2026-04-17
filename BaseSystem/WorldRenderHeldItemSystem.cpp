#pragma once

namespace FontSystemLogic {
    bool RasterizeBookTextBitmap(const BaseSystem& baseSystem,
                                 const std::string& fontName,
                                 float pixelHeight,
                                 const std::string& text,
                                 int maxCharsPerLine,
                                 int maxLines,
                                 std::vector<unsigned char>& outAlpha,
                                 int& outWidth,
                                 int& outHeight,
                                 int& outWrappedLineCount);
}

namespace WorldRenderSystemLogic {

struct WorldRenderViewState {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::vec3 playerPos{0.0f};
    glm::vec3 cameraForward{0.0f, 0.0f, -1.0f};
    glm::vec3 lightDir{0.0f, 1.0f, 0.0f};
    glm::vec3 ambientLightColor{1.0f};
    glm::vec3 diffuseLightColor{1.0f};
    float time = 0.0f;
    bool mapViewActive = false;
    bool leafOpaqueOutsideTier0 = true;
    bool foliageWindAnimationEnabled = true;
    bool waterCascadeBrightnessEnabled = true;
    float waterCascadeBrightnessStrength = 0.0f;
    float waterCascadeBrightnessSpeed = 1.0f;
    float waterCascadeBrightnessScale = 1.0f;
};

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

bool isStonePebbleXName(const std::string& name) {
    return name == "StonePebbleTexX"
        || (startsWith(name, "StonePebble") && endsWith(name, "TexX"));
}

bool isStonePebbleZName(const std::string& name) {
    return name == "StonePebbleTexZ"
        || (startsWith(name, "StonePebble") && endsWith(name, "TexZ"));
}

bool isGrassCoverXName(const std::string& name) {
    return name == "GrassCoverTexX"
        || (startsWith(name, "GrassCover") && endsWith(name, "TexX"));
}

bool isGrassCoverZName(const std::string& name) {
    return name == "GrassCoverTexZ"
        || (startsWith(name, "GrassCover") && endsWith(name, "TexZ"));
}

bool isPetalPileName(const std::string& name) {
    if (startsWith(name, "StonePebblePetalsBook")) return false;
    return startsWith(name, "StonePebblePetals")
        || startsWith(name, "StonePebblePatch")
        || startsWith(name, "StonePebbleLeaf")
        || startsWith(name, "StonePebbleLilypad")
        || startsWith(name, "StonePebbleSandDollar");
}

uint32_t hashCell3D(int x, int y, int z) {
    uint32_t h = static_cast<uint32_t>(x) * 73856093u;
    h ^= static_cast<uint32_t>(y) * 19349663u;
    h ^= static_cast<uint32_t>(z) * 83492791u;
    h ^= (h >> 13);
    h *= 1274126177u;
    h ^= (h >> 16);
    return h;
}

struct NarrowHalfExtents {
    float x;
    float y;
    float z;
};

constexpr int kSurfaceStonePileMin = 1;
constexpr int kSurfaceStonePileMax = 8;

struct StonePebblePilePieces {
    int count = 0;
    std::array<glm::vec2, kSurfaceStonePileMax> offsets{};
    std::array<NarrowHalfExtents, kSurfaceStonePileMax> halfExtents{};
};

StonePebblePilePieces stonePebblePilePiecesForCell(const glm::ivec3& lodCoord, int requestedCount) {
    StonePebblePilePieces out;
    out.count = std::clamp(requestedCount, kSurfaceStonePileMin, kSurfaceStonePileMax);
    int placed = 0;
    constexpr float kPlacementPad = 1.0f / 96.0f;
    for (int i = 0; i < out.count; ++i) {
        const uint32_t sizeHash = hashCell3D(
            lodCoord.x + i * 83,
            lodCoord.y - i * 47,
            lodCoord.z + i * 59
        );
        NarrowHalfExtents ext;
        ext.x = (2.0f + static_cast<float>(sizeHash & 3u)) / 48.0f;
        ext.z = (2.0f + static_cast<float>((sizeHash >> 2u) & 3u)) / 48.0f;
        ext.y = (1.0f + static_cast<float>((sizeHash >> 4u) % 3u)) / 48.0f;
        if ((sizeHash >> 6u) & 1u) std::swap(ext.x, ext.z);
        if (out.count == 1) {
            ext.x *= 1.35f;
            ext.z *= 1.35f;
            ext.y *= 1.20f;
        }

        bool placedThis = false;
        for (int attempt = 0; attempt < 12; ++attempt) {
            const uint32_t h = hashCell3D(
                lodCoord.x + i * 37 + attempt * 11,
                lodCoord.y + i * 19 - attempt * 7,
                lodCoord.z - i * 53 + attempt * 13
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
            out.offsets[static_cast<size_t>(placed)] = glm::vec2(0.0f);
            out.halfExtents[static_cast<size_t>(placed)] = ext;
            ++placed;
        }
    }
    out.count = placed;
    return out;
}

struct GrassCoverDots {
    int count = 0;
    std::array<glm::vec2, 48> offsets{};
};

GrassCoverDots grassCoverDotsForCell(const glm::ivec3& lodCoord) {
    GrassCoverDots out;
    constexpr int kMinDots = 36;
    constexpr int kMaxDots = 48;
    constexpr int kGridCellsPerAxis = 24;
    constexpr float kGridUnit = 1.0f / 24.0f;
    const uint32_t seed = hashCell3D(lodCoord.x + 913, lodCoord.y + 37, lodCoord.z - 211);
    out.count = kMinDots + static_cast<int>(seed % static_cast<uint32_t>(kMaxDots - kMinDots + 1));
    for (int i = 0; i < out.count; ++i) {
        const uint32_t h = hashCell3D(
            lodCoord.x + i * 31,
            lodCoord.y - i * 17,
            lodCoord.z + i * 13
        );
        const int oxSlot = static_cast<int>(h % static_cast<uint32_t>(kGridCellsPerAxis));
        const int ozSlot = static_cast<int>((h >> 8u) % static_cast<uint32_t>(kGridCellsPerAxis));
        const float ox = (static_cast<float>(oxSlot) + 0.5f) * kGridUnit - 0.5f;
        const float oz = (static_cast<float>(ozSlot) + 0.5f) * kGridUnit - 0.5f;
        out.offsets[static_cast<size_t>(i)] = glm::vec2(ox, oz);
    }
    return out;
}

int decodeSurfaceStonePileCount(uint32_t packedColor) {
    const int encoded = static_cast<int>((packedColor >> 24) & 0xffu);
    if (encoded <= 0) return kSurfaceStonePileMin;
    return std::clamp(encoded, kSurfaceStonePileMin, kSurfaceStonePileMax);
}
} // namespace

void RenderHeldItemPass(
    BaseSystem& baseSystem,
    std::vector<Entity>& prototypes,
    const WorldRenderViewState& frame,
    const std::function<void(Shader&, bool)>& bindFaceTextureUniforms) {
    if (!baseSystem.renderer || !baseSystem.player || !baseSystem.world || !baseSystem.renderBackend) return;

    RendererContext& renderer = *baseSystem.renderer;
    PlayerContext& player = *baseSystem.player;
    IRenderBackend& renderBackend = *baseSystem.renderBackend;

    const glm::mat4& view = frame.view;
    const glm::mat4& projection = frame.projection;
    const glm::vec3& playerPos = frame.playerPos;
    const glm::vec3& cameraForward = frame.cameraForward;
    const glm::vec3& lightDir = frame.lightDir;
    const glm::vec3& ambientLightColor = frame.ambientLightColor;
    const glm::vec3& diffuseLightColor = frame.diffuseLightColor;
    const float time = frame.time;
    const bool mapViewActive = frame.mapViewActive;
    const bool leafOpaqueOutsideTier0 = frame.leafOpaqueOutsideTier0;
    const bool foliageWindAnimationEnabled = frame.foliageWindAnimationEnabled;
    const bool waterCascadeBrightnessEnabled = frame.waterCascadeBrightnessEnabled;
    const float waterCascadeBrightnessStrength = frame.waterCascadeBrightnessStrength;
    const float waterCascadeBrightnessSpeed = frame.waterCascadeBrightnessSpeed;
    const float waterCascadeBrightnessScale = frame.waterCascadeBrightnessScale;
    const bool voxelLightingEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "VoxelLightingEnabled", true);
    const bool voxelGridLinesEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "VoxelGridLinesEnabled", true);
    const bool voxelGridLineInvertColorEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "VoxelGridLineInvertColorEnabled", false);

    auto setDepthTestEnabled = [&](bool enabled) { renderBackend.setDepthTestEnabled(enabled); };
    auto setCullEnabled = [&](bool enabled) { renderBackend.setCullEnabled(enabled); };
    auto setCullBackFaceCCWEnabled = [&](bool enabled) {
        setCullEnabled(enabled);
        if (enabled) renderBackend.setCullBackFaceCCW();
    };
        auto renderHeldItemForCurrentState = [&](bool renderLeftHand, bool& renderedAny) {
            if (mapViewActive || !player.isHoldingBlock || player.heldPrototypeID < 0) return;
            const float heldItemForward = glm::clamp(
                getRegistryFloat(baseSystem, "HeldItemViewForward", 0.58f),
                0.2f,
                2.5f
            );
            const float heldItemVertical = glm::clamp(
                getRegistryFloat(baseSystem, "HeldItemViewVertical", -0.24f),
                -1.0f,
                1.0f
            );
            const float heldItemSide = glm::clamp(
                getRegistryFloat(baseSystem, "HeldItemViewSide", 0.34f),
                0.0f,
                1.5f
            );
            glm::vec3 cameraRight = glm::cross(cameraForward, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::length(cameraRight) < 1e-4f) {
                cameraRight = glm::vec3(1.0f, 0.0f, 0.0f);
            } else {
                cameraRight = glm::normalize(cameraRight);
            }
            glm::vec3 cameraUp = glm::cross(cameraRight, cameraForward);
            if (glm::length(cameraUp) < 1e-4f) {
                cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
            } else {
                cameraUp = glm::normalize(cameraUp);
            }
            const float handSideSign = renderLeftHand ? -1.0f : 1.0f;
            glm::vec3 heldPos(0.0f);
            const bool heldStickMode = getRegistryBool(baseSystem, "HeldItemStickMode", true);
            if (heldStickMode) {
                const float stickLength = glm::clamp(
                    getRegistryFloat(baseSystem, "HeldItemStickLength", 0.64f),
                    0.2f,
                    2.5f
                );
                const float stickYawDeg = glm::clamp(
                    getRegistryFloat(baseSystem, "HeldItemStickYawDeg", 32.0f),
                    -89.0f,
                    89.0f
                );
                const float stickPitchDeg = glm::clamp(
                    getRegistryFloat(baseSystem, "HeldItemStickPitchDeg", -16.0f),
                    -89.0f,
                    89.0f
                );
                const float yawRad = glm::radians(stickYawDeg * handSideSign);
                const float pitchRad = glm::radians(stickPitchDeg);
                const float cosPitch = std::cos(pitchRad);
                glm::vec3 stickDir = cameraForward * (cosPitch * std::cos(yawRad))
                    + cameraRight * (cosPitch * std::sin(yawRad))
                    + cameraUp * std::sin(pitchRad);
                if (glm::length(stickDir) < 1e-4f) {
                    stickDir = cameraForward;
                } else {
                    stickDir = glm::normalize(stickDir);
                }
                heldPos = player.cameraPosition + stickDir * stickLength;
            } else {
                heldPos = player.cameraPosition
                    + cameraForward * heldItemForward
                    + cameraUp * heldItemVertical
                    + cameraRight * (heldItemSide * handSideSign);
            }
            if (baseSystem.gamemode == "survival"
                && getRegistryBool(baseSystem, "WalkViewBobbingEnabled", true)
                && getRegistryBool(baseSystem, "HeldItemViewBobbingEnabled", true)) {
                const float bobWeight = glm::clamp(player.viewBobWeight, 0.0f, 1.0f);
                if (bobWeight > 1e-4f) {
                    const float sideWave = std::sin(player.viewBobPhase);
                    const float verticalWave = std::sin(player.viewBobPhase - 0.6f);
                    const float bobLateralAmp = glm::clamp(
                        getRegistryFloat(baseSystem, "WalkViewBobLateralAmplitude", 0.030f),
                        0.0f,
                        0.25f
                    ) * glm::clamp(getRegistryFloat(baseSystem, "HeldItemViewBobLateralScale", 1.0f), -4.0f, 4.0f);
                    const float bobVerticalAmp = glm::clamp(
                        getRegistryFloat(baseSystem, "WalkViewBobVerticalAmplitude", 0.060f),
                        0.0f,
                        0.25f
                    ) * glm::clamp(getRegistryFloat(baseSystem, "HeldItemViewBobVerticalScale", 1.0f), -4.0f, 4.0f);
                    heldPos += cameraRight * (bobLateralAmp * sideWave * bobWeight)
                        + cameraUp * (bobVerticalAmp * verticalWave * bobWeight);
                }
            }
            float heldLightFactor = 1.0f;
            if (voxelLightingEnabled && baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                const float voxelLightingStrength = glm::clamp(
                    getRegistryFloat(baseSystem, "VoxelLightingStrength", 1.0f),
                    0.0f,
                    1.0f
                );
                const float voxelLightingMinBrightness = glm::clamp(
                    getRegistryFloat(baseSystem, "VoxelLightingMinBrightness", 0.08f),
                    0.0f,
                    1.0f
                );
                const float voxelLightingGamma = glm::clamp(
                    getRegistryFloat(baseSystem, "VoxelLightingGamma", 1.35f),
                    0.25f,
                    4.0f
                );
                const glm::ivec3 lightCell = glm::ivec3(glm::round(heldPos));
                const uint8_t sky = baseSystem.voxelWorld->getSkyLightWorld(lightCell);
                const uint8_t block = baseSystem.voxelWorld->getBlockLightWorld(lightCell);
                const uint8_t level = static_cast<uint8_t>(std::max<int>(sky, block));
                const float normalized = glm::clamp(static_cast<float>(level) / 15.0f, 0.0f, 1.0f);
                const float curve = std::pow(normalized, voxelLightingGamma);
                const float factor = voxelLightingMinBrightness + (1.0f - voxelLightingMinBrightness) * curve;
                heldLightFactor = 1.0f + (factor - 1.0f) * voxelLightingStrength;
            }
            const glm::vec4 heldAo = glm::vec4(heldLightFactor);
            bool drewTextured = false;
            if (player.heldPrototypeID < static_cast<int>(prototypes.size())) {
                const Entity& heldProto = prototypes[player.heldPrototypeID];
                const bool heldIsLeaf = (heldProto.name == "Leaf");
                const bool heldIsTallGrass = (heldProto.name.rfind("GrassTuft", 0) == 0)
                    && (heldProto.name.rfind("GrassTuftShort", 0) != 0);
                const bool heldIsShortGrass = (heldProto.name.rfind("GrassTuftShort", 0) == 0);
                const bool heldIsFlower = (heldProto.name.rfind("Flower", 0) == 0);
                const bool heldIsCavePot = (heldProto.name == "StonePebbleCavePotTexX"
                    || heldProto.name == "StonePebbleCavePotTexZ");
                const bool heldIsPlant = heldIsTallGrass || heldIsShortGrass || heldIsFlower || heldIsCavePot;
                const bool heldIsStick = (heldProto.name == "StickTexX"
                    || heldProto.name == "StickTexZ"
                    || heldProto.name == "StickWinterTexX"
                    || heldProto.name == "StickWinterTexZ");
                const bool heldIsPetalPile = isPetalPileName(heldProto.name);
                const bool heldIsBook = BookSystemLogic::IsBookPrototypeName(heldProto.name);
                const bool heldIsWallStone = (heldProto.name == "WallStoneTexPosX"
                    || heldProto.name == "WallStoneTexNegX"
                    || heldProto.name == "WallStoneTexPosZ"
                    || heldProto.name == "WallStoneTexNegZ"
                    || heldProto.name == "WallBranchLongTexPosX"
                    || heldProto.name == "WallBranchLongTexNegX"
                    || heldProto.name == "WallBranchLongTexPosZ"
                    || heldProto.name == "WallBranchLongTexNegZ"
                    || heldProto.name == "WallBranchLongTipTexPosX"
                    || heldProto.name == "WallBranchLongTipTexNegX"
                    || heldProto.name == "WallBranchLongTipTexPosZ"
                    || heldProto.name == "WallBranchLongTipTexNegZ");
                const bool heldIsCeilingStone = (heldProto.name.rfind("CeilingStoneTex", 0) == 0);
                const bool heldIsSurfaceStonePebble = (isStonePebbleXName(heldProto.name)
                    || isStonePebbleZName(heldProto.name));
                const bool heldIsGrassCover = (isGrassCoverXName(heldProto.name)
                    || isGrassCoverZName(heldProto.name));
                const bool heldIsStonePebble = (isStonePebbleXName(heldProto.name)
                    || isStonePebbleZName(heldProto.name)
                    || heldIsWallStone
                    || heldIsCeilingStone);
                const bool heldIsNarrowProp = heldIsStick || heldIsStonePebble || heldIsGrassCover;
                const bool drawAsFace = heldProto.useTexture || heldIsLeaf || heldIsPlant;
                if (drawAsFace && renderer.faceShader && renderer.faceVAO) {
                    const glm::ivec3 heldSeedCell = player.heldHasSourceCell
                        ? player.heldSourceCell
                        : glm::ivec3(
                            player.heldPrototypeID * 37 + static_cast<int>((player.heldPackedColor >> 4u) & 0xffu),
                            player.heldPrototypeID * -23 + static_cast<int>((player.heldPackedColor >> 12u) & 0xffu),
                            player.heldPrototypeID * 53 + static_cast<int>((player.heldPackedColor >> 20u) & 0xffu)
                        );
                    static const std::array<glm::vec3, 6> kFaceOffsets = {
                        glm::vec3(0.5f, 0.0f, 0.0f),  glm::vec3(-0.5f, 0.0f, 0.0f),
                        glm::vec3(0.0f, 0.5f, 0.0f),  glm::vec3(0.0f, -0.5f, 0.0f),
                        glm::vec3(0.0f, 0.0f, 0.5f),  glm::vec3(0.0f, 0.0f, -0.5f)
                    };
                    renderer.faceShader->use();
                    renderer.faceShader->setMat4("view", view);
                    renderer.faceShader->setMat4("projection", projection);
                    renderer.faceShader->setMat4("model", glm::mat4(1.0f));
                    renderer.faceShader->setVec3("cameraPos", playerPos);
                    renderer.faceShader->setFloat("time", time);
                    renderer.faceShader->setVec3("lightDir", lightDir);
                    renderer.faceShader->setVec3("ambientLight", ambientLightColor);
                    renderer.faceShader->setVec3("diffuseLight", diffuseLightColor);
                    renderer.faceShader->setInt("faceType", 0);
                    renderer.faceShader->setInt("sectionTier", 0);
                    renderer.faceShader->setInt("leafOpaqueOutsideTier0", leafOpaqueOutsideTier0 ? 1 : 0);
                    renderer.faceShader->setInt("leafBackfacesWhenInside", 0);
                    renderer.faceShader->setInt("foliageWindEnabled", foliageWindAnimationEnabled ? 1 : 0);
                    renderer.faceShader->setInt("waterCascadeBrightnessEnabled", waterCascadeBrightnessEnabled ? 1 : 0);
                    renderer.faceShader->setInt("maskedFoliagePassMode", 0);
                    renderer.faceShader->setFloat("waterCascadeBrightnessStrength", waterCascadeBrightnessStrength);
                    renderer.faceShader->setFloat("waterCascadeBrightnessSpeed", waterCascadeBrightnessSpeed);
                    renderer.faceShader->setFloat("waterCascadeBrightnessScale", waterCascadeBrightnessScale);
                    renderer.faceShader->setInt("wireframeDebug", 0);
                    bindFaceTextureUniforms(*renderer.faceShader, true);
                    BlockChargeSystemLogic::ApplyBlockDamageMaskUniforms(baseSystem, prototypes, *renderer.faceShader, false);
                    setCullBackFaceCCWEnabled(true);
                    renderBackend.bindVertexArray(renderer.faceVAO);

                    if (heldIsBook) {
                        auto safeNormalize = [](const glm::vec3& v, const glm::vec3& fallback) -> glm::vec3 {
                            if (glm::length(v) < 1e-4f) return fallback;
                            return glm::normalize(v);
                        };
                        auto makeBasisModel = [&](const glm::vec3& center,
                                                  const glm::vec3& xAxis,
                                                  const glm::vec3& yAxis,
                                                  const glm::vec3& zAxis) -> glm::mat4 {
                            glm::mat4 m(1.0f);
                            m[0] = glm::vec4(xAxis, 0.0f);
                            m[1] = glm::vec4(yAxis, 0.0f);
                            m[2] = glm::vec4(zAxis, 0.0f);
                            m[3] = glm::vec4(center, 1.0f);
                            return m;
                        };
                        auto drawBookFace = [&](const glm::mat4& modelMat,
                                                int faceType,
                                                const glm::vec3& localCenter,
                                                const glm::vec2& faceScale,
                                                int tileIndex,
                                                const glm::vec3& tintColor,
                                                float faceAlpha = 1.0f) {
                            FaceInstanceRenderData heldFace;
                            heldFace.position = localCenter;
                            heldFace.color = tintColor;
                            heldFace.tileIndex = tileIndex;
                            heldFace.alpha = faceAlpha;
                            heldFace.ao = heldAo;
                            heldFace.scale = faceScale;
                            heldFace.uvScale = faceScale;
                            renderer.faceShader->setMat4("model", modelMat);
                            renderer.faceShader->setInt("faceType", faceType);
                            renderBackend.uploadArrayBufferData(renderer.faceInstanceVBO, &heldFace, sizeof(FaceInstanceRenderData), true);
                            renderBackend.drawArraysTrianglesInstanced(0, 6, 1);
                        };
                        auto drawBookCuboid = [&](const glm::mat4& modelMat,
                                                  const glm::vec3& halfExtents,
                                                  int tileIndex,
                                                  const glm::vec3& tintColor,
                                                  float faceAlpha = 1.0f) {
                            for (int faceType = 0; faceType < 6; ++faceType) {
                                const glm::vec3 normal = (faceType == 0) ? glm::vec3(1.0f, 0.0f, 0.0f)
                                    : (faceType == 1) ? glm::vec3(-1.0f, 0.0f, 0.0f)
                                    : (faceType == 2) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                    : (faceType == 3) ? glm::vec3(0.0f, -1.0f, 0.0f)
                                    : (faceType == 4) ? glm::vec3(0.0f, 0.0f, 1.0f)
                                    : glm::vec3(0.0f, 0.0f, -1.0f);
                                const float halfExtent = (faceType == 0 || faceType == 1) ? halfExtents.x
                                    : (faceType == 2 || faceType == 3) ? halfExtents.y
                                    : halfExtents.z;
                                glm::vec2 faceScale(1.0f);
                                if (faceType == 0 || faceType == 1) {
                                    faceScale = glm::vec2(halfExtents.z * 2.0f, halfExtents.y * 2.0f);
                                } else if (faceType == 2 || faceType == 3) {
                                    faceScale = glm::vec2(halfExtents.x * 2.0f, halfExtents.z * 2.0f);
                                } else {
                                    faceScale = glm::vec2(halfExtents.x * 2.0f, halfExtents.y * 2.0f);
                                }
                                drawBookFace(
                                    modelMat,
                                    faceType,
                                    normal * halfExtent,
                                    faceScale,
                                    tileIndex,
                                    tintColor,
                                    faceAlpha
                                );
                            }
                        };
                        auto drawBookTextRows = [&](const glm::mat4& modelMat,
                                                    const std::string& text,
                                                    float pageCenterZ,
                                                    float pageHalfDepth,
                                                    float jitterX) {
                            const std::string fontName = getRegistryString(
                                baseSystem,
                                "BookInspectFontName",
                                "AlegreyaSans-Regular.ttf"
                            );
                            const float fontPixelHeight = glm::clamp(
                                getRegistryFloat(baseSystem, "BookInspectFontPixelHeight", 28.0f),
                                10.0f,
                                72.0f
                            );
                            const int maxChars = std::clamp(
                                RenderInitSystemLogic::getRegistryInt(baseSystem, "BookInspectFontMaxCharsPerLine", 24),
                                8,
                                64
                            );
                            const int maxLines = std::clamp(
                                RenderInitSystemLogic::getRegistryInt(baseSystem, "BookInspectFontMaxLines", 11),
                                2,
                                32
                            );

                            struct BookTextBitmap {
                                std::vector<unsigned char> alpha;
                                int width = 0;
                                int height = 0;
                                int wrappedLines = 0;
                            };
                            static std::unordered_map<std::string, BookTextBitmap> s_bookTextBitmapCache;

                            const std::string cacheKey = fontName + "|" + std::to_string(static_cast<int>(std::round(fontPixelHeight)))
                                + "|" + std::to_string(maxChars)
                                + "|" + std::to_string(maxLines)
                                + "|" + text;
                            BookTextBitmap* bitmap = nullptr;
                            auto cacheIt = s_bookTextBitmapCache.find(cacheKey);
                            if (cacheIt == s_bookTextBitmapCache.end()) {
                                BookTextBitmap generated;
                                if (FontSystemLogic::RasterizeBookTextBitmap(
                                    baseSystem,
                                    fontName,
                                    fontPixelHeight,
                                    text,
                                    maxChars,
                                    maxLines,
                                    generated.alpha,
                                    generated.width,
                                    generated.height,
                                    generated.wrappedLines
                                ) && generated.width > 0 && generated.height > 0 && !generated.alpha.empty()) {
                                    cacheIt = s_bookTextBitmapCache.emplace(cacheKey, std::move(generated)).first;
                                }
                            }
                            if (cacheIt != s_bookTextBitmapCache.end()) {
                                bitmap = &cacheIt->second;
                            }
                            if (!bitmap || bitmap->width <= 0 || bitmap->height <= 0 || bitmap->alpha.empty()) {
                                return;
                            }

                            const glm::vec3 inkColor(0.19f, 0.16f, 0.13f);
                            const float textSurfaceOffset = 0.0011f;
                            const float frontZ = pageCenterZ + pageHalfDepth + textSurfaceOffset;
                            const float backZ = pageCenterZ - pageHalfDepth - textSurfaceOffset;

                            const float textAreaWidth = 0.188f;
                            const float textAreaHeight = 0.300f;
                            const float leftX = (-textAreaWidth * 0.5f) + jitterX;
                            const float topY = textAreaHeight * 0.5f - 0.004f;
                            const float pixelW = textAreaWidth / static_cast<float>(bitmap->width);
                            const float pixelH = textAreaHeight / static_cast<float>(bitmap->height);
                            if (pixelW <= 0.0f || pixelH <= 0.0f) return;

                            std::vector<FaceInstanceRenderData> frontInstances;
                            std::vector<FaceInstanceRenderData> backInstances;
                            const size_t reserveHint = static_cast<size_t>(bitmap->height) * 8u;
                            frontInstances.reserve(reserveHint);
                            backInstances.reserve(reserveHint);

                            for (int py = 0; py < bitmap->height; ++py) {
                                int runStart = -1;
                                for (int px = 0; px <= bitmap->width; ++px) {
                                    const bool filled = (px < bitmap->width)
                                        && (bitmap->alpha[static_cast<size_t>(py * bitmap->width + px)] > 48u);
                                    if (filled && runStart < 0) {
                                        runStart = px;
                                    }
                                    if ((!filled || px == bitmap->width) && runStart >= 0) {
                                        const int runEnd = px;
                                        const float runX0 = leftX + static_cast<float>(runStart) * pixelW;
                                        const float runX1 = leftX + static_cast<float>(runEnd) * pixelW;
                                        const float cx = (runX0 + runX1) * 0.5f;
                                        const float cy = topY - (static_cast<float>(py) + 0.5f) * pixelH;
                                        const glm::vec2 runScale(
                                            std::max(0.002f, (runX1 - runX0) * 0.94f),
                                            std::max(0.002f, pixelH * 0.92f)
                                        );
                                        FaceInstanceRenderData front{};
                                        front.position = glm::vec3(cx, cy, frontZ);
                                        front.color = inkColor;
                                        front.tileIndex = -1;
                                        front.alpha = -40.0f;
                                        front.ao = heldAo;
                                        front.scale = runScale;
                                        front.uvScale = runScale;
                                        frontInstances.push_back(front);

                                        FaceInstanceRenderData back = front;
                                        back.position.z = backZ;
                                        backInstances.push_back(back);
                                        runStart = -1;
                                    }
                                }
                            }

                            renderer.faceShader->setMat4("model", modelMat);
                            if (!frontInstances.empty()) {
                                renderer.faceShader->setInt("faceType", 4);
                                renderBackend.uploadArrayBufferData(
                                    renderer.faceInstanceVBO,
                                    frontInstances.data(),
                                    frontInstances.size() * sizeof(FaceInstanceRenderData),
                                    true
                                );
                                renderBackend.drawArraysTrianglesInstanced(0, 6, static_cast<int>(frontInstances.size()));
                            }
                            if (!backInstances.empty()) {
                                renderer.faceShader->setInt("faceType", 5);
                                renderBackend.uploadArrayBufferData(
                                    renderer.faceInstanceVBO,
                                    backInstances.data(),
                                    backInstances.size() * sizeof(FaceInstanceRenderData),
                                    true
                                );
                                renderBackend.drawArraysTrianglesInstanced(0, 6, static_cast<int>(backInstances.size()));
                            }
                        };
                        auto rotateAroundUp = [&](const glm::vec3& axisX,
                                                  const glm::vec3& axisY,
                                                  const glm::vec3& axisZ,
                                                  float radians,
                                                  glm::vec3& outX,
                                                  glm::vec3& outY,
                                                  glm::vec3& outZ) {
                            const float c = std::cos(radians);
                            const float s = std::sin(radians);
                            outX = safeNormalize(axisX * c + axisZ * s, axisX);
                            outY = axisY;
                            outZ = safeNormalize(-axisX * s + axisZ * c, axisZ);
                        };

                        const float inspectForward = glm::clamp(
                            getRegistryFloat(baseSystem, "BookInspectViewForward", 0.30f),
                            0.10f,
                            1.4f
                        );
                        const float inspectActiveForward = glm::clamp(
                            getRegistryFloat(baseSystem, "BookInspectActiveViewForward", 0.12f),
                            0.05f,
                            1.4f
                        );
                        const float inspectVertical = glm::clamp(
                            getRegistryFloat(baseSystem, "BookInspectViewVertical", -0.03f),
                            -0.8f,
                            0.8f
                        );
                        const float inspectSide = glm::clamp(
                            getRegistryFloat(baseSystem, "BookInspectViewSide", 0.04f),
                            -0.5f,
                            0.5f
                        );
                        const int maxSpread = std::max(1, RenderInitSystemLogic::getRegistryInt(baseSystem, "BookInspectMaxSpread", 8));
                        const int inspectPage = std::clamp(player.bookInspectPage, 0, maxSpread);
                        const bool inspectReadingActive = player.bookInspectActive && inspectPage > 0;
                        const float openAngle = glm::clamp(
                            getRegistryFloat(baseSystem, "BookInspectOpenAngle", 0.0f),
                            0.0f,
                            1.70f
                        );

                        const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
                        const glm::vec3 inspectRight = cameraRight;
                        const glm::vec3 inspectUp = cameraUp;
                        const glm::vec3 inspectForwardAxis = -cameraForward;
                        const float inspectForwardDistance = player.bookInspectActive
                            ? inspectActiveForward
                            : inspectForward;
                        glm::vec3 inspectCenter = player.cameraPosition
                            + cameraForward * inspectForwardDistance
                            + inspectUp * inspectVertical
                            + inspectRight * (inspectSide * handSideSign);

                        glm::vec3 flatForward(cameraForward.x, 0.0f, cameraForward.z);
                        if (glm::length(flatForward) < 1e-4f) {
                            flatForward = glm::vec3(0.0f, 0.0f, -1.0f);
                        } else {
                            flatForward = glm::normalize(flatForward);
                        }
                        glm::vec3 flatRight = glm::cross(flatForward, worldUp);
                        if (glm::length(flatRight) < 1e-4f) {
                            flatRight = cameraRight;
                        } else {
                            flatRight = glm::normalize(flatRight);
                        }
                        const float heldBookForwardOffset = glm::clamp(
                            getRegistryFloat(baseSystem, "BookHeldViewForward", 0.00f),
                            -0.8f,
                            0.8f
                        );
                        const float heldBookVerticalOffset = glm::clamp(
                            getRegistryFloat(baseSystem, "BookHeldViewVertical", 0.00f),
                            -0.8f,
                            0.8f
                        );
                        const float heldBookSideOffset = glm::clamp(
                            getRegistryFloat(baseSystem, "BookHeldViewSide", 0.00f),
                            -0.8f,
                            0.8f
                        );
                        const float heldBookYawDeg = glm::clamp(
                            getRegistryFloat(baseSystem, "BookHeldYawDeg", 0.0f),
                            -80.0f,
                            80.0f
                        );
                        glm::vec3 heldBookX(0.0f), heldBookY(0.0f), heldBookZ(0.0f);
                        rotateAroundUp(
                            flatRight,
                            worldUp,
                            -flatForward,
                            glm::radians(heldBookYawDeg * handSideSign),
                            heldBookX,
                            heldBookY,
                            heldBookZ
                        );
                        const glm::vec3 heldBookCenter = heldPos
                            + flatForward * heldBookForwardOffset
                            + worldUp * heldBookVerticalOffset
                            + flatRight * (heldBookSideOffset * handSideSign);

                        const glm::vec3 baseRight = inspectReadingActive ? inspectRight : heldBookX;
                        const glm::vec3 baseUp = inspectReadingActive ? inspectUp : heldBookY;
                        const glm::vec3 baseForwardAxis = inspectReadingActive ? inspectForwardAxis : heldBookZ;
                        const glm::vec3 baseCenter = inspectReadingActive ? inspectCenter : heldBookCenter;

                        const int coverTile = RenderInitSystemLogic::FaceTileIndexFor(baseSystem.world.get(), heldProto, 2);
                        const int coverTileIndex = coverTile >= 0 ? coverTile : -1;
                        const int pageTileIndex = 56;
                        const float bookPageAlphaTag = -40.0f;
                        const glm::vec3 coverTint = coverTile >= 0 ? glm::vec3(1.0f) : glm::vec3(0.30f, 0.23f, 0.18f);
                        const glm::vec3 pageTint(1.0f, 1.0f, 1.0f);
                        const glm::vec3 spineTint(0.22f, 0.16f, 0.13f);

                        const glm::mat4 inspectBaseModel = makeBasisModel(
                            baseCenter,
                            baseRight,
                            baseUp,
                            baseForwardAxis
                        );

                        if (!inspectReadingActive) {
                            constexpr float kPxHalf = 1.0f / 48.0f;
                            constexpr float kBinderHalf = 1.0f * kPxHalf;
                            constexpr float kPageHalf = 2.0f * kPxHalf;
                            const float layerCenterA = -5.0f * kPxHalf; // binder
                            const float layerCenterB = -2.0f * kPxHalf; // page
                            const float layerCenterC =  2.0f * kPxHalf; // page
                            const float layerCenterD =  5.0f * kPxHalf; // binder

                            drawBookCuboid(
                                glm::translate(inspectBaseModel, glm::vec3(0.0f, 0.0f, layerCenterA)),
                                glm::vec3(0.182f, 0.224f, kBinderHalf),
                                coverTileIndex,
                                coverTint,
                                1.0f
                            );
                            drawBookCuboid(
                                glm::translate(inspectBaseModel, glm::vec3(0.0f, 0.0f, layerCenterB)),
                                glm::vec3(0.172f, 0.212f, kPageHalf),
                                pageTileIndex,
                                pageTint,
                                bookPageAlphaTag
                            );
                            drawBookCuboid(
                                glm::translate(inspectBaseModel, glm::vec3(0.0f, 0.0f, layerCenterC)),
                                glm::vec3(0.172f, 0.212f, kPageHalf),
                                pageTileIndex,
                                pageTint,
                                bookPageAlphaTag
                            );
                            drawBookCuboid(
                                glm::translate(inspectBaseModel, glm::vec3(0.0f, 0.0f, layerCenterD)),
                                glm::vec3(0.182f, 0.224f, kBinderHalf),
                                coverTileIndex,
                                coverTint,
                                1.0f
                            );
                        } else {
                            drawBookCuboid(inspectBaseModel, glm::vec3(0.015f, 0.220f, 0.033f), -1, spineTint, 1.0f);

                            glm::vec3 leftX(0.0f), leftY(0.0f), leftZ(0.0f);
                            glm::vec3 rightX(0.0f), rightY(0.0f), rightZ(0.0f);
                            rotateAroundUp(baseRight, baseUp, baseForwardAxis, -openAngle, leftX, leftY, leftZ);
                            rotateAroundUp(baseRight, baseUp, baseForwardAxis, openAngle, rightX, rightY, rightZ);

                            const glm::vec3 leftCenter = baseCenter - baseRight * 0.122f + baseForwardAxis * 0.006f;
                            const glm::vec3 rightCenter = baseCenter + baseRight * 0.122f + baseForwardAxis * 0.006f;
                            const glm::mat4 leftModel = makeBasisModel(leftCenter, leftX, leftY, leftZ);
                            const glm::mat4 rightModel = makeBasisModel(rightCenter, rightX, rightY, rightZ);

                            constexpr float kOpenPxHalf = 1.0f / 48.0f;
                            constexpr float kPageCenterZ = 1.5f * kOpenPxHalf;
                            drawBookCuboid(
                                glm::translate(leftModel, glm::vec3(0.0f, 0.0f, -kPageCenterZ)),
                                glm::vec3(0.126f, 0.206f, kOpenPxHalf),
                                coverTileIndex,
                                coverTint,
                                1.0f
                            );
                            drawBookCuboid(
                                glm::translate(rightModel, glm::vec3(0.0f, 0.0f, -kPageCenterZ)),
                                glm::vec3(0.126f, 0.206f, kOpenPxHalf),
                                coverTileIndex,
                                coverTint,
                                1.0f
                            );
                            drawBookCuboid(
                                glm::translate(leftModel, glm::vec3(0.0f, 0.0f, kPageCenterZ)),
                                glm::vec3(0.118f, 0.196f, kOpenPxHalf),
                                pageTileIndex,
                                pageTint,
                                bookPageAlphaTag
                            );
                            drawBookCuboid(
                                glm::translate(rightModel, glm::vec3(0.0f, 0.0f, kPageCenterZ)),
                                glm::vec3(0.118f, 0.196f, kOpenPxHalf),
                                pageTileIndex,
                                pageTint,
                                bookPageAlphaTag
                            );

                            const std::string pageText = BookSystemLogic::ResolveBookPageText(inspectPage);
                            drawBookTextRows(leftModel, pageText, kPageCenterZ, kOpenPxHalf, 0.000f);
                            drawBookTextRows(rightModel, pageText, kPageCenterZ, kOpenPxHalf, 0.002f);
                        }

                        setCullEnabled(false);
                        renderer.faceShader->setMat4("model", glm::mat4(1.0f));
                        drewTextured = true;
                    } else
                    // Match in-world petal-pile profile while held: a thin floor mat, not crossed foliage cards.
                    if (heldIsPetalPile) {
                        constexpr float kHalf1 = 1.0f / 48.0f;
                        constexpr float kHalf24 = 24.0f / 48.0f;
                        const glm::vec3 petalHalfExtents(kHalf24, kHalf1, kHalf24);
                        const float petalCenterYOffset = glm::clamp(
                            getRegistryFloat(baseSystem, "HeldPetalPileViewVertical", -0.02f),
                            -1.0f,
                            1.0f
                        );
                        const glm::vec3 petalCenter = heldPos + glm::vec3(0.0f, petalCenterYOffset, 0.0f);

                        for (int faceType = 0; faceType < 6; ++faceType) {
                            const glm::vec3 normal = (faceType == 0) ? glm::vec3(1.0f, 0.0f, 0.0f)
                                : (faceType == 1) ? glm::vec3(-1.0f, 0.0f, 0.0f)
                                : (faceType == 2) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                : (faceType == 3) ? glm::vec3(0.0f, -1.0f, 0.0f)
                                : (faceType == 4) ? glm::vec3(0.0f, 0.0f, 1.0f)
                                : glm::vec3(0.0f, 0.0f, -1.0f);

                            float halfExtent = 0.5f;
                            if (faceType == 0 || faceType == 1) halfExtent = petalHalfExtents.x;
                            else if (faceType == 2 || faceType == 3) halfExtent = petalHalfExtents.y;
                            else if (faceType == 4 || faceType == 5) halfExtent = petalHalfExtents.z;

                            float uScale = 1.0f;
                            float vScale = 1.0f;
                            if (faceType == 0 || faceType == 1) {
                                uScale = petalHalfExtents.z * 2.0f;
                                vScale = petalHalfExtents.y * 2.0f;
                            } else if (faceType == 2 || faceType == 3) {
                                uScale = petalHalfExtents.x * 2.0f;
                                vScale = petalHalfExtents.z * 2.0f;
                            } else if (faceType == 4 || faceType == 5) {
                                uScale = petalHalfExtents.x * 2.0f;
                                vScale = petalHalfExtents.y * 2.0f;
                            }

                            FaceInstanceRenderData heldFace;
                            heldFace.position = petalCenter + normal * halfExtent;
                            heldFace.tileIndex = RenderInitSystemLogic::FaceTileIndexFor(baseSystem.world.get(), heldProto, faceType);
                            heldFace.color = (heldFace.tileIndex >= 0) ? glm::vec3(1.0f) : player.heldBlockColor;
                            heldFace.alpha = 1.0f;
                            heldFace.ao = heldAo;
                            heldFace.scale = glm::vec2(uScale, vScale);
                            heldFace.uvScale = heldFace.scale;
                            renderer.faceShader->setInt("faceType", faceType);
                            renderBackend.uploadArrayBufferData(renderer.faceInstanceVBO, &heldFace, sizeof(FaceInstanceRenderData), true);
                            renderBackend.drawArraysTrianglesInstanced(0, 6, 1);
                        }
                        setCullEnabled(false);
                        drewTextured = true;
                    } else if (heldIsPlant) {
                        static const std::array<int, 4> kPlantFaces = {0, 1, 4, 5};
                        float alphaMode = -2.0f;
                        if (heldIsCavePot) alphaMode = -10.0f;
                        else if (heldIsFlower) alphaMode = -3.0f;
                        else if (heldIsShortGrass) alphaMode = -2.3f;
                        const int resolvedHeldPlantTile = RenderInitSystemLogic::FaceTileIndexFor(baseSystem.world.get(), heldProto, 2);
                        const int heldPlantBaseTile = (heldIsFlower && resolvedHeldPlantTile < 0)
                            ? -1
                            : resolvedHeldPlantTile;
                        const glm::vec3 heldPlantTint = (heldPlantBaseTile >= 0) ? glm::vec3(1.0f) : player.heldBlockColor;
                        glm::vec2 plantScale = glm::vec2(1.0f);
                        if (heldIsFlower && heldPlantBaseTile < 0) plantScale = glm::vec2(0.86f, 0.92f);
                        for (int faceType : kPlantFaces) {
                            FaceInstanceRenderData heldFace;
                            heldFace.position = heldPos;
                            heldFace.color = heldPlantTint;
                            heldFace.tileIndex = heldPlantBaseTile;
                            heldFace.alpha = alphaMode;
                            heldFace.ao = heldAo;
                            heldFace.scale = plantScale;
                            heldFace.uvScale = glm::vec2(1.0f);
                            renderer.faceShader->setInt("faceType", faceType);
                            renderBackend.uploadArrayBufferData(renderer.faceInstanceVBO, &heldFace, sizeof(FaceInstanceRenderData), true);
                            renderBackend.drawArraysTrianglesInstanced(0, 6, 1);
                        }
                        setCullEnabled(false);
                        drewTextured = true;
                    } else if (heldIsStonePebble && heldIsSurfaceStonePebble) {
                        const int pileCount = decodeSurfaceStonePileCount(player.heldPackedColor);
                        const StonePebblePilePieces pile = stonePebblePilePiecesForCell(heldSeedCell, pileCount);
                        for (int piece = 0; piece < pile.count; ++piece) {
                            const glm::vec2 pieceOffset = pile.offsets[static_cast<size_t>(piece)];
                            const NarrowHalfExtents pieceExt = pile.halfExtents[static_cast<size_t>(piece)];
                            glm::vec3 pieceCenter = heldPos;
                            pieceCenter.x += pieceOffset.x;
                            pieceCenter.z += pieceOffset.y;
                            pieceCenter.y += (-0.5f + pieceExt.y + 0.01f);
                            for (int faceType = 0; faceType < 6; ++faceType) {
                                const glm::vec3 normal = (faceType == 0) ? glm::vec3(1.0f, 0.0f, 0.0f)
                                    : (faceType == 1) ? glm::vec3(-1.0f, 0.0f, 0.0f)
                                    : (faceType == 2) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                    : (faceType == 3) ? glm::vec3(0.0f, -1.0f, 0.0f)
                                    : (faceType == 4) ? glm::vec3(0.0f, 0.0f, 1.0f)
                                    : glm::vec3(0.0f, 0.0f, -1.0f);
                                float halfExtent = 0.5f;
                                if (faceType == 0 || faceType == 1) halfExtent = pieceExt.x;
                                else if (faceType == 2 || faceType == 3) halfExtent = pieceExt.y;
                                else if (faceType == 4 || faceType == 5) halfExtent = pieceExt.z;
                                glm::vec2 faceScale(1.0f);
                                if (faceType == 0 || faceType == 1) {
                                    faceScale = glm::vec2(pieceExt.z * 2.0f, pieceExt.y * 2.0f);
                                } else if (faceType == 2 || faceType == 3) {
                                    faceScale = glm::vec2(pieceExt.x * 2.0f, pieceExt.z * 2.0f);
                                } else {
                                    faceScale = glm::vec2(pieceExt.x * 2.0f, pieceExt.y * 2.0f);
                                }

                                FaceInstanceRenderData heldFace;
                                heldFace.position = pieceCenter + normal * halfExtent;
                                heldFace.tileIndex = RenderInitSystemLogic::FaceTileIndexFor(baseSystem.world.get(), heldProto, faceType);
                                heldFace.color = (heldFace.tileIndex >= 0) ? glm::vec3(1.0f) : player.heldBlockColor;
                                heldFace.alpha = 1.0f;
                                heldFace.ao = heldAo;
                                heldFace.scale = faceScale;
                                heldFace.uvScale = faceScale;
                                renderer.faceShader->setInt("faceType", faceType);
                                renderBackend.uploadArrayBufferData(renderer.faceInstanceVBO, &heldFace, sizeof(FaceInstanceRenderData), true);
                                renderBackend.drawArraysTrianglesInstanced(0, 6, 1);
                            }
                        }
                        setCullEnabled(false);
                        drewTextured = true;
                    } else if (heldIsGrassCover) {
                        constexpr float kDotHalf = 1.0f / 48.0f;
                        constexpr float kGrassCoverAlpha = -14.0f;
                        const GrassCoverDots dots = grassCoverDotsForCell(heldSeedCell);
                        for (int dot = 0; dot < dots.count; ++dot) {
                            const glm::vec2 offset = dots.offsets[static_cast<size_t>(dot)];
                            glm::vec3 dotCenter = heldPos;
                            dotCenter.x += offset.x;
                            dotCenter.y += (-0.5f + kDotHalf + 0.01f);
                            dotCenter.z += offset.y;
                            for (int faceType = 0; faceType < 6; ++faceType) {
                                const glm::vec3 normal = (faceType == 0) ? glm::vec3(1.0f, 0.0f, 0.0f)
                                    : (faceType == 1) ? glm::vec3(-1.0f, 0.0f, 0.0f)
                                    : (faceType == 2) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                    : (faceType == 3) ? glm::vec3(0.0f, -1.0f, 0.0f)
                                    : (faceType == 4) ? glm::vec3(0.0f, 0.0f, 1.0f)
                                    : glm::vec3(0.0f, 0.0f, -1.0f);
                                FaceInstanceRenderData heldFace;
                                heldFace.position = dotCenter + normal * kDotHalf;
                                heldFace.tileIndex = RenderInitSystemLogic::FaceTileIndexFor(baseSystem.world.get(), heldProto, faceType);
                                heldFace.color = (heldFace.tileIndex >= 0) ? glm::vec3(1.0f) : player.heldBlockColor;
                                heldFace.alpha = kGrassCoverAlpha;
                                heldFace.ao = heldAo;
                                heldFace.scale = glm::vec2(kDotHalf * 2.0f);
                                heldFace.uvScale = heldFace.scale;
                                renderer.faceShader->setInt("faceType", faceType);
                                renderBackend.uploadArrayBufferData(renderer.faceInstanceVBO, &heldFace, sizeof(FaceInstanceRenderData), true);
                                renderBackend.drawArraysTrianglesInstanced(0, 6, 1);
                            }
                        }
                        setCullEnabled(false);
                        drewTextured = true;
                    } else {
                        const bool ceilingAlongX = (heldProto.name == "CeilingStoneTexX"
                            || heldProto.name == "CeilingStoneTexPosX"
                            || heldProto.name == "CeilingStoneTexNegX");
                        const bool ceilingAlongZ = (heldProto.name == "CeilingStoneTexZ"
                            || heldProto.name == "CeilingStoneTexPosZ"
                            || heldProto.name == "CeilingStoneTexNegZ");
                        const bool narrowAlongX = (heldProto.name == "StickTexX"
                            || heldProto.name == "StickWinterTexX"
                            || isGrassCoverXName(heldProto.name)
                            || isStonePebbleXName(heldProto.name)
                            || ceilingAlongX);
                        const bool narrowAlongZ = (heldProto.name == "StickTexZ"
                            || heldProto.name == "StickWinterTexZ"
                            || isGrassCoverZName(heldProto.name)
                            || isStonePebbleZName(heldProto.name)
                            || ceilingAlongZ);
                        const float half1 = 1.0f / 48.0f;
                        const float half2 = 2.0f / 48.0f;
                        const float half6 = 6.0f / 48.0f;
                        const float half12 = 12.0f / 48.0f;
                        glm::vec3 narrowHalfExtents(0.5f);
                        if (heldIsStick) {
                            narrowHalfExtents = narrowAlongX
                                ? glm::vec3(half12, half1, half1)
                                : (narrowAlongZ ? glm::vec3(half1, half1, half12) : glm::vec3(0.5f));
                        } else if (heldIsGrassCover) {
                            narrowHalfExtents = glm::vec3(0.5f, half1, 0.5f);
                        } else if (heldIsStonePebble) {
                            if (heldIsWallStone) {
                                // Wall-stone variant is a rotated pebble profile.
                                narrowHalfExtents = glm::vec3(half2, half6, half2);
                            } else {
                                narrowHalfExtents = narrowAlongX
                                    ? glm::vec3(half6, half2, half2)
                                    : (narrowAlongZ ? glm::vec3(half2, half2, half6) : glm::vec3(0.5f));
                            }
                        }
                        for (int faceType = 0; faceType < 6; ++faceType) {
                            glm::vec3 facePos = heldPos + kFaceOffsets[faceType];
                            glm::vec2 faceScale(1.0f);
                            glm::vec2 faceUvScale(1.0f);
                            if (heldIsNarrowProp) {
                                float halfExtent = 0.5f;
                                if (faceType == 0 || faceType == 1) halfExtent = narrowHalfExtents.x;
                                else if (faceType == 2 || faceType == 3) halfExtent = narrowHalfExtents.y;
                                else if (faceType == 4 || faceType == 5) halfExtent = narrowHalfExtents.z;
                                const glm::vec3 normal = (faceType == 0) ? glm::vec3(1.0f, 0.0f, 0.0f)
                                    : (faceType == 1) ? glm::vec3(-1.0f, 0.0f, 0.0f)
                                    : (faceType == 2) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                    : (faceType == 3) ? glm::vec3(0.0f, -1.0f, 0.0f)
                                    : (faceType == 4) ? glm::vec3(0.0f, 0.0f, 1.0f)
                                    : glm::vec3(0.0f, 0.0f, -1.0f);
                                facePos = heldPos + normal * halfExtent;

                                float uScale = 1.0f;
                                float vScale = 1.0f;
                                if (faceType == 0 || faceType == 1) {
                                    uScale = narrowHalfExtents.z * 2.0f;
                                    vScale = narrowHalfExtents.y * 2.0f;
                                } else if (faceType == 2 || faceType == 3) {
                                    uScale = narrowHalfExtents.x * 2.0f;
                                    vScale = narrowHalfExtents.z * 2.0f;
                                } else if (faceType == 4 || faceType == 5) {
                                    uScale = narrowHalfExtents.x * 2.0f;
                                    vScale = narrowHalfExtents.y * 2.0f;
                                }
                                faceScale = glm::vec2(uScale, vScale);
                                faceUvScale = faceScale;
                            }

                            FaceInstanceRenderData heldFace;
                            heldFace.position = facePos;
                            int heldTileIndex = heldIsLeaf ? -1 : RenderInitSystemLogic::FaceTileIndexFor(baseSystem.world.get(), heldProto, faceType);
                            heldFace.color = (heldTileIndex >= 0) ? glm::vec3(1.0f) : player.heldBlockColor;
                            heldFace.tileIndex = heldTileIndex;
                            heldFace.alpha = heldIsLeaf ? -1.0f : 1.0f;
                            heldFace.ao = heldAo;
                            heldFace.scale = faceScale;
                            heldFace.uvScale = faceUvScale;
                            renderer.faceShader->setInt("faceType", faceType);
                            renderBackend.uploadArrayBufferData(renderer.faceInstanceVBO, &heldFace, sizeof(FaceInstanceRenderData), true);
                            renderBackend.drawArraysTrianglesInstanced(0, 6, 1);
                        }
                        setCullEnabled(false);
                        drewTextured = true;
                    }
                }
            }
            if (!drewTextured) {
                InstanceData heldInstance;
                heldInstance.position = heldPos;
                heldInstance.color = player.heldBlockColor * heldLightFactor;
                int behaviorIndex = static_cast<int>(RenderBehavior::STATIC_DEFAULT);
                renderer.blockShader->use();
                renderer.blockShader->setMat4("view", view);
                renderer.blockShader->setMat4("projection", projection);
                renderer.blockShader->setVec3("cameraPos", playerPos);
                renderer.blockShader->setFloat("time", time);
                renderer.blockShader->setFloat("instanceScale", 1.0f);
                renderer.blockShader->setVec3("lightDir", lightDir);
                renderer.blockShader->setVec3("ambientLight", ambientLightColor);
                renderer.blockShader->setVec3("diffuseLight", diffuseLightColor);
                renderer.blockShader->setInt("voxelGridLinesEnabled", voxelGridLinesEnabled ? 1 : 0);
                renderer.blockShader->setInt("voxelGridLineInvertColorEnabled", voxelGridLineInvertColorEnabled ? 1 : 0);
                renderer.blockShader->setMat4("model", glm::mat4(1.0f));
                renderer.blockShader->setInt("behaviorType", behaviorIndex);
                BlockChargeSystemLogic::ApplyBlockDamageMaskUniforms(baseSystem, prototypes, *renderer.blockShader, false);
                renderBackend.bindVertexArray(renderer.behaviorVAOs[behaviorIndex]);
                renderBackend.uploadArrayBufferData(renderer.behaviorInstanceVBOs[behaviorIndex], &heldInstance, sizeof(InstanceData), true);
                renderBackend.drawArraysTrianglesInstanced(0, 36, 1);
            }
            renderedAny = true;
        };

        if (!mapViewActive) {
            const bool savedHolding = player.isHoldingBlock;
            const int savedPrototypeID = player.heldPrototypeID;
            const glm::vec3 savedColor = player.heldBlockColor;
            const uint32_t savedPackedColor = player.heldPackedColor;
            const bool savedHasSourceCell = player.heldHasSourceCell;
            const glm::ivec3 savedSourceCell = player.heldSourceCell;

            bool renderedAnyHeld = false;

            player.isHoldingBlock = player.rightHandHoldingBlock;
            player.heldPrototypeID = player.rightHandHeldPrototypeID;
            player.heldBlockColor = player.rightHandHeldBlockColor;
            player.heldPackedColor = player.rightHandHeldPackedColor;
            player.heldHasSourceCell = player.rightHandHeldHasSourceCell;
            player.heldSourceCell = player.rightHandHeldSourceCell;
            renderHeldItemForCurrentState(false, renderedAnyHeld);

            player.isHoldingBlock = player.leftHandHoldingBlock;
            player.heldPrototypeID = player.leftHandHeldPrototypeID;
            player.heldBlockColor = player.leftHandHeldBlockColor;
            player.heldPackedColor = player.leftHandHeldPackedColor;
            player.heldHasSourceCell = player.leftHandHeldHasSourceCell;
            player.heldSourceCell = player.leftHandHeldSourceCell;
            renderHeldItemForCurrentState(true, renderedAnyHeld);

            if (!renderedAnyHeld && savedHolding && savedPrototypeID >= 0) {
                player.isHoldingBlock = savedHolding;
                player.heldPrototypeID = savedPrototypeID;
                player.heldBlockColor = savedColor;
                player.heldPackedColor = savedPackedColor;
                player.heldHasSourceCell = savedHasSourceCell;
                player.heldSourceCell = savedSourceCell;
                renderHeldItemForCurrentState(player.buildMode == BuildModeType::PickupLeft, renderedAnyHeld);
            }

            player.isHoldingBlock = savedHolding;
            player.heldPrototypeID = savedPrototypeID;
            player.heldBlockColor = savedColor;
            player.heldPackedColor = savedPackedColor;
            player.heldHasSourceCell = savedHasSourceCell;
            player.heldSourceCell = savedSourceCell;
        }

        const bool renderHeldPickaxe = player.pickaxeHeld;
        if (renderHeldPickaxe
            && renderer.faceShader
            && renderer.faceVAO
            && renderer.faceInstanceVBO) {
            const Entity* pickaxeStickProto = nullptr;
            std::array<const Entity*, 4> pickaxeHeadByKind = {nullptr, nullptr, nullptr, nullptr};
            const Entity* pickaxeHeadFallbackProto = nullptr;
            for (const auto& proto : prototypes) {
                if (!proto.useTexture) continue;
                if (!pickaxeStickProto && (proto.name == "StickTexX" || proto.name == "FirLog1Tex")) {
                    pickaxeStickProto = &proto;
                }
                if (!pickaxeHeadByKind[0] && proto.name == "StonePebbleRubyTexX") {
                    pickaxeHeadByKind[0] = &proto;
                }
                if (!pickaxeHeadByKind[1] && proto.name == "StonePebbleAmethystTexX") {
                    pickaxeHeadByKind[1] = &proto;
                }
                if (!pickaxeHeadByKind[2] && proto.name == "StonePebbleFlouriteTexX") {
                    pickaxeHeadByKind[2] = &proto;
                }
                if (!pickaxeHeadByKind[3] && proto.name == "StonePebbleSilverTexX") {
                    pickaxeHeadByKind[3] = &proto;
                }
                if (!pickaxeHeadFallbackProto && proto.name == "StonePebbleTexX") {
                    pickaxeHeadFallbackProto = &proto;
                }
            }
            if (!pickaxeStickProto) pickaxeStickProto = pickaxeHeadFallbackProto;
            if (!pickaxeHeadFallbackProto) pickaxeHeadFallbackProto = pickaxeStickProto;
            for (const Entity*& proto : pickaxeHeadByKind) {
                if (!proto) proto = pickaxeHeadFallbackProto;
            }

            auto normalizeOrDefault = [](const glm::vec3& v, const glm::vec3& fallback) -> glm::vec3 {
                if (glm::length(v) < 1e-4f) return fallback;
                return glm::normalize(v);
            };
            auto projectDirectionOnSurface = [&](const glm::vec3& direction, const glm::vec3& surfaceNormal) -> glm::vec3 {
                glm::vec3 n = normalizeOrDefault(surfaceNormal, glm::vec3(0.0f, 1.0f, 0.0f));
                glm::vec3 projected = direction - n * glm::dot(direction, n);
                if (glm::length(projected) < 1e-4f) {
                    projected = glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f));
                    if (glm::length(projected) < 1e-4f) {
                        projected = glm::cross(n, glm::vec3(1.0f, 0.0f, 0.0f));
                    }
                }
                return normalizeOrDefault(projected, glm::vec3(1.0f, 0.0f, 0.0f));
            };
            auto buildOrientedModel = [&](const glm::vec3& center,
                                          const glm::vec3& axisY,
                                          const glm::vec3& upHint) -> glm::mat4 {
                glm::vec3 yAxis = normalizeOrDefault(axisY, glm::vec3(1.0f, 0.0f, 0.0f));
                glm::vec3 xAxis = glm::cross(upHint, yAxis);
                if (glm::length(xAxis) < 1e-4f) xAxis = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), yAxis);
                xAxis = normalizeOrDefault(xAxis, glm::vec3(0.0f, 0.0f, 1.0f));
                glm::vec3 zAxis = normalizeOrDefault(glm::cross(xAxis, yAxis), glm::vec3(0.0f, 1.0f, 0.0f));
                glm::mat4 rot(1.0f);
                rot[0] = glm::vec4(xAxis, 0.0f);
                rot[1] = glm::vec4(yAxis, 0.0f);
                rot[2] = glm::vec4(zAxis, 0.0f);
                return glm::translate(glm::mat4(1.0f), center) * rot;
            };

            renderer.faceShader->use();
            renderer.faceShader->setMat4("view", view);
            renderer.faceShader->setMat4("projection", projection);
            renderer.faceShader->setMat4("model", glm::mat4(1.0f));
            renderer.faceShader->setVec3("cameraPos", playerPos);
            renderer.faceShader->setFloat("time", time);
            renderer.faceShader->setVec3("lightDir", lightDir);
            renderer.faceShader->setVec3("ambientLight", ambientLightColor);
            renderer.faceShader->setVec3("diffuseLight", diffuseLightColor);
            renderer.faceShader->setInt("faceType", 0);
            renderer.faceShader->setInt("sectionTier", 0);
            renderer.faceShader->setInt("leafOpaqueOutsideTier0", leafOpaqueOutsideTier0 ? 1 : 0);
            renderer.faceShader->setInt("leafBackfacesWhenInside", 0);
            renderer.faceShader->setInt("foliageWindEnabled", foliageWindAnimationEnabled ? 1 : 0);
            renderer.faceShader->setInt("waterCascadeBrightnessEnabled", waterCascadeBrightnessEnabled ? 1 : 0);
            renderer.faceShader->setInt("maskedFoliagePassMode", 0);
            renderer.faceShader->setFloat("waterCascadeBrightnessStrength", waterCascadeBrightnessStrength);
            renderer.faceShader->setFloat("waterCascadeBrightnessSpeed", waterCascadeBrightnessSpeed);
            renderer.faceShader->setFloat("waterCascadeBrightnessScale", waterCascadeBrightnessScale);
            renderer.faceShader->setInt("wireframeDebug", 0);
            bindFaceTextureUniforms(*renderer.faceShader, true);
            BlockChargeSystemLogic::ApplyBlockDamageMaskUniforms(baseSystem, prototypes, *renderer.faceShader, false);

            setCullBackFaceCCWEnabled(true);
            renderBackend.bindVertexArray(renderer.faceVAO);

            auto drawCuboid = [&](const glm::mat4& model,
                                  const Entity* textureProto,
                                  int fallbackTileIndex,
                                  const glm::vec3& localCenter,
                                  const glm::vec3& halfExtents,
                                  const glm::vec3& tintColor = glm::vec3(1.0f)) {
                if (!textureProto) return;
                renderer.faceShader->setMat4("model", model);
                for (int faceType = 0; faceType < 6; ++faceType) {
                    glm::vec3 normal = (faceType == 0) ? glm::vec3(1.0f, 0.0f, 0.0f)
                        : (faceType == 1) ? glm::vec3(-1.0f, 0.0f, 0.0f)
                        : (faceType == 2) ? glm::vec3(0.0f, 1.0f, 0.0f)
                        : (faceType == 3) ? glm::vec3(0.0f, -1.0f, 0.0f)
                        : (faceType == 4) ? glm::vec3(0.0f, 0.0f, 1.0f)
                        : glm::vec3(0.0f, 0.0f, -1.0f);
                    float normalExtent = (faceType == 0 || faceType == 1) ? halfExtents.x
                        : (faceType == 2 || faceType == 3) ? halfExtents.y
                        : halfExtents.z;
                    glm::vec3 facePos = localCenter + normal * normalExtent;
                    glm::vec2 faceScale(1.0f);
                    if (faceType == 0 || faceType == 1) {
                        faceScale = glm::vec2(halfExtents.z * 2.0f, halfExtents.y * 2.0f);
                    } else if (faceType == 2 || faceType == 3) {
                        faceScale = glm::vec2(halfExtents.x * 2.0f, halfExtents.z * 2.0f);
                    } else {
                        faceScale = glm::vec2(halfExtents.x * 2.0f, halfExtents.y * 2.0f);
                    }
                    FaceInstanceRenderData face;
                    face.position = facePos;
                    face.color = tintColor;
                    int tile = RenderInitSystemLogic::FaceTileIndexFor(baseSystem.world.get(), *textureProto, faceType);
                    face.tileIndex = (tile >= 0) ? tile : fallbackTileIndex;
                    face.alpha = 1.0f;
                    face.ao = glm::vec4(1.0f);
                    face.scale = faceScale;
                    face.uvScale = faceScale;
                    renderer.faceShader->setInt("faceType", faceType);
                    renderBackend.uploadArrayBufferData(renderer.faceInstanceVBO, &face, sizeof(FaceInstanceRenderData), true);
                    renderBackend.drawArraysTrianglesInstanced(0, 6, 1);
                }
            };

            constexpr float kHandleLength = 12.0f / 24.0f;
            constexpr glm::vec3 kHandleHalf = glm::vec3(1.0f / 48.0f, kHandleLength * 0.5f, 1.0f / 48.0f);
            constexpr float kHeadVoxelHalf = 0.5f / 24.0f;
            auto resolvePickaxeHeadProto = [&](int gemKind) -> const Entity* {
                const int clampedKind = glm::clamp(gemKind, 0, 3);
                const Entity* proto = pickaxeHeadByKind[static_cast<size_t>(clampedKind)];
                return proto ? proto : pickaxeHeadFallbackProto;
            };

            glm::vec3 forward(0.0f), right(0.0f), up(0.0f);
            forward.x = std::cos(glm::radians(player.cameraYaw)) * std::cos(glm::radians(player.cameraPitch));
            forward.y = std::sin(glm::radians(player.cameraPitch));
            forward.z = std::sin(glm::radians(player.cameraYaw)) * std::cos(glm::radians(player.cameraPitch));
            forward = normalizeOrDefault(forward, glm::vec3(0.0f, 0.0f, -1.0f));
            right = normalizeOrDefault(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
            up = normalizeOrDefault(glm::cross(right, forward), glm::vec3(0.0f, 1.0f, 0.0f));

            float chargePull = 0.0f;
            if (player.isChargingBlock && player.blockChargeAction == BlockChargeAction::Destroy) {
                chargePull = glm::clamp(player.blockChargeValue, 0.0f, 1.0f);
            }

            glm::vec3 handleBase = player.cameraPosition
                + forward * (0.20f - 0.15f * chargePull)
                + right * (0.17f + 0.02f * chargePull)
                + glm::vec3(0.0f, 0.13f - 0.04f * chargePull, 0.0f);
            glm::vec3 handleDir = normalizeOrDefault(
                up * (0.95f + 0.05f * chargePull)
                + forward * (0.30f - 0.18f * chargePull)
                + right * (0.09f + 0.02f * chargePull),
                up
            );
            glm::vec3 center = handleBase + handleDir * (kHandleLength * 0.5f);
            glm::mat4 model = buildOrientedModel(center, handleDir, right);
            const float heldPickaxeHeadShortOffset = glm::clamp(
                getRegistryFloat(baseSystem, "HeldPickaxeHeadShortOffset", -1.0f / 48.0f),
                -6.0f / 48.0f,
                6.0f / 48.0f
            );
            const float headAnchorY = glm::clamp(
                getRegistryFloat(baseSystem, "HeldPickaxeHeadAnchorY", 3.0f / 24.0f),
                -0.5f,
                0.5f
            );

            const Entity* headProto = resolvePickaxeHeadProto(player.pickaxeGemKind);
            const std::vector<glm::ivec3>& headVoxelsSrc = player.pickaxeHeadVoxels;
            std::vector<glm::ivec3> fallbackHead;
            if (headVoxelsSrc.empty()) {
                fallbackHead.emplace_back(0, 0, 0);
            }
            const std::vector<glm::ivec3>& headVoxels = headVoxelsSrc.empty() ? fallbackHead : headVoxelsSrc;
            int minX = std::numeric_limits<int>::max();
            int minY = std::numeric_limits<int>::max();
            int minZ = std::numeric_limits<int>::max();
            int maxX = std::numeric_limits<int>::min();
            int maxY = std::numeric_limits<int>::min();
            int maxZ = std::numeric_limits<int>::min();
            for (const glm::ivec3& c : headVoxels) {
                minX = std::min(minX, c.x); maxX = std::max(maxX, c.x);
                minY = std::min(minY, c.y); maxY = std::max(maxY, c.y);
                minZ = std::min(minZ, c.z); maxZ = std::max(maxZ, c.z);
            }
            const float centerX = 0.5f * static_cast<float>(minX + maxX);
            const float centerY = 0.5f * static_cast<float>(minY + maxY);
            const float centerZ = 0.5f * static_cast<float>(minZ + maxZ);

            setDepthTestEnabled(false);
            drawCuboid(model, pickaxeStickProto, 12, glm::vec3(0.0f), kHandleHalf);
            for (const glm::ivec3& cell : headVoxels) {
                const glm::vec3 centeredCell(
                    -(static_cast<float>(cell.x) - centerX),
                    (static_cast<float>(cell.y) - centerY),
                    (static_cast<float>(cell.z) - centerZ)
                );
                const glm::vec3 localCenter = centeredCell * (1.0f / 24.0f)
                    + glm::vec3(0.0f, headAnchorY, heldPickaxeHeadShortOffset);
                drawCuboid(
                    model,
                    headProto,
                    6,
                    localCenter,
                    glm::vec3(kHeadVoxelHalf),
                    glm::vec3(1.0f)
                );
            }
            setDepthTestEnabled(true);

            setCullEnabled(false);
            renderer.faceShader->setMat4("model", glm::mat4(1.0f));
        }

}

} // namespace WorldRenderSystemLogic
