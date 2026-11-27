#include "snes_preview.h"
#include "snes_audio.h"
#include "snes_frame.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

using PreviewClock = std::chrono::steady_clock;
// 341 dots per scanline, 262 scanlines at the NTSC dot rate.
constexpr auto frame_period = std::chrono::nanoseconds(16639357);
constexpr int window_scale = 3;
// Drop audio blocks once about eight frames are queued, like the Win32 buffer ring.
constexpr int audio_queue_limit = 8 * 534 * SnesAudio::channels * static_cast<int>(sizeof(int16_t));
constexpr int audio_prefill_blocks = 3;

class AudioOutput {
public:
    AudioOutput() = default;
    ~AudioOutput() { close(); }
    AudioOutput(const AudioOutput &) = delete;
    AudioOutput &operator=(const AudioOutput &) = delete;

    bool open() {
        close();
        const SDL_AudioSpec spec{SDL_AUDIO_S16, SnesAudio::channels, SnesAudio::sample_rate};
        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        prefill_ = audio_prefill_blocks;
        return stream_ != nullptr;
    }

    void close() {
        if (stream_) {
            SDL_DestroyAudioStream(stream_);
            stream_ = nullptr;
        }
    }

    void submit(std::span<const int16_t> samples) {
        if (!stream_ || samples.empty()) {
            return;
        }
        if (SDL_GetAudioStreamQueued(stream_) > audio_queue_limit) {
            ++dropped_blocks;
            return;
        }
        if (SDL_PutAudioStreamData(stream_, samples.data(), static_cast<int>(samples.size_bytes()))) {
            submitted_bytes += samples.size_bytes();
            if (prefill_ != 0 && --prefill_ == 0) {
                SDL_ResumeAudioStreamDevice(stream_);
            }
        }
    }

    bool playing() const { return stream_ && !SDL_AudioStreamDevicePaused(stream_); }

    uint64_t submitted_bytes = 0;
    unsigned dropped_blocks = 0;

private:
    SDL_AudioStream *stream_ = nullptr;
    unsigned prefill_ = 0;
};

// SNES_PREVIEW_SCRIPT="frame:command;..." where command is a hex joypad mask held from
// that frame on, "shot" to save the presented window, or "quit".
struct ScriptStep {
    uint32_t frame;
    std::string command;
};

std::vector<ScriptStep> parse_script(const char *text) {
    std::vector<ScriptStep> steps;
    std::string_view rest = text ? text : "";
    while (!rest.empty()) {
        const auto end = std::min(rest.find(';'), rest.size());
        const auto entry = rest.substr(0, end);
        rest.remove_prefix(std::min(end + 1, rest.size()));
        const auto colon = entry.find(':');
        if (colon == std::string_view::npos) {
            continue;
        }
        steps.push_back({static_cast<uint32_t>(std::stoul(std::string(entry.substr(0, colon)))),
                         std::string(entry.substr(colon + 1))});
    }
    std::ranges::stable_sort(steps, {}, &ScriptStep::frame);
    return steps;
}

struct PreviewSession {
    SnesPreviewProgram program;
    std::string title;
    Cpu65816 cpu{};
    SnesFrame frame;
    SnesAudio audio;
    AudioOutput output;
    PreviewClock::time_point next_frame;
    uint16_t keyboard = 0;
    uint16_t gamepad = 0;
    bool running = false;
    std::vector<ScriptStep> script;
    size_t script_next = 0;
    uint32_t frames = 0;
    uint16_t scripted = 0;
    bool shot_pending = false;
    bool script_quit = false;

    void run_script() {
        for (; script_next < script.size() && script[script_next].frame <= frames; ++script_next) {
            const auto &command = script[script_next].command;
            if (command == "shot") {
                shot_pending = true;
            } else if (command == "quit") {
                script_quit = true;
            } else {
                scripted = static_cast<uint16_t>(std::stoul(command, nullptr, 16));
            }
        }
    }

    bool boot() {
        output.close();
        frames = 0;
        script_next = 0;
        scripted = 0;
        const auto *rom = cpu.rom;
        const auto rom_size = cpu.rom_size;
        cpu_reset(&cpu);
        cpu.rom = rom;
        cpu.rom_size = rom_size;
        audio.attach(cpu.apu);
        if (!program.boot(cpu) || !snes_render_mode1(cpu.ppu, frame) ||
            std::ranges::none_of(frame.pixels, [](uint32_t pixel) { return pixel != 0; })) {
            return false;
        }
        audio.take_samples();
        title = std::string(program.name) + " - Enter: Start | Arrows: Move | Z/X: B/A | F5: Restart";
        if (!output.open()) {
            title += " | Audio output unavailable";
        }
        next_frame = PreviewClock::now();
        running = true;
        return true;
    }
};

