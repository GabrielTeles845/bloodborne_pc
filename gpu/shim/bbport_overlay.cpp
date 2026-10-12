// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_overlay.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <cfloat>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

#include <SDL3/SDL.h>
#include "bbport_settings.h"
#include "bbport_trophies.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_vulkan.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

// DejaVu Sans (Cyrillic), embedded (third_party/fonts, Bitstream Vera license).
#ifdef _WIN32
asm(".section .rdata,\"dr\"\n"
    ".balign 16\n"
    ".global bb_font_ttf\n"
    "bb_font_ttf:\n"
    ".incbin \"" BB_FONT_PATH "\"\n"
    ".global bb_font_ttf_end\n"
    "bb_font_ttf_end:\n"
    ".text\n");
#else
asm(".section .rodata\n"
    ".balign 16\n"
    ".hidden bb_font_ttf\n"
    ".global bb_font_ttf\n"
    "bb_font_ttf:\n"
    ".incbin \"" BB_FONT_PATH "\"\n"
    ".hidden bb_font_ttf_end\n"
    ".global bb_font_ttf_end\n"
    "bb_font_ttf_end:\n"
    ".previous\n");
#endif
extern "C" const unsigned char bb_font_ttf[];
extern "C" const unsigned char bb_font_ttf_end[];

extern "C" void runtime_restart(void); // bb-probe (probe.c)

namespace BbOverlay {

namespace {

std::mutex imgui_mutex; // the ImGui context: window thread (input) and present thread
bool initialized = false;
std::atomic<bool> menu_open{false};
std::atomic<std::int64_t> menu_opened_ms{0};
bool l3_down = false, r3_down = false;
int active_tab = 0; // Hunt, Trophy gallery, Image tools, Gameplay extras
bool dirty = false; // settings changed while open: saved on close
bool mod_selection_changed = false;
std::string mod_selection_error;
// The game's text dialog (SetTextEntry), guarded by imgui_mutex.
bool text_entry_active = false;
std::string text_entry_prompt, text_entry_text;
float base_scale = 1.0f;
ImFont* guide_serif_font = nullptr; // Georgia from Windows when present; embedded font remains the fallback.

// Present rate for the FPS counter.
std::chrono::steady_clock::time_point last_present{};
float frame_ms_avg = 0.0f;

// Trophy events can arrive on the game thread while the present thread draws the overlay.
std::mutex trophy_mutex;
std::array<unsigned char, 128> trophies{};
struct ToastEvent {
    int id;
    bool preview;
};
std::deque<ToastEvent> toast_queue;
std::atomic<int> queued_toasts{0};
std::atomic<int> active_toast{-1};
std::atomic<bool> active_toast_preview{false};
int64_t toast_start_ms = 0; // present thread only
std::unordered_map<int, std::unique_ptr<ImTextureData>> trophy_icons;

std::vector<unsigned char> DecodePng(const std::filesystem::path& path, int& width, int& height) {
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec || bytes == 0 || bytes > 2 * 1024 * 1024) return {};
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::vector<unsigned char> input(bytes);
    if (!file.read(reinterpret_cast<char*>(input.data()), std::streamsize(input.size()))) return {};
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_PNG);
    if (!codec) return {};
    AVCodecContext* context = avcodec_alloc_context3(codec);
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    std::vector<unsigned char> pixels;
    if (context && packet && frame && avcodec_open2(context, codec, nullptr) >= 0 &&
        av_new_packet(packet, int(input.size())) >= 0) {
        std::memcpy(packet->data, input.data(), input.size());
        if (avcodec_send_packet(context, packet) >= 0 &&
            avcodec_receive_frame(context, frame) >= 0 &&
            frame->width > 0 && frame->height > 0 && frame->width <= 1024 &&
            frame->height <= 1024) {
            width = frame->width;
            height = frame->height;
            pixels.resize(std::size_t(width) * height * 4);
            SwsContext* converter = sws_getContext(width, height,
                static_cast<AVPixelFormat>(frame->format), width, height, AV_PIX_FMT_RGBA,
                SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (converter) {
                std::uint8_t* out[] = {pixels.data()};
                const int stride[] = {width * 4};
                if (sws_scale(converter, frame->data, frame->linesize, 0, height, out, stride) != height)
                    pixels.clear();
                sws_freeContext(converter);
            } else pixels.clear();
        }
    }
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&context);
    return pixels;
}

ImTextureRef IconRef(int id) {
    auto it = trophy_icons.find(id);
    if (it != trophy_icons.end())
        return it->second ? it->second->GetTexRef() : ImTextureRef{};
    const auto entry = BbTrophies::Find(id);
    int width = 0, height = 0;
    const auto pixels = entry.icon.empty() ? std::vector<unsigned char>{} :
                        DecodePng(entry.icon, width, height);
    if (pixels.empty()) {
        trophy_icons.emplace(id, nullptr);
        return {};
    }
    auto texture = std::make_unique<ImTextureData>();
    texture->Create(ImTextureFormat_RGBA32, width, height);
    std::memcpy(texture->Pixels, pixels.data(), pixels.size());
    texture->UseColors = true;
    ImGui::RegisterUserTexture(texture.get());
    const auto ref = texture->GetTexRef();
    trophy_icons.emplace(id, std::move(texture));
    return ref;
}

ImU32 GradeColor(BbTrophies::Grade grade, float alpha = 1.0f) {
    const ImVec4 color = grade == BbTrophies::Grade::Bronze ? ImVec4(.77f, .45f, .24f, alpha) :
                         grade == BbTrophies::Grade::Silver ? ImVec4(.74f, .79f, .84f, alpha) :
                         grade == BbTrophies::Grade::Gold ? ImVec4(1.0f, .78f, .27f, alpha) :
                         grade == BbTrophies::Grade::Platinum ? ImVec4(.66f, .83f, 1.0f, alpha) :
                         ImVec4(.35f, .78f, .42f, alpha);
    return ImGui::ColorConvertFloat4ToU32(color);
}

void TrophyGlyph(ImDrawList* draw, ImVec2 center, float size, float alpha) {
    const ImU32 white = IM_COL32(255, 255, 255, int(255 * alpha));
    const float x = center.x, y = center.y, s = size;
    const ImVec2 bowl[] = {{x - .30f*s, y - .24f*s}, {x + .30f*s, y - .24f*s},
                           {x + .22f*s, y + .13f*s}, {x, y + .25f*s},
                           {x - .22f*s, y + .13f*s}};
    draw->AddConvexPolyFilled(bowl, 5, white);
    draw->AddLine({x - .29f*s, y - .15f*s}, {x - .43f*s, y - .15f*s}, white, .065f*s);
    draw->AddLine({x - .43f*s, y - .15f*s}, {x - .35f*s, y + .09f*s}, white, .065f*s);
    draw->AddLine({x + .29f*s, y - .15f*s}, {x + .43f*s, y - .15f*s}, white, .065f*s);
    draw->AddLine({x + .43f*s, y - .15f*s}, {x + .35f*s, y + .09f*s}, white, .065f*s);
    draw->AddRectFilled({x - .055f*s, y + .23f*s}, {x + .055f*s, y + .42f*s}, white);
    draw->AddRectFilled({x - .25f*s, y + .39f*s}, {x + .25f*s, y + .46f*s}, white,
                        .035f*s);
}

int64_t ClockMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

void SetOpen(bool value) {
    if (menu_open.exchange(value) == value) {
        return;
    }
    ImGui::GetIO().MouseDrawCursor = value;
    if (value) {
        active_tab = 0;
        menu_opened_ms = ClockMs();
    }
    else for (const auto key : {ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight,
                                ImGuiKey_GamepadLStickUp, ImGuiKey_GamepadLStickDown})
        ImGui::GetIO().AddKeyAnalogEvent(key, false, 0.0f);
    if (!value && dirty) {
        dirty = false;
        BbSettings::Save();
    }
}

