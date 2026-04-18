#include "Structures/BinaryGreedyMesher.h"

#include <algorithm>
#include <array>

namespace SalamanderBinaryGreedyMesher {
namespace {

inline int voxelIndex(int x, int y, int z) {
    return z + x * kPaddedChunkSize + y * kPaddedChunkArea;
}

inline int maskIndex(int u, int v) {
    return v * kChunkSize + u;
}

struct FaceBuildConfig {
    Face face;
    int renderFaceIndex;
};

void emitFaceQuads(const std::array<uint8_t, kChunkArea>& mask,
                   Face face,
                   int fixed,
                   std::vector<Quad>& outQuads,
                   int& faceBegin,
                   int& faceLength) {
    std::array<uint8_t, kChunkArea> scratch = mask;
    faceBegin = static_cast<int>(outQuads.size());

    for (int v = 0; v < kChunkSize; ++v) {
        for (int u = 0; u < kChunkSize; ++u) {
            const int startIndex = maskIndex(u, v);
            const uint8_t type = scratch[static_cast<size_t>(startIndex)];
            if (type == 0) continue;

            int width = 1;
            while (u + width < kChunkSize
                   && scratch[static_cast<size_t>(maskIndex(u + width, v))] == type) {
                ++width;
            }

            int height = 1;
            bool canGrow = true;
            while (v + height < kChunkSize && canGrow) {
                for (int rowU = 0; rowU < width; ++rowU) {
                    if (scratch[static_cast<size_t>(maskIndex(u + rowU, v + height))] != type) {
                        canGrow = false;
                        break;
                    }
                }
                if (canGrow) ++height;
            }

            for (int clearV = 0; clearV < height; ++clearV) {
                for (int clearU = 0; clearU < width; ++clearU) {
                    scratch[static_cast<size_t>(maskIndex(u + clearU, v + clearV))] = 0;
                }
            }

            Quad quad;
            quad.face = face;
            quad.type = type;
            quad.width = static_cast<uint8_t>(width);
            quad.height = static_cast<uint8_t>(height);

            switch (face) {
                case Face::PosX:
                case Face::NegX:
                    quad.x = static_cast<uint8_t>(fixed);
                    quad.y = static_cast<uint8_t>(u);
                    quad.z = static_cast<uint8_t>(v);
                    break;
                case Face::PosY:
                case Face::NegY:
                    quad.x = static_cast<uint8_t>(u);
                    quad.y = static_cast<uint8_t>(fixed);
                    quad.z = static_cast<uint8_t>(v);
                    break;
                case Face::PosZ:
                case Face::NegZ:
                    quad.x = static_cast<uint8_t>(u);
                    quad.y = static_cast<uint8_t>(v);
                    quad.z = static_cast<uint8_t>(fixed);
                    break;
            }

            outQuads.push_back(quad);
        }
    }

    faceLength = static_cast<int>(outQuads.size()) - faceBegin;
}

void buildFaceMask(const uint8_t* voxels,
                   Face face,
                   int fixed,
                   std::array<uint8_t, kChunkArea>& outMask) {
    outMask.fill(0);

    for (int v = 0; v < kChunkSize; ++v) {
        for (int u = 0; u < kChunkSize; ++u) {
            int x = 0;
            int y = 0;
            int z = 0;
            int nx = 0;
            int ny = 0;
            int nz = 0;

            switch (face) {
                case Face::PosX:
                    x = fixed;
                    y = u;
                    z = v;
                    nx = x + 1;
                    ny = y;
                    nz = z;
                    break;
                case Face::NegX:
                    x = fixed;
                    y = u;
                    z = v;
                    nx = x - 1;
                    ny = y;
                    nz = z;
                    break;
                case Face::PosY:
                    x = u;
                    y = fixed;
                    z = v;
                    nx = x;
                    ny = y + 1;
                    nz = z;
                    break;
                case Face::NegY:
                    x = u;
                    y = fixed;
                    z = v;
                    nx = x;
                    ny = y - 1;
                    nz = z;
                    break;
                case Face::PosZ:
                    x = u;
                    y = v;
                    z = fixed;
                    nx = x;
                    ny = y;
                    nz = z + 1;
                    break;
                case Face::NegZ:
                    x = u;
                    y = v;
                    z = fixed;
                    nx = x;
                    ny = y;
                    nz = z - 1;
                    break;
            }

            const uint8_t type = voxels[static_cast<size_t>(voxelIndex(x + 1, y + 1, z + 1))];
            if (type == 0) continue;

            const uint8_t neighbor = voxels[static_cast<size_t>(voxelIndex(nx + 1, ny + 1, nz + 1))];
            if (neighbor != 0) continue;

            outMask[static_cast<size_t>(maskIndex(u, v))] = type;
        }
    }
}

}  // namespace

void mesh(const uint8_t* voxels, MeshData& meshData) {
    if (!voxels || !meshData.quads) return;

    meshData.quads->clear();
    meshData.quadCount = 0;
    for (int i = 0; i < 6; ++i) {
        meshData.faceQuadBegin[i] = 0;
        meshData.faceQuadLength[i] = 0;
    }

    std::array<uint8_t, kChunkArea> mask{};
    constexpr std::array<Face, 6> kFaces = {
        Face::PosX,
        Face::NegX,
        Face::PosY,
        Face::NegY,
        Face::PosZ,
        Face::NegZ
    };

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        const Face face = kFaces[static_cast<size_t>(faceIndex)];
        const int faceBegin = static_cast<int>(meshData.quads->size());
        for (int fixed = 0; fixed < kChunkSize; ++fixed) {
            buildFaceMask(voxels, face, fixed, mask);
            int sliceBegin = 0;
            int sliceLength = 0;
            emitFaceQuads(mask, face, fixed, *meshData.quads, sliceBegin, sliceLength);
        }
        meshData.faceQuadBegin[faceIndex] = faceBegin;
        meshData.faceQuadLength[faceIndex] =
            static_cast<int>(meshData.quads->size()) - faceBegin;
    }

    meshData.quadCount = static_cast<int>(meshData.quads->size());
}

}  // namespace SalamanderBinaryGreedyMesher
