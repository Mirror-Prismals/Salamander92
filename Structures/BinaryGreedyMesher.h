#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace SalamanderBinaryGreedyMesher {

constexpr int kChunkSize = 62;
constexpr int kPaddedChunkSize = kChunkSize + 2;
constexpr int kChunkArea = kChunkSize * kChunkSize;
constexpr int kPaddedChunkArea = kPaddedChunkSize * kPaddedChunkSize;
constexpr int kPaddedChunkVolume = kPaddedChunkSize * kPaddedChunkSize * kPaddedChunkSize;

enum class Face : uint8_t {
    PosX = 0,
    NegX = 1,
    PosY = 2,
    NegY = 3,
    PosZ = 4,
    NegZ = 5,
};

struct Quad {
    Face face = Face::PosX;
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t z = 0;
    uint8_t width = 0;
    uint8_t height = 0;
    uint8_t type = 0;
};

struct MeshData {
    std::vector<Quad>* quads = nullptr;
    int quadCount = 0;
    int faceQuadBegin[6] = {0, 0, 0, 0, 0, 0};
    int faceQuadLength[6] = {0, 0, 0, 0, 0, 0};
};

void mesh(const uint8_t* voxels, MeshData& meshData);

}  // namespace SalamanderBinaryGreedyMesher
