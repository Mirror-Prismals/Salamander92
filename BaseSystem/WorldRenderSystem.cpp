namespace {
    enum class DebugSlopeDir {
        PosX,
        NegX,
        PosZ,
        NegZ
    };

    bool tryParseDebugSlopeDir(const std::string& name, DebugSlopeDir& outDir) {
        if (name.find("PosX") != std::string::npos) { outDir = DebugSlopeDir::PosX; return true; }
        if (name.find("NegX") != std::string::npos) { outDir = DebugSlopeDir::NegX; return true; }
        if (name.find("PosZ") != std::string::npos) { outDir = DebugSlopeDir::PosZ; return true; }
        if (name.find("NegZ") != std::string::npos) { outDir = DebugSlopeDir::NegZ; return true; }
        return false;
    }

    int resolveLeafPrototypeID(const std::vector<Entity>& prototypes) {
        for (const auto& proto : prototypes) {
            if (proto.name == "Leaf") return proto.prototypeID;
        }
        return -1;
    }

    bool cameraInLeaves(const VoxelWorldContext& voxelWorld, int leafPrototypeID, const glm::vec3& cameraPosition) {
        if (!voxelWorld.enabled || leafPrototypeID < 0) return false;
        static const glm::vec3 kOffsets[] = {
            glm::vec3(0.0f,  0.0f,  0.0f),
            glm::vec3(0.0f,  0.9f,  0.0f),
            glm::vec3(0.0f, -0.9f,  0.0f),
            glm::vec3(0.34f, 0.2f,  0.0f),
            glm::vec3(-0.34f,0.2f,  0.0f),
            glm::vec3(0.0f,  0.2f,  0.34f),
            glm::vec3(0.0f,  0.2f, -0.34f),
            glm::vec3(0.34f, 0.9f,  0.0f),
            glm::vec3(-0.34f,0.9f,  0.0f),
            glm::vec3(0.0f,  0.9f,  0.34f),
            glm::vec3(0.0f,  0.9f, -0.34f)
        };
        for (const glm::vec3& offset : kOffsets) {
            glm::ivec3 cell(
                static_cast<int>(std::floor(cameraPosition.x + offset.x)),
                static_cast<int>(std::floor(cameraPosition.y + offset.y)),
                static_cast<int>(std::floor(cameraPosition.z + offset.z))
            );
            if (voxelWorld.getBlockWorld(cell) == static_cast<uint32_t>(leafPrototypeID)) return true;
        }
        return false;
    }
}

namespace WorldRenderSystemLogic {

