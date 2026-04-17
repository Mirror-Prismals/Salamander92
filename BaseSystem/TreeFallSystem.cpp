        int findPrototypeIDByName(const std::vector<Entity>& prototypes, const std::string& name) {
            for (const auto& proto : prototypes) {
                if (proto.name == name) return proto.prototypeID;
            }
            return -1;
        }

        int horizontalLogPrototypeFor(const std::vector<Entity>& prototypes,
                                      int sourcePrototypeID,
                                      FallDirection direction) {
            if (sourcePrototypeID < 0 || sourcePrototypeID >= static_cast<int>(prototypes.size())) {
                return sourcePrototypeID;
            }
            const std::string& srcName = prototypes[sourcePrototypeID].name;
            bool familyTwo = srcName.find("2") != std::string::npos;
            const bool axisX = (direction == FallDirection::PosX || direction == FallDirection::NegX);
            const std::string targetName = axisX
                ? (familyTwo ? "FirLog2TexX" : "FirLog1TexX")
                : (familyTwo ? "FirLog2TexZ" : "FirLog1TexZ");
            int targetID = findPrototypeIDByName(prototypes, targetName);
            return (targetID >= 0) ? targetID : sourcePrototypeID;
        }

        int horizontalNubLogPrototypeFor(const std::vector<Entity>& prototypes,
                                         int sourcePrototypeID,
                                         FallDirection direction) {
            if (sourcePrototypeID < 0 || sourcePrototypeID >= static_cast<int>(prototypes.size())) {
                return sourcePrototypeID;
            }
            const std::string& srcName = prototypes[sourcePrototypeID].name;
            const bool familyTwo = srcName.find("2") != std::string::npos;
            const bool axisX = (direction == FallDirection::PosX || direction == FallDirection::NegX);
            const std::string targetName = axisX
                ? (familyTwo ? "FirLog2NubTexX" : "FirLog1NubTexX")
                : (familyTwo ? "FirLog2NubTexZ" : "FirLog1NubTexZ");
            int targetID = findPrototypeIDByName(prototypes, targetName);
            if (targetID >= 0) return targetID;
            return horizontalLogPrototypeFor(prototypes, sourcePrototypeID, direction);
        }

        int topLogPrototypeFor(const std::vector<Entity>& prototypes,
                               int sourcePrototypeID) {
            if (sourcePrototypeID < 0 || sourcePrototypeID >= static_cast<int>(prototypes.size())) {
                return sourcePrototypeID;
            }
            const std::string& srcName = prototypes[sourcePrototypeID].name;
            const bool familyTwo = srcName.find("2") != std::string::npos;
            const std::string targetName = familyTwo ? "FirLog2TopTex" : "FirLog1TopTex";
            int targetID = findPrototypeIDByName(prototypes, targetName);
            return (targetID >= 0) ? targetID : sourcePrototypeID;
        }

        FallDirection chooseFallDirection(const glm::ivec3& pivot, uint32_t salt) {
            const uint32_t h = hash3D(pivot.x + static_cast<int>(salt), pivot.y, pivot.z - static_cast<int>(salt));
            switch (h % 4u) {
                case 0: return FallDirection::PosX;
                case 1: return FallDirection::NegX;
                case 2: return FallDirection::PosZ;
                default: return FallDirection::NegZ;
            }
        }

        glm::ivec3 rotateAroundPivot90(const glm::ivec3& pos,
                                       const glm::ivec3& pivot,
                                       FallDirection direction) {
            const glm::ivec3 local = pos - pivot;
            switch (direction) {
                case FallDirection::PosX: // +Y -> +X
                    return glm::ivec3(pivot.x + local.y, pivot.y - local.x, pivot.z + local.z);
                case FallDirection::NegX: // +Y -> -X
                    return glm::ivec3(pivot.x - local.y, pivot.y + local.x, pivot.z + local.z);
                case FallDirection::PosZ: // +Y -> +Z
                    return glm::ivec3(pivot.x + local.x, pivot.y - local.z, pivot.z + local.y);
                case FallDirection::NegZ: // +Y -> -Z
                    return glm::ivec3(pivot.x + local.x, pivot.y + local.z, pivot.z - local.y);
                default:
                    return pos;
            }
        }

        void collectConnectedLogs(const VoxelWorldContext& voxelWorld,
                                  const std::vector<Entity>& prototypes,
                                  const glm::ivec3& seedCell,
                                  std::unordered_set<glm::ivec3, IVec3Hash>& outLogs) {
            static const std::array<glm::ivec3, 6> kNeighbors = {
                glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0),
                glm::ivec3(0, 1, 0), glm::ivec3(0, -1, 0),
                glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)
            };
            outLogs.clear();
            if (!isPineAnyLogPrototypeID(prototypes, voxelWorld.getBlockWorld(seedCell))) return;
            std::queue<glm::ivec3> q;
            q.push(seedCell);
            outLogs.insert(seedCell);
            while (!q.empty()) {
                glm::ivec3 cell = q.front();
                q.pop();
                for (const auto& step : kNeighbors) {
                    glm::ivec3 next = cell + step;
                    if (outLogs.count(next) > 0) continue;
                    if (!isPineAnyLogPrototypeID(prototypes, voxelWorld.getBlockWorld(next))) continue;
                    outLogs.insert(next);
                    q.push(next);
                }
            }
        }

        void collectConnectedLeaves(const VoxelWorldContext& voxelWorld,
                                    const std::vector<Entity>& prototypes,
                                    const std::unordered_set<glm::ivec3, IVec3Hash>& logs,
                                    std::unordered_set<glm::ivec3, IVec3Hash>& outLeaves) {
            static const std::array<glm::ivec3, 6> kLeafSteps = {
                glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0),
                glm::ivec3(0, 1, 0), glm::ivec3(0, -1, 0),
                glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)
            };
            outLeaves.clear();
            if (logs.empty()) return;

            glm::ivec3 minC(1 << 30);
            glm::ivec3 maxC(-(1 << 30));
            for (const auto& cell : logs) {
                minC = glm::min(minC, cell);
                maxC = glm::max(maxC, cell);
            }
            minC -= glm::ivec3(6, 6, 6);
            maxC += glm::ivec3(6, 6, 6);
            auto inBounds = [&](const glm::ivec3& p) {
                return p.x >= minC.x && p.y >= minC.y && p.z >= minC.z
                    && p.x <= maxC.x && p.y <= maxC.y && p.z <= maxC.z;
            };

            std::queue<glm::ivec3> q;
            for (const auto& log : logs) {
                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            glm::ivec3 near = log + glm::ivec3(dx, dy, dz);
                            if (!inBounds(near)) continue;
                            if (outLeaves.count(near) > 0) continue;
                            if (!isLeafPrototypeID(prototypes, voxelWorld.getBlockWorld(near))) continue;
                            outLeaves.insert(near);
                            q.push(near);
                        }
                    }
                }
            }

            while (!q.empty()) {
                glm::ivec3 cell = q.front();
                q.pop();
                for (const auto& step : kLeafSteps) {
                    glm::ivec3 next = cell + step;
                    if (!inBounds(next)) continue;
                    if (outLeaves.count(next) > 0) continue;
                    if (!isLeafPrototypeID(prototypes, voxelWorld.getBlockWorld(next))) continue;
                    outLeaves.insert(next);
                    q.push(next);
                }
            }
        }

        void processSingleTreeFall(BaseSystem& baseSystem,
                                   std::vector<Entity>& prototypes,
                                   const glm::ivec3& removedCell) {
            if (!baseSystem.voxelWorld || !baseSystem.voxelWorld->enabled) return;
            VoxelWorldContext& voxelWorld = *baseSystem.voxelWorld;

            glm::ivec3 seed = removedCell + glm::ivec3(0, 1, 0);
            if (!isPineAnyLogPrototypeID(prototypes, voxelWorld.getBlockWorld(seed))) return;

            std::unordered_set<glm::ivec3, IVec3Hash> logs;
            collectConnectedLogs(voxelWorld, prototypes, seed, logs);
            if (logs.empty()) return;

            std::unordered_set<glm::ivec3, IVec3Hash> leaves;
            collectConnectedLeaves(voxelWorld, prototypes, logs, leaves);

            struct CellData {
                glm::ivec3 pos;
                uint32_t id = 0;
                uint32_t color = 0;
                bool isLog = false;
            };
            std::vector<CellData> cells;
            cells.reserve(logs.size() + leaves.size());
            std::unordered_set<glm::ivec3, IVec3Hash> cleared;
            cleared.reserve(logs.size() + leaves.size());

            for (const auto& cell : logs) {
                uint32_t id = voxelWorld.getBlockWorld(cell);
                if (!isPineAnyLogPrototypeID(prototypes, id)) continue;
                cells.push_back({cell, id, voxelWorld.getColorWorld(cell), true});
                cleared.insert(cell);
            }
            for (const auto& cell : leaves) {
                uint32_t id = voxelWorld.getBlockWorld(cell);
                if (!isLeafPrototypeID(prototypes, id)) continue;
                cells.push_back({cell, id, voxelWorld.getColorWorld(cell), false});
                cleared.insert(cell);
            }
            if (cells.empty()) return;

            for (const auto& cell : cleared) {
                voxelWorld.setBlockWorld(cell, 0, 0);
            }

            const FallDirection direction = chooseFallDirection(removedCell, static_cast<uint32_t>(baseSystem.frameIndex & 0xffffffffu));

            struct Placement {
                glm::ivec3 pos;
                uint32_t id = 0;
                uint32_t color = 0;
                bool isLog = false;
            };
            std::vector<Placement> placements;
            placements.reserve(cells.size());
            std::unordered_set<glm::ivec3, IVec3Hash> occupied;
            occupied.reserve(cells.size());

            for (const auto& cell : cells) {
                glm::ivec3 rotated = rotateAroundPivot90(cell.pos, removedCell, direction);
                uint32_t placeId = cell.id;
                if (cell.isLog) {
                    int horizontalID = horizontalLogPrototypeFor(prototypes, static_cast<int>(cell.id), direction);
                    if (horizontalID >= 0) placeId = static_cast<uint32_t>(horizontalID);
                }
                placements.push_back({rotated, placeId, cell.color, cell.isLog});
                occupied.insert(rotated);
            }

            auto canDropOne = [&]() {
                for (const auto& p : placements) {
                    glm::ivec3 below = p.pos + glm::ivec3(0, -1, 0);
                    if (below.y < -512) return false;
                    if (occupied.count(below) > 0) continue;
                    if (voxelWorld.getBlockWorld(below) != 0) return false;
                }
                return true;
            };

            while (canDropOne()) {
                occupied.clear();
                for (auto& p : placements) {
                    p.pos.y -= 1;
                    occupied.insert(p.pos);
                }
            }

            std::vector<glm::ivec3> changedCells;
            changedCells.reserve(cleared.size() + placements.size());
            int placedLogCount = 0;
            for (const auto& cell : cleared) changedCells.push_back(cell);
            for (const auto& p : placements) {
                if (voxelWorld.getBlockWorld(p.pos) != 0) continue;
                voxelWorld.setBlockWorld(p.pos, p.id, p.color);
                changedCells.push_back(p.pos);
                if (p.isLog) placedLogCount += 1;
            }
            if (placedLogCount > 0) {
                triggerGameplaySfx(baseSystem, "tree_fall.ck", 0.12f);
            }

            if (!changedCells.empty()) {
                std::unordered_set<glm::ivec3, IVec3Hash> touchedSections;
                touchedSections.reserve(changedCells.size());
                const int sectionSize = voxelWorld.sectionSize > 0 ? voxelWorld.sectionSize : 64;
                for (const auto& cell : changedCells) {
                    glm::ivec3 sec(
                        floorDivInt(cell.x, sectionSize),
                        floorDivInt(cell.y, sectionSize),
                        floorDivInt(cell.z, sectionSize)
                    );
                    touchedSections.insert(sec);
                }
                for (const auto& sec : touchedSections) {
                    glm::ivec3 requestCell = sec * sectionSize;
                    VoxelMeshingSystemLogic::RequestPriorityVoxelRemesh(baseSystem, prototypes, requestCell);
                }
            }
        }