uint16_t keyboard_button(SDL_Scancode key) {
    switch (key) {
    case SDL_SCANCODE_RETURN: return JOYPAD_START;
    case SDL_SCANCODE_UP: return JOYPAD_UP;
    case SDL_SCANCODE_DOWN: return JOYPAD_DOWN;
    case SDL_SCANCODE_LEFT: return JOYPAD_LEFT;
    case SDL_SCANCODE_RIGHT: return JOYPAD_RIGHT;
    case SDL_SCANCODE_Z: return JOYPAD_B;
    case SDL_SCANCODE_X: return JOYPAD_A;
    default: return 0;
    }
}

uint16_t gamepad_button(Uint8 button) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_START: return JOYPAD_START;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return JOYPAD_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return JOYPAD_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return JOYPAD_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return JOYPAD_RIGHT;
    case SDL_GAMEPAD_BUTTON_SOUTH: return JOYPAD_B;
    case SDL_GAMEPAD_BUTTON_EAST: return JOYPAD_A;
    default: return 0;
    }
}

std::string stop_message(BootResult result, const Cpu65816 &cpu) {
    const char *reason = "Unsupported display mode";
    uint32_t address = cpu.pc;
    switch (result) {
    case BOOT_CHECKPOINT: reason = "Preview checkpoint"; break;
    case BOOT_UNKNOWN_BLOCK: reason = "Untranslated game code"; break;
    case BOOT_UNMAPPED_ACCESS:
        reason = "Unsupported bus access";
        address = cpu.unmapped_address;
        break;
    case BOOT_AUDIO_FAULT: reason = "Sound driver stopped"; break;
    case BOOT_STEP_LIMIT: reason = "Frame step limit reached"; break;
    case BOOT_FRAME: break;
    }
    char message[96];
    std::snprintf(message, sizeof(message), "%s at 0x%06X | F5: Restart", reason,
                  static_cast<unsigned>(address));
    return message;
}

void advance_frames(PreviewSession &session, SDL_Window *window) {
    const auto now = PreviewClock::now();
    if (!session.running || now < session.next_frame) {
        return;
    }
    // keep audio and video on the same clock after a stall
    session.next_frame = std::max(session.next_frame, now - 3 * frame_period);
    do {
        session.run_script();
        if (session.shot_pending || session.script_quit) {
            // present the frame before the scripted capture or exit
            return;
        }
        session.cpu.joypad_input[0] = session.keyboard | session.gamepad | session.scripted;
        const auto result = session.program.advance(&session.cpu);
        ++session.frames;
        if (result != BOOT_FRAME || !snes_render_mode1(session.cpu.ppu, session.frame)) {
            session.running = false;
            session.output.close();
            const auto stopped = std::string(session.program.name) + " - " + stop_message(result, session.cpu);
            SDL_SetWindowTitle(window, stopped.c_str());
            std::fprintf(stderr, "%s\n", stopped.c_str());
            return;
        }
        session.output.submit(session.audio.take_samples());
        session.next_frame += frame_period;
    } while (session.next_frame <= now);
}

void save_surface(SDL_Surface *surface, const SnesPreviewProgram &program, const char *prefix) {
    static unsigned counter = 0;
    if (!surface) {
        return;
    }
    char name[48];
    std::snprintf(name, sizeof(name), "%s-%03u.bmp", prefix, counter++);
    const auto path = (preview_executable_directory() / name).string();
    if (SDL_SaveBMP(surface, path.c_str())) {
        std::fprintf(stderr, "%s: saved %s\n", program.name, path.c_str());
    }
    SDL_DestroySurface(surface);
}

void save_screenshot(const SnesFrame &frame, const SnesPreviewProgram &program) {
    auto *view = SDL_CreateSurfaceFrom(SnesFrame::width, SnesFrame::height, SDL_PIXELFORMAT_XRGB8888,
                                       const_cast<uint32_t *>(frame.pixels.data()),
                                       SnesFrame::width * static_cast<int>(sizeof(uint32_t)));
    save_surface(view ? SDL_DuplicateSurface(view) : nullptr, program, "screenshot");
    SDL_DestroySurface(view);
}

struct SdlState {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;

