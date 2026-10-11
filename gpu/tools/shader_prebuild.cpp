// SPDX-License-Identifier: GPL-2.0-or-later
// bbport shader pre-build, offline check: translates every shader permutation a pipeline cache
// knows (its .meta: the specialization and resource layout a draw saw) from the program code
// in the player's own game files, without the game, and compares the SPIR-V with the cache's.
//
//   bb-shader-prebuild <cache dir> <code dir> [--write]
//
// <code dir> holds <program hash>.bin, the GCN code as the game maps it (scripts/shader_code.py
// extracts it from the dump). With --write the translated SPIR-V replaces the cache's.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "common/serdes.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/frontend/fetch_shader.h"
#include "shader_recompiler/info.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/recompiler.h"
#include "shader_recompiler/specialization.h"

namespace Vulkan {
bool LoadShaderMeta(Serialization::Archive& ar, Shader::Info& info,
                    std::optional<Shader::Gcn::FetchShaderData>& fetch_shader_data,
                    Shader::StageSpecialization& spec, size_t& perm_idx);
}

namespace fs = std::filesystem;

static std::vector<u8> ReadFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<u8>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: bb-shader-prebuild <cache dir> <code dir> [--write]\n");
        return 2;
    }
    const fs::path cache = argv[1], code_dir = argv[2];
    const bool write = argc > 3 && std::strcmp(argv[3], "--write") == 0;
    const bool trace = std::getenv("BB_PREBUILD_TRACE") != nullptr;

    const auto profile_blob = ReadFile(cache / "profile.bin");
    if (profile_blob.size() != sizeof(Shader::Profile)) {
        std::fprintf(stderr, "no usable profile.bin in %s\n", cache.string().c_str());
        return 2;
    }
    Shader::Profile profile{};
    std::memcpy(&profile, profile_blob.data(), sizeof(profile));

    u32 total = 0, identical = 0, different = 0, no_code = 0, skipped = 0, failed = 0;
    for (const auto& entry : fs::directory_iterator(cache)) {
        if (entry.path().extension() != ".meta") {
            continue;
        }
        ++total;
        if (trace) {
            std::fprintf(stderr, "meta %s\n", entry.path().filename().string().c_str());
            std::fflush(stderr);
        }
        Serialization::Archive ar{ReadFile(entry.path())};
        Shader::Info stored{};
        std::optional<Shader::Gcn::FetchShaderData> fetch;
        Shader::StageSpecialization spec{};
        size_t perm_idx = 0;
        if (!Vulkan::LoadShaderMeta(ar, stored, fetch, spec, perm_idx)) {
            ++skipped; // another cache version, or a motion-vector shader (session-local)
            continue;
        }
        if (stored.hw_stage == Shader::HwStage::Hull && !spec.tess_constants) {
            ++skipped; // its tessellation constants were not stored
            continue;
        }
        // Each vertex input's descriptor as the draw read it, by semantic.
        std::array<std::optional<AmdGpu::Buffer>, 32> attrib_sharps{};
        if (fetch) {
            if (spec.vs_attrib_sharps.size() != fetch->attributes.size()) {
                ++skipped; // stored before the descriptors were kept
                continue;
            }
            for (size_t i = 0; i < fetch->attributes.size(); ++i) {
                const auto semantic = fetch->attributes[i].semantic;
                if (semantic < attrib_sharps.size()) {
                    attrib_sharps[semantic] = spec.vs_attrib_sharps[i];
                }
            }
        }
        char name[64];
        std::snprintf(name, sizeof(name), "%016llx.bin", (unsigned long long)stored.pgm_hash);
        const auto code_bytes = ReadFile(code_dir / name);
        if (code_bytes.empty() || code_bytes.size() % 4) {
            ++no_code;
            continue;
        }
        std::vector<u32> code(code_bytes.size() / 4);
        std::memcpy(code.data(), code_bytes.data(), code_bytes.size());

        // The user data the draw saw: the first registers of its flattened copy.
        std::array<u32, Shader::ShaderParams::NumShaderUserData> user_data{};
        std::memcpy(user_data.data(), stored.flattened_ud_buf.data(),
                    std::min(stored.flattened_ud_buf.size(), user_data.size()) * sizeof(u32));
        const Shader::ShaderParams params{user_data, code, stored.pgm_hash};
        Shader::Info info(stored.hw_stage, stored.sw_stage, params);

        if (trace) {
            std::fprintf(stderr, "  translate %016llx perm %zu stage %d\n",
                         (unsigned long long)stored.pgm_hash, perm_idx, int(stored.hw_stage));
            std::fflush(stderr);
        }
        std::vector<u32> spv;
        Shader::Info::offline_flat_ud = &stored.flattened_ud_buf;
        Shader::Info::offline_tess_constants = spec.tess_constants ? &*spec.tess_constants : nullptr;
        Shader::Gcn::offline_fetch_shader = &fetch;
        Shader::Gcn::offline_attrib_sharps = &attrib_sharps;
        try {
            Shader::Pools pools;
            Shader::RuntimeInfo runtime_info = spec.runtime_info;
            // A geometry shader's copy shader is another program of the game: from the dump too.
            std::vector<u32> copy_code;
            if (stored.hw_stage == Shader::HwStage::Geometry) {
                char copy_name[64];
                std::snprintf(copy_name, sizeof(copy_name), "%016llx.copy.bin",
                              (unsigned long long)runtime_info.hw.gs.vs_copy_hash);
                const auto bytes = ReadFile(code_dir / copy_name);
                copy_code.resize(bytes.size() / 4);
                std::memcpy(copy_code.data(), bytes.data(), copy_code.size() * 4);
                runtime_info.hw.gs.vs_copy = copy_code;
            }
            Shader::Backend::Bindings binding = spec.start;
            const auto program = Shader::TranslateProgram(code, pools, info, runtime_info, profile);
            spv = Shader::Backend::SPIRV::EmitSPIRV(profile, runtime_info, program, binding);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%016llx_%zu: %s\n", (unsigned long long)stored.pgm_hash, perm_idx,
                         error.what());
        }
        Shader::Info::offline_flat_ud = nullptr;
        Shader::Info::offline_tess_constants = nullptr;
        Shader::Gcn::offline_fetch_shader = nullptr;
        Shader::Gcn::offline_attrib_sharps = nullptr;
        if (spv.empty()) {
            ++failed;
            continue;
        }

        char spv_name[64];
        std::snprintf(spv_name, sizeof(spv_name), "0x%016llx_%zu.spv",
                      (unsigned long long)stored.pgm_hash, perm_idx);
        const auto cached = ReadFile(cache / spv_name);
        const bool same = cached.size() == spv.size() * 4 &&
                          std::memcmp(cached.data(), spv.data(), cached.size()) == 0;
        if (same) {
            ++identical;
        } else {
            ++different;
            if (different <= 8) {
                std::printf("differs: %s (%zu bytes cached, %zu translated)\n", spv_name,
                            cached.size(), spv.size() * 4);
            }
        }
        if (write && !same) {
            std::ofstream(cache / spv_name, std::ios::binary)
                .write(reinterpret_cast<const char*>(spv.data()), spv.size() * 4);
        }
    }
    std::printf("shader pre-build: %u permutations: %u identical, %u different, %u without code, "
                "%u skipped, %u failed\n",
                total, identical, different, no_code, skipped, failed);
    return different || failed ? 1 : 0;
}