ImGuiKey KeyFromSdl(SDL_Keycode key) {
    switch (key) {
    case SDLK_TAB: return ImGuiKey_Tab;
    case SDLK_LEFT: return ImGuiKey_LeftArrow;
    case SDLK_RIGHT: return ImGuiKey_RightArrow;
    case SDLK_UP: return ImGuiKey_UpArrow;
    case SDLK_DOWN: return ImGuiKey_DownArrow;
    case SDLK_PAGEUP: return ImGuiKey_PageUp;
    case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
    case SDLK_HOME: return ImGuiKey_Home;
    case SDLK_END: return ImGuiKey_End;
    case SDLK_DELETE: return ImGuiKey_Delete;
    case SDLK_BACKSPACE: return ImGuiKey_Backspace;
    case SDLK_SPACE: return ImGuiKey_Space;
    case SDLK_RETURN: return ImGuiKey_Enter;
    case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
    case SDLK_ESCAPE: return ImGuiKey_Escape;
    case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
    case SDLK_RCTRL: return ImGuiKey_RightCtrl;
    case SDLK_LSHIFT: return ImGuiKey_LeftShift;
    case SDLK_RSHIFT: return ImGuiKey_RightShift;
    case SDLK_LALT: return ImGuiKey_LeftAlt;
    case SDLK_RALT: return ImGuiKey_RightAlt;
    default: return ImGuiKey_None;
    }
}

ImGuiKey KeyFromGamepad(u8 button) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return ImGuiKey_GamepadFaceDown;
    case SDL_GAMEPAD_BUTTON_EAST: return ImGuiKey_GamepadFaceRight;
    case SDL_GAMEPAD_BUTTON_WEST: return ImGuiKey_GamepadFaceLeft;
    case SDL_GAMEPAD_BUTTON_NORTH: return ImGuiKey_GamepadFaceUp;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return ImGuiKey_GamepadDpadUp;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return ImGuiKey_GamepadDpadDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return ImGuiKey_GamepadDpadLeft;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return ImGuiKey_GamepadDpadRight;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return ImGuiKey_GamepadL1;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return ImGuiKey_GamepadR1;
    case SDL_GAMEPAD_BUTTON_START: return ImGuiKey_GamepadStart;
    case SDL_GAMEPAD_BUTTON_BACK: return ImGuiKey_GamepadBack;
    default: return ImGuiKey_None;
    }
}

float PixelDensity(SDL_WindowID id) {
    SDL_Window* window = SDL_GetWindowFromID(id);
    const float density = window ? SDL_GetWindowPixelDensity(window) : 1.0f;
    return density > 0.0f ? density : 1.0f;
}

// Marks the settings dirty when a widget changed them.
template <typename T>
void Store(std::atomic<T>& target, T value, bool changed) {
    if (changed) {
        target = value;
        dirty = true;
    }
}

void Checkbox(const char* label, std::atomic<bool>& value) {
    bool v = value;
    const bool changed = ImGui::Checkbox(label, &v);
    Store(value, v, changed);
}

void Slider(const char* label, std::atomic<float>& value, float lo, float hi) {
    float v = value;
    const bool changed = ImGui::SliderFloat(label, &v, lo, hi, "%.2f");
    Store(value, v, changed);
}

void Hint(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

bool RestartNeeded() {
    const auto& s = BbSettings::Get();
    bool restart = s.object_motion != s.startup_object_motion ||
                   s.model_lod != s.startup_model_lod ||
                   s.live_resolution != s.startup_live_resolution ||
                   BbSettings::ResolutionNeedsRestart();
    for (int e = 0; e < BbSettings::EffectCount; ++e)
        restart |= s.effects[e] != s.startup_effects[e];
    return restart;
}

bool NativePcMenuAvailable() {
    const char* assets = std::getenv("BB_PC_MENU_ASSETS");
    const char* enabled = std::getenv("BB_PC_MENU");
    return assets && std::strcmp(assets, "1") == 0 &&
           (!enabled || std::strcmp(enabled, "0") != 0);
}

bool ExtrasRestartNeeded() {
    const auto& s = BbSettings::Get();
    for (int e = 10; e < 15; ++e) // Cheats: native PC menu has no room for this section.
        if (s.effects[e] != s.startup_effects[e]) return true;
    for (int e = BbSettings::FirstEnhancement; e < BbSettings::EffectCount; ++e)
        if (s.effects[e] != s.startup_effects[e]) return true;
    return false;
}

void RestartPrompt() {
    if (!RestartNeeded()) return;
    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f),
                       "Changes will apply after restarting the game");
    if (ImGui::Button("Apply and restart")) {
        BbSettings::Save();
        runtime_restart();
    }
}