    void RenderWorld(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        if (!baseSystem.renderer || !baseSystem.world || !baseSystem.player || !baseSystem.level || !baseSystem.renderBackend) return;
        IRenderBackend& renderBackend = *baseSystem.renderBackend;
        renderBackend.clearDefaultFramebuffer(0.0f, 0.0f, 0.0f, 1.0f, true);
        PlayerContext& player = *baseSystem.player;
        WorldContext& world = *baseSystem.world;
        RendererContext& renderer = *baseSystem.renderer;
        LevelContext& level = *baseSystem.level;

        float time = static_cast<float>(PlatformInput::GetTimeSeconds());
        const bool mapViewActive = false;
        auto setDepthTestEnabled = [&](bool enabled) { renderBackend.setDepthTestEnabled(enabled); };
        auto setDepthWriteEnabled = [&](bool enabled) { renderBackend.setDepthWriteEnabled(enabled); };
        auto setBlendEnabled = [&](bool enabled) { renderBackend.setBlendEnabled(enabled); };
        auto setBlendModeAlpha = [&]() { renderBackend.setBlendModeAlpha(); };
        auto setCullEnabled = [&](bool enabled) { renderBackend.setCullEnabled(enabled); };
        auto setCullBackFaceCCW = [&]() { renderBackend.setCullBackFaceCCW(); };
        auto setCullBackFaceCCWEnabled = [&](bool enabled) {
            setCullEnabled(enabled);
            if (enabled) setCullBackFaceCCW();
        };
        glm::mat4 view = player.viewMatrix;
        glm::mat4 projection = player.projectionMatrix;
        glm::vec3 playerPos = player.cameraPosition;
        glm::vec3 cameraForward;
        cameraForward.x = cos(glm::radians(player.cameraYaw)) * cos(glm::radians(player.cameraPitch));
        cameraForward.y = sin(glm::radians(player.cameraPitch));
        cameraForward.z = sin(glm::radians(player.cameraYaw)) * cos(glm::radians(player.cameraPitch));
        cameraForward = glm::normalize(cameraForward);
        if (baseSystem.securityCamera && baseSystem.securityCamera->dawViewActive) {
            const SecurityCameraContext& securityCamera = *baseSystem.securityCamera;
            if (glm::length(securityCamera.viewForward) > 1e-4f) {
                playerPos = securityCamera.viewPosition;
                cameraForward = glm::normalize(securityCamera.viewForward);
                view = glm::lookAt(playerPos, playerPos + cameraForward, glm::vec3(0.0f, 1.0f, 0.0f));
            }
        }
        
        auto now = std::chrono::system_clock::now();
        double epochSeconds = std::chrono::duration<double>(now.time_since_epoch()).count();
        time_t ct = static_cast<time_t>(std::floor(epochSeconds));
        double subSecond = epochSeconds - static_cast<double>(ct);
        tm lt;
        #ifdef _WIN32
        localtime_s(&lt, &ct);
        #else
        localtime_r(&ct, &lt);
        #endif
        double daySeconds = static_cast<double>(lt.tm_hour) * 3600.0
                          + static_cast<double>(lt.tm_min) * 60.0
                          + static_cast<double>(lt.tm_sec)
                          + subSecond;
        float dayFraction = static_cast<float>(daySeconds / 86400.0);
        const float hour = dayFraction * 24.0f;
        const float sunU = (hour - 6.0f) / 12.0f;
        const float moonHour = (hour < 6.0f) ? (hour + 24.0f) : hour;
        const float moonU = (moonHour - 18.0f) / 12.0f;
        const float sunY = glm::max(0.0f, std::sin(sunU * 3.14159265359f));
        const float moonY = glm::max(0.0f, std::sin(moonU * 3.14159265359f));
        const float moonStrength = glm::clamp(getRegistryFloat(baseSystem, "TimeOfDayMoonlightStrength", 0.20f), 0.0f, 1.0f);
        const float dayLightFactor = glm::clamp(glm::max(sunY, moonY * moonStrength), 0.0f, 1.0f);
        const float ambientNight = glm::clamp(getRegistryFloat(baseSystem, "TimeOfDayAmbientNight", 0.08f), 0.0f, 1.0f);
        const float ambientDay = glm::clamp(getRegistryFloat(baseSystem, "TimeOfDayAmbientDay", 0.40f), ambientNight, 1.0f);
        const float diffuseNight = glm::clamp(getRegistryFloat(baseSystem, "TimeOfDayDiffuseNight", 0.10f), 0.0f, 1.0f);
        const float diffuseDay = glm::clamp(getRegistryFloat(baseSystem, "TimeOfDayDiffuseDay", 0.60f), diffuseNight, 1.0f);
        const float ambientScalar = ambientNight + (ambientDay - ambientNight) * dayLightFactor;
        const float diffuseScalar = diffuseNight + (diffuseDay - diffuseNight) * dayLightFactor;
        const glm::vec3 ambientLightColor(ambientScalar);
        const glm::vec3 diffuseLightColor(diffuseScalar);
        std::vector<glm::vec3> starPositions;
        std::vector<std::vector<InstanceData>> behaviorInstances(static_cast<int>(RenderBehavior::COUNT));
        std::vector<BranchInstanceData> branchInstances;
        std::array<std::vector<FaceInstanceRenderData>, 6> faceInstances;
        struct DebugSlopeRenderInstance {
            glm::vec3 position;
            int prototypeID = -1;
            glm::vec3 color = glm::vec3(1.0f);
            DebugSlopeDir dir = DebugSlopeDir::PosX;
        };
        std::vector<DebugSlopeRenderInstance> debugSlopeInstances;
        const bool twoSidedAlphaFaces = RenderInitSystemLogic::getRegistryBool(baseSystem, "WaterSurfaceDoubleSided", true);
        const bool leafOpaqueOutsideTier0 = true;
        const bool leafBackfacesWhenInsideEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "LeafBackfacesWhenInside", true);
        const bool foliageWindAnimationEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "FoliageWindAnimationEnabled", true);
        const bool leafFanAlignEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "LeafFanAlignEnabled", true);
        const bool voxelLightingEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "VoxelLightingEnabled", true);
        const bool foliageLightingEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "FoliageLightingEnabled", false);
        const bool leafDirectionalLightingEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "LeafDirectionalLightingEnabled", true);
        const float leafDirectionalLightingIntensity = glm::clamp(
            getRegistryFloat(baseSystem, "LeafDirectionalLightingIntensity", 1.0f),
            0.0f,
            1.0f
        );
        bool renderOpaqueFacesTwoSided = false;
        if (leafBackfacesWhenInsideEnabled && baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
            const int leafPrototypeID = resolveLeafPrototypeID(prototypes);
            // Keep leaf neighbor-face culling intact, but show the existing canopy shell from inside.
            renderOpaqueFacesTwoSided = cameraInLeaves(*baseSystem.voxelWorld, leafPrototypeID, playerPos);
        }
        const bool waterCascadeBrightnessEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "WaterCascadeBrightnessEnabled", true);
        constexpr int waterPaletteVariant = 0;
        const float waterCascadeBrightnessStrength = glm::clamp(
            getRegistryFloat(baseSystem, "WaterCascadeBrightnessStrength", 0.22f),
            0.0f,
            1.0f
        );
        const float waterCascadeBrightnessSpeed = glm::clamp(
            getRegistryFloat(baseSystem, "WaterCascadeBrightnessSpeed", 1.1f),
            0.01f,
            8.0f
        );
        const float waterCascadeBrightnessScale = glm::clamp(
            getRegistryFloat(baseSystem, "WaterCascadeBrightnessScale", 0.18f),
            0.005f,
            3.0f
        );
        const bool waterPlanarReflectionsEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "WaterPlanarReflectionsEnabled", false);
        const bool waterUnderwaterCausticsEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "WaterUnderwaterCausticsEnabled", false);
        const float waterReflectionPlaneY = getRegistryFloat(
            baseSystem,
            "WaterPlanarReflectionPlaneY",
            world.expanse.waterSurface
        );
        const bool wallStoneUvJitterEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "WallStoneUvJitterEnabled", true);
        const bool voxelGridLinesEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "VoxelGridLinesEnabled", true);
        const bool voxelGridLineInvertColorEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "VoxelGridLineInvertColorEnabled", false);
        const float wallStoneUvJitterMinPixels = glm::clamp(
            getRegistryFloat(baseSystem, "WallStoneUvJitterMinPixels", 1.0f),
            0.0f,
            24.0f
        );
        const float wallStoneUvJitterMaxPixels = glm::clamp(
            getRegistryFloat(baseSystem, "WallStoneUvJitterMaxPixels", 5.0f),
            wallStoneUvJitterMinPixels,
            24.0f
        );
        bool useVoxelRendering = baseSystem.voxelWorld && baseSystem.voxelWorld->enabled && baseSystem.voxelRender;
        const bool columnStorageMode = false;
        const bool renderChunkablesViaVoxel = useVoxelRendering;
        const bool hideHeadVisualizer = RenderInitSystemLogic::getRegistryBool(baseSystem, "PlayerHeadAudioVisualizerHidden", true);
        const int hiddenSparkleVisualizerID = (baseSystem.audio ? baseSystem.audio->sparkleRayEmitterInstanceID : -1);
        const int hiddenSparkleVisualizerWorld = (baseSystem.audio ? baseSystem.audio->sparkleRayEmitterWorldIndex : -1);
        const bool hideActiveSecurityCamera = baseSystem.securityCamera
            && baseSystem.securityCamera->dawViewActive
            && baseSystem.ui
            && baseSystem.ui->active;

        struct VisibleVoxelRenderSection {
            const ChunkRenderBuffers* buffers = nullptr;
            float dist2 = 0.0f;
        };
        std::vector<VisibleVoxelRenderSection> visibleVoxelRenderSections;
        const int voxelRenderMaxVisibleSections = std::max(
            0,
            RenderInitSystemLogic::getRegistryInt(baseSystem, "voxelRenderMaxVisibleSections", 512)
        );
        auto collectVisibleVoxelRenderSections = [&](const glm::vec3& cameraPos,
                                                     std::vector<VisibleVoxelRenderSection>& outSections) {
            outSections.clear();
            if (!useVoxelRendering || !baseSystem.voxelWorld || !baseSystem.voxelRender) return;
            VoxelWorldContext& voxelWorld = *baseSystem.voxelWorld;
            VoxelRenderContext& voxelRender = *baseSystem.voxelRender;
            outSections.reserve(voxelRender.renderBuffers.size());
            for (const auto& [sectionKey, buffers] : voxelRender.renderBuffers) {
                auto secIt = voxelWorld.sections.find(sectionKey);
                if (secIt == voxelWorld.sections.end()) continue;
                const VoxelSection& section = secIt->second;
                if (!mapViewActive
                    && !RenderInitSystemLogic::shouldRenderVoxelSection(baseSystem, section, cameraPos)) {
                    continue;
                }
                const float span = static_cast<float>(section.size);
                const glm::vec3 center(
                    (static_cast<float>(section.coord.x) + 0.5f) * span,
                    (static_cast<float>(section.coord.y) + 0.5f) * span,
                    (static_cast<float>(section.coord.z) + 0.5f) * span
                );
                const glm::vec3 delta = center - cameraPos;
                outSections.push_back({&buffers, glm::dot(delta, delta)});
            }
            if (voxelRenderMaxVisibleSections > 0
                && static_cast<int>(outSections.size()) > voxelRenderMaxVisibleSections) {
                std::sort(
                    outSections.begin(),
                    outSections.end(),
                    [](const VisibleVoxelRenderSection& a, const VisibleVoxelRenderSection& b) {
                        return a.dist2 < b.dist2;
                    }
                );
                outSections.resize(static_cast<size_t>(voxelRenderMaxVisibleSections));
            }
        };


        for (size_t worldIndex = 0; worldIndex < level.worlds.size(); ++worldIndex) {
            const auto& worldProto = level.worlds[worldIndex];
            for (const auto& instance : worldProto.instances) {
                if (instance.prototypeID < 0 || instance.prototypeID >= static_cast<int>(prototypes.size())) continue;
                const Entity& proto = prototypes[instance.prototypeID];
                if (hideHeadVisualizer
                    && worldProto.name == "PlayerHeadAudioVisualizerWorld"
                    && (proto.name == "AudioVisualizer" || instance.name == "AudioVisualizer")) {
                    continue;
                }
                if (hiddenSparkleVisualizerID > 0
                    && instance.instanceID == hiddenSparkleVisualizerID
                    && (hiddenSparkleVisualizerWorld < 0 || hiddenSparkleVisualizerWorld == static_cast<int>(worldIndex))) {
                    continue;
                }
                if (hideActiveSecurityCamera
                    && proto.name == "SecurityCamera"
                    && static_cast<int>(worldIndex) == baseSystem.securityCamera->activeWorldIndex
                    && instance.instanceID == baseSystem.securityCamera->activeInstanceID) {
                    continue;
                }
                if (proto.isStar) {
                    starPositions.push_back(instance.position);
                }
                // Chunkable terrain/foliage should render from voxel meshes when voxel mode is enabled.
                // Keeping the legacy instance draw active causes stale "ghost" visuals after voxel edits.
                const bool isThrownHeldVisual = instance.name == "__ThrownHeldBlockVisual";
                if (renderChunkablesViaVoxel && proto.isBlock && proto.isChunkable && !isThrownHeldVisual) {
                    continue;
                }
                if (isThrownHeldVisual && proto.isBlock) {
                    static const std::array<glm::vec3, 6> kThrownFaceNormals = {
                        glm::vec3(1.0f, 0.0f, 0.0f),
                        glm::vec3(-1.0f, 0.0f, 0.0f),
                        glm::vec3(0.0f, 1.0f, 0.0f),
                        glm::vec3(0.0f, -1.0f, 0.0f),
                        glm::vec3(0.0f, 0.0f, 1.0f),
                        glm::vec3(0.0f, 0.0f, -1.0f)
                    };
                    for (int faceType = 0; faceType < 6; ++faceType) {
                        FaceInstanceRenderData thrownFace;
                        thrownFace.position = instance.position + kThrownFaceNormals[static_cast<size_t>(faceType)] * 0.5f;
                        thrownFace.tileIndex = RenderInitSystemLogic::FaceTileIndexFor(baseSystem.world.get(), proto, faceType);
                        thrownFace.color = (thrownFace.tileIndex >= 0) ? glm::vec3(1.0f) : instance.color;
                        thrownFace.alpha = 1.0f;
                        thrownFace.ao = glm::vec4(1.0f);
                        thrownFace.scale = glm::vec2(1.0f);
                        thrownFace.uvScale = glm::vec2(1.0f);
                        faceInstances[faceType].push_back(thrownFace);
                    }
                    continue;
                }
                if (proto.name == "Face_PosX") { faceInstances[0].push_back({instance.position, instance.color, -1, 1.0f, glm::vec4(1.0f), glm::vec2(1.0f), glm::vec2(1.0f)}); continue; }
                if (proto.name == "Face_NegX") { faceInstances[1].push_back({instance.position, instance.color, -1, 1.0f, glm::vec4(1.0f), glm::vec2(1.0f), glm::vec2(1.0f)}); continue; }
                if (proto.name == "Face_PosY") { faceInstances[2].push_back({instance.position, instance.color, -1, 1.0f, glm::vec4(1.0f), glm::vec2(1.0f), glm::vec2(1.0f)}); continue; }
                if (proto.name == "Face_NegY") { faceInstances[3].push_back({instance.position, instance.color, -1, 1.0f, glm::vec4(1.0f), glm::vec2(1.0f), glm::vec2(1.0f)}); continue; }
                if (proto.name == "Face_PosZ") { faceInstances[4].push_back({instance.position, instance.color, -1, 1.0f, glm::vec4(1.0f), glm::vec2(1.0f), glm::vec2(1.0f)}); continue; }
                if (proto.name == "Face_NegZ") { faceInstances[5].push_back({instance.position, instance.color, -1, 1.0f, glm::vec4(1.0f), glm::vec2(1.0f), glm::vec2(1.0f)}); continue; }
                if (MiniModelSystemLogic::AppendRenderableFacesForInstance(baseSystem, proto, instance, faceInstances)) {
                    continue;
                }
                if (proto.name == "Computer" || proto.name == "SecurityCamera") {
                    static const std::array<glm::vec3, 6> kComputerFaceNormals = {
                        glm::vec3(1.0f, 0.0f, 0.0f),
                        glm::vec3(-1.0f, 0.0f, 0.0f),
                        glm::vec3(0.0f, 1.0f, 0.0f),
                        glm::vec3(0.0f, -1.0f, 0.0f),
                        glm::vec3(0.0f, 0.0f, 1.0f),
                        glm::vec3(0.0f, 0.0f, -1.0f)
                    };
                    for (int faceType = 0; faceType < 6; ++faceType) {
                        FaceInstanceRenderData face;
                        face.position = instance.position + kComputerFaceNormals[static_cast<size_t>(faceType)] * 0.5f;
                        face.color = glm::vec3(1.0f);
                        face.tileIndex = RenderInitSystemLogic::FaceTileIndexFor(baseSystem.world.get(), proto, faceType);
                        face.alpha = 1.0f;
                        face.ao = glm::vec4(1.0f);
                        face.scale = glm::vec2(1.0f);
                        face.uvScale = glm::vec2(1.0f);
                        faceInstances[faceType].push_back(face);
                    }
                    continue;
                }
                DebugSlopeDir slopeDir = DebugSlopeDir::PosX;
                if (tryParseDebugSlopeDir(proto.name, slopeDir)) {
                    debugSlopeInstances.push_back({instance.position, instance.prototypeID, instance.color, slopeDir});
                    continue;
                }
                if (proto.isRenderable && proto.isBlock) {
                    RenderBehavior behavior = RenderBehavior::STATIC_DEFAULT;
                    if (proto.name == "Branch") behavior = RenderBehavior::STATIC_BRANCH;
                    else if (proto.name == "Water") behavior = RenderBehavior::ANIMATED_WATER;
                    else if (proto.name == "TransparentWave") behavior = RenderBehavior::ANIMATED_TRANSPARENT_WAVE;
                    else if (proto.hasWireframe && proto.isAnimated) behavior = RenderBehavior::ANIMATED_WIREFRAME;
                    if (behavior == RenderBehavior::STATIC_BRANCH) branchInstances.push_back({instance.position, instance.rotation, instance.color});
                    else behaviorInstances[static_cast<int>(behavior)].push_back({instance.position, instance.color});
                }
            }
        }
        MiniModelSystemLogic::AppendWorkbenchFaces(baseSystem, faceInstances);

        if (RenderInitSystemLogic::getRegistryBool(baseSystem, "DebugVoxelRender", false) && baseSystem.voxelWorld) {
            size_t sectionCount = baseSystem.voxelWorld->sections.size();
            size_t renderCount = baseSystem.voxelRender ? baseSystem.voxelRender->renderBuffers.size() : 0;
            std::cout << "[DebugVoxelRender] sections=" << sectionCount
                      << " renderBuffers=" << renderCount
                      << " useVoxelRendering=" << (useVoxelRendering ? 1 : 0)
                      << std::endl;
        }
        glm::vec3 skyTop(0.52f, 0.66f, 0.95f);
        glm::vec3 skyBottom(0.03f, 0.06f, 0.18f);
        SkyboxSystemLogic::getCurrentSkyColors(dayFraction, world.skyKeys, skyTop, skyBottom);
        glm::vec3 lightDir;
        SkyboxSystemLogic::RenderSkyAndCelestials(baseSystem, prototypes, starPositions, time, dayFraction, view, projection, playerPos, lightDir);
        if (dt > 0.0f) {
            MiniVoxelParticleSystemLogic::UpdateParticles(baseSystem, dt, faceInstances);
        }
        // Aurora and cloud passes are intentionally removed from the active render path.

        // Establish deterministic world-state defaults before voxel/block passes.
        // WebGPU render-state starts from backend defaults each frame, so relying
        // on previous systems can leave depth testing disabled and cause severe
        // overdraw artifacts (e.g. only certain face orientations appearing).
        setDepthTestEnabled(true);
        setDepthWriteEnabled(true);
        setBlendEnabled(false);
        setCullEnabled(false);

        renderer.blockShader->use();
        renderer.blockShader->setMat4("view", view);
        renderer.blockShader->setMat4("projection", projection);
        renderer.blockShader->setVec3("cameraPos", playerPos);
        renderer.blockShader->setFloat("time", time);
        renderer.blockShader->setFloat("instanceScale", 1.0f);
        renderer.blockShader->setVec3("lightDir",lightDir);
        renderer.blockShader->setVec3("ambientLight", ambientLightColor);
        renderer.blockShader->setVec3("diffuseLight", diffuseLightColor);
        renderer.blockShader->setInt("voxelGridLinesEnabled", voxelGridLinesEnabled ? 1 : 0);
        renderer.blockShader->setInt("voxelGridLineInvertColorEnabled", voxelGridLineInvertColorEnabled ? 1 : 0);
        renderer.blockShader->setMat4("model", glm::mat4(1.0f));
        BlockChargeSystemLogic::ApplyBlockDamageMaskUniforms(baseSystem, prototypes, *renderer.blockShader, true);
        setBlendEnabled(true);
        setBlendModeAlpha();

        if (useVoxelRendering) {
            collectVisibleVoxelRenderSections(playerPos, visibleVoxelRenderSections);
        }

        for (int i = 0; i < static_cast<int>(RenderBehavior::COUNT); ++i) {
            RenderBehavior currentBehavior = static_cast<RenderBehavior>(i);
            bool translucent = (currentBehavior == RenderBehavior::ANIMATED_WATER || currentBehavior == RenderBehavior::ANIMATED_TRANSPARENT_WAVE);
            if (translucent) {
                // Let translucent passes read depth but avoid writing it so surfaces beneath stay visible.
                setDepthWriteEnabled(false);
            }
            if (useVoxelRendering) {
                for (const auto& section : visibleVoxelRenderSections) {
                    if (section.buffers->usesTexturedFaceBuffers) continue;
                    int count = section.buffers->counts[i];
                    if (count <= 0) continue;
                    renderer.blockShader->setFloat("instanceScale", 1.0f);
                    renderer.blockShader->setInt("behaviorType", i);
                    if (section.buffers->vaos[i] == 0) continue;
                    renderBackend.bindVertexArray(section.buffers->vaos[i]);
                    renderBackend.drawArraysTrianglesInstanced(0, 36, count);
                }
                renderer.blockShader->setFloat("instanceScale", 1.0f);
            }
            if (currentBehavior == RenderBehavior::STATIC_BRANCH) {
                if (!branchInstances.empty()) {
                    renderer.blockShader->setInt("behaviorType", i);
                    renderBackend.bindVertexArray(renderer.behaviorVAOs[i]);
                    renderBackend.uploadArrayBufferData(
                        renderer.behaviorInstanceVBOs[i],
                        branchInstances.data(),
                        branchInstances.size() * sizeof(BranchInstanceData),
                        true
                    );
                    renderBackend.drawArraysTrianglesInstanced(0, 36, static_cast<int>(branchInstances.size()));
                }
            } else {
                if (!behaviorInstances[i].empty()) {
                    renderer.blockShader->setInt("behaviorType", i);
                    renderBackend.bindVertexArray(renderer.behaviorVAOs[i]);
                    renderBackend.uploadArrayBufferData(
                        renderer.behaviorInstanceVBOs[i],
                        behaviorInstances[i].data(),
                        behaviorInstances[i].size() * sizeof(InstanceData),
                        true
                    );
                    renderBackend.drawArraysTrianglesInstanced(0, 36, static_cast<int>(behaviorInstances[i].size()));
                }
            }
            if (translucent) {
                setDepthWriteEnabled(true);
            }
        }

        auto bindTextureUnit2D = [&](int unit, RenderHandle texture) {
            renderBackend.bindTexture2D(texture, unit);
        };

        auto bindFaceTextureUniforms = [&](Shader& shader, bool allowPlanarReflection = true){
            auto resolveAtlasTile = [&](const std::string& textureKey) -> int {
                auto it = world.atlasMappings.find(textureKey);
                if (it == world.atlasMappings.end()) return -1;
                const FaceTextureSet& set = it->second;
                if (set.all >= 0) return set.all;
                if (set.side >= 0) return set.side;
                if (set.top >= 0) return set.top;
                if (set.bottom >= 0) return set.bottom;
                return -1;
            };
            const int wallStoneUvJitterTile0 = resolveAtlasTile("CobblestoneRYBBlue");
            const int wallStoneUvJitterTile1 = resolveAtlasTile("CobblestoneRYBRed");
            const int wallStoneUvJitterTile2 = resolveAtlasTile("CobblestoneRYBYellow");
            int grassAtlasShortBaseTile = resolveAtlasTile("24x24Zt1ConiferShortGrassSideV001");
            if (grassAtlasShortBaseTile < 0) grassAtlasShortBaseTile = resolveAtlasTile("ShortGrassV001");
            int grassAtlasTallBaseTile = resolveAtlasTile("24x24Zt1ConiferTallGrassSideV001");
            if (grassAtlasTallBaseTile < 0) grassAtlasTallBaseTile = resolveAtlasTile("TallGrassV001");

            shader.setInt("atlasEnabled", (renderer.atlasTexture != 0 && renderer.atlasTilesPerRow > 0 && renderer.atlasTilesPerCol > 0) ? 1 : 0);
            shader.setVec2("atlasTileSize", glm::vec2(renderer.atlasTileSize));
            shader.setVec2("atlasTextureSize", glm::vec2(renderer.atlasTextureSize));
            shader.setInt("tilesPerRow", renderer.atlasTilesPerRow);
            shader.setInt("tilesPerCol", renderer.atlasTilesPerCol);
            shader.setInt("grassAtlasShortBaseTile", grassAtlasShortBaseTile);
            shader.setInt("grassAtlasTallBaseTile", grassAtlasTallBaseTile);
            shader.setInt("wallStoneUvJitterEnabled", wallStoneUvJitterEnabled ? 1 : 0);
            shader.setInt("wallStoneUvJitterTile0", wallStoneUvJitterTile0);
            shader.setInt("wallStoneUvJitterTile1", wallStoneUvJitterTile1);
            shader.setInt("wallStoneUvJitterTile2", wallStoneUvJitterTile2);
            shader.setFloat("wallStoneUvJitterMinPixels", wallStoneUvJitterMinPixels);
            shader.setFloat("wallStoneUvJitterMaxPixels", wallStoneUvJitterMaxPixels);
            shader.setInt("foliageLightingEnabled", foliageLightingEnabled ? 1 : 0);
            shader.setInt("leafDirectionalLightingEnabled", leafDirectionalLightingEnabled ? 1 : 0);
            shader.setFloat("leafDirectionalLightingIntensity", leafDirectionalLightingIntensity);
            shader.setInt("voxelGridLinesEnabled", voxelGridLinesEnabled ? 1 : 0);
            shader.setInt("voxelGridLineInvertColorEnabled", voxelGridLineInvertColorEnabled ? 1 : 0);
            shader.setInt("leafFanAlignEnabled", leafFanAlignEnabled ? 1 : 0);
            shader.setInt("waterPaletteVariant", waterPaletteVariant);
            shader.setInt("waterUnderwaterCausticsEnabled", waterUnderwaterCausticsEnabled ? 1 : 0);
            shader.setVec3("topColor", skyTop);
            shader.setVec3("bottomColor", skyBottom);
            shader.setInt("atlasTexture", 0);
            const bool hasAllGrassTextures =
                renderer.grassTextureCount >= 3
                && renderer.grassTextures[0] != 0
                && renderer.grassTextures[1] != 0
                && renderer.grassTextures[2] != 0;
            const bool hasAllShortGrassTextures =
                renderer.shortGrassTextureCount >= 3
                && renderer.shortGrassTextures[0] != 0
                && renderer.shortGrassTextures[1] != 0
                && renderer.shortGrassTextures[2] != 0;
            const bool hasAllOreTextures =
                renderer.oreTextureCount >= 4
                && renderer.oreTextures[0] != 0
                && renderer.oreTextures[1] != 0
                && renderer.oreTextures[2] != 0
                && renderer.oreTextures[3] != 0;
            const bool hasAllTerrainTextures =
                renderer.terrainTextureCount >= 2
                && renderer.terrainTextures[0] != 0
                && renderer.terrainTextures[1] != 0;
            const bool waterOverlayEnabled = RenderInitSystemLogic::getRegistryBool(baseSystem, "WaterOverlayTextureEnabled", false);
            const bool hasWaterOverlayTexture = waterOverlayEnabled && renderer.waterOverlayTexture != 0;
            const bool hasWaterReflectionTexture = waterPlanarReflectionsEnabled
                && allowPlanarReflection
                && renderer.waterReflectionTex != 0;
            static int sMaxTextureUnits = -1;
            if (sMaxTextureUnits < 0) {
                sMaxTextureUnits = renderBackend.getMaxTextureImageUnits();
            }
            const bool enoughUnitsForDualGrassSets = (sMaxTextureUnits >= 7);
            const bool enoughUnitsForOreSet = (sMaxTextureUnits >= 11);
            const bool enoughUnitsForTerrainSet = (sMaxTextureUnits >= 13);
            const bool enoughUnitsForWaterOverlay = (sMaxTextureUnits >= 14);
            const bool enoughUnitsForWaterReflection = (sMaxTextureUnits >= 15);
            shader.setInt("grassTextureEnabled", hasAllGrassTextures ? 1 : 0);
            shader.setInt("grassTexture0", 1);
            shader.setInt("grassTexture1", 2);
            shader.setInt("grassTexture2", 3);
            shader.setInt("shortGrassTextureEnabled", (hasAllShortGrassTextures && enoughUnitsForDualGrassSets) ? 1 : 0);
            shader.setInt("shortGrassTexture0", 4);
            shader.setInt("shortGrassTexture1", 5);
            shader.setInt("shortGrassTexture2", 6);
            shader.setInt("oreTextureEnabled", (hasAllOreTextures && enoughUnitsForOreSet) ? 1 : 0);
            shader.setInt("oreTexture0", 7);
            shader.setInt("oreTexture1", 8);
            shader.setInt("oreTexture2", 9);
            shader.setInt("oreTexture3", 10);
            shader.setInt("terrainTextureEnabled", (hasAllTerrainTextures && enoughUnitsForTerrainSet) ? 1 : 0);
            shader.setInt("terrainTextureDirt", 11);
            shader.setInt("terrainTextureStone", 12);
            shader.setInt("waterOverlayTextureEnabled", (hasWaterOverlayTexture && enoughUnitsForWaterOverlay) ? 1 : 0);
            shader.setInt("waterOverlayTexture", 13);
            shader.setInt(
                "waterPlanarReflectionEnabled",
                (hasWaterReflectionTexture && enoughUnitsForWaterReflection) ? 1 : 0
            );
            shader.setFloat("waterReflectionPlaneY", waterReflectionPlaneY);
            shader.setInt("waterReflectionTexture", 14);
            if (hasAllGrassTextures) {
                bindTextureUnit2D(1, renderer.grassTextures[0]);
                bindTextureUnit2D(2, renderer.grassTextures[1]);
                bindTextureUnit2D(3, renderer.grassTextures[2]);
            }
            if (hasAllShortGrassTextures && enoughUnitsForDualGrassSets) {
                bindTextureUnit2D(4, renderer.shortGrassTextures[0]);
                bindTextureUnit2D(5, renderer.shortGrassTextures[1]);
                bindTextureUnit2D(6, renderer.shortGrassTextures[2]);
            }
            if (hasAllOreTextures && enoughUnitsForOreSet) {
                bindTextureUnit2D(7, renderer.oreTextures[0]);
                bindTextureUnit2D(8, renderer.oreTextures[1]);
                bindTextureUnit2D(9, renderer.oreTextures[2]);
                bindTextureUnit2D(10, renderer.oreTextures[3]);
            }
            if (hasAllTerrainTextures && enoughUnitsForTerrainSet) {
                bindTextureUnit2D(11, renderer.terrainTextures[0]);
                bindTextureUnit2D(12, renderer.terrainTextures[1]);
            }
            if (hasWaterOverlayTexture && enoughUnitsForWaterOverlay) {
                bindTextureUnit2D(13, renderer.waterOverlayTexture);
            }
            if (hasWaterReflectionTexture && enoughUnitsForWaterReflection) {
                bindTextureUnit2D(14, renderer.waterReflectionTex);
            }
            bindTextureUnit2D(0, renderer.atlasTexture);
        };

        auto drawFaceBatches = [&](const std::array<std::vector<FaceInstanceRenderData>, 6>& batches, bool depthWrite){
            if (!renderer.faceShader || !renderer.faceVAO) return;
            if (!depthWrite) setDepthWriteEnabled(false);
            setBlendEnabled(true);
            setBlendModeAlpha();
            bool enableCull = depthWrite ? !renderOpaqueFacesTwoSided : !twoSidedAlphaFaces;
            setCullBackFaceCCWEnabled(enableCull);

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
            bindFaceTextureUniforms(*renderer.faceShader);
            BlockChargeSystemLogic::ApplyBlockDamageMaskUniforms(baseSystem, prototypes, *renderer.faceShader, true);
            renderBackend.bindVertexArray(renderer.faceVAO);
            for (int faceType = 0; faceType < 6; ++faceType) {
                const auto& instances = batches[faceType];
                if (instances.empty()) continue;
                renderer.faceShader->setInt("faceType", faceType);
                renderBackend.uploadArrayBufferData(
                    renderer.faceInstanceVBO,
                    instances.data(),
                    instances.size() * sizeof(FaceInstanceRenderData),
                    true
                );
                renderBackend.drawArraysTrianglesInstanced(0, 6, static_cast<int>(instances.size()));
            }

            setCullEnabled(false);
            if (!depthWrite) setDepthWriteEnabled(true);
        };
        auto isWaterSlopeAlpha = [](float alpha) {
            return alpha <= -23.5f && alpha > -33.5f;
        };
        auto isFoliageTaggedAlpha = [](float alpha) {
            // Sentinel-tagged translucent foliage/card ranges authored by mesher:
            // short/tall grass (-2.x), flower (-3.x), cave pot (-10.x).
            // Keep leaf blocks (-1.x) on opaque path for stable depth behavior.
            if (alpha <= -1.5f && alpha > -3.5f) return true;
            if (alpha <= -9.5f && alpha > -13.5f) return true;
            return false;
        };
        auto isWaterLikeTranslucentAlpha = [&](float alpha) {
            return (alpha >= 0.0f && alpha < 0.999f) || isWaterSlopeAlpha(alpha);
        };

        const bool waterReflectionTargetReady = waterPlanarReflectionsEnabled
            && renderer.waterReflectionFBO != 0
            && renderer.waterReflectionTex != 0
            && renderer.waterReflectionWidth > 0
            && renderer.waterReflectionHeight > 0;
        if (waterReflectionTargetReady) {
            const glm::mat4 reflectionTransform = glm::translate(
                glm::mat4(1.0f),
                glm::vec3(0.0f, 2.0f * waterReflectionPlaneY, 0.0f)
            ) * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
            const glm::mat4 reflectionView = view * reflectionTransform;
            glm::vec3 reflectionCameraPos = playerPos;
            reflectionCameraPos.y = 2.0f * waterReflectionPlaneY - playerPos.y;
            std::vector<VisibleVoxelRenderSection> visibleVoxelReflectionSections;
            collectVisibleVoxelRenderSections(reflectionCameraPos, visibleVoxelReflectionSections);

            renderBackend.beginOffscreenColorPass(
                renderer.waterReflectionFBO,
                renderer.waterReflectionWidth,
                renderer.waterReflectionHeight,
                skyBottom.r,
                skyBottom.g,
                skyBottom.b,
                1.0f
            );
            setDepthTestEnabled(true);
            setDepthWriteEnabled(true);
            setBlendEnabled(false);
            setCullEnabled(false);
            // Avoid read/write hazard: reflection target must not be bound as sampled input
            // during the offscreen pass that writes it.
            bindTextureUnit2D(14, 0);

            renderer.blockShader->use();
            renderer.blockShader->setMat4("view", reflectionView);
            renderer.blockShader->setMat4("projection", projection);
            renderer.blockShader->setVec3("cameraPos", reflectionCameraPos);
            renderer.blockShader->setFloat("time", time);
            renderer.blockShader->setFloat("instanceScale", 1.0f);
            renderer.blockShader->setVec3("lightDir", lightDir);
            renderer.blockShader->setVec3("ambientLight", ambientLightColor);
            renderer.blockShader->setVec3("diffuseLight", diffuseLightColor);
            renderer.blockShader->setInt("voxelGridLinesEnabled", voxelGridLinesEnabled ? 1 : 0);
            renderer.blockShader->setInt("voxelGridLineInvertColorEnabled", voxelGridLineInvertColorEnabled ? 1 : 0);
            renderer.blockShader->setMat4("model", glm::mat4(1.0f));
            BlockChargeSystemLogic::ApplyBlockDamageMaskUniforms(baseSystem, prototypes, *renderer.blockShader, true);

            for (int i = 0; i < static_cast<int>(RenderBehavior::COUNT); ++i) {
                RenderBehavior currentBehavior = static_cast<RenderBehavior>(i);
                if (currentBehavior == RenderBehavior::ANIMATED_WATER
                    || currentBehavior == RenderBehavior::ANIMATED_TRANSPARENT_WAVE) {
                    continue;
                }
                if (useVoxelRendering) {
                    for (const auto& section : visibleVoxelReflectionSections) {
                        if (section.buffers->usesTexturedFaceBuffers) continue;
                        int count = section.buffers->counts[i];
                        if (count <= 0) continue;
                        renderer.blockShader->setFloat("instanceScale", 1.0f);
                        renderer.blockShader->setInt("behaviorType", i);
                        if (section.buffers->vaos[i] == 0) continue;
                        renderBackend.bindVertexArray(section.buffers->vaos[i]);
                        renderBackend.drawArraysTrianglesInstanced(0, 36, count);
                    }
                    renderer.blockShader->setFloat("instanceScale", 1.0f);
                }
                if (currentBehavior == RenderBehavior::STATIC_BRANCH) {
                    if (!branchInstances.empty()) {
                        renderer.blockShader->setInt("behaviorType", i);
                        renderBackend.bindVertexArray(renderer.behaviorVAOs[i]);
                        renderBackend.uploadArrayBufferData(
                            renderer.behaviorInstanceVBOs[i],
                            branchInstances.data(),
                            branchInstances.size() * sizeof(BranchInstanceData),
                            true
                        );
                        renderBackend.drawArraysTrianglesInstanced(0, 36, static_cast<int>(branchInstances.size()));
                    }
                } else if (!behaviorInstances[i].empty()) {
                    renderer.blockShader->setInt("behaviorType", i);
                    renderBackend.bindVertexArray(renderer.behaviorVAOs[i]);
                    renderBackend.uploadArrayBufferData(
                        renderer.behaviorInstanceVBOs[i],
                        behaviorInstances[i].data(),
                        behaviorInstances[i].size() * sizeof(InstanceData),
                        true
                    );
                    renderBackend.drawArraysTrianglesInstanced(0, 36, static_cast<int>(behaviorInstances[i].size()));
                }
            }

            if (renderer.faceShader && renderer.faceVAO) {
                setBlendEnabled(true);
                setBlendModeAlpha();
                std::array<std::vector<FaceInstanceRenderData>, 6> faceInstancesOpaque;
                for (int f = 0; f < 6; ++f) {
                    for (const auto& inst : faceInstances[f]) {
                        if (((inst.alpha < 0.0f)
                                && !isWaterSlopeAlpha(inst.alpha)
                                && !isFoliageTaggedAlpha(inst.alpha))
                            || inst.alpha >= 0.999f) {
                            faceInstancesOpaque[f].push_back(inst);
                        }
                    }
                }

                renderer.faceShader->use();
                renderer.faceShader->setMat4("view", reflectionView);
                renderer.faceShader->setMat4("projection", projection);
                renderer.faceShader->setMat4("model", glm::mat4(1.0f));
                renderer.faceShader->setVec3("cameraPos", reflectionCameraPos);
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
                bindFaceTextureUniforms(*renderer.faceShader, false);
                BlockChargeSystemLogic::ApplyBlockDamageMaskUniforms(baseSystem, prototypes, *renderer.faceShader, true);
                renderBackend.bindVertexArray(renderer.faceVAO);
                if (renderOpaqueFacesTwoSided) {
                    setCullEnabled(false);
                } else {
                    setCullBackFaceCCWEnabled(true);
                }
                for (int faceType = 0; faceType < 6; ++faceType) {
                    const auto& instances = faceInstancesOpaque[faceType];
                    if (instances.empty()) continue;
                    renderer.faceShader->setInt("faceType", faceType);
                    renderBackend.uploadArrayBufferData(
                        renderer.faceInstanceVBO,
                        instances.data(),
                        instances.size() * sizeof(FaceInstanceRenderData),
                        true
                    );
                    renderBackend.drawArraysTrianglesInstanced(0, 6, static_cast<int>(instances.size()));
                }
                setCullEnabled(false);
            }

            if (useVoxelRendering && renderer.faceShader && renderer.faceVAO) {
                setBlendEnabled(true);
                setBlendModeAlpha();

                renderer.faceShader->use();
                renderer.faceShader->setMat4("view", reflectionView);
                renderer.faceShader->setMat4("projection", projection);
                renderer.faceShader->setMat4("model", glm::mat4(1.0f));
                renderer.faceShader->setVec3("cameraPos", reflectionCameraPos);
                renderer.faceShader->setFloat("time", time);
                renderer.faceShader->setVec3("lightDir", lightDir);
                renderer.faceShader->setVec3("ambientLight", ambientLightColor);
                renderer.faceShader->setVec3("diffuseLight", diffuseLightColor);
                renderer.faceShader->setInt("faceType", 0);
                renderer.faceShader->setInt("leafOpaqueOutsideTier0", leafOpaqueOutsideTier0 ? 1 : 0);
                renderer.faceShader->setInt("leafBackfacesWhenInside", 0);
                renderer.faceShader->setInt("foliageWindEnabled", foliageWindAnimationEnabled ? 1 : 0);
                renderer.faceShader->setInt("waterCascadeBrightnessEnabled", waterCascadeBrightnessEnabled ? 1 : 0);
                renderer.faceShader->setInt("maskedFoliagePassMode", 1);
                renderer.faceShader->setFloat("waterCascadeBrightnessStrength", waterCascadeBrightnessStrength);
                renderer.faceShader->setFloat("waterCascadeBrightnessSpeed", waterCascadeBrightnessSpeed);
                renderer.faceShader->setFloat("waterCascadeBrightnessScale", waterCascadeBrightnessScale);
                bindFaceTextureUniforms(*renderer.faceShader, false);
                BlockChargeSystemLogic::ApplyBlockDamageMaskUniforms(baseSystem, prototypes, *renderer.faceShader, true);

                if (renderOpaqueFacesTwoSided) {
                    setCullEnabled(false);
                } else {
                    setCullBackFaceCCWEnabled(true);
                }
                for (const auto& section : visibleVoxelReflectionSections) {
                    if (!section.buffers->usesTexturedFaceBuffers) continue;
                    renderer.faceShader->setInt("sectionTier", 0);
                    for (int faceType = 0; faceType < 6; ++faceType) {
                        int count = section.buffers->faceBuffers.opaqueCounts[faceType];
                        if (count > 0 && section.buffers->faceBuffers.opaqueVaos[faceType] != 0) {
                            renderer.faceShader->setInt("faceType", faceType);
                            renderBackend.bindVertexArray(section.buffers->faceBuffers.opaqueVaos[faceType]);
                            renderBackend.drawArraysTrianglesInstanced(0, 6, count);
                        }
                    }
                }

                setDepthWriteEnabled(false);
                renderer.faceShader->setInt("maskedFoliagePassMode", 2);
                if (twoSidedAlphaFaces) {
                    setCullEnabled(false);
                } else {
                    setCullBackFaceCCWEnabled(true);
                }
                for (const auto& section : visibleVoxelReflectionSections) {
                    if (!section.buffers->usesTexturedFaceBuffers) continue;
                    renderer.faceShader->setInt("sectionTier", 0);
                    for (int faceType = 0; faceType < 6; ++faceType) {
                        int count = section.buffers->faceBuffers.alphaCounts[faceType];
                        if (count > 0 && section.buffers->faceBuffers.alphaVaos[faceType] != 0) {
                            renderer.faceShader->setInt("faceType", faceType);
                            renderBackend.bindVertexArray(section.buffers->faceBuffers.alphaVaos[faceType]);
                            renderBackend.drawArraysTrianglesInstanced(0, 6, count);
                        }
                    }
                }
                setDepthWriteEnabled(true);
                setCullEnabled(false);
            }

            renderBackend.endOffscreenColorPass();
            setDepthTestEnabled(true);
            setDepthWriteEnabled(true);
            setBlendEnabled(false);
            setCullEnabled(false);
        }

        if (renderer.faceShader && renderer.faceVAO) {
            std::array<std::vector<FaceInstanceRenderData>, 6> faceInstancesOpaque;
            std::array<std::vector<FaceInstanceRenderData>, 6> faceInstancesAlpha;
            std::array<std::vector<FaceInstanceRenderData>, 6> faceInstancesAlphaWaterLike;
            for (int f = 0; f < 6; ++f) {
                for (const auto& inst : faceInstances[f]) {
                    if (inst.alpha < 0.0f
                        && !isWaterSlopeAlpha(inst.alpha)
                        && !isFoliageTaggedAlpha(inst.alpha)) {
                        faceInstancesOpaque[f].push_back(inst);
                    }
                    else if (isWaterLikeTranslucentAlpha(inst.alpha)) {
                        // Draw water-like faces last so submerged foliage does not overdraw water.
                        faceInstancesAlphaWaterLike[f].push_back(inst);
                    } else if (inst.alpha < 0.999f) {
                        faceInstancesAlpha[f].push_back(inst);
                    } else {
                        faceInstancesOpaque[f].push_back(inst);
                    }
                }
            }
            drawFaceBatches(faceInstancesOpaque, true);
            drawFaceBatches(faceInstancesAlpha, false);
            drawFaceBatches(faceInstancesAlphaWaterLike, false);
        }

        if (useVoxelRendering && renderer.faceShader && renderer.faceVAO) {
            setBlendEnabled(true);
            setBlendModeAlpha();
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
            renderer.faceShader->setInt("leafOpaqueOutsideTier0", leafOpaqueOutsideTier0 ? 1 : 0);
            renderer.faceShader->setInt("leafBackfacesWhenInside", 0);
            renderer.faceShader->setInt("foliageWindEnabled", foliageWindAnimationEnabled ? 1 : 0);
            renderer.faceShader->setInt("waterCascadeBrightnessEnabled", waterCascadeBrightnessEnabled ? 1 : 0);
            renderer.faceShader->setInt("maskedFoliagePassMode", 1);
            renderer.faceShader->setFloat("waterCascadeBrightnessStrength", waterCascadeBrightnessStrength);
            renderer.faceShader->setFloat("waterCascadeBrightnessSpeed", waterCascadeBrightnessSpeed);
            renderer.faceShader->setFloat("waterCascadeBrightnessScale", waterCascadeBrightnessScale);
            bindFaceTextureUniforms(*renderer.faceShader);
            BlockChargeSystemLogic::ApplyBlockDamageMaskUniforms(baseSystem, prototypes, *renderer.faceShader, true);

            if (renderOpaqueFacesTwoSided) {
                setCullEnabled(false);
            } else {
                setCullBackFaceCCWEnabled(true);
            }
            for (const auto& section : visibleVoxelRenderSections) {
                if (!section.buffers->usesTexturedFaceBuffers) continue;
                renderer.faceShader->setInt("sectionTier", 0);
                for (int faceType = 0; faceType < 6; ++faceType) {
                    int count = section.buffers->faceBuffers.opaqueCounts[faceType];
                    if (count > 0 && section.buffers->faceBuffers.opaqueVaos[faceType] != 0) {
                        renderer.faceShader->setInt("faceType", faceType);
                        renderBackend.bindVertexArray(section.buffers->faceBuffers.opaqueVaos[faceType]);
                        renderBackend.drawArraysTrianglesInstanced(0, 6, count);
                    }
                }
            }

            setDepthWriteEnabled(false);
            renderer.faceShader->setInt("maskedFoliagePassMode", 2);
            if (twoSidedAlphaFaces) {
                setCullEnabled(false);
            } else {
                setCullBackFaceCCWEnabled(true);
            }
            for (auto it = visibleVoxelRenderSections.rbegin(); it != visibleVoxelRenderSections.rend(); ++it) {
                const auto& section = *it;
                if (!section.buffers->usesTexturedFaceBuffers) continue;
                renderer.faceShader->setInt("sectionTier", 0);
                for (int faceType = 0; faceType < 6; ++faceType) {
                    int count = section.buffers->faceBuffers.alphaCounts[faceType];
                    if (count > 0 && section.buffers->faceBuffers.alphaVaos[faceType] != 0) {
                        renderer.faceShader->setInt("faceType", faceType);
                        renderBackend.bindVertexArray(section.buffers->faceBuffers.alphaVaos[faceType]);
                        renderBackend.drawArraysTrianglesInstanced(0, 6, count);
                    }
                }
            }
            setDepthWriteEnabled(true);
            setCullEnabled(false);
        }

        // Ensure block-damage masking does not leak into unrelated face/block draws
        // that may run later in this frame or in other systems.
        if (renderer.faceShader) {
            renderer.faceShader->use();
            BlockChargeSystemLogic::ApplyBlockDamageMaskUniforms(baseSystem, prototypes, *renderer.faceShader, false);
        }
        if (renderer.blockShader) {
            renderer.blockShader->use();
            BlockChargeSystemLogic::ApplyBlockDamageMaskUniforms(baseSystem, prototypes, *renderer.blockShader, false);
        }

        if (!debugSlopeInstances.empty() && renderer.faceShader && renderer.faceVAO) {
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
            bindFaceTextureUniforms(*renderer.faceShader);
            BlockChargeSystemLogic::ApplyBlockDamageMaskUniforms(baseSystem, prototypes, *renderer.faceShader, false);

            setCullBackFaceCCWEnabled(true);
            renderBackend.bindVertexArray(renderer.faceVAO);

            auto drawSlopeFace = [&](const Entity& proto,
                                     int faceType,
                                     const glm::mat4& modelMat,
                                     const glm::vec3& facePosition,
                                     const glm::vec2& faceScale,
                                     const glm::vec2& faceUvScale,
                                     float faceAlpha,
                                     const glm::vec3& tintColor) {
                FaceInstanceRenderData face;
                face.position = facePosition;
                face.color = tintColor;
                int tile = RenderInitSystemLogic::FaceTileIndexFor(baseSystem.world.get(), proto, faceType);
                face.tileIndex = tile;
                face.alpha = faceAlpha;
                face.ao = glm::vec4(1.0f);
                face.scale = faceScale;
                face.uvScale = faceUvScale;
                renderer.faceShader->setMat4("model", modelMat);
                renderer.faceShader->setInt("faceType", faceType);
                renderBackend.uploadArrayBufferData(renderer.faceInstanceVBO, &face, sizeof(FaceInstanceRenderData), true);
                renderBackend.drawArraysTrianglesInstanced(0, 6, 1);
            };

            for (const auto& slopeInst : debugSlopeInstances) {
                if (slopeInst.prototypeID < 0 || slopeInst.prototypeID >= static_cast<int>(prototypes.size())) continue;
                const Entity& slopeProto = prototypes[slopeInst.prototypeID];
                glm::mat4 baseModel = glm::translate(glm::mat4(1.0f), slopeInst.position);

                constexpr float kSlopeCapA = -4.0f;
                constexpr float kSlopeCapB = -5.0f;
                constexpr float kSlopeTopPosX = -6.0f;
                constexpr float kSlopeTopNegX = -7.0f;
                constexpr float kSlopeTopPosZ = -8.0f;
                constexpr float kSlopeTopNegZ = -9.0f;

                auto faceCenterOffset = [](int faceType) -> glm::vec3 {
                    switch (faceType) {
                        case 0: return glm::vec3(0.5f, 0.0f, 0.0f);
                        case 1: return glm::vec3(-0.5f, 0.0f, 0.0f);
                        case 2: return glm::vec3(0.0f, 0.5f, 0.0f);
                        case 3: return glm::vec3(0.0f, -0.5f, 0.0f);
                        case 4: return glm::vec3(0.0f, 0.0f, 0.5f);
                        case 5: return glm::vec3(0.0f, 0.0f, -0.5f);
                        default: return glm::vec3(0.0f);
                    }
                };

                // Bottom face.
                drawSlopeFace(slopeProto, 3, baseModel, faceCenterOffset(3), glm::vec2(1.0f), glm::vec2(1.0f), 1.0f, slopeInst.color);

                int tallFaceType = 0;
                int capFaceA = 4;
                int capFaceB = 5;
                float capAlphaA = kSlopeCapA;
                float capAlphaB = kSlopeCapB;
                float topAlpha = kSlopeTopPosX;
                switch (slopeInst.dir) {
                    case DebugSlopeDir::PosX:
                        tallFaceType = 0;
                        capFaceA = 4; capAlphaA = kSlopeCapA;
                        capFaceB = 5; capAlphaB = kSlopeCapB;
                        topAlpha = kSlopeTopPosX;
                        break;
                    case DebugSlopeDir::NegX:
                        tallFaceType = 1;
                        capFaceA = 4; capAlphaA = kSlopeCapB;
                        capFaceB = 5; capAlphaB = kSlopeCapA;
                        topAlpha = kSlopeTopNegX;
                        break;
                    case DebugSlopeDir::PosZ:
                        tallFaceType = 4;
                        capFaceA = 0; capAlphaA = kSlopeCapB;
                        capFaceB = 1; capAlphaB = kSlopeCapA;
                        topAlpha = kSlopeTopPosZ;
                        break;
                    case DebugSlopeDir::NegZ:
                        tallFaceType = 5;
                        capFaceA = 0; capAlphaA = kSlopeCapA;
                        capFaceB = 1; capAlphaB = kSlopeCapB;
                        topAlpha = kSlopeTopNegZ;
                        break;
                }

                // Tall side.
                drawSlopeFace(slopeProto, tallFaceType, baseModel, faceCenterOffset(tallFaceType), glm::vec2(1.0f), glm::vec2(1.0f), 1.0f, slopeInst.color);
                // Triangular side caps.
                drawSlopeFace(slopeProto, capFaceA, baseModel, faceCenterOffset(capFaceA), glm::vec2(1.0f), glm::vec2(1.0f), capAlphaA, slopeInst.color);
                drawSlopeFace(slopeProto, capFaceB, baseModel, faceCenterOffset(capFaceB), glm::vec2(1.0f), glm::vec2(1.0f), capAlphaB, slopeInst.color);
                // Sloped top.
                drawSlopeFace(slopeProto, 2, baseModel, faceCenterOffset(2), glm::vec2(1.0f), glm::vec2(1.0f), topAlpha, slopeInst.color);
            }

            setCullEnabled(false);
            renderer.faceShader->setMat4("model", glm::mat4(1.0f));
        }

        WorldRenderViewState heldItemFrame;
        heldItemFrame.view = view;
        heldItemFrame.projection = projection;
        heldItemFrame.playerPos = playerPos;
        heldItemFrame.cameraForward = cameraForward;
        heldItemFrame.lightDir = lightDir;
        heldItemFrame.ambientLightColor = ambientLightColor;
        heldItemFrame.diffuseLightColor = diffuseLightColor;
        heldItemFrame.time = time;
        heldItemFrame.mapViewActive = mapViewActive;
        heldItemFrame.leafOpaqueOutsideTier0 = leafOpaqueOutsideTier0;
        heldItemFrame.foliageWindAnimationEnabled = foliageWindAnimationEnabled;
        heldItemFrame.waterCascadeBrightnessEnabled = waterCascadeBrightnessEnabled;
        heldItemFrame.waterCascadeBrightnessStrength = waterCascadeBrightnessStrength;
        heldItemFrame.waterCascadeBrightnessSpeed = waterCascadeBrightnessSpeed;
        heldItemFrame.waterCascadeBrightnessScale = waterCascadeBrightnessScale;
        RenderHeldItemPass(baseSystem, prototypes, heldItemFrame, bindFaceTextureUniforms);

    }
}
