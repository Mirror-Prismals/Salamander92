    void UpdateBlockCharge(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        if (!baseSystem.player || !baseSystem.level) return;
        PlayerContext& player = *baseSystem.player;
        LevelContext& level = *baseSystem.level;
        decayBlockDamageOverTime(baseSystem, prototypes);
        const BuildModeType heldModeAtEntry = player.buildMode;
        if (isPickupHandMode(heldModeAtEntry)) {
            loadActiveHeldBlockForMode(player, heldModeAtEntry);
        }
        ScopeExit heldStateSync{[&]() {
            if (isPickupHandMode(heldModeAtEntry)) {
                saveActiveHeldBlockForMode(player, heldModeAtEntry);
            }
        }};
        GemContext* gems = baseSystem.gems ? baseSystem.gems.get() : nullptr;
        if (gems) {
            gems->placementPreviewActive = false;
            gems->placementPreviewPosition = glm::vec3(0.0f);
            gems->placementPreviewRenderYOffset = 0.0f;
        }
        constexpr bool kHatchetsEnabled = false;
        if (kHatchetsEnabled) {
            player.hatchetSelectedMaterial = glm::clamp(player.hatchetSelectedMaterial, 0, HATCHET_MATERIAL_COUNT - 1);
            player.hatchetPlacedMaterial = glm::clamp(player.hatchetPlacedMaterial, 0, HATCHET_MATERIAL_COUNT - 1);
            player.hatchetInventoryCount = sumHatchetInventory(player);
            if (player.hatchetInventoryCount <= 0) {
                player.hatchetHeld = false;
            }
            if (player.hatchetHeld
                && player.hatchetInventoryByMaterial[static_cast<size_t>(player.hatchetSelectedMaterial)] <= 0) {
                player.hatchetSelectedMaterial = firstAvailableHatchetMaterial(player);
            }
        } else {
            player.hatchetHeld = false;
            player.hatchetInventoryCount = 0;
            player.hatchetSelectedMaterial = HATCHET_MATERIAL_STONE;
            player.hatchetPlacedInWorld = false;
            player.hatchetPlacedCell = glm::ivec3(0);
            player.hatchetPlacedWorldIndex = -1;
            player.hatchetPlacedMaterial = HATCHET_MATERIAL_STONE;
            player.hatchetPlacedPosition = glm::vec3(0.0f);
            player.hatchetPlacedNormal = glm::vec3(0.0f, 1.0f, 0.0f);
            player.hatchetPlacedDirection = glm::vec3(1.0f, 0.0f, 0.0f);
            for (int i = 0; i < HATCHET_MATERIAL_COUNT; ++i) {
                player.hatchetInventoryByMaterial[static_cast<size_t>(i)] = 0;
            }
        }
        auto resetChargeState = [&]() {
            player.isChargingBlock = false;
            player.blockChargeReady = false;
            player.blockChargeValue = 0.0f;
            player.blockChargeAction = BlockChargeAction::None;
            player.blockChargeDecayTimer = 0.0f;
            player.blockChargeExecuteGraceTimer = 0.0f;
        };
        const float chargeDecaySeconds = std::max(
            0.01f,
            readRegistryFloat(baseSystem, "BlockChargeDecaySeconds", 1.0f));
        const float chargeExecuteGraceSeconds = std::max(
            0.0f,
            readRegistryFloat(baseSystem, "BlockChargeExecuteGraceSeconds", 1.0f));
        auto releaseChargeToTail = [&]() {
            if (!player.isChargingBlock) return;
            player.isChargingBlock = false;
            player.blockChargeDecayTimer = std::max(player.blockChargeDecayTimer, chargeDecaySeconds);
            player.blockChargeExecuteGraceTimer = std::max(player.blockChargeExecuteGraceTimer, chargeExecuteGraceSeconds);
        };
        auto updateChargeTail = [&]() {
            if (player.isChargingBlock) return;
            if (player.blockChargeAction == BlockChargeAction::None) return;
            if (player.blockChargeDecayTimer > 0.0f) {
                player.blockChargeDecayTimer = std::max(0.0f, player.blockChargeDecayTimer - dt);
                player.blockChargeValue = std::max(0.0f, player.blockChargeValue - (dt / chargeDecaySeconds));
            } else {
                player.blockChargeValue = 0.0f;
            }
            if (player.blockChargeExecuteGraceTimer > 0.0f) {
                player.blockChargeExecuteGraceTimer = std::max(0.0f, player.blockChargeExecuteGraceTimer - dt);
            }
            if (player.blockChargeExecuteGraceTimer <= 0.0f) {
                player.blockChargeReady = false;
            }
            if (player.blockChargeValue <= 0.001f && player.blockChargeExecuteGraceTimer <= 0.0f) {
                resetChargeState();
            }
        };
        auto releaseChargeUseToTail = [&]() {
            releaseChargeToTail();
            if (player.blockChargeAction != BlockChargeAction::None) {
                player.blockChargeDecayTimer = std::max(player.blockChargeDecayTimer, chargeDecaySeconds);
            }
            player.blockChargeReady = false;
            player.blockChargeExecuteGraceTimer = 0.0f;
        };
        auto triggerChargeFireInvertTail = [&]() {
            if (!baseSystem.colorEmotion) return;
            ColorEmotionContext& emotion = *baseSystem.colorEmotion;
            const float invertTailSeconds = std::max(
                0.05f,
                readRegistryFloat(baseSystem, "ColorEmotionChargeFireTailSeconds", 1.0f)
            );
            emotion.chargeFireInvertDuration = invertTailSeconds;
            emotion.chargeFireInvertTimer = invertTailSeconds;
            emotion.chargeFireInvertTail = 1.0f;
        };
        if (baseSystem.ui && baseSystem.ui->active) {
            resetChargeState();
            return;
        }
        if (OreMiningSystemLogic::IsMiningActive(baseSystem)) {
            resetChargeState();
            return;
        }
        if (GroundCraftingSystemLogic::IsRitualActive(baseSystem)) {
            resetChargeState();
            return;
        }
        if (GemChiselSystemLogic::IsChiselActive(baseSystem)) {
            resetChargeState();
            return;
        }
        auto clearHeldGem = [&]() {
            if (!gems) return;
            gems->blockModeHoldingGem = false;
            gems->heldDrop = GemDropState{};
            gems->placementPreviewActive = false;
            gems->placementPreviewPosition = glm::vec3(0.0f);
            gems->placementPreviewRenderYOffset = 0.0f;
        };
        auto clearPlacedHatchet = [&]() {
            player.hatchetPlacedInWorld = false;
            player.hatchetPlacedCell = glm::ivec3(0);
            player.hatchetPlacedWorldIndex = -1;
            player.hatchetPlacedMaterial = HATCHET_MATERIAL_STONE;
            player.hatchetPlacedPosition = glm::vec3(0.0f);
            player.hatchetPlacedNormal = glm::vec3(0.0f, 1.0f, 0.0f);
            player.hatchetPlacedDirection = glm::vec3(1.0f, 0.0f, 0.0f);
        };
        auto setHeldHatchetMaterial = [&](int material) {
            const int m = glm::clamp(material, 0, HATCHET_MATERIAL_COUNT - 1);
            for (int i = 0; i < HATCHET_MATERIAL_COUNT; ++i) {
                player.hatchetInventoryByMaterial[static_cast<size_t>(i)] = 0;
            }
            player.hatchetInventoryByMaterial[static_cast<size_t>(m)] = 1;
            player.hatchetInventoryCount = 1;
            player.hatchetSelectedMaterial = m;
            player.hatchetHeld = true;
        };

        const bool pickupHandMode = isPickupHandMode(player.buildMode);
        bool legacyDestroyMode = player.buildMode == BuildModeType::Destroy;
        bool interactionMode = pickupHandMode || legacyDestroyMode;
        bool fishingMode = player.buildMode == BuildModeType::Fishing;
        bool boulderingMode = player.buildMode == BuildModeType::Bouldering;
        const bool leafClimbEnabled = readRegistryBool(baseSystem, "LeafClimbEnabled", true);
        const bool leafBoulderingAnchorsEnabled = readRegistryBool(
            baseSystem,
            "LeafClimbBoulderingAnchorsEnabled",
            leafClimbEnabled);
        const int leafPrototypeID = leafBoulderingAnchorsEnabled ? resolveLeafPrototypeID(prototypes) : -1;
        const bool holdingChalkTool = player.isHoldingBlock
            && player.heldPrototypeID >= 0
            && player.heldPrototypeID < static_cast<int>(prototypes.size())
            && isChalkStickPrototypeName(prototypes[static_cast<size_t>(player.heldPrototypeID)].name);
        static bool bKeyDownLastFrame = false;
        const bool bKeyDown = PlatformInput::IsKeyDown(win, PlatformInput::Key::B);
        const bool bKeyJustPressed = bKeyDown && !bKeyDownLastFrame;
        bKeyDownLastFrame = bKeyDown;
        static bool throwWDownLastFrame = false;
        static bool throwRequireWReleaseForNextStart = false;
        static double throwLastWPressTime = -1000.0;
        static double throwLastLmbPressTime = -1000.0;
        UpdateThrownHeldBlockRuntime(baseSystem, level, player, prototypes, dt);
        const bool holdingInspectableBook = player.isHoldingBlock
            && player.heldPrototypeID >= 0
            && player.heldPrototypeID < static_cast<int>(prototypes.size())
            && BookSystemLogic::IsBookPrototypeName(prototypes[static_cast<size_t>(player.heldPrototypeID)].name);
        if (holdingInspectableBook && player.bookInspectActive) {
            resetChargeState();
            return;
        }
        auto worldMatches = [](int lhs, int rhs) {
            return lhs < 0 || rhs < 0 || lhs == rhs;
        };
        auto isLatchedAnchorCell = [&](const glm::ivec3& cell, int worldIndex) {
            if (player.boulderPrimaryLatched
                && player.boulderPrimaryCell == cell
                && worldMatches(worldIndex, player.boulderPrimaryWorldIndex)) {
                return true;
            }
            if (player.boulderSecondaryLatched
                && player.boulderSecondaryCell == cell
                && worldMatches(worldIndex, player.boulderSecondaryWorldIndex)) {
                return true;
            }
            return false;
        };
        auto targetIsLatchedAnchorSuppressed = [&]() {
            if (player.buildMode == BuildModeType::Bouldering) return false;
            if (!(player.boulderPrimaryLatched || player.boulderSecondaryLatched)) return false;
            if (!player.hasBlockTarget) return false;
            const glm::ivec3 targetCell = glm::ivec3(glm::round(player.targetedBlockPosition));
            return isLatchedAnchorCell(targetCell, player.targetedWorldIndex);
        };
        auto releaseLatchedAnchorAt = [&](const glm::ivec3& cell, int worldIndex) {
            bool released = false;
            if (player.boulderPrimaryLatched
                && player.boulderPrimaryCell == cell
                && worldMatches(worldIndex, player.boulderPrimaryWorldIndex)) {
                player.boulderPrimaryLatched = false;
                player.boulderPrimaryRestLength = 0.0f;
                player.boulderPrimaryWorldIndex = -1;
                player.boulderPrimaryNormal = glm::vec3(0.0f, 0.0f, 1.0f);
                released = true;
            }
            if (player.boulderSecondaryLatched
                && player.boulderSecondaryCell == cell
                && worldMatches(worldIndex, player.boulderSecondaryWorldIndex)) {
                player.boulderSecondaryLatched = false;
                player.boulderSecondaryRestLength = 0.0f;
                player.boulderSecondaryWorldIndex = -1;
                player.boulderSecondaryNormal = glm::vec3(0.0f, 0.0f, 1.0f);
                released = true;
            }
            if (released) {
                player.boulderLaunchVelocity = glm::vec3(0.0f);
            }
            return released;
        };
        if (fishingMode) {
            // Fishing mode owns the charge meter semantics; avoid stomping state here.
            return;
        }
        if (boulderingMode) {
            auto validateLatch = [&](bool& latched, const glm::ivec3& cell, int worldIndex, float& restLength) {
                if (!latched) return;
                if (!hasBoulderingAnchorAtCell(
                        baseSystem,
                        level,
                        prototypes,
                        cell,
                        worldIndex,
                        leafBoulderingAnchorsEnabled,
                        leafPrototypeID)) {
                    latched = false;
                    restLength = 0.0f;
                }
            };
            validateLatch(player.boulderPrimaryLatched, player.boulderPrimaryCell, player.boulderPrimaryWorldIndex, player.boulderPrimaryRestLength);
            validateLatch(player.boulderSecondaryLatched, player.boulderSecondaryCell, player.boulderSecondaryWorldIndex, player.boulderSecondaryRestLength);

            const float boulderChargeSeconds = std::max(0.05f, readRegistryFloat(baseSystem, "BoulderingChargeSeconds", CHARGE_TIME_PICKUP));
            const float boulderLatchMaxDistance = std::max(0.25f, readRegistryFloat(baseSystem, "BoulderingLatchMaxDistance", 2.5f));
            const float boulderRestMin = std::max(0.05f, readRegistryFloat(baseSystem, "BoulderingRestLengthMin", 0.22f));
            const float boulderRestMax = std::max(boulderRestMin, readRegistryFloat(baseSystem, "BoulderingRestLengthMax", 1.6f));
            const float boulderRestTarget = glm::clamp(readRegistryFloat(baseSystem, "BoulderingLatchRestLength", 0.26f), boulderRestMin, boulderRestMax);
            const float boulderSnapBlend = glm::clamp(readRegistryFloat(baseSystem, "BoulderingLatchSnapBlend", 0.62f), 0.0f, 1.0f);
            const bool boulderSwappedControls = player.blockChargeControlsSwapped;
            const bool primaryChargeDown = boulderSwappedControls ? player.leftMouseDown : player.rightMouseDown;
            const bool primaryChargePressed = boulderSwappedControls ? player.leftMousePressed : player.rightMousePressed;
            const bool primaryExecutePressed = boulderSwappedControls ? player.rightMousePressed : player.leftMousePressed;
            const bool secondaryChargeDown = boulderSwappedControls ? player.rightMouseDown : player.leftMouseDown;
            const bool secondaryChargePressed = boulderSwappedControls ? player.rightMousePressed : player.leftMousePressed;
            const bool secondaryExecutePressed = boulderSwappedControls ? player.leftMousePressed : player.rightMousePressed;

            BlockChargeAction activeAction = player.blockChargeAction;
            if (activeAction != BlockChargeAction::BoulderPrimary && activeAction != BlockChargeAction::BoulderSecondary) {
                activeAction = BlockChargeAction::None;
            }
            if (activeAction == BlockChargeAction::None) {
                bool wantsPrimaryCharge = primaryChargeDown && !secondaryChargeDown;
                bool wantsSecondaryCharge = secondaryChargeDown && !primaryChargeDown;
                if (primaryChargePressed && !secondaryChargeDown) wantsPrimaryCharge = true;
                if (secondaryChargePressed && !primaryChargeDown) wantsSecondaryCharge = true;
                if (wantsPrimaryCharge) activeAction = BlockChargeAction::BoulderPrimary;
                else if (wantsSecondaryCharge) activeAction = BlockChargeAction::BoulderSecondary;
            }

            bool wantsCharge = false;
            if (activeAction == BlockChargeAction::BoulderPrimary) wantsCharge = primaryChargeDown;
            else if (activeAction == BlockChargeAction::BoulderSecondary) wantsCharge = secondaryChargeDown;

            if (wantsCharge) {
                if (!player.isChargingBlock || player.blockChargeAction != activeAction) {
                    player.blockChargeValue = 0.0f;
                    player.blockChargeReady = false;
                }
                player.isChargingBlock = true;
                player.blockChargeAction = activeAction;
                player.blockChargeDecayTimer = chargeDecaySeconds;
                player.blockChargeExecuteGraceTimer = 0.0f;
                player.blockChargeValue += dt / boulderChargeSeconds;
                if (player.blockChargeValue >= 1.0f) {
                    player.blockChargeValue = 1.0f;
                    player.blockChargeReady = true;
                }
            } else {
                releaseChargeToTail();
                updateChargeTail();
            }

            const bool hasGraceWindow = player.blockChargeExecuteGraceTimer > 0.0f;
            const bool executePrimary = player.blockChargeAction == BlockChargeAction::BoulderPrimary
                && primaryExecutePressed
                && (primaryChargeDown || hasGraceWindow);
            const bool executeSecondary = player.blockChargeAction == BlockChargeAction::BoulderSecondary
                && secondaryExecutePressed
                && (secondaryChargeDown || hasGraceWindow);

            auto tryLatchHand = [&](bool primaryHand) -> bool {
                if (!player.hasBlockTarget) return false;
                if (glm::length(player.targetedBlockNormal) < 0.1f) return false;
                const glm::vec3 eye = cameraEyePosition(baseSystem, player);
                if (glm::distance(eye, player.targetedBlockPosition) > boulderLatchMaxDistance) return false;

                glm::ivec3 targetCell(0);
                bool fromVoxel = false;
                const int targetPrototypeID = resolveTargetPrototypeID(baseSystem, level, prototypes, player, &targetCell, &fromVoxel);
                (void)fromVoxel;
                if (!isBoulderingAnchorPrototypeID(
                        prototypes,
                        targetPrototypeID,
                        leafBoulderingAnchorsEnabled,
                        leafPrototypeID)) {
                    return false;
                }

                glm::vec3 normal = player.targetedBlockNormal;
                if (glm::length(normal) < 0.01f) normal = glm::vec3(0.0f, 0.0f, 1.0f);
                normal = glm::normalize(normal);
                const glm::vec3 anchorPos = player.targetedBlockPosition + normal * 0.45f;
                const float restLength = boulderRestTarget;
                const glm::vec3 snapTarget = anchorPos + normal * restLength;
                player.cameraPosition = glm::mix(player.cameraPosition, snapTarget, boulderSnapBlend);
                // Keep latch at wall-stone eye level for consistent lateral climbing controls.
                player.cameraPosition.y = player.targetedBlockPosition.y;
                player.verticalVelocity = 0.0f;
                player.boulderLaunchVelocity = glm::vec3(0.0f);
                if (primaryHand) {
                    player.boulderPrimaryLatched = true;
                    player.boulderPrimaryAnchor = anchorPos;
                    player.boulderPrimaryNormal = normal;
                    player.boulderPrimaryCell = targetCell;
                    player.boulderPrimaryRestLength = restLength;
                    player.boulderPrimaryWorldIndex = player.targetedWorldIndex;
                } else {
                    player.boulderSecondaryLatched = true;
                    player.boulderSecondaryAnchor = anchorPos;
                    player.boulderSecondaryNormal = normal;
                    player.boulderSecondaryCell = targetCell;
                    player.boulderSecondaryRestLength = restLength;
                    player.boulderSecondaryWorldIndex = player.targetedWorldIndex;
                }
                player.onGround = false;
                triggerGameplaySfx(baseSystem, "pickup_block.ck", 0.02f);
                return true;
            };

            if ((executePrimary || executeSecondary) && player.blockChargeReady) {
                triggerChargeFireInvertTail();
                if (executePrimary) {
                    (void)tryLatchHand(true);
                } else {
                    (void)tryLatchHand(false);
                }
                releaseChargeUseToTail();
            }
            return;
        }
        if (!interactionMode) {
            resetChargeState();
            return;
        }
        const Entity* audioVisualizerProto = nullptr;
        for (const auto& proto : prototypes) {
            if (proto.name == "AudioVisualizer") {
                audioVisualizerProto = &proto;
                break;
            }
        }

        auto tryPlaceHeldBlock = [&](PlayerContext& playerCtx) {
            if (!playerCtx.leftMousePressed) return;
            if (!playerCtx.hasBlockTarget || glm::length(playerCtx.targetedBlockNormal) < 0.1f) return;
            if (playerCtx.targetedWorldIndex < 0 || playerCtx.targetedWorldIndex >= static_cast<int>(level.worlds.size())) return;
            if (playerCtx.heldPrototypeID < 0 || playerCtx.heldPrototypeID >= static_cast<int>(prototypes.size())) return;
            const Entity& heldProto = prototypes[playerCtx.heldPrototypeID];
            const bool heldIsChalkDust = isChalkDustPrototypeID(prototypes, playerCtx.heldPrototypeID);
            const bool heldIsComputer = (heldProto.name == "Computer");

            if (isSurfaceStonePrototypeName(heldProto.name)) {
                glm::ivec3 targetCell(0);
                bool targetFromVoxel = false;
                const int targetPrototypeID = resolveTargetPrototypeID(
                    baseSystem,
                    level,
                    prototypes,
                    playerCtx,
                    &targetCell,
                    &targetFromVoxel
                );
                (void)targetFromVoxel;
                if (targetPrototypeID >= 0 && targetPrototypeID < static_cast<int>(prototypes.size())) {
                    const Entity& targetProto = prototypes[static_cast<size_t>(targetPrototypeID)];
                    if (baseSystem.voxelWorld
                        && baseSystem.voxelWorld->enabled
                        && targetCell == glm::ivec3(glm::round(playerCtx.targetedBlockPosition))
                        && isSurfaceStonePrototypeName(targetProto.name)
                        && targetProto.prototypeID == playerCtx.heldPrototypeID) {
                        const uint32_t packed = baseSystem.voxelWorld->getColorWorld(targetCell);
                        const int pileCount = decodeSurfaceStonePileCount(packed);
                        if (pileCount < SURFACE_STONE_PILE_MAX) {
                            uint32_t basePacked = packed;
                            if ((basePacked & 0x00ffffffu) == 0u) {
                                basePacked = packColor(playerCtx.heldBlockColor);
                            }
                            baseSystem.voxelWorld->setBlockWorld(
                                targetCell,
                                static_cast<uint32_t>(targetPrototypeID),
                                withSurfaceStonePileCount(basePacked, pileCount + 1)
                            );
                            VoxelMeshingSystemLogic::RequestPriorityVoxelRemesh(baseSystem, prototypes, targetCell);
                            StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, playerCtx.targetedWorldIndex, glm::vec3(targetCell));
                            triggerGameplaySfx(baseSystem, "place_block.ck", 0.02f);
                            playerCtx.isHoldingBlock = false;
                            playerCtx.heldPrototypeID = -1;
                            playerCtx.heldPackedColor = 0u;
                            playerCtx.heldHasSourceCell = false;
                            playerCtx.heldSourceCell = glm::ivec3(0);
                            return;
                        }
                        return;
                    }
                    if (kHatchetsEnabled && isStickPrototypeName(targetProto.name)) {
                        RemovedBlockInfo removedStick;
                        if (RemoveBlockAtPosition(
                                baseSystem,
                                level,
                                prototypes,
                                playerCtx.targetedBlockPosition,
                                playerCtx.targetedWorldIndex,
                                &removedStick)
                            && removedStick.prototypeID >= 0
                            && removedStick.prototypeID < static_cast<int>(prototypes.size())
                            && isStickPrototypeName(prototypes[static_cast<size_t>(removedStick.prototypeID)].name)) {
                            bool materialRecognized = false;
                            int material = hatchetMaterialFromStonePrototypeName(heldProto.name, &materialRecognized);
                            if (!materialRecognized) {
                                material = detectHatchetMaterialFromColor(playerCtx.heldBlockColor);
                            }
                            material = glm::clamp(material, 0, HATCHET_MATERIAL_COUNT - 1);

                            const glm::vec3 surfaceNormal = normalizeOrDefault(playerCtx.targetedBlockNormal, glm::vec3(0.0f, 1.0f, 0.0f));
                            const glm::vec3 forward = cameraForwardDirection(playerCtx);
                            playerCtx.hatchetPlacedInWorld = true;
                            playerCtx.hatchetPlacedCell = targetCell;
                            playerCtx.hatchetPlacedWorldIndex = playerCtx.targetedWorldIndex;
                            playerCtx.hatchetPlacedNormal = surfaceNormal;
                            playerCtx.hatchetPlacedDirection = projectDirectionOnSurface(forward, surfaceNormal);
                            playerCtx.hatchetPlacedPosition = glm::vec3(targetCell) - surfaceNormal * 0.47f;
                            playerCtx.hatchetPlacedMaterial = material;
                            playerCtx.hatchetSelectedMaterial = material;
                            playerCtx.hatchetHeld = false;
                            for (int i = 0; i < HATCHET_MATERIAL_COUNT; ++i) {
                                playerCtx.hatchetInventoryByMaterial[static_cast<size_t>(i)] = 0;
                            }
                            playerCtx.hatchetInventoryCount = 0;
                            playerCtx.isHoldingBlock = false;
                            playerCtx.heldPrototypeID = -1;
                            playerCtx.heldPackedColor = 0u;
                            playerCtx.heldHasSourceCell = false;
                            playerCtx.heldSourceCell = glm::ivec3(0);
                            triggerGameplaySfx(baseSystem, "place_block.ck", 0.02f);
                            return;
                        }
                    }
                    if (isFlowerPrototypeName(targetProto.name)) {
                        RemovedBlockInfo removedFlower;
                        if (RemoveBlockAtPosition(
                                baseSystem,
                                level,
                                prototypes,
                                playerCtx.targetedBlockPosition,
                                playerCtx.targetedWorldIndex,
                                &removedFlower)) {
                            const int variant = detectFlowerPetalVariantFromColor(removedFlower.color);
                            glm::ivec3 petalsCell = targetCell;
                            if (removedFlower.fromVoxel) {
                                petalsCell = removedFlower.voxelCell;
                            }
                            const bool preferX = (((petalsCell.x ^ petalsCell.y ^ petalsCell.z) & 1) == 0);
                            const int petalsPrototypeID = resolveFlowerPetalsPrototypeID(prototypes, variant, preferX);
                            if (petalsPrototypeID >= 0) {
                                if (removedFlower.fromVoxel && baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                                    baseSystem.voxelWorld->setBlockWorld(
                                        petalsCell,
                                        static_cast<uint32_t>(petalsPrototypeID),
                                        packColor(glm::vec3(1.0f))
                                    );
                                    VoxelMeshingSystemLogic::RequestPriorityVoxelRemesh(baseSystem, prototypes, petalsCell);
                                    StructureCaptureSystemLogic::NotifyBlockChanged(
                                        baseSystem,
                                        playerCtx.targetedWorldIndex,
                                        glm::vec3(petalsCell)
                                    );
                                } else {
                                    BlockSelectionSystemLogic::RemoveBlockFromCache(
                                        baseSystem,
                                        prototypes,
                                        playerCtx.targetedWorldIndex,
                                        playerCtx.targetedBlockPosition
                                    );
                                    const glm::vec3 petalsPos = glm::vec3(petalsCell);
                                    Entity& world = level.worlds[static_cast<size_t>(playerCtx.targetedWorldIndex)];
                                    world.instances.push_back(HostLogic::CreateInstance(
                                        baseSystem,
                                        petalsPrototypeID,
                                        petalsPos,
                                        glm::vec3(1.0f)
                                    ));
                                    BlockSelectionSystemLogic::AddBlockToCache(
                                        baseSystem,
                                        prototypes,
                                        playerCtx.targetedWorldIndex,
                                        petalsPos,
                                        petalsPrototypeID
                                    );
                                    StructureCaptureSystemLogic::NotifyBlockChanged(
                                        baseSystem,
                                        playerCtx.targetedWorldIndex,
                                        petalsPos
                                    );
                                }
                                triggerGameplaySfx(baseSystem, "place_block.ck", 0.02f);
                                return;
                            }
                        }
                    }
                }
            }

            glm::vec3 placePos = playerCtx.targetedBlockPosition + playerCtx.targetedBlockNormal;
            if (BlockSelectionSystemLogic::HasBlockAt(baseSystem, prototypes, playerCtx.targetedWorldIndex, placePos)) return;
            glm::ivec3 placedCell = glm::ivec3(glm::round(placePos));

            bool placedInVoxel = false;
            if (baseSystem.voxelWorld && baseSystem.voxelWorld->enabled && heldProto.isChunkable) {
                glm::ivec3 placeCell = glm::ivec3(glm::round(placePos));
                uint32_t packedColor = packColor(playerCtx.heldBlockColor);
                if (isSurfaceStonePrototypeName(heldProto.name)) {
                    packedColor = withSurfaceStonePileCount(packedColor, SURFACE_STONE_PILE_MIN);
                }
                baseSystem.voxelWorld->setBlockWorld(
                    placeCell,
                    static_cast<uint32_t>(playerCtx.heldPrototypeID),
                    packedColor
                );
                VoxelMeshingSystemLogic::RequestPriorityVoxelRemesh(baseSystem, prototypes, placeCell);
                StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, playerCtx.targetedWorldIndex, glm::vec3(placeCell));
                placedCell = placeCell;
                placedInVoxel = true;
            }

            if (!placedInVoxel) {
                Entity& world = level.worlds[playerCtx.targetedWorldIndex];
                world.instances.push_back(HostLogic::CreateInstance(baseSystem, playerCtx.heldPrototypeID, placePos, playerCtx.heldBlockColor));
                BlockSelectionSystemLogic::AddBlockToCache(baseSystem, prototypes, playerCtx.targetedWorldIndex, placePos, playerCtx.heldPrototypeID);
                StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, playerCtx.targetedWorldIndex, placePos);
            }
            if (heldIsChalkDust) {
                refreshChalkDustNeighborhood(baseSystem, level, prototypes, playerCtx.targetedWorldIndex, placedCell);
            }
            if (heldIsComputer && baseSystem.ui) {
                baseSystem.ui->computerCacheBuilt = false;
            }
            if (audioVisualizerProto && playerCtx.heldPrototypeID == audioVisualizerProto->prototypeID) {
                RayTracedAudioSystemLogic::InvalidateSourceCache(baseSystem);
            }
            triggerGameplaySfx(baseSystem, "place_block.ck", 0.02f);
            playerCtx.isHoldingBlock = false;
            playerCtx.heldPrototypeID = -1;
            playerCtx.heldPackedColor = 0u;
            playerCtx.heldHasSourceCell = false;
            playerCtx.heldSourceCell = glm::ivec3(0);
        };

        auto tryPlaceHeldGem = [&](PlayerContext& playerCtx) {
            if (!gems || !gems->blockModeHoldingGem) return;
            if (!playerCtx.hasBlockTarget || glm::length(playerCtx.targetedBlockNormal) < 0.1f) return;
            if (playerCtx.targetedWorldIndex < 0 || playerCtx.targetedWorldIndex >= static_cast<int>(level.worlds.size())) return;

            glm::ivec3 targetCell = glm::ivec3(glm::round(playerCtx.targetedBlockPosition));
            bool targetFromVoxel = false;
            const int targetPrototypeID = resolveTargetPrototypeID(
                baseSystem,
                level,
                prototypes,
                playerCtx,
                &targetCell,
                &targetFromVoxel
            );
            (void)targetFromVoxel;
            glm::ivec3 blueprintCell(0);
            bool targetIsBlueprint = false;
            if (targetPrototypeID >= 0 && targetPrototypeID < static_cast<int>(prototypes.size())) {
                const Entity& targetProto = prototypes[static_cast<size_t>(targetPrototypeID)];
                if (isBlueprintPrototypeName(targetProto.name)) {
                    targetIsBlueprint = true;
                }
            }
            if (!targetIsBlueprint && findBlueprintInstanceAtTarget(level, prototypes, playerCtx, &blueprintCell)) {
                targetIsBlueprint = true;
                targetCell = blueprintCell;
            }

            // Chipping station: placing a raw gem on a natural surface stone converts it into
            // a placeable "ingot" block (same stone-pebble shape) held in hand.
            if (targetPrototypeID >= 0 && targetPrototypeID < static_cast<int>(prototypes.size())) {
                const Entity& targetProto = prototypes[static_cast<size_t>(targetPrototypeID)];
                if (isStickPrototypeName(targetProto.name) && playerCtx.leftMousePressed) {
                    std::vector<glm::ivec3> stencilHeadVoxels;
                    if (!buildPickaxeHeadVoxelsFromStencil(baseSystem, stencilHeadVoxels)
                        || stencilHeadVoxels.empty()) {
                        return;
                    }
                    if (!isGemValidPickaxeHeadShape(baseSystem, gems->heldDrop, stencilHeadVoxels)) {
                        return;
                    }

                    RemovedBlockInfo removedStick;
                    if (RemoveBlockAtPosition(
                            baseSystem,
                            level,
                            prototypes,
                            playerCtx.targetedBlockPosition,
                            playerCtx.targetedWorldIndex,
                            &removedStick)
                        && removedStick.prototypeID >= 0
                        && removedStick.prototypeID < static_cast<int>(prototypes.size())
                        && isStickPrototypeName(prototypes[static_cast<size_t>(removedStick.prototypeID)].name)) {
                        playerCtx.pickaxeHeld = true;
                        playerCtx.pickaxeGemKind = glm::clamp(gems->heldDrop.kind, 0, 3);
                        playerCtx.pickaxeHeadVoxels = std::move(stencilHeadVoxels);
                        gems->blockModeHoldingGem = false;
                        gems->heldDrop = GemDropState{};
                        gems->placementPreviewActive = false;
                        gems->placementPreviewPosition = glm::vec3(0.0f);
                        gems->placementPreviewRenderYOffset = 0.0f;
                        triggerGameplaySfx(baseSystem, "pickup_block.ck", 0.02f);
                        return;
                    }
                }
                if (!targetIsBlueprint
                    && isNaturalSurfaceStonePrototypeName(targetProto.name)
                    && playerCtx.leftMousePressed) {
                    const int ingotMaterial = hatchetMaterialFromGemKind(gems->heldDrop.kind);
                    const int ingotPrototypeID = resolveGemIngotPrototypeID(prototypes, ingotMaterial);
                    if (ingotPrototypeID >= 0) {
                        playerCtx.isHoldingBlock = true;
                        playerCtx.heldPrototypeID = ingotPrototypeID;
                        playerCtx.heldBlockColor = glm::vec3(1.0f);
                        playerCtx.heldPackedColor = 0u;
                        playerCtx.heldHasSourceCell = false;
                        playerCtx.heldSourceCell = glm::ivec3(0);
                        gems->blockModeHoldingGem = false;
                        gems->heldDrop = GemDropState{};
                        triggerGameplaySfx(baseSystem, "pickup_block.ck", 0.02f);
                        return;
                    }
                }
            }

            if (targetIsBlueprint) {
                glm::vec3 previewPos = glm::vec3(targetCell);
                if (!computeGemPlacementWithinCell(
                        baseSystem,
                        gems->heldDrop,
                        targetCell,
                        playerCtx.targetedBlockHitPosition,
                        &previewPos)) {
                    return;
                }

                const float previewYOffset = 2.0f / 24.0f;
                gems->placementPreviewActive = true;
                gems->placementPreviewPosition = previewPos;
                gems->placementPreviewRenderYOffset = previewYOffset;

                if (!playerCtx.leftMousePressed) return;

                gems->heldDrop.renderYOffset = previewYOffset;
                GemSystemLogic::PlaceGemDrop(baseSystem, std::move(gems->heldDrop), previewPos);
                gems->blockModeHoldingGem = false;
                gems->heldDrop = GemDropState{};
                gems->placementPreviewActive = false;
                triggerGameplaySfx(baseSystem, "place_block.ck", 0.02f);
                return;
            }

            if (!playerCtx.leftMousePressed) return;
            glm::vec3 placePos = playerCtx.targetedBlockPosition + playerCtx.targetedBlockNormal;
            if (BlockSelectionSystemLogic::HasBlockAt(baseSystem, prototypes, playerCtx.targetedWorldIndex, placePos)) return;

            gems->heldDrop.renderYOffset = 0.0f;
            GemSystemLogic::PlaceGemDrop(baseSystem, std::move(gems->heldDrop), placePos);
            gems->blockModeHoldingGem = false;
            gems->heldDrop = GemDropState{};
            triggerGameplaySfx(baseSystem, "place_block.ck", 0.02f);
        };
        auto tryPlaceHeldHatchet = [&](PlayerContext& playerCtx) -> bool {
            if (!playerCtx.hatchetHeld) return false;
            if (!playerCtx.hasBlockTarget || glm::length(playerCtx.targetedBlockNormal) < 0.1f) return false;
            if (playerCtx.targetedWorldIndex < 0 || playerCtx.targetedWorldIndex >= static_cast<int>(level.worlds.size())) return false;
            if (targetIsLatchedAnchorSuppressed()) return false;

            const glm::vec3 surfaceNormal = normalizeOrDefault(playerCtx.targetedBlockNormal, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::vec3 forward = cameraForwardDirection(playerCtx);
            playerCtx.hatchetPlacedInWorld = true;
            playerCtx.hatchetPlacedCell = glm::ivec3(glm::round(playerCtx.targetedBlockPosition));
            playerCtx.hatchetPlacedWorldIndex = playerCtx.targetedWorldIndex;
            playerCtx.hatchetPlacedNormal = surfaceNormal;
            playerCtx.hatchetPlacedDirection = projectDirectionOnSurface(forward, surfaceNormal);
            playerCtx.hatchetPlacedPosition = playerCtx.targetedBlockPosition - surfaceNormal * 0.47f;
            playerCtx.hatchetPlacedMaterial = glm::clamp(playerCtx.hatchetSelectedMaterial, 0, HATCHET_MATERIAL_COUNT - 1);
            playerCtx.hatchetHeld = false;
            for (int i = 0; i < HATCHET_MATERIAL_COUNT; ++i) {
                playerCtx.hatchetInventoryByMaterial[static_cast<size_t>(i)] = 0;
            }
            playerCtx.hatchetInventoryCount = 0;
            triggerGameplaySfx(baseSystem, "place_block.ck", 0.02f);
            return true;
        };

        auto tryDrawChalkDustUsingHeldChalk = [&](PlayerContext& playerCtx) -> bool {
            if (!playerCtx.isHoldingBlock) return false;
            if (playerCtx.heldPrototypeID < 0 || playerCtx.heldPrototypeID >= static_cast<int>(prototypes.size())) return false;
            const Entity& heldProto = prototypes[static_cast<size_t>(playerCtx.heldPrototypeID)];
            if (!isChalkDrawToolPrototypeName(heldProto.name)) return false;
            if (!playerCtx.hasBlockTarget) return false;
            if (glm::length(playerCtx.targetedBlockNormal) < 0.1f) return false;
            const glm::vec3 n = normalizeOrDefault(playerCtx.targetedBlockNormal, glm::vec3(0.0f, 1.0f, 0.0f));
            if (n.y < 0.7f) return false;

            int worldIndex = playerCtx.targetedWorldIndex;
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) {
                worldIndex = level.activeWorldIndex;
            }
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) {
                worldIndex = 0;
            }
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) return false;
            const glm::ivec3 placeCell = glm::ivec3(glm::round(playerCtx.targetedBlockPosition + playerCtx.targetedBlockNormal));
            CellBlockInfo existing;
            const bool hasExisting = queryBlockAtCell(baseSystem, level, prototypes, worldIndex, placeCell, &existing);
            if (hasExisting && existing.present && !isChalkDustPrototypeID(prototypes, existing.prototypeID)) {
                return false;
            }

            int dustPrototypeID = existing.prototypeID;
            if (!hasExisting || !existing.present || !isChalkDustPrototypeID(prototypes, dustPrototypeID)) {
                const bool preferX = (((placeCell.x ^ placeCell.y ^ placeCell.z) & 1) == 0);
                dustPrototypeID = resolveChalkDustPrototypeID(prototypes, preferX);
                if (dustPrototypeID < 0) return false;
                const uint32_t packed = packChalkDustSnapshotColor(CHALK_TILE_DOT, 0);
                if (baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                    baseSystem.voxelWorld->setBlockWorld(
                        placeCell,
                        static_cast<uint32_t>(dustPrototypeID),
                        packed
                    );
                    VoxelMeshingSystemLogic::RequestPriorityVoxelRemesh(baseSystem, prototypes, placeCell);
                    StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, worldIndex, glm::vec3(placeCell));
                } else {
                    const glm::vec3 placePos = glm::vec3(placeCell);
                    if (BlockSelectionSystemLogic::HasBlockAt(baseSystem, prototypes, worldIndex, placePos)) return false;
                    Entity& world = level.worlds[static_cast<size_t>(worldIndex)];
                    world.instances.push_back(HostLogic::CreateInstance(
                        baseSystem,
                        dustPrototypeID,
                        placePos,
                        glm::vec3(1.0f)
                    ));
                    BlockSelectionSystemLogic::AddBlockToCache(baseSystem, prototypes, worldIndex, placePos, dustPrototypeID);
                    StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, worldIndex, placePos);
                }
            }

            refreshChalkDustNeighborhood(baseSystem, level, prototypes, worldIndex, placeCell);
            triggerGameplaySfx(baseSystem, "place_block.ck", 0.02f);
            return true;
        };

        if (interactionMode
            && bKeyJustPressed
            && player.isHoldingBlock
            && player.heldPrototypeID >= 0
            && player.heldPrototypeID < static_cast<int>(prototypes.size())
            && isChalkDrawToolPrototypeName(prototypes[static_cast<size_t>(player.heldPrototypeID)].name)) {
            if (tryDrawChalkDustUsingHeldChalk(player)) {
                resetChargeState();
                return;
            }
        }

        auto tryCraftWorkbenchFromHammerHit = [&](PlayerContext& playerCtx) -> bool {
            if (!playerCtx.hasBlockTarget) return false;
            int worldIndex = playerCtx.targetedWorldIndex;
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) {
                worldIndex = level.activeWorldIndex;
            }
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) {
                worldIndex = 0;
            }
            if (worldIndex < 0 || worldIndex >= static_cast<int>(level.worlds.size())) return false;
            const glm::ivec3 targetCell = glm::ivec3(glm::round(playerCtx.targetedBlockPosition));

            CellBlockInfo targetInfo;
            if (!queryBlockAtCell(baseSystem, level, prototypes, worldIndex, targetCell, &targetInfo) || !targetInfo.present) return false;
            if (targetInfo.prototypeID < 0 || targetInfo.prototypeID >= static_cast<int>(prototypes.size())) return false;
            const Entity& targetProto = prototypes[static_cast<size_t>(targetInfo.prototypeID)];
            if (!isVerticalLogPrototypeName(targetProto.name)) return false;
            if (!isValidChalkCraftRing(baseSystem, level, prototypes, worldIndex, targetCell)) return false;

            const glm::vec3 forward = cameraForwardDirection(playerCtx);
            const glm::vec3 facingDir = -glm::vec3(forward.x, 0.0f, forward.z);
            const int workbenchID = resolveWorkbenchPrototypeID(prototypes, facingDir);
            if (workbenchID < 0) return false;

            if (!replaceBlockAtCell(
                    baseSystem,
                    level,
                    prototypes,
                    worldIndex,
                    targetCell,
                    targetInfo.prototypeID,
                    workbenchID,
                    glm::vec3(1.0f),
                    packColor(glm::vec3(1.0f)))) {
                return false;
            }
            clearBlockDamageAt(worldIndex, targetCell);
            triggerGameplaySfx(baseSystem, "place_block.ck", 0.02f);
            return true;
        };

        const bool throwModifierDown = PlatformInput::IsKeyDown(win, PlatformInput::Key::W);
        const bool throwWJustPressed = throwModifierDown && !throwWDownLastFrame;
        throwWDownLastFrame = throwModifierDown;
        if (!throwModifierDown) {
            throwRequireWReleaseForNextStart = false;
        }
        const double throwNow = PlatformInput::GetTimeSeconds();
        if (throwWJustPressed) throwLastWPressTime = throwNow;
        if (player.leftMousePressed) throwLastLmbPressTime = throwNow;
        const float throwChordWindowSeconds = std::max(
            0.01f,
            readRegistryFloat(baseSystem, "HeldThrowChordWindowSeconds", 0.14f)
        );

        if (player.isHoldingBlock && !holdingChalkTool) {
            const bool throwTailActive =
                player.blockChargeAction == BlockChargeAction::Throw
                && (player.isChargingBlock
                    || player.blockChargeExecuteGraceTimer > 0.0f
                    || player.blockChargeValue > 0.001f);
            const bool throwStartChord =
                !throwTailActive
                && !throwRequireWReleaseForNextStart
                && throwModifierDown
                && player.leftMouseDown
                && (throwWJustPressed || player.leftMousePressed)
                && (std::abs(throwLastWPressTime - throwLastLmbPressTime) <= static_cast<double>(throwChordWindowSeconds));
            const bool throwChargeGesture = throwStartChord || throwTailActive;
            if (!legacyDestroyMode && throwChargeGesture) {
                if (throwStartChord) {
                    // After one valid start, W must be released before the next start.
                    throwRequireWReleaseForNextStart = true;
                }
                const bool throwChargeDown = player.leftMouseDown
                    && (player.blockChargeAction == BlockChargeAction::Throw || throwStartChord);
                const bool throwExecutePressed = player.rightMousePressed;

                if (throwChargeDown) {
                    if (!player.isChargingBlock || player.blockChargeAction != BlockChargeAction::Throw) {
                        player.blockChargeValue = 0.0f;
                        player.blockChargeReady = false;
                    }
                    player.isChargingBlock = true;
                    player.blockChargeAction = BlockChargeAction::Throw;
                    player.blockChargeDecayTimer = chargeDecaySeconds;
                    player.blockChargeExecuteGraceTimer = 0.0f;
                    const float throwChargeSeconds = std::max(
                        0.05f,
                        readRegistryFloat(baseSystem, "HeldThrowChargeSeconds", CHARGE_TIME_PICKUP)
                    );
                    player.blockChargeValue += dt / throwChargeSeconds;
                    if (player.blockChargeValue >= 1.0f) {
                        player.blockChargeValue = 1.0f;
                        player.blockChargeReady = true;
                    }
                } else if (player.blockChargeAction == BlockChargeAction::Throw) {
                    releaseChargeToTail();
                    updateChargeTail();
                }

                const bool hasGraceWindow = player.blockChargeExecuteGraceTimer > 0.0f;
                const bool executeThrow = player.blockChargeAction == BlockChargeAction::Throw
                    && throwExecutePressed
                    && (throwChargeDown || hasGraceWindow);
                if (executeThrow) {
                    if (player.blockChargeReady && !HasActiveThrownHeldBlock()) {
                        if (StartThrownHeldBlockFromPlayer(baseSystem, level, player, prototypes)) {
                            triggerChargeFireInvertTail();
                        }
                        releaseChargeUseToTail();
                    } else {
                        resetChargeState();
                    }
                    return;
                }

                if (player.blockChargeAction == BlockChargeAction::Throw || throwChargeDown) {
                    return;
                }
            }

            if (!legacyDestroyMode) {
                tryPlaceHeldBlock(player);
                resetChargeState();
                return;
            }
            player.isHoldingBlock = false;
            player.heldPrototypeID = -1;
            player.heldPackedColor = 0u;
            player.heldHasSourceCell = false;
            player.heldSourceCell = glm::ivec3(0);
        }
        if (gems && gems->blockModeHoldingGem) {
            if (!legacyDestroyMode) {
                tryPlaceHeldGem(player);
                resetChargeState();
                return;
            }
            clearHeldGem();
        }

        // Combined interaction mode (toggle with E while in Pickup mode):
        // Default:
        //   Pickup: hold RMB to charge, then press LMB to execute.
        //   Destroy: hold LMB to charge, then press RMB to execute.
        // Swapped:
        //   Pickup: hold LMB to charge, then press RMB to execute.
        //   Destroy: hold RMB to charge, then press LMB to execute.
        const bool swappedControls = (!legacyDestroyMode && pickupHandMode && player.blockChargeControlsSwapped);
        const bool pickupChargeDown = swappedControls ? player.leftMouseDown : player.rightMouseDown;
        const bool pickupChargePressed = swappedControls ? player.leftMousePressed : player.rightMousePressed;
        const bool pickupExecutePressed = swappedControls ? player.rightMousePressed : player.leftMousePressed;
        const bool destroyChargeDown = swappedControls ? player.rightMouseDown : player.leftMouseDown;
        const bool destroyChargePressed = swappedControls ? player.rightMousePressed : player.leftMousePressed;
        const bool destroyExecutePressed = swappedControls ? player.leftMousePressed : player.rightMousePressed;

        BlockChargeAction activeAction = player.blockChargeAction;
        const bool destroyChargeRequiresHatchet = readRegistryBool(baseSystem, "DestroyChargeRequiresHatchet", false);
        const bool destroyUnlocked = !destroyChargeRequiresHatchet || !kHatchetsEnabled || player.hatchetHeld || holdingChalkTool;
        if (legacyDestroyMode) {
            activeAction = destroyUnlocked ? BlockChargeAction::Destroy : BlockChargeAction::None;
        } else if (activeAction == BlockChargeAction::Destroy && !destroyUnlocked) {
            activeAction = BlockChargeAction::None;
        } else if (activeAction == BlockChargeAction::None) {
            if ((!player.isHoldingBlock || holdingChalkTool) && !(gems && gems->blockModeHoldingGem)) {
                bool wantsDestroyCharge = destroyUnlocked && destroyChargeDown && !pickupChargeDown;
                bool wantsPickupCharge = pickupChargeDown && !destroyChargeDown;
                if (destroyUnlocked && destroyChargePressed && !pickupChargeDown) wantsDestroyCharge = true;
                if (pickupChargePressed && !destroyChargeDown) wantsPickupCharge = true;
                if (wantsDestroyCharge) activeAction = BlockChargeAction::Destroy;
                else if (wantsPickupCharge) activeAction = BlockChargeAction::Pickup;
            }
        }

        bool wantsCharge = false;
        if (activeAction == BlockChargeAction::Pickup) {
            wantsCharge = pickupChargeDown;
        } else if (activeAction == BlockChargeAction::Destroy) {
            wantsCharge = destroyChargeDown;
        }

        if (wantsCharge) {
            if (!player.isChargingBlock || player.blockChargeAction != activeAction) {
                player.blockChargeValue = 0.0f;
                player.blockChargeReady = false;
            }
            player.isChargingBlock = true;
            player.blockChargeAction = activeAction;
            player.blockChargeDecayTimer = chargeDecaySeconds;
            player.blockChargeExecuteGraceTimer = 0.0f;
            const bool destroyAction = (activeAction == BlockChargeAction::Destroy);
            float chargeTime = destroyAction ? CHARGE_TIME_DESTROY : CHARGE_TIME_PICKUP;
            player.blockChargeValue += dt / chargeTime;
            if (player.blockChargeValue >= 1.0f) {
                player.blockChargeValue = 1.0f;
                player.blockChargeReady = true;
            }
        } else {
            releaseChargeToTail();
            updateChargeTail();
        }

        const bool hasGraceWindow = player.blockChargeExecuteGraceTimer > 0.0f;
        bool executePickup = player.blockChargeAction == BlockChargeAction::Pickup
            && pickupExecutePressed
            && (pickupChargeDown || hasGraceWindow);
        bool executeDestroy = player.blockChargeAction == BlockChargeAction::Destroy
            && destroyExecutePressed
            && (destroyChargeDown || hasGraceWindow);

        if (executePickup || executeDestroy) {
            const bool firedCharge = player.blockChargeReady;
            if (firedCharge) {
                triggerChargeFireInvertTail();
                bool actionPerformed = false;
                const bool destroyAction = executeDestroy;

                if (kHatchetsEnabled
                    && !destroyAction
                    && !player.hatchetHeld
                    && player.hatchetPlacedInWorld
                    && player.hasBlockTarget) {
                    const glm::ivec3 targetCell = glm::ivec3(glm::round(player.targetedBlockPosition));
                    if (targetCell == player.hatchetPlacedCell
                        && worldMatches(player.targetedWorldIndex, player.hatchetPlacedWorldIndex)) {
                        setHeldHatchetMaterial(player.hatchetPlacedMaterial);
                        clearPlacedHatchet();
                        triggerGameplaySfx(baseSystem, "pickup_block.ck", 0.02f);
                        actionPerformed = true;
                    }
                }

                if (kHatchetsEnabled
                    && !actionPerformed
                    && !destroyAction
                    && player.hatchetHeld
                    && !player.isHoldingBlock
                    && !(gems && gems->blockModeHoldingGem)) {
                    if (tryPlaceHeldHatchet(player)) {
                        actionPerformed = true;
                    }
                }

                // In pickup action, gem interaction takes priority so terrain blocks don't consume the click first.
                if (!actionPerformed
                    && !destroyAction
                    && gems
                    && !player.isHoldingBlock
                    && !gems->blockModeHoldingGem) {
                    GemDropState pickedGem;
                    const glm::vec3 rayOrigin = cameraEyePosition(baseSystem, player);
                    const glm::vec3 rayDirection = cameraForwardDirection(player);
                    const float rayDistance = std::max(0.25f, readRegistryFloat(baseSystem, "GemPickupRayDistance", 5.0f));
                    if (GemSystemLogic::TryPickupGemFromRay(baseSystem, rayOrigin, rayDirection, rayDistance, &pickedGem)) {
                        gems->heldDrop = std::move(pickedGem);
                        gems->blockModeHoldingGem = true;
                        triggerGameplaySfx(baseSystem, "pickup_block.ck", 0.02f);
                        actionPerformed = true;
                    }
                }

                if (!actionPerformed && player.hasBlockTarget) {
                    if (targetIsLatchedAnchorSuppressed()) {
                        releaseChargeUseToTail();
                        return;
                    }
                    if (destroyAction && holdingChalkTool) {
                        (void)tryDrawChalkDustUsingHeldChalk(player);
                        releaseChargeUseToTail();
                        return;
                    }
                    if (destroyAction && !holdingChalkTool) {
                        if (tryCraftWorkbenchFromHammerHit(player)) {
                            actionPerformed = true;
                            releaseChargeUseToTail();
                            return;
                        }
                    }
                    if (destroyAction) {
                        const glm::ivec3 targetCell = glm::ivec3(glm::round(player.targetedBlockPosition));
                        if (GemChiselSystemLogic::StartGemChiselAtCell(baseSystem, targetCell, player.targetedWorldIndex)) {
                            actionPerformed = true;
                            releaseChargeUseToTail();
                            return;
                        }
                    }
                    if (destroyAction) {
                        glm::ivec3 targetCell = glm::ivec3(glm::round(player.targetedBlockPosition));
                        bool targetFromVoxel = false;
                        const int targetPrototypeID = resolveTargetPrototypeID(
                            baseSystem,
                            level,
                            prototypes,
                            player,
                            &targetCell,
                            &targetFromVoxel);
                        if (targetPrototypeID >= 0) {
                            glm::vec3 spawnForward = -player.targetedBlockNormal;
                            if (glm::length(spawnForward) < 0.01f) {
                                spawnForward = glm::vec3(0.0f, 0.0f, -1.0f);
                            }
                            const glm::vec3 targetPos = targetFromVoxel
                                ? glm::vec3(targetCell)
                                : player.targetedBlockPosition;
                            if (OreMiningSystemLogic::StartOreMiningFromBlock(
                                    baseSystem,
                                    prototypes,
                                    player.targetedWorldIndex,
                                    targetCell,
                                    targetPrototypeID,
                                    targetPos,
                                    spawnForward)) {
                                actionPerformed = true;
                                releaseChargeUseToTail();
                                return;
                            }
                        }
                    }
                    if (destroyAction) {
                        glm::ivec3 damageCell = glm::ivec3(glm::round(player.targetedBlockPosition));
                        bool damageFromVoxel = false;
                        const int damagePrototypeID = resolveTargetPrototypeID(
                            baseSystem,
                            level,
                            prototypes,
                            player,
                            &damageCell,
                            &damageFromVoxel
                        );
                        (void)damageFromVoxel;
                        if (damagePrototypeID >= 0
                            && damagePrototypeID < static_cast<int>(prototypes.size())
                            && isRemovableGameplayBlock(prototypes[static_cast<size_t>(damagePrototypeID)])) {
                            CellBlockInfo damageInfo;
                            queryBlockAtCell(baseSystem, level, prototypes, player.targetedWorldIndex, damageCell, &damageInfo);
                            const glm::vec3 damageColor = damageInfo.present ? damageInfo.color : glm::vec3(1.0f);
                            MiniVoxelParticleSystemLogic::SpawnFromBlock(
                                baseSystem,
                                prototypes,
                                player.targetedWorldIndex,
                                damageCell,
                                damagePrototypeID,
                                damageColor,
                                player.targetedBlockHitPosition,
                                player.targetedBlockNormal
                            );
                            const int requiredHits = std::max(1, readRegistryInt(baseSystem, "BlockBreakHitsBase", 8));
                            if (requiredHits > 1) {
                                const bool shouldBreakNow = applyBlockDamageHit(
                                    player.targetedWorldIndex,
                                    damageCell,
                                    damagePrototypeID,
                                    requiredHits,
                                    nullptr
                                );
                                if (!shouldBreakNow) {
                                    if (damageFromVoxel) {
                                        VoxelMeshingSystemLogic::RequestPriorityVoxelRemesh(baseSystem, prototypes, damageCell);
                                    }
                                    triggerGameplaySfx(baseSystem, "break_stone.ck", 0.02f);
                                    actionPerformed = true;
                                    releaseChargeUseToTail();
                                    return;
                                }
                            } else {
                                clearBlockDamageAt(player.targetedWorldIndex, damageCell);
                            }
                        }
                    }
                    RemovedBlockInfo removedBlock;
                    if (RemoveBlockAtPosition(baseSystem, level, prototypes, player.targetedBlockPosition, player.targetedWorldIndex, &removedBlock)) {
                        const glm::ivec3 removedCell = removedBlock.fromVoxel
                            ? removedBlock.voxelCell
                            : glm::ivec3(glm::round(player.targetedBlockPosition));
                        const bool removedWasChalkDust = removedBlock.prototypeID >= 0
                            && isChalkDustPrototypeID(prototypes, removedBlock.prototypeID);
                        const bool removedWasComputer = isComputerPrototypeID(prototypes, removedBlock.prototypeID);
                        clearBlockDamageAt(player.targetedWorldIndex, removedCell);
                        releaseLatchedAnchorAt(removedCell, player.targetedWorldIndex);
                        if (removedBlock.fromVoxel) {
                            StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, player.targetedWorldIndex, glm::vec3(removedBlock.voxelCell));
                        } else {
                            BlockSelectionSystemLogic::RemoveBlockFromCache(baseSystem, prototypes, player.targetedWorldIndex, player.targetedBlockPosition);
                            StructureCaptureSystemLogic::NotifyBlockChanged(baseSystem, player.targetedWorldIndex, player.targetedBlockPosition);
                        }
                        if (removedWasChalkDust) {
                            refreshChalkDustNeighborhood(baseSystem, level, prototypes, player.targetedWorldIndex, removedCell);
                        }
                        if (removedWasComputer && baseSystem.ui) {
                            baseSystem.ui->computerCacheBuilt = false;
                        }
                        // Ore rewards are owned by the ore-mining minigame success path.
                        // Direct destroy should never spawn gem rewards.
                        if (!destroyAction) {
                            player.isHoldingBlock = true;
                            player.heldPrototypeID = removedBlock.prototypeID;
                            player.heldBlockColor = removedBlock.color;
                            player.heldPackedColor = removedBlock.packedColor;
                            player.heldHasSourceCell = removedBlock.hasSourceCell;
                            player.heldSourceCell = removedBlock.sourceCell;
                        }
                        if (audioVisualizerProto && removedBlock.prototypeID == audioVisualizerProto->prototypeID) {
                            RayTracedAudioSystemLogic::InvalidateSourceCache(baseSystem);
                            ChucKSystemLogic::StopNoiseShred(baseSystem);
                        }
                        if (destroyAction) {
                            (void)SpawnCavePotLoot(
                                baseSystem,
                                level,
                                prototypes,
                                player.targetedWorldIndex,
                                removedBlock,
                                player
                            );
                            triggerGameplaySfx(baseSystem, "break_stone.ck", 0.02f);
                        } else {
                            triggerGameplaySfx(baseSystem, "pickup_block.ck", 0.02f);
                        }
                        actionPerformed = true;
                    }
                }
            }
            if (firedCharge) {
                releaseChargeUseToTail();
            } else {
                resetChargeState();
            }
        }

        if (!player.isHoldingBlock) {
            tryPlaceHeldBlock(player);
        }
    }

    void ApplyBlockDamageMaskUniforms(BaseSystem& baseSystem,
                                      std::vector<Entity>& prototypes,
                                      const Shader& shader,
                                      bool enableMask) {
        std::vector<BlockDamageMaskRenderEntry> entries;
        collectBlockDamageMaskEntries(baseSystem, prototypes, entries);
        const bool cracksEnabled = readRegistryBool(baseSystem, "BlockBreakCrackEnabled", true);
        uploadBlockDamageUniforms(baseSystem, entries, shader, enableMask && cracksEnabled);
    }

    void CollectDamagedVoxelCells(const BaseSystem&,
                                  int worldIndex,
                                  std::vector<glm::ivec3>& outCells) {
        outCells.clear();
        if (worldIndex < 0) return;
        const auto& damage = blockDamageMap();
        if (damage.empty()) return;
        outCells.reserve(damage.size());
        for (const auto& pair : damage) {
            const BlockDamageKey& key = pair.first;
            const BlockDamageEntry& entry = pair.second;
            if (key.worldIndex != worldIndex) continue;
            if (entry.hits <= 0 || entry.requiredHits <= 0) continue;
            outCells.push_back(key.cell);
        }
    }

    void RenderBlockDamage(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        (void)dt;
        (void)win;
        // Crack lines were replaced by in-shader erosion masks. Keep this hook to
        // prune stale damage state for systems that still call it.
        std::vector<BlockDamageMaskRenderEntry> entries;
        collectBlockDamageMaskEntries(baseSystem, prototypes, entries);
    }
}