void SettingsTab() {
    auto& s = BbSettings::Get();
    ImGui::PushFont(guide_serif_font, 39.0f);
    ImGui::TextUnformatted("Visual settings");
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::SeparatorText("Temporal upscaling");
    static const char* upscalers[] = {"Off", "FSR 3.1", "FSR 4 (INT8)", "FSR 4.1.1 (INT8)",
                                     "TAA (native anti-aliasing)", "DLSS (NVIDIA RTX)"};
    static const char* later[] = {"XeSS"};
    int upscaler = s.upscaler;
    if (ImGui::BeginCombo("Upscaler", upscalers[upscaler])) {
        for (int i = 0; i < BbSettings::UpscalerCount; ++i) {
            const bool supported = i == BbSettings::UpscalerFsr4 ? s.fsr4_supported.load()
                : i == BbSettings::UpscalerFsr411 ? s.fsr411_supported.load()
                : i == BbSettings::UpscalerDlss   ? s.dlss_supported.load() : true;
            ImGui::BeginDisabled(!supported);
            if (ImGui::Selectable(upscalers[i], i == upscaler)) {
                Store(s.upscaler, i, true);
            }
            ImGui::EndDisabled();
            if (!supported) {
                ImGui::SameLine();
                ImGui::TextDisabled("- unsupported by this GPU");
            }
        }
        for (const char* name : later) {
            ImGui::BeginDisabled();
            ImGui::Selectable(name, false);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("- in development");
        }
        ImGui::EndCombo();
    }
    if (const char* problem = s.dlss_problem.load(); problem && upscaler == BbSettings::UpscalerDlss) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "DLSS: %s", problem);
    }
    if (const char* problem = s.fsr4_problem.load()) {
        ImGui::PushTextWrapPos();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "FSR 4 unavailable: %s", problem);
        if (!BbSettings::IsFsr4(s.upscaler))
            ImGui::TextUnformatted("The selected mode is active. You can select FSR 4 again.");
        ImGui::PopTextWrapPos();
    }
    if (BbSettings::IsFsr4(s.upscaler)) {
        if (s.upscaler == BbSettings::UpscalerFsr411) {
            Hint("FSR 4.1.1 INT8: AMD 4.1.1 DLL model reproduced in Vulkan. One model covers Native through Performance, with another for Ultra Performance. Assets: tools/fsr4cap/build_assets.sh (requires the DLL and Proton).");
        } else {
            Hint("FSR 4 INT8 uses the v07 model from the AMD FidelityFX SDK. Higher quality than FSR 3.1, with more GPU work. Changing presets rebuilds the model and may pause briefly. Assets: tools/fetch_fsr4_assets.sh.");
        }
        Checkbox("FSR 4: auto exposure", s.fsr4_auto_exposure);
        Checkbox("FSR 4: invert jitter", s.fsr4_invert_jitter);
        Hint("Ghosting diagnostics: FSR 4 uses exposure to normalize color and decide when to discard history. These options apply immediately.");
    }
    const bool upscaler_on = s.upscaler != BbSettings::UpscalerOff;
    const bool taa = s.upscaler == BbSettings::UpscalerTaa;
    ImGui::BeginDisabled(!upscaler_on);
    ImGui::BeginDisabled(taa);
    int preset = taa ? BbSettings::NativeAA : s.preset.load();
    char preset_label[64];
    std::snprintf(preset_label, sizeof(preset_label), "%s (x%.1f)", BbSettings::PresetName(preset),
                  BbSettings::PresetScale(preset));
    if (ImGui::BeginCombo("Preset", preset_label)) {
        for (int i = 0; i < BbSettings::PresetCount; ++i) {
            char label[64];
            const float scale = BbSettings::PresetScale(i);
            const int output = s.output_res;
            std::snprintf(label, sizeof(label), "%s (x%.1f, render %dx%d)",
                          BbSettings::PresetName(i), scale,
                          int(std::lround(BbSettings::OutputWidths[output] / scale / 2) * 2),
                          int(std::lround(BbSettings::OutputHeights[output] / scale / 2) * 2));
            if (ImGui::Selectable(label, i == preset)) {
                Store(s.preset, i, true);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (taa) {
        ImGui::TextWrapped("TAA smooths the scene at output resolution without FSR upscaling. The saved upscaling preset returns when you select an upscaler.");
    }
    ImGui::Text("Active scene resolution: %d x %d", s.active_render_width.load(),
                s.active_render_height.load());
    if (BbSettings::AspectNeedsRestart()) {
        Hint("Changing aspect ratio requires a restart. HUD remains centered at 16:9.");
    } else if (BbSettings::FixedRenderSession()) {
        ImGui::Text("Startup preset: %s", BbSettings::PresetName(s.startup_preset));
        if (const char* automatic = std::getenv("BB_AUTO_RENDER_RES");
            automatic && automatic[0] == '1') {
            Hint("At outputs other than 1080p, the scene resolution is patched at startup. Changes to the preset or output require a restart. This saves GPU work on older GPUs. Non-16:9 outputs always use this path to keep camera and HUD geometry synchronized.");
        } else {
            Hint("BB_RENDER_RES fixes the scene resolution at startup. Remove this explicit override to allow live resolution and preset changes. Changes to aspect ratio still require a restart.");
        }
    } else {
        Hint("Native AA uses the upscaler for anti-aliasing. Other presets reduce scene resolution relative to the output. The HUD is drawn at output resolution. Preset changes apply on the next frame in a live session.");
    }
    Checkbox("Sharpening (RCAS)", s.sharpen);
    ImGui::BeginDisabled(!s.sharpen);
    Slider("Sharpness", s.sharpness, 0.0f, 2.0f);
    Hint("Up to 1 uses the upscaler sharpening pass (RCAS). Values above 1 add another RCAS pass. Ctrl+click the slider to enter an exact value.");
    ImGui::EndDisabled();
    Checkbox("Subpixel jitter", s.jitter);
    Hint("The camera shifts by a fraction of a pixel each frame so the upscaler can recover detail across multiple frames. Without jitter, it only smooths using frame history.");

    ImGui::SeparatorText("Reactive mask");
    ImGui::BeginDisabled(taa);
    Checkbox("Enable mask", s.reactive);
    Hint("Marks transparent effects such as particles and fog so the upscaler relies less on history. Reduces trails, but can increase shimmer behind those effects.");
    ImGui::BeginDisabled(!s.reactive);
    Slider("Scale", s.reactive_scale, 0.0f, 4.0f);
    Slider("Threshold", s.reactive_threshold, 0.0f, 1.0f);
    Slider("Maximum", s.reactive_max, 0.0f, 1.0f);
    bool show_mask = s.debug_view == BbSettings::DebugReactive;
    if (ImGui::Checkbox("Show mask (debug)", &show_mask)) {
        s.debug_view = show_mask ? BbSettings::DebugReactive : BbSettings::DebugNone;
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    Checkbox("Object motion vectors", s.object_motion);
    Hint("Accurate vectors for animated objects reduce breakup on clothing and weapons during movement. Static geometry receives no extra pass. Changes require a restart.");
    bool show_motion = s.debug_view == BbSettings::DebugMotion;
    if (ImGui::Checkbox("Show motion vectors (debug)", &show_motion)) {
        s.debug_view = show_motion ? BbSettings::DebugMotion : BbSettings::DebugNone;
    }
    Hint("Red/green: horizontal/vertical motion (8 pixels = full brightness). Blue: the pixel has accurate object motion, beyond camera motion. A moving object with no blue or red/green is treated as stationary and may leave trails.");
    ImGui::EndDisabled(); // upscaler off

    ImGui::SeparatorText("Output resolution");
    auto output_label = [](int i) {
        return std::to_string(BbSettings::OutputWidths[i]) + " x " +
               std::to_string(BbSettings::OutputHeights[i]);
    };
    int output = s.output_res;
    if (ImGui::BeginCombo("Output resolution", output_label(output).c_str())) {
        for (int i = 0; i < BbSettings::OutputCount; ++i) {
            if (ImGui::Selectable(output_label(i).c_str(), i == output)) {
                Store(s.output_res, i, true);
            }
        }
        ImGui::EndCombo();
    }
    if (BbSettings::AspectNeedsRestart()) {
        Hint("Changing aspect ratio requires a restart. HUD remains centered at 16:9.");
    } else if (BbSettings::FixedRenderSession()) {
        Hint("Final image and HUD resolution. The preset controls scene resolution relative to the output. 4K Performance is approximately 1920x1080. Changes require a restart.");
    } else {
        Hint("Output and HUD resolution changes at the next frame boundary. The preset controls scene resolution relative to the output. Resizing resets upscaler history and may pause briefly.");
    }
    static const char* live_modes[] = {"Auto (based on GPU)", "Off (faster)", "On"};
    int live = s.live_resolution + 1;
    if (ImGui::BeginCombo("Live resolution changes", live_modes[live])) {
        for (int i = 0; i < 3; ++i) {
            if (ImGui::Selectable(live_modes[i], i == live)) {
                Store(s.live_resolution, i - 1, true);
            }
        }
        ImGui::EndCombo();
    }
    Hint("On: output and presets change without restarting, while game post-processing stays at 1080p. Off: the scene is patched to the preset resolution at startup. Auto enables the live path on capable discrete GPUs. Changes apply after restarting the game.");
    ImGui::SeparatorText("Game effects (restart required)");
    static const char* lods[] = {"Highest (-2)", "Original", "Lower (1)", "Lowest (2)"};
    static constexpr int lod_values[] = {-2, 0, 1, 2};
    int lod_index = 1;
    for (int i = 0; i < 4; ++i) {
        if (lod_values[i] == s.model_lod) lod_index = i;
    }
    if (ImGui::BeginCombo("Model detail", lods[lod_index])) {
        for (int i = 0; i < 4; ++i) {
            if (ImGui::Selectable(lods[i], i == lod_index)) {
                Store(s.model_lod, lod_values[i], true);
            }
        }
        ImGui::EndCombo();
    }
    for (int e = 0; e < 8; ++e) {
        Checkbox(BbSettings::Effects[e].label, s.effects[e]);
    }
    Hint("Effects are applied through game patches at startup (patches/Bloodborne.xml). Motion blur and dynamic shadows can increase GPU load.");

    RestartPrompt();

    ImGui::SeparatorText("Other");
    Checkbox("Show FPS counter", s.show_fps);
}

void ImageToolsTab() {
    auto& s = BbSettings::Get();
    ImGui::PushFont(guide_serif_font, 39.0f);
    ImGui::TextUnformatted("Image tools");
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::TextWrapped("Graphics, display, effects and the FPS counter are in Pause > System. "
                       "These tools provide diagnostics beyond the game menu.");
    ImGui::SeparatorText("Current image");
    ImGui::Text("Scene resolution: %d x %d", s.active_render_width.load(),
                s.active_render_height.load());
    if (BbSettings::FixedRenderSession())
        ImGui::Text("Startup preset: %s (changes need a restart)",
                    BbSettings::PresetName(s.startup_preset));
    if (const char* problem = s.dlss_problem.load();
        problem && s.upscaler == BbSettings::UpscalerDlss)
        ImGui::TextWrapped("DLSS: %s", problem);
    if (const char* problem = s.fsr4_problem.load())
        ImGui::TextWrapped("FSR 4 unavailable: %s", problem);

    ImGui::SeparatorText("Fine tuning");
    ImGui::BeginDisabled(!s.sharpen);
    Slider("Sharpness", s.sharpness, 0.0f, 2.0f);
    ImGui::EndDisabled();
    Hint("Enable Sharpening in System > PC Graphics first. Values above 1 add another RCAS pass.");
    if (BbSettings::IsFsr4(s.upscaler)) {
        Checkbox("FSR 4: auto exposure", s.fsr4_auto_exposure);
        Checkbox("FSR 4: invert jitter", s.fsr4_invert_jitter);
        Hint("Provider checks for FSR 4. These changes apply immediately.");
    }
    const bool temporal = s.upscaler != BbSettings::UpscalerOff &&
                          s.upscaler != BbSettings::UpscalerTaa;
    ImGui::BeginDisabled(!temporal);
    Checkbox("Reactive mask", s.reactive);
    ImGui::BeginDisabled(!s.reactive);
    Slider("Mask scale", s.reactive_scale, 0.0f, 4.0f);
    Slider("Mask threshold", s.reactive_threshold, 0.0f, 1.0f);
    Slider("Mask maximum", s.reactive_max, 0.0f, 1.0f);
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    Hint("Marks particles and fog so the upscaler relies less on frame history.");

    ImGui::SeparatorText("Debug views");
    int view = s.debug_view;
    if (ImGui::RadioButton("Normal image", view == BbSettings::DebugNone))
        s.debug_view = BbSettings::DebugNone;
    if (ImGui::RadioButton("Reactive mask view", view == BbSettings::DebugReactive))
        s.debug_view = BbSettings::DebugReactive;
    if (ImGui::RadioButton("Motion vector view", view == BbSettings::DebugMotion))
        s.debug_view = BbSettings::DebugMotion;
    Hint("Debug views are temporary diagnostics; return to Normal image to play.");
}

std::filesystem::path ModConfigPath() {
    if (const char* value = std::getenv("BB_MODS_CONFIG"); value && *value) return value;
    return std::filesystem::current_path() / "mods.json";
}

std::optional<std::string> XboxPromptFolder() {
    const char* configured = std::getenv("BB_MODS_DIR");
    const auto root = configured && *configured ? std::filesystem::path(configured) :
                      std::filesystem::current_path() / "mods";
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) return std::nullopt;
    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (!entry.is_directory(ec)) continue;
        auto name = entry.path().filename().string();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (name.find("xbox") != std::string::npos && name.find("prompt") != std::string::npos)
            return entry.path().filename().string();
    }
    return std::nullopt;
}

nlohmann::json ModProfile() {
    std::ifstream file(ModConfigPath());
    if (!file) return nlohmann::json::object();
    try {
        auto profile = nlohmann::json::parse(file);
        return profile.is_object() ? profile : nlohmann::json::object();
    } catch (const nlohmann::json::exception&) {
        return nlohmann::json::object();
    }
}

bool PromptEnabled(const std::string& folder) {
    const auto profile = ModProfile();
    const auto it = profile.find("disabled");
    if (it == profile.end() || !it->is_array()) return true;
    return std::find(it->begin(), it->end(), folder) == it->end();
}

bool SetPromptEnabled(const std::string& folder, bool enabled) {
    nlohmann::json profile = nlohmann::json::object();
    std::ifstream existing(ModConfigPath());
    if (existing) {
        try {
            profile = nlohmann::json::parse(existing);
            if (!profile.is_object()) return false;
        } catch (const nlohmann::json::exception&) { return false; }
    }
    auto& disabled = profile["disabled"];
    if (!disabled.is_array()) disabled = nlohmann::json::array();
    nlohmann::json next = nlohmann::json::array();
    for (const auto& name : disabled)
        if (!name.is_string() || name.get<std::string>() != folder) next.push_back(name);
    if (!enabled) next.push_back(folder);
    profile["disabled"] = std::move(next);
    std::ofstream file(ModConfigPath(), std::ios::trunc);
    if (!file) return false;
    file << profile.dump(2) << '\n';
    return bool(file);
}

void ModsTab() {
    auto& s = BbSettings::Get();
    const bool native = NativePcMenuAvailable();
    ImGui::PushFont(guide_serif_font, 39.0f);
    ImGui::TextUnformatted(native ? "Gameplay extras" : "Mods & patches");
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::TextWrapped(native ?
        "Gameplay tweaks and PC settings are in Pause > System. Cheats and enhancements remain here because the game menu has no room for them." :
        "Select button prompts and gameplay patches, then restart to apply them.");
    if (const char* enabled = std::getenv("BB_MODS_ENABLED"); enabled && std::strcmp(enabled,"0") == 0)
        ImGui::TextColored(ImVec4(.95f,.73f,.40f,1), "Mod loading is off in the launcher. Enable it there first.");
    // 0 the game's PlayStation buttons, 1 the Xbox prompt mod, 2 the keyboard key images the
    // launcher imports (PCBButtons; run.py reads the choice again at every start).
    ImGui::SeparatorText("Button prompts");
    static const auto xbox_folder = XboxPromptFolder();
    static const std::optional<std::string> keyboard_folder = [] () -> std::optional<std::string> {
        const char* configured = std::getenv("BB_MODS_DIR");
        const auto root = configured && *configured ? std::filesystem::path(configured) :
                          std::filesystem::current_path() / "mods";
        std::error_code ec;
        const std::string name = "Keyboard Prompts (PCBButtons)";
        if (std::filesystem::is_directory(root / name, ec)) return name;
        return std::nullopt;
    }();
    static int prompt_choice = -1;
    if (prompt_choice < 0)
        prompt_choice = keyboard_folder && PromptEnabled(*keyboard_folder) ? 2
                      : xbox_folder && PromptEnabled(*xbox_folder) ? 1 : 0;
    int choice = prompt_choice;
    if (ImGui::RadioButton("Original PlayStation buttons", choice == 0)) choice = 0;
    ImGui::BeginDisabled(!xbox_folder);
    if (ImGui::RadioButton("Xbox buttons", choice == 1)) choice = 1;
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!keyboard_folder);
    if (ImGui::RadioButton("Keyboard keys", choice == 2)) choice = 2;
    ImGui::EndDisabled();
    if (choice != prompt_choice) {
        const bool saved = (!xbox_folder || SetPromptEnabled(*xbox_folder, choice == 1)) &&
                           (!keyboard_folder || SetPromptEnabled(*keyboard_folder, choice == 2));
        if (saved) {
            prompt_choice = choice;
            mod_selection_changed = true;
            mod_selection_error.clear();
        } else mod_selection_error = "Could not save mods.json. Check the port folder permissions.";
    }
    if (!xbox_folder)
        ImGui::TextDisabled("Install an Xbox prompt mod in the launcher's Mods folder to enable Xbox buttons.");
    if (!keyboard_folder)
        ImGui::TextDisabled("Keyboard keys: choose Keyboard once on the launcher's Controller page to import them.");
    if (!mod_selection_error.empty())
        ImGui::TextColored(ImVec4(.95f,.35f,.35f,1), "%s", mod_selection_error.c_str());
    if (!native) ImGui::SeparatorText("Gameplay and debug");
    else ImGui::SeparatorText("Cheats (restart required)");
    for (int e = native ? 10 : 8;
         e < (native ? 15 : BbSettings::FirstEnhancement); ++e)
        Checkbox(BbSettings::Effects[e].label, s.effects[e]);
    if (!native)
        Hint("Free camera: hold Cross/A and press L3 (keyboard: E + Left Ctrl). The debug menu needs DbgFont14h.ccm and DbgFont14h.tpf in dvdroot_ps4/font.");
    ImGui::SeparatorText("PC enhancements (restart required)");
    for (int e = BbSettings::FirstEnhancement; e < BbSettings::EffectCount; ++e)
        Checkbox(BbSettings::Effects[e].label, s.effects[e]);
    Hint("Made from your own game files at start; off is the game as it shipped. The mirror in the Hunter's Dream opens the appearance editor. Two invaders: Mensis, Mergo's Loft, the Nightmare Frontier and the Old Hunters' areas allow two, as the chalice dungeons do. After bbhost by droogie.");
    ImGui::Separator();
    const bool needs_restart = mod_selection_changed ||
        (native ? ExtrasRestartNeeded() : RestartNeeded());
    if (needs_restart)
        ImGui::TextColored(ImVec4(.95f,.73f,.40f,1), "Changes are ready. Restart to apply them.");
    if (needs_restart && ImGui::Button("Apply extras and restart", ImVec2(-1, 0))) {
        BbSettings::Save();
        runtime_restart();
    }
    ImGui::TextDisabled("Other mod choices and load order are managed in the launcher.");
}

