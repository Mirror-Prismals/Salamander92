        bool triggerGameplaySfx(BaseSystem& baseSystem, const char* fileName, float cooldownSeconds = 0.0f) {
            if (!fileName) return false;
            static std::unordered_map<std::string, double> s_lastTrigger;
            const std::string keyName(fileName);
            const double now = PlatformInput::GetTimeSeconds();
            auto it = s_lastTrigger.find(keyName);
            if (it != s_lastTrigger.end() && (now - it->second) < static_cast<double>(cooldownSeconds)) {
                return false;
            }

            // Primary path: preloaded/game-thread-safe one-shot audio from AudioSystem.
            if (AudioSystemLogic::TriggerGameplaySfx(baseSystem, keyName, 1.0f)) {
                s_lastTrigger[keyName] = now;
                return true;
            }

            // Optional fallback for debugging/legacy behavior.
            if (!readRegistryBool(baseSystem, "GameplaySfxFallbackToChuck", false)) {
                return false;
            }
            if (!baseSystem.audio || !baseSystem.audio->chuck) return false;
            const std::string scriptPath = std::string("Procedures/chuck/gameplay/") + fileName;
            std::vector<t_CKUINT> ids;
            std::lock_guard<std::mutex> chuckLock(baseSystem.audio->chuck_vm_mutex);
            bool ok = baseSystem.audio->chuck->compileFile(scriptPath, "", 1, FALSE, &ids);
            if (!ok || ids.empty()) return false;
            s_lastTrigger[keyName] = now;
            return true;
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

        void clearBlockDamageAt(int worldIndex, const glm::ivec3& cell) {
            blockDamageMap().erase(BlockDamageKey{worldIndex, cell});
        }

        bool queryRemovableBlockAtCell(const BaseSystem& baseSystem,
                                       const std::vector<Entity>& prototypes,
                                       int worldIndex,
                                       const glm::ivec3& cell,
                                       int* outPrototypeID) {
            if (outPrototypeID) *outPrototypeID = -1;

            if (baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                const uint32_t id = baseSystem.voxelWorld->getBlockWorld(cell);
                if (id > 0 && id < prototypes.size()) {
                    const int protoID = static_cast<int>(id);
                    if (isRemovableGameplayBlock(prototypes[static_cast<size_t>(protoID)])) {
                        if (outPrototypeID) *outPrototypeID = protoID;
                        return true;
                    }
                }
            }

            if (!baseSystem.level) return false;
            if (worldIndex < 0 || worldIndex >= static_cast<int>(baseSystem.level->worlds.size())) return false;
            const glm::vec3 cellPos = glm::vec3(cell);
            const Entity& world = baseSystem.level->worlds[static_cast<size_t>(worldIndex)];
            for (const EntityInstance& inst : world.instances) {
                if (glm::distance(inst.position, cellPos) > POSITION_EPSILON) continue;
                if (inst.prototypeID < 0 || inst.prototypeID >= static_cast<int>(prototypes.size())) continue;
                if (!isRemovableGameplayBlock(prototypes[static_cast<size_t>(inst.prototypeID)])) continue;
                if (outPrototypeID) *outPrototypeID = inst.prototypeID;
                return true;
            }
            return false;
        }

        bool applyBlockDamageHit(int worldIndex,
                                 const glm::ivec3& cell,
                                 int prototypeID,
                                 int requiredHits,
                                 int* outHits) {
            const int clampedRequired = std::max(1, requiredHits);
            BlockDamageKey key;
            key.worldIndex = worldIndex;
            key.cell = cell;

            auto& damage = blockDamageMap();
            BlockDamageEntry& entry = damage[key];
            if (entry.prototypeID != prototypeID || entry.requiredHits != clampedRequired) {
                entry.hits = 0;
            }
            entry.prototypeID = prototypeID;
            entry.requiredHits = clampedRequired;
            entry.hits = std::min(entry.requiredHits, entry.hits + 1);
            entry.lastHitTime = PlatformInput::GetTimeSeconds();

            const int hitsNow = entry.hits;
            const bool shouldBreak = (hitsNow >= entry.requiredHits);
            if (shouldBreak) {
                damage.erase(key);
            }
            if (outHits) *outHits = hitsNow;
            return shouldBreak;
        }

        void decayBlockDamageOverTime(BaseSystem& baseSystem,
                                      std::vector<Entity>& prototypes) {
            auto& damage = blockDamageMap();
            if (damage.empty()) return;

            const bool repairEnabled = readRegistryBool(baseSystem, "BlockBreakRepairEnabled", true);
            if (!repairEnabled) return;
            const double repairTickSeconds = static_cast<double>(std::max(
                0.05f,
                readRegistryFloat(baseSystem, "BlockBreakRepairTickSeconds", 5.0f)
            ));
            const double now = PlatformInput::GetTimeSeconds();

            for (auto it = damage.begin(); it != damage.end();) {
                int presentPrototypeID = -1;
                if (!queryRemovableBlockAtCell(
                        baseSystem,
                        prototypes,
                        it->first.worldIndex,
                        it->first.cell,
                        &presentPrototypeID)) {
                    it = damage.erase(it);
                    continue;
                }
                if (it->second.prototypeID >= 0 && presentPrototypeID != it->second.prototypeID) {
                    it = damage.erase(it);
                    continue;
                }
                if (it->second.hits <= 0 || it->second.requiredHits <= 0) {
                    it = damage.erase(it);
                    continue;
                }

                const double elapsed = now - it->second.lastHitTime;
                if (elapsed < repairTickSeconds) {
                    ++it;
                    continue;
                }

                const int healTicks = static_cast<int>(std::floor(elapsed / repairTickSeconds));
                if (healTicks <= 0) {
                    ++it;
                    continue;
                }

                const int beforeHits = it->second.hits;
                it->second.hits = std::max(0, it->second.hits - healTicks);
                it->second.lastHitTime += static_cast<double>(healTicks) * repairTickSeconds;

                const bool changed = (it->second.hits != beforeHits);
                const glm::ivec3 changedCell = it->first.cell;
                const bool removeNow = (it->second.hits <= 0);
                if (removeNow) {
                    it = damage.erase(it);
                } else {
                    ++it;
                }

                if (changed && baseSystem.voxelWorld && baseSystem.voxelWorld->enabled) {
                    VoxelMeshingSystemLogic::RequestPriorityVoxelRemesh(baseSystem, prototypes, changedCell);
                }
            }
        }

        struct BlockDamageMaskRenderEntry {
            glm::ivec3 cell = glm::ivec3(0);
            float progress = 0.0f;
        };

        void collectBlockDamageMaskEntries(BaseSystem& baseSystem,
                                           std::vector<Entity>& prototypes,
                                           std::vector<BlockDamageMaskRenderEntry>& outEntries) {
            outEntries.clear();

            auto& damage = blockDamageMap();
            if (damage.empty()) return;

            struct Candidate {
                float distance2 = 0.0f;
                BlockDamageMaskRenderEntry entry;
            };
            std::vector<Candidate> candidates;
            candidates.reserve(damage.size());

            const bool hasPlayer = (baseSystem.player != nullptr);
            const glm::vec3 cameraPos = hasPlayer ? baseSystem.player->cameraPosition : glm::vec3(0.0f);

            for (auto it = damage.begin(); it != damage.end();) {
                int presentPrototypeID = -1;
                if (!queryRemovableBlockAtCell(
                        baseSystem,
                        prototypes,
                        it->first.worldIndex,
                        it->first.cell,
                        &presentPrototypeID)) {
                    it = damage.erase(it);
                    continue;
                }
                if (it->second.prototypeID >= 0 && presentPrototypeID != it->second.prototypeID) {
                    it = damage.erase(it);
                    continue;
                }
                if (it->second.hits <= 0 || it->second.requiredHits <= 0) {
                    it = damage.erase(it);
                    continue;
                }

                Candidate c;
                c.entry.cell = it->first.cell;
                c.entry.progress = glm::clamp(
                    static_cast<float>(it->second.hits) / static_cast<float>(std::max(1, it->second.requiredHits)),
                    0.0f,
                    1.0f
                );
                if (hasPlayer) {
                    const glm::vec3 d = glm::vec3(c.entry.cell) - cameraPos;
                    c.distance2 = glm::dot(d, d);
                }
                candidates.push_back(c);
                ++it;
            }

            if (candidates.empty()) return;

            std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
                return a.distance2 < b.distance2;
            });

            const int maxEntries = std::clamp(
                readRegistryInt(baseSystem, "BlockBreakMaskMaxBlocks", BLOCK_DAMAGE_SHADER_MAX),
                1,
                BLOCK_DAMAGE_SHADER_MAX
            );
            const int emitCount = std::min(static_cast<int>(candidates.size()), maxEntries);
            outEntries.reserve(static_cast<size_t>(emitCount));
            for (int i = 0; i < emitCount; ++i) {
                outEntries.push_back(candidates[static_cast<size_t>(i)].entry);
            }
        }

        void uploadBlockDamageUniforms(const BaseSystem& baseSystem,
                                       const std::vector<BlockDamageMaskRenderEntry>& entries,
                                       const Shader& shader,
                                       bool enableMask) {
            const int enabledLoc = shader.findUniform("blockDamageEnabled");
            if (enabledLoc < 0) return;

            const int count = enableMask ? static_cast<int>(entries.size()) : 0;
            shader.setIntUniform(enabledLoc, (count > 0) ? 1 : 0);

            const int countLoc = shader.findUniform("blockDamageCount");
            if (countLoc >= 0) {
                shader.setIntUniform(countLoc, count);
            }

            const int gridLoc = shader.findUniform("blockDamageGrid");
            if (gridLoc >= 0) {
                const float grid = glm::clamp(
                    readRegistryFloat(baseSystem, "BlockBreakMaskGrid", 24.0f),
                    4.0f,
                    96.0f
                );
                shader.setFloatUniform(gridLoc, grid);
            }

            if (count <= 0) return;

            const int cellsLoc = shader.findUniform("blockDamageCells");
            const int progressLoc = shader.findUniform("blockDamageProgress");
            if (cellsLoc < 0 || progressLoc < 0) return;

            std::vector<int> packedCells(static_cast<size_t>(count) * 3u, 0);
            std::vector<float> progress(static_cast<size_t>(count), 0.0f);
            for (int i = 0; i < count; ++i) {
                const BlockDamageMaskRenderEntry& e = entries[static_cast<size_t>(i)];
                packedCells[static_cast<size_t>(i) * 3u + 0u] = e.cell.x;
                packedCells[static_cast<size_t>(i) * 3u + 1u] = e.cell.y;
                packedCells[static_cast<size_t>(i) * 3u + 2u] = e.cell.z;
                progress[static_cast<size_t>(i)] = glm::clamp(e.progress, 0.0f, 1.0f);
            }

            shader.setInt3ArrayUniform(cellsLoc, count, packedCells.data());
            shader.setFloatArrayUniform(progressLoc, count, progress.data());
        }
