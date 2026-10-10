// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_overlay.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

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
int active_tab = 0; // Home, Achievements, Settings, Mods / Cheats
bool dirty = false; // settings changed while open: saved on close
// The game's text dialog (SetTextEntry), guarded by imgui_mutex.
bool text_entry_active = false;
std::string text_entry_prompt, text_entry_text;
float base_scale = 1.0f;

// Present rate for the FPS counter.
std::chrono::steady_clock::time_point last_present{};
float frame_ms_avg = 0.0f;

// Trophy events can arrive on the game thread while the present thread draws the overlay.
std::mutex trophy_mutex;
std::array<unsigned char, 128> trophies{};
std::deque<int> toast_queue;
std::atomic<int> queued_toasts{0};
std::atomic<int> active_toast{-1};
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
    if (value) menu_opened_ms = ClockMs();
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
    if (BbSettings::FixedRenderSession()) {
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
    static const char* outputs[] = {"1280 x 720", "1920 x 1080", "2560 x 1440", "3840 x 2160"};
    int output = s.output_res;
    if (ImGui::BeginCombo("Output resolution", outputs[output])) {
        for (int i = 0; i < BbSettings::OutputCount; ++i) {
            if (ImGui::Selectable(outputs[i], i == output)) {
                Store(s.output_res, i, true);
            }
        }
        ImGui::EndCombo();
    }
    if (BbSettings::FixedRenderSession()) {
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

void ModsTab() {
    auto& s = BbSettings::Get();
    ImGui::TextColored(ImVec4(.39f, .86f, .46f, 1), "Mods / Cheats");
    ImGui::TextWrapped("These switches use the existing game patches. Changes apply after a restart.");
    ImGui::SeparatorText("Gameplay and debug");
    for (int e = 8; e < BbSettings::EffectCount; ++e)
        Checkbox(BbSettings::Effects[e].label, s.effects[e]);
    Hint("Free camera: hold Cross/A and press L3 (keyboard: Space + Z). The debug menu needs DbgFont14h.ccm and DbgFont14h.tpf in dvdroot_ps4/font.");
    ImGui::Separator();
    RestartPrompt();
    ImGui::TextDisabled("Mod order and the FPS cap are set in the launcher before starting the game.");
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
    const float h = 98.0f * base_scale;
    const bool earned = entry.unlocked_at > 0;
    ImGui::PushID(entry.id);
    ImGui::Selectable("##achievement", false, 0, ImVec2(0, h));
    const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float icon = 52.0f * base_scale;
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
    const std::string name = secret ? "Secret achievement" : entry.name;
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
                  score.c_str());
    if (!detail.empty())
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * .83f,
                      ImVec2(text_x, min.y + 34.0f * base_scale),
                      earned ? IM_COL32(180, 192, 183, 255) : IM_COL32(112, 122, 115, 255),
                      detail.c_str(), nullptr, right_x - text_x - 7.0f * base_scale);
    if (earned) {
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
    auto entries = BbTrophies::Entries();
    const int total = int(entries.size());
    int earned = 0;
    for (const auto& entry : entries) earned += entry.unlocked_at > 0;
    const int score = BbTrophies::EarnedScore();
    ImGui::TextColored(ImVec4(.39f, .86f, .46f, 1), "Achievements");
    if (BbTrophies::TotalScore() == 1000)
        ImGui::Text("Gamerscore: %d / 1000G", score);
    else
        ImGui::Text("Gamerscore: -- / 1000G");
    ImGui::Text("%d of %d unlocked", earned, total);
    ImGui::ProgressBar(total ? float(earned) / total : 0.0f,
                       ImVec2(-1, 9.0f * base_scale), "");
    if (!BbTrophies::HasMetadata())
        ImGui::TextWrapped("Names and scores need the trophy key from your own PS4. Icons and local unlock history still work.");
    ImGui::Separator();
    if (entries.empty()) {
        ImGui::TextDisabled("No trophy pack found in the selected game folder.");
        return;
    }
    ImGuiListClipper clipper;
    clipper.Begin(total, 98.0f * base_scale + ImGui::GetStyle().ItemSpacing.y);
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            AchievementRow(entries[i]);
    ImGui::TextDisabled("Local achievements only. No online account is connected.");
}

void HomeTab() {
    const auto entries = BbTrophies::Entries();
    const int earned = int(std::count_if(entries.begin(), entries.end(),
                                         [](const auto& e) { return e.unlocked_at > 0; }));
    ImGui::TextColored(ImVec4(.39f, .86f, .46f, 1), "Bloodborne");
    ImGui::Text("%.0f FPS  |  %.1f ms", frame_ms_avg > 0.0f ? 1000.0f / frame_ms_avg : 0.0f,
                frame_ms_avg);
    ImGui::Separator();
    ImGui::Text("%d of %d achievements unlocked", earned, int(entries.size()));
    if (BbTrophies::TotalScore() == 1000)
        ImGui::Text("%d / 1000G", BbTrophies::EarnedScore());
    if (ImGui::Button("View achievements", ImVec2(-1, 0))) active_tab = 1;
    if (ImGui::Button("Settings", ImVec2(-1, 0))) active_tab = 2;
    if (ImGui::Button("Mods / Cheats", ImVec2(-1, 0))) active_tab = 3;
    ImGui::Spacing();
    ImGui::TextDisabled("Insert or L3+R3 closes the guide.");
}

void Menu() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float appear = std::clamp(float(ClockMs() - menu_opened_ms.load()) / 180.0f,
                                    0.0f, 1.0f);
    ImGui::GetBackgroundDrawList()->AddRectFilled(viewport->Pos,
        ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
        IM_COL32(0, 0, 0, int(145 * appear)));
    const float margin = 24.0f * base_scale;
    const float width = std::min(610.0f * base_scale, viewport->WorkSize.x - margin * 2.0f);
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + margin -
                                   (1.0f - appear) * 36.0f * base_scale,
                                   viewport->WorkPos.y + margin));
    ImGui::SetNextWindowSize(ImVec2(width, viewport->WorkSize.y - margin * 2.0f));
    ImGui::SetNextWindowBgAlpha(.97f * appear);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, .6f + .4f * appear);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f * base_scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(20.0f * base_scale, 18.0f * base_scale));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(.13f, .13f, .14f, 1));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(.23f, .22f, .26f, 1));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(.27f, .25f, .31f, 1));
    ImGui::PushStyleColor(ImGuiCol_NavHighlight, ImVec4(.70f, .48f, .94f, 1));
    ImGui::Begin("##bloodborne_guide", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored(ImVec4(.90f, .91f, .91f, 1), "BLOODBORNE");
    ImGui::SameLine();
    ImGui::TextDisabled("/ GUIDE");
    ImGui::Spacing();
    const char* tabs[] = {"Home", "Achievements", "Settings", "Mods"};
    const float tab_width = (ImGui::GetContentRegionAvail().x -
                             ImGui::GetStyle().ItemSpacing.x * 3.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        if (i) ImGui::SameLine();
        const bool selected = i == active_tab;
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f * base_scale);
        ImGui::PushStyleColor(ImGuiCol_Button, selected ?
            ImVec4(.23f, .22f, .26f, 1) : ImVec4(.16f, .16f, .17f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.29f, .27f, .32f, 1));
        if (ImGui::Button(tabs[i], ImVec2(tab_width, 46.0f * base_scale))) active_tab = i;
        if (selected) {
            const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(min.x + 14.0f * base_scale, max.y - 3.0f * base_scale),
                ImVec2(max.x - 14.0f * base_scale, max.y), IM_COL32(166, 107, 224, 255),
                2.0f * base_scale);
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::BeginChild("##guide_content", ImVec2(0, -35.0f * base_scale), false);
    switch (active_tab) {
    case 0: HomeTab(); break;
    case 1: AchievementsTab(); break;
    case 2: SettingsTab(); break;
    case 3: ModsTab(); break;
    }
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::TextDisabled("D-pad / stick  Move     A  Select     B  Back     LB/RB  Tabs");
    ImGui::End();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(3);
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
        active_toast = toast_queue.front();
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
                  IM_COL32(255, 255, 255, int(255 * alpha)), "Achievement unlocked");
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

    ImFontConfig font_config;
    font_config.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(bb_font_ttf),
                                   int(bb_font_ttf_end - bb_font_ttf), 18.0f, &font_config);

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
}

void TrophyUnlocked(int id) {
    if (id < 0 || id >= int(trophies.size())) return;
    {
        std::scoped_lock lock{trophy_mutex};
        trophies[id] = 1;
    }
    {
        std::scoped_lock lock{trophy_mutex};
        toast_queue.push_back(id);
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