std::string UnlockDate(std::int64_t timestamp) {
    if (timestamp <= 0) return {};
    const std::time_t when = static_cast<std::time_t>(timestamp);
    const std::tm* local = std::localtime(&when);
    if (!local) return {};
    char date[48]{};
    std::strftime(date, sizeof(date), "%d %b %Y", local);
    return date;
}

void AchievementRow(const BbTrophies::Entry& entry) {
    const bool gallery = NativePcMenuAvailable();
    const float h = (gallery ? 72.0f : 98.0f) * base_scale;
    const bool earned = entry.unlocked_at > 0;
    ImGui::PushID(entry.id);
    ImGui::Selectable("##achievement", false, 0, ImVec2(0, h));
    const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float icon = (gallery ? 46.0f : 52.0f) * base_scale;
    const ImVec2 icon_min(min.x + 9.0f * base_scale, min.y + (h - icon) * .5f);
    const ImVec2 icon_max(icon_min.x + icon, icon_min.y + icon);
    const auto texture = IconRef(entry.id);
    const ImU32 tint = earned ? IM_COL32_WHITE : IM_COL32(105, 112, 110, 255);
    if (texture._TexData || texture._TexID != ImTextureID_Invalid)
        draw->AddImageRounded(texture, icon_min, icon_max, ImVec2(0, 0), ImVec2(1, 1),
                              tint, 8.0f * base_scale);
    else
        draw->AddRectFilled(icon_min, icon_max, IM_COL32(40, 48, 46, 255), 8.0f * base_scale);
    const ImVec2 center(icon_min.x + icon * .5f, icon_min.y + icon * .5f);
    draw->AddCircle(center, icon * .51f, GradeColor(entry.grade, earned ? 1.0f : .45f),
                    0, 2.0f * base_scale);
    const float text_x = icon_max.x + 14.0f * base_scale;
    const float right_x = max.x - 64.0f * base_scale;
    const bool secret = entry.hidden && !earned;
    const std::string name = secret ? "Secret trophy" : entry.name;
    const std::string detail = secret ? "Unlock to reveal this achievement" : entry.description;
    const std::string score = secret || entry.score < 0 ? "??G" :
                              std::to_string(entry.score) + "G";
    const ImVec4 name_clip(text_x, min.y, right_x - 7.0f * base_scale, max.y);
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                  ImVec2(text_x, min.y + 10.0f * base_scale),
                  earned ? IM_COL32(240, 247, 241, 255) : IM_COL32(158, 165, 161, 255),
                  name.c_str(), nullptr, 0.0f, &name_clip);
    draw->AddText(ImVec2(right_x, min.y + 10.0f * base_scale),
                  earned ? IM_COL32(126, 224, 135, 255) : IM_COL32(133, 141, 136, 255),
                  gallery ? (earned ? "Earned" : "Locked") : score.c_str());
    if (!gallery && !detail.empty())
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * .83f,
                      ImVec2(text_x, min.y + 34.0f * base_scale),
                      earned ? IM_COL32(180, 192, 183, 255) : IM_COL32(112, 122, 115, 255),
                      detail.c_str(), nullptr, right_x - text_x - 7.0f * base_scale);
    if (!gallery && earned) {
        const auto date = UnlockDate(entry.unlocked_at);
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * .78f,
                      ImVec2(text_x, min.y + 76.0f * base_scale),
                      IM_COL32(137, 209, 145, 255), date.c_str());
    }
    draw->AddLine(ImVec2(min.x, max.y), ImVec2(max.x, max.y),
                  IM_COL32(80, 93, 83, 130));
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(name.c_str());
        if (!detail.empty()) ImGui::TextWrapped("%s", detail.c_str());
        ImGui::EndTooltip();
    }
    ImGui::PopID();
}