    ~SdlState() {
        if (texture) SDL_DestroyTexture(texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

} // namespace

int show_preview_error(const char *title, const char *message) {
    std::fprintf(stderr, "%s: %s\n", title, message);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, nullptr);
    return 1;
}

std::filesystem::path preview_executable_directory() {
    const char *base = SDL_GetBasePath();
    if (base == nullptr) {
        return {};
    }
    return std::filesystem::path(base);
}

int run_snes_preview(const SnesPreviewProgram &program, std::span<const uint8_t> rom) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        return show_preview_error(program.name, SDL_GetError());
    }
    SdlState sdl;
    auto session = std::make_unique<PreviewSession>();
    session->program = program;
    try {
        session->script = parse_script(SDL_getenv("SNES_PREVIEW_SCRIPT"));
        for (const auto &step : session->script) {
            if (step.command != "shot" && step.command != "quit") {
                (void)std::stoul(step.command, nullptr, 16);
            }
        }
    } catch (const std::exception &) {
        return show_preview_error(program.name, "SNES_PREVIEW_SCRIPT is malformed.");
    }
    session->cpu.rom = rom.data();
    session->cpu.rom_size = static_cast<uint32_t>(rom.size());
    if (!session->boot()) {
        return show_preview_error(program.name, "The recompiled boot did not reach a visible frame.");
    }

    if (!SDL_CreateWindowAndRenderer(session->title.c_str(), SnesFrame::width * window_scale,
                                     SnesFrame::height * window_scale, SDL_WINDOW_RESIZABLE,
                                     &sdl.window, &sdl.renderer)) {
        return show_preview_error(program.name, SDL_GetError());
    }
    SDL_SetRenderVSync(sdl.renderer, 1);
    SDL_SetRenderLogicalPresentation(sdl.renderer, SnesFrame::width, SnesFrame::height,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    sdl.texture = SDL_CreateTexture(sdl.renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
                                    SnesFrame::width, SnesFrame::height);
    if (!sdl.texture) {
        return show_preview_error(program.name, SDL_GetError());
    }
    SDL_SetTextureScaleMode(sdl.texture, SDL_SCALEMODE_NEAREST);

    bool quit = false;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    quit = true;
                } else if (event.key.scancode == SDL_SCANCODE_F5 && !event.key.repeat) {
                    if (session->boot()) {
                        SDL_SetWindowTitle(sdl.window, session->title.c_str());
                    } else {
                        show_preview_error(program.name, "Could not restart the interactive preview.");
                    }
                } else if (event.key.scancode == SDL_SCANCODE_F12 && !event.key.repeat) {
                    save_screenshot(session->frame, program);
                } else {
                    session->keyboard |= keyboard_button(event.key.scancode);
                }
                break;
            case SDL_EVENT_KEY_UP:
                session->keyboard &= ~keyboard_button(event.key.scancode);
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                session->keyboard = 0;
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                SDL_OpenGamepad(event.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                if (auto *pad = SDL_GetGamepadFromID(event.gdevice.which)) {
                    SDL_CloseGamepad(pad);
                }
                session->gamepad = 0;
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
                    if (session->boot()) {
                        SDL_SetWindowTitle(sdl.window, session->title.c_str());
                    }
                } else {
                    session->gamepad |= gamepad_button(event.gbutton.button);
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                session->gamepad &= ~gamepad_button(event.gbutton.button);
                break;
            }
        }

        advance_frames(*session, sdl.window);
        SDL_UpdateTexture(sdl.texture, nullptr, session->frame.pixels.data(),
                          SnesFrame::width * static_cast<int>(sizeof(uint32_t)));
        SDL_SetRenderDrawColor(sdl.renderer, 0, 0, 0, 255);
        SDL_RenderClear(sdl.renderer);
        SDL_RenderTexture(sdl.renderer, sdl.texture, nullptr, nullptr);
        if (session->shot_pending) {
            session->shot_pending = false;
            std::fprintf(stderr, "%s: frame %u, audio %s, %llu bytes queued, %u blocks dropped\n",
                         program.name, session->frames, session->output.playing() ? "playing" : "stopped",
                         static_cast<unsigned long long>(session->output.submitted_bytes),
                         session->output.dropped_blocks);
            save_surface(SDL_RenderReadPixels(sdl.renderer, nullptr), program, "window");
        }
        SDL_RenderPresent(sdl.renderer);
        if (session->script_quit) {
            quit = true;
        }

        // vsync may be off or faster than 60 Hz: sleep until the next emulated frame
        const auto wait = session->next_frame - PreviewClock::now();
        if (session->running && wait > std::chrono::milliseconds(1)) {
            SDL_DelayPrecise(static_cast<Uint64>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(wait).count() - 500000));
        } else if (!session->running) {
            SDL_Delay(16);
        }
    }
    return 0;
}