void AchievementsTab() {
    const bool gallery = NativePcMenuAvailable();
    auto entries = BbTrophies::Entries();
    const int total = int(entries.size());
    int earned = 0;
    for (const auto& entry : entries) earned += entry.unlocked_at > 0;
    const int score = BbTrophies::EarnedScore();
    ImGui::PushFont(guide_serif_font, 39.0f);
    ImGui::TextUnformatted(gallery ? "Trophy gallery" : "Trophies");
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();
    if (gallery)
        ImGui::TextWrapped("Browse trophy artwork here. Full details, dates and Gamerscore are in Pause > System > Achievements.");
    else {
        if (BbTrophies::TotalScore() == 1000)
            ImGui::Text("Gamerscore: %d / 1000G", score);
        else
            ImGui::Text("Gamerscore: -- / 1000G");
        ImGui::Text("%d of %d unlocked", earned, total);
        ImGui::ProgressBar(total ? float(earned) / total : 0.0f,
                           ImVec2(-1, 9.0f * base_scale), "");
    }
    ImGui::BeginDisabled(entries.empty());
    if (ImGui::Button("Preview achievement toast", ImVec2(-1, 0))) {
        const auto sample = std::find_if(entries.begin(), entries.end(), [](const auto& entry) {
            return entry.grade == BbTrophies::Grade::Bronze && !entry.hidden;
        });
        const int id = sample == entries.end() ? entries.front().id : sample->id;
        std::scoped_lock lock{trophy_mutex};
        toast_queue.push_back({id, true});
        ++queued_toasts;
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("Preview only: this does not unlock or save a trophy.");
    if (!BbTrophies::HasMetadata())
        ImGui::TextWrapped("Names and scores need the trophy key from your own PS4. Icons and local unlock history still work.");
    ImGui::Separator();
    if (entries.empty()) {
        ImGui::TextDisabled("No trophy pack found in the selected game folder.");
        return;
    }
    ImGuiListClipper clipper;
    clipper.Begin(total, (gallery ? 72.0f : 98.0f) * base_scale + ImGui::GetStyle().ItemSpacing.y);
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            AchievementRow(entries[i]);
    if (!gallery) ImGui::TextDisabled("Local achievements only. No online account is connected.");
}

void HomeTab() {
    const bool native = NativePcMenuAvailable();
    const auto entries = BbTrophies::Entries();
    const int earned = int(std::count_if(entries.begin(), entries.end(),
                                         [](const auto& e) { return e.unlocked_at > 0; }));
    const float s = base_scale;
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::PushFont(guide_serif_font, 48.0f);
    ImGui::TextUnformatted("The Hunt");
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 8.0f * s));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 9.0f * s));
    ImGui::PushFont(guide_serif_font, 23.0f);
    ImGui::Text("%.0f FPS  ·  %.1f ms", frame_ms_avg > 0.0f ? 1000.0f / frame_ms_avg : 0.0f,
                frame_ms_avg);
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 25.0f * s));

    const ImVec2 trophy_pos = ImGui::GetCursorScreenPos();
    TrophyGlyph(ImGui::GetWindowDrawList(),
                ImVec2(trophy_pos.x + 19.0f * s, trophy_pos.y + 20.0f * s),
                43.0f * s, .82f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 68.0f * s);
    ImGui::PushFont(guide_serif_font, 22.0f);
    ImGui::Text("%d of %d trophies", earned, int(entries.size()));
    ImGui::PopFont();
    const char* local = "Local only";
    ImGui::SameLine(width - ImGui::CalcTextSize(local).x);
    ImGui::TextDisabled("%s", local);
    const ImVec2 bar = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float bar_x = bar.x + 68.0f * s;
    const float bar_width = std::max(80.0f * s, width - 190.0f * s);
    draw->AddRectFilled(ImVec2(bar_x, bar.y), ImVec2(bar_x + bar_width, bar.y + 10.0f * s),
                        IM_COL32(43, 49, 55, 255), 5.0f * s);
    if (!entries.empty())
        draw->AddRectFilled(ImVec2(bar_x, bar.y),
                            ImVec2(bar_x + bar_width * float(earned) / entries.size(), bar.y + 10.0f * s),
                            IM_COL32(194, 49, 65, 255), 5.0f * s);
    ImGui::Dummy(ImVec2(0, 42.0f * s));

    const char* labels[] = {
        native ? "Trophy gallery" : "View trophies",
        native ? "Image tools" : "Visual settings",
        native ? "Gameplay extras" : "Mods & patches",
    };
    for (int i = 0; i < 3; ++i) {
        ImGui::PushID(i);
        const ImVec2 min = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##guide_action", ImVec2(width, 70.0f * s),
                                   ImGuiButtonFlags_EnableNav)) active_tab = i + 1;
        const bool highlight = ImGui::IsItemHovered() || ImGui::IsItemFocused();
        const ImVec2 max(min.x + width, min.y + 70.0f * s);
        draw->AddRectFilled(min, max, IM_COL32(20, 24, 28, 245), 8.0f * s);
        draw->AddRect(min, max, highlight || i == 0 ? IM_COL32(193, 52, 67, 255) :
                      IM_COL32(53, 60, 66, 255), 8.0f * s, 0, highlight || i == 0 ? 2.0f * s : 1.0f * s);
        const ImVec2 icon_center(min.x + 39.0f * s, min.y + 35.0f * s);
        const ImU32 ink = IM_COL32(222, 221, 221, 255);
        if (i == 0) TrophyGlyph(draw, icon_center, 35.0f * s, .84f);
        else if (i == 1) {
            const float r = 16.0f * s;
            draw->AddCircle(icon_center, r * .52f, ink, 0, 2.0f * s);
            for (int spoke = 0; spoke < 8; ++spoke) {
                const float a = float(spoke) * 3.14159265f / 4.0f;
                draw->AddLine(ImVec2(icon_center.x + std::cos(a) * r * .7f,
                                     icon_center.y + std::sin(a) * r * .7f),
                              ImVec2(icon_center.x + std::cos(a) * r,
                                     icon_center.y + std::sin(a) * r), ink, 3.0f * s);
            }
        } else {
            const float r = 15.0f * s;
            draw->AddRect(ImVec2(icon_center.x-r*.7f, icon_center.y-r*.7f),
                          ImVec2(icon_center.x+r*.7f, icon_center.y+r*.7f),
                          ink, 3.0f * s, 0, 2.0f * s);
            draw->AddCircle(ImVec2(icon_center.x, icon_center.y-r*.7f),
                            r*.25f, ink, 0, 2.0f * s);
            draw->AddCircle(ImVec2(icon_center.x+r*.7f, icon_center.y),
                            r*.25f, ink, 0, 2.0f * s);
        }
        draw->AddText(guide_serif_font ? guide_serif_font : ImGui::GetFont(), 23.0f * s,
                      ImVec2(min.x + 85.0f * s, min.y + 19.0f * s),
                      IM_COL32(238, 235, 234, 255), labels[i]);
        draw->AddText(ImVec2(max.x - 36.0f * s, min.y + 23.0f * s), ink, ">");
        ImGui::Dummy(ImVec2(0, 15.0f * s));
        ImGui::PopID();
    }
    if (native) {
        ImGui::Spacing();
        ImGui::TextDisabled("For standard options, pause the game and open System.");
    }
}

void GuideNavGlyph(ImDrawList* draw, int tab, ImVec2 center, float size, ImU32 ink) {
    const float x = center.x, y = center.y, r = size * .5f;
    if (tab == 0) {
        draw->AddLine(ImVec2(x, y-r), ImVec2(x, y+r), ink, 2.0f * base_scale);
        draw->AddLine(ImVec2(x-r*.58f, y), ImVec2(x+r*.58f, y), ink, 2.0f * base_scale);
        draw->AddLine(ImVec2(x, y-r*.74f), ImVec2(x+r*.45f, y), ink, 1.7f * base_scale);
        draw->AddLine(ImVec2(x+r*.45f, y), ImVec2(x, y+r*.74f), ink, 1.7f * base_scale);
        draw->AddLine(ImVec2(x, y+r*.74f), ImVec2(x-r*.45f, y), ink, 1.7f * base_scale);
        draw->AddLine(ImVec2(x-r*.45f, y), ImVec2(x, y-r*.74f), ink, 1.7f * base_scale);
    } else if (tab == 1) {
        TrophyGlyph(draw, center, size, .73f);
    } else if (tab == 2) {
        draw->AddCircle(center, r*.58f, ink, 0, 2.7f * base_scale);
        draw->AddCircle(center, r*.18f, ink, 0, 2.0f * base_scale);
        for (int spoke = 0; spoke < 8; ++spoke) {
            const float a = float(spoke) * 3.14159265f / 4.0f;
            draw->AddLine(ImVec2(x + std::cos(a)*r*.65f, y + std::sin(a)*r*.65f),
                          ImVec2(x + std::cos(a)*r, y + std::sin(a)*r), ink, 3.2f * base_scale);
        }
    } else {
        draw->AddRect(ImVec2(x-r*.72f, y-r*.72f), ImVec2(x+r*.72f, y+r*.72f),
                      ink, 3.0f * base_scale, 0, 2.1f * base_scale);
        draw->AddCircle(ImVec2(x, y-r*.72f), r*.23f, ink, 0, 2.0f * base_scale);
        draw->AddCircle(ImVec2(x+r*.72f, y), r*.23f, ink, 0, 2.0f * base_scale);
    }
}

void Menu() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float s = base_scale;
    const float appear = std::clamp(float(ClockMs() - menu_opened_ms.load()) / 180.0f,
                                    0.0f, 1.0f);
    ImGui::GetBackgroundDrawList()->AddRectFilled(viewport->Pos,
        ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
        IM_COL32(0, 0, 0, int(38 * appear)));
    const float panel_width = std::min(viewport->WorkSize.x,
        std::clamp(viewport->WorkSize.x * .425f, 500.0f * s, 840.0f * s));
    const float rail_width = 112.0f * s;
    const float pad = 34.0f * s;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x - (1.0f - appear) * 36.0f * s,
                                   viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(panel_width, viewport->WorkSize.y));
    ImGui::SetNextWindowBgAlpha(.99f * appear);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, .6f + .4f * appear);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(.035f, .050f, .060f, 1));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.035f, .050f, .060f, 1));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(.18f, .14f, .16f, 1));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(.24f, .13f, .16f, 1));
    ImGui::PushStyleColor(ImGuiCol_NavHighlight, ImVec4(.79f, .23f, .30f, 1));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.10f, .13f, .15f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.20f, .15f, .17f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.28f, .13f, .17f, 1));
    ImGui::Begin("##bloodborne_guide", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 panel_pos = ImGui::GetWindowPos();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(panel_pos.x + panel_width - 1.0f, panel_pos.y),
        ImVec2(panel_pos.x + panel_width - 1.0f, panel_pos.y + viewport->WorkSize.y),
        IM_COL32(84, 91, 96, 200));

    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::BeginChild("##guide_rail", ImVec2(rail_width, 0), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 rail_pos = ImGui::GetWindowPos();
    ImDrawList* rail_draw = ImGui::GetWindowDrawList();
    rail_draw->AddRectFilled(rail_pos, ImVec2(rail_pos.x + rail_width,
                              rail_pos.y + ImGui::GetWindowHeight()), IM_COL32(11, 16, 20, 255));
    rail_draw->AddLine(ImVec2(rail_pos.x + rail_width - 1, rail_pos.y),
                       ImVec2(rail_pos.x + rail_width - 1,
                              rail_pos.y + ImGui::GetWindowHeight()), IM_COL32(48, 54, 59, 255));
    const bool native = NativePcMenuAvailable();
    const char* nav_ids[] = {"Hunt##nav", "Trophies##nav",
                             native ? "Image tools##nav" : "Visual settings##nav",
                             native ? "Gameplay extras##nav" : "Mods and patches##nav"};
    for (int i = 0; i < 4; ++i) {
        ImGui::SetCursorPos(ImVec2(16.0f * s, (50.0f + 104.0f * i) * s));
        if (ImGui::InvisibleButton(nav_ids[i], ImVec2(rail_width - 32.0f * s, 96.0f * s),
                                   ImGuiButtonFlags_EnableNav))
            active_tab = i;
        const bool selected = active_tab == i;
        const bool hovered = ImGui::IsItemHovered() || ImGui::IsItemFocused();
        const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
        if (selected || hovered)
            rail_draw->AddRectFilled(ImVec2(rail_pos.x + 16.0f * s, min.y),
                                     ImVec2(rail_pos.x + rail_width - 16.0f * s, max.y),
                                     selected ? IM_COL32(38, 24, 28, 255) : IM_COL32(28, 34, 39, 255));
        if (selected)
            rail_draw->AddRectFilled(ImVec2(rail_pos.x, min.y),
                                     ImVec2(rail_pos.x + 4.0f * s, max.y),
                                     IM_COL32(207, 52, 68, 255));
        GuideNavGlyph(rail_draw, i, ImVec2((min.x + max.x) * .5f, (min.y + max.y) * .5f),
                      38.0f * s, selected ? IM_COL32(242, 236, 231, 255) :
                                             IM_COL32(160, 165, 168, 255));
        rail_draw->AddLine(ImVec2(min.x + 18.0f * s, max.y + 2.0f * s),
                           ImVec2(max.x - 18.0f * s, max.y + 2.0f * s), IM_COL32(55, 60, 64, 180));
    }
    ImGui::EndChild();
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginChild("##guide_main", ImVec2(0, 0), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 main_pos = ImGui::GetWindowPos();
    const ImVec2 main_size = ImGui::GetWindowSize();
    ImDrawList* main_draw = ImGui::GetWindowDrawList();
    ImGui::SetCursorPos(ImVec2(pad, 47.0f * s));
    ImGui::PushFont(guide_serif_font, 20.0f);
    ImGui::TextUnformatted("BLOODBORNE");
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("/   HUNTER'S GUIDE");
    ImGui::SetCursorPos(ImVec2(main_size.x - pad - 92.0f * s, 43.0f * s));
    if (ImGui::InvisibleButton("Close guide", ImVec2(92.0f * s, 40.0f * s),
                               ImGuiButtonFlags_EnableNav)) SetOpen(false);
    const ImU32 header_ink = IM_COL32(235, 231, 226, 255);
    main_draw->AddText(ImVec2(main_pos.x + main_size.x - pad - 92.0f * s,
                              main_pos.y + 51.0f * s), header_ink, "Close");
    const ImVec2 close_center(main_pos.x + main_size.x - pad - 16.0f * s,
                               main_pos.y + 63.0f * s);
    main_draw->AddCircle(close_center, 14.0f * s, header_ink, 0, 1.4f * s);
    for (int i = -1; i <= 1; ++i)
        main_draw->AddLine(ImVec2(close_center.x - 6.0f * s, close_center.y + i * 4.0f * s),
                           ImVec2(close_center.x + 6.0f * s, close_center.y + i * 4.0f * s),
                           header_ink, 1.2f * s);
    main_draw->AddLine(ImVec2(main_pos.x + pad, main_pos.y + 112.0f * s),
                       ImVec2(main_pos.x + main_size.x - pad, main_pos.y + 112.0f * s),
                       IM_COL32(62, 68, 72, 255));

    ImGui::SetCursorPos(ImVec2(pad, 142.0f * s));
    ImGui::BeginChild("##guide_content", ImVec2(main_size.x - 2.0f * pad,
                      std::max(100.0f * s, main_size.y - 260.0f * s)), false);
    switch (active_tab) {
    case 0: HomeTab(); break;
    case 1: AchievementsTab(); break;
    case 2: if (native) ImageToolsTab(); else SettingsTab(); break;
    case 3: ModsTab(); break;
    }
    ImGui::EndChild();
    main_draw->AddLine(ImVec2(main_pos.x + pad, main_pos.y + main_size.y - 112.0f * s),
                       ImVec2(main_pos.x + main_size.x - pad, main_pos.y + main_size.y - 112.0f * s),
                       IM_COL32(62, 68, 72, 255));
    main_draw->AddText(ImVec2(main_pos.x + pad, main_pos.y + main_size.y - 82.0f * s),
                       IM_COL32(170, 174, 178, 255),
                       "D-pad  Move   ·   A  Select   ·   B  Back   ·   LB/RB  Sections");
    main_draw->AddText(ImVec2(main_pos.x + pad, main_pos.y + main_size.y - 48.0f * s),
                       IM_COL32(170, 174, 178, 255), "Insert or L3+R3 closes guide");
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleColor(8);
    ImGui::PopStyleVar(4);
}

// The game's text dialog: what is typed, and how to finish (keyboard or controller).
void TextEntryBox() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.42f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(viewport->WorkSize.x * 0.32f, 0.0f),
                                        ImVec2(viewport->WorkSize.x * 0.8f, FLT_MAX));
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::Begin("##text_entry", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::TextColored(ImVec4(0.85f, 0.72f, 0.45f, 1.0f), "%s", text_entry_prompt.c_str());
    ImGui::Separator();
    ImGui::SetWindowFontScale(1.4f);
    ImGui::Text("%s_", text_entry_text.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::TextDisabled("Keyboard: type, Backspace to erase, Enter = OK, Esc = cancel");
    ImGui::TextDisabled("Controller: Cross (A) = OK, Circle (B) = cancel");
    ImGui::End();
}

void FpsCounter() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float pad = 12.0f * base_scale;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - pad,
                                   viewport->WorkPos.y + pad),
                            ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.5f);
    ImGui::Begin("##fps", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing);
    const auto& s = BbSettings::Get();
    ImGui::Text("%.0f FPS  %.1f ms  %s", frame_ms_avg > 0.0f ? 1000.0f / frame_ms_avg : 0.0f,
                frame_ms_avg,
                s.upscaler == BbSettings::UpscalerFsr3   ? "FSR 3.1"
                : s.upscaler == BbSettings::UpscalerFsr4 ? "FSR 4"
                : s.upscaler == BbSettings::UpscalerFsr411 ? "FSR 4.1.1"
                : s.upscaler == BbSettings::UpscalerTaa ? "TAA"
                : s.upscaler == BbSettings::UpscalerDlss ? "DLSS"
                                                         : "");
    ImGui::End();
}

void TrophyToast() {
    const int64_t now = ClockMs();
    if (active_toast.load() < 0) {
        std::scoped_lock lock{trophy_mutex};
        if (toast_queue.empty()) return;
        active_toast = toast_queue.front().id;
        active_toast_preview = toast_queue.front().preview;
        toast_queue.pop_front();
        --queued_toasts;
        toast_start_ms = now;
    }
    constexpr int64_t kFadeInMs = 320, kHoldMs = 5000, kFadeOutMs = 420;
    int64_t elapsed = now - toast_start_ms;
    if (elapsed >= kFadeInMs + kHoldMs + kFadeOutMs) {
        active_toast = -1;
        if (queued_toasts.load() > 0) TrophyToast();
        return;
    }
    const float appear = std::clamp(float(elapsed) / kFadeInMs, 0.0f, 1.0f);
    const float disappear = std::clamp(float(kFadeInMs + kHoldMs + kFadeOutMs - elapsed) /
                                            kFadeOutMs, 0.0f, 1.0f);
    const float alpha = std::min(appear, disappear);
    const auto entry = BbTrophies::Find(active_toast.load());
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float w = std::min(590.0f * base_scale, viewport->WorkSize.x - 24.0f * base_scale);
    const float h = 92.0f * base_scale;
    const float slide = (1.0f - appear) * 28.0f * base_scale;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * .5f - w * .5f,
                                   viewport->WorkPos.y + viewport->WorkSize.y - h -
                                       30.0f * base_scale + slide));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::SetNextWindowBgAlpha(.96f * alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, h * .5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(.055f, .48f, .065f, 1));
    ImGui::Begin("##trophy_toast", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetWindowPos();
    const float icon = 72.0f * base_scale;
    const ImVec2 center(pos.x + 49.0f * base_scale, pos.y + h * .5f);
    draw->AddCircleFilled(center, icon * .5f, IM_COL32(64, 170, 37, int(255 * alpha)));
    draw->AddCircle(center, icon * .49f, GradeColor(entry.grade, alpha), 0,
                    2.0f * base_scale);
    TrophyGlyph(draw, center, 45.0f * base_scale, alpha);
    const float text_x = pos.x + 103.0f * base_scale;
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.03f,
                  ImVec2(text_x, pos.y + 19.0f * base_scale),
                  IM_COL32(255, 255, 255, int(255 * alpha)),
                  active_toast_preview.load() ? "Achievement preview" : "Achievement unlocked");
    const std::string title = entry.score >= 0 ?
        std::to_string(entry.score) + "G - " + entry.name : entry.name;
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.12f,
                  ImVec2(text_x, pos.y + 49.0f * base_scale),
                  IM_COL32(255, 255, 255, int(255 * alpha)), title.c_str(), nullptr,
                  w - (text_x - pos.x) - 18.0f * base_scale);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

} // namespace

void Init(const Vulkan::Instance& instance, vk::Format format, u32 image_count) {
    std::scoped_lock lock{imgui_mutex};
    if (initialized) {
        return;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // window positions are not kept
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.BackendPlatformName = "bbport";

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg].w = 0.92f;
    // Slate controls with a crimson interaction state shared by the launcher.
    style.Colors[ImGuiCol_FrameBg] = ImVec4(.16f, .19f, .23f, 1);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(.25f, .22f, .28f, 1);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(.32f, .20f, .25f, 1);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(.57f, .31f, .39f, 1);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(.73f, .36f, .47f, 1);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(.40f, .83f, .48f, 1);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(.79f, .23f, .30f, 1);
    style.Colors[ImGuiCol_Separator] = ImVec4(.33f, .35f, .40f, 1);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(.35f, .36f, .41f, 1);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(.48f, .40f, .48f, 1);

    ImFontConfig font_config;
    font_config.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(bb_font_ttf),
                                   int(bb_font_ttf_end - bb_font_ttf), 18.0f, &font_config);
#ifdef _WIN32
    // Use the system serif for the guide's display typography without bundling a Windows font.
    if (const char* windows = std::getenv("WINDIR")) {
        const auto georgia = std::filesystem::path(windows) / "Fonts" / "georgia.ttf";
        if (std::filesystem::is_regular_file(georgia))
            guide_serif_font = io.Fonts->AddFontFromFileTTF(georgia.string().c_str(), 28.0f);
    }
#endif

    const vk::Instance vk_instance = instance.GetInstance();
    ImGui_ImplVulkan_LoadFunctions(
        instance.ApiVersion(),
        [](const char* name, void* user) {
            return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(
                *static_cast<const vk::Instance*>(user), name);
        },
        const_cast<vk::Instance*>(&vk_instance));

    const VkFormat color_format = static_cast<VkFormat>(format);
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = instance.ApiVersion();
    info.Instance = vk_instance;
    info.PhysicalDevice = instance.GetPhysicalDevice();
    info.Device = instance.GetDevice();
    info.QueueFamily = instance.GetGraphicsQueueFamilyIndex();
    info.Queue = instance.GetGraphicsQueue();
    info.DescriptorPoolSize = 256; // font atlas plus the game's trophy icons
    info.MinImageCount = std::max(image_count, 2u);
    info.ImageCount = std::max(image_count, 2u);
    info.UseDynamicRendering = true;
    info.PipelineInfoMain.PipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_format,
    };
    if (!ImGui_ImplVulkan_Init(&info)) {
        std::printf("Overlay: ImGui Vulkan backend init failed\n");
        ImGui::DestroyContext();
        return;
    }
    initialized = true;
    std::printf("Overlay: menu ready (Insert or L3+R3)\n");
}

void UpdateTextInput(SDL_Window* window) {
    bool want = false;
    {
        std::scoped_lock lock{imgui_mutex};
        want = initialized && menu_open && ImGui::GetIO().WantTextInput;
    }
    if (want != SDL_TextInputActive(window)) {
        if (want) {
            SDL_StartTextInput(window);
        } else {
            SDL_StopTextInput(window);
        }
    }
}

bool HandleEvent(const SDL_Event& event) {
    std::scoped_lock lock{imgui_mutex};
    if (!initialized) {
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    const bool is_open = menu_open;
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        const bool down = event.type == SDL_EVENT_KEY_DOWN;
        if (down && !event.key.repeat &&
            (event.key.key == SDLK_INSERT || (is_open && event.key.key == SDLK_ESCAPE))) {
            SetOpen(event.key.key == SDLK_INSERT ? !is_open : false);
            return true;
        }
        if (!is_open) {
            return false;
        }
        io.AddKeyEvent(ImGuiMod_Ctrl, (event.key.mod & SDL_KMOD_CTRL) != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (event.key.mod & SDL_KMOD_SHIFT) != 0);
        io.AddKeyEvent(ImGuiMod_Alt, (event.key.mod & SDL_KMOD_ALT) != 0);
        if (const ImGuiKey key = KeyFromSdl(event.key.key); key != ImGuiKey_None) {
            io.AddKeyEvent(key, down);
        }
        return true;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        const bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        const u8 button = event.gbutton.button;
        if (button == SDL_GAMEPAD_BUTTON_LEFT_STICK) {
            l3_down = down;
        } else if (button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
            r3_down = down;
        }
        if (down && l3_down && r3_down) {
            SetOpen(!is_open);
            return true;
        }
        if (!is_open) {
            return false;
        }
        if (button == SDL_GAMEPAD_BUTTON_EAST && down) {
            if (active_tab == 0) SetOpen(false);
            else active_tab = 0;
            return true;
        }
        if (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER ||
            button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) {
            if (down) active_tab = (active_tab +
                (button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER ? 1 : 3)) % 4;
            return true;
        }
        if (const ImGuiKey key = KeyFromGamepad(button); key != ImGuiKey_None) {
            io.AddKeyEvent(key, down);
        }
        return true;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        if (!is_open) return false;
        const float value = std::clamp(float(event.gaxis.value) / 32767.0f, -1.0f, 1.0f);
        const auto analog = [&](ImGuiKey key, float magnitude) {
            const float amount = std::clamp((magnitude - .24f) / .76f, 0.0f, 1.0f);
            io.AddKeyAnalogEvent(key, amount > 0.0f, amount);
        };
        if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
            analog(ImGuiKey_GamepadLStickLeft, -value);
            analog(ImGuiKey_GamepadLStickRight, value);
        } else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
            analog(ImGuiKey_GamepadLStickUp, -value);
            analog(ImGuiKey_GamepadLStickDown, value);
        }
        return true;
    }
    case SDL_EVENT_TEXT_INPUT: {
        // Typed characters (Ctrl+click on a slider, a text field): key events alone erase but
        // do not type. SDL sends them while text input is on (UpdateTextInput).
        if (!is_open) {
            return false;
        }
        io.AddInputCharactersUTF8(event.text.text);
        return true;
    }
    case SDL_EVENT_MOUSE_MOTION: {
        if (!is_open) {
            return false;
        }
        const float density = PixelDensity(event.motion.windowID);
        io.AddMousePosEvent(event.motion.x * density, event.motion.y * density);
        return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (!is_open) {
            return false;
        }
        const int button = event.button.button == SDL_BUTTON_LEFT    ? 0
                           : event.button.button == SDL_BUTTON_RIGHT  ? 1
                           : event.button.button == SDL_BUTTON_MIDDLE ? 2
                                                                      : -1;
        if (button >= 0) {
            io.AddMouseButtonEvent(button, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        }
        return true;
    }
    case SDL_EVENT_MOUSE_WHEEL:
        if (!is_open) {
            return false;
        }
        io.AddMouseWheelEvent(event.wheel.x, event.wheel.y);
        return true;
    default:
        return false;
    }
}

bool Visible() {
    return initialized && (menu_open || text_entry_active || BbSettings::Get().show_fps ||
                           active_toast.load() >= 0 || queued_toasts.load() > 0);
}

bool CapturesInput() {
    return menu_open || text_entry_active;
}

void SetTextEntry(bool active, const std::string& prompt, const std::string& text) {
    std::scoped_lock lock{imgui_mutex};
    text_entry_active = active;
    text_entry_prompt = prompt;
    text_entry_text = text;
}

void LoadTrophies(const std::int64_t* timestamps, unsigned count) {
    std::scoped_lock lock{trophy_mutex};
    trophies.fill(0);
    if (timestamps) {
        for (unsigned i = 0; i < std::min<unsigned>(count, trophies.size()); ++i)
            trophies[i] = timestamps[i] ? 1 : 0;
    }
    toast_queue.clear();
    queued_toasts = 0;
    active_toast = -1;
    active_toast_preview = false;
}

void TrophyUnlocked(int id) {
    if (id < 0 || id >= int(trophies.size())) return;
    {
        std::scoped_lock lock{trophy_mutex};
        trophies[id] = 1;
    }
    {
        std::scoped_lock lock{trophy_mutex};
        toast_queue.push_back({id, false});
        ++queued_toasts;
    }
}

void Render(vk::CommandBuffer cmdbuf, vk::ImageView view, vk::Extent2D extent) {
    // Present interval for the FPS readout (measured also while nothing is drawn).
    const auto now = std::chrono::steady_clock::now();
    const float ms = std::chrono::duration<float, std::milli>(now - last_present).count();
    last_present = now;
    if (ms > 0.0f && ms < 1000.0f) {
        frame_ms_avg = frame_ms_avg == 0.0f ? ms : frame_ms_avg * 0.95f + ms * 0.05f;
    }
    if (!Visible()) {
        return;
    }
    std::scoped_lock lock{imgui_mutex};
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(float(extent.width), float(extent.height));
    io.DeltaTime = ms > 0.0f && ms < 1000.0f ? ms / 1000.0f : 1.0f / 60.0f;
    // UI scale follows the display height (1080p = 1).
    const float scale = std::max(float(extent.height) / 1080.0f, 0.75f);
    if (std::abs(scale - base_scale) > 0.01f) {
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(scale / base_scale);
        style.FontScaleMain = scale;
        base_scale = scale;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    if (menu_open) {
        Menu();
    }
    if (BbSettings::Get().show_fps && !menu_open) {
        FpsCounter();
    }
    if (text_entry_active) {
        TextEntryBox();
    }
    if (active_toast.load() >= 0 || queued_toasts.load() > 0) {
        TrophyToast();
    }
    ImGui::Render();

    const vk::RenderingAttachmentInfo attachment{
        .imageView = view,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
    };
    cmdbuf.beginRendering(vk::RenderingInfo{
        .renderArea = {{0, 0}, extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment,
    });
    {
        // Font atlas uploads submit to the graphics queue themselves.
        std::scoped_lock submit_lock{Vulkan::Scheduler::submit_mutex};
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdbuf);
    }
    cmdbuf.endRendering();
}

} // namespace BbOverlay
