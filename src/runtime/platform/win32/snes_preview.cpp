#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "snes_preview.h"
#include "snes_frame.h"
#include "wave_output.h"

#include <algorithm>
#include <chrono>
#include <cwchar>
#include <string>

namespace {

constexpr UINT_PTR frame_timer = 1;
using PreviewClock = std::chrono::steady_clock;
// 341 dots per scanline, 262 scanlines at the NTSC dot rate.
constexpr auto frame_period = std::chrono::nanoseconds(16639357);
constexpr UINT timer_interval_ms = 10;

struct PreviewSession {
    SnesPreviewProgram program;
    std::wstring title;
    Cpu65816 cpu;
    SnesFrame frame;
    SnesAudio audio;
    WaveOutput output;
    PreviewClock::time_point next_frame;

    bool boot() {
        output.close();
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
        title = std::wstring(program.name) + L" - Enter: Start | Arrows: Move | F5: Restart";
        if (!output.open()) {
            title += L" | Audio output unavailable";
        }
        next_frame = PreviewClock::now();
        return true;
    }
};

uint16_t controller_button(WPARAM key) {
    switch (key) {
    case VK_RETURN: return JOYPAD_START;
    case VK_UP: return JOYPAD_UP;
    case VK_DOWN: return JOYPAD_DOWN;
    case VK_LEFT: return JOYPAD_LEFT;
    case VK_RIGHT: return JOYPAD_RIGHT;
    case 'Z': return JOYPAD_B;
    case 'X': return JOYPAD_A;
    default: return 0;
    }
}

std::wstring stop_message(BootResult result, const Cpu65816 &cpu) {
    const wchar_t *reason = L"Unsupported display mode";
    uint32_t address = cpu.pc;
    switch (result) {
    case BOOT_CHECKPOINT: reason = L"Preview checkpoint"; break;
    case BOOT_UNKNOWN_BLOCK: reason = L"Untranslated game code"; break;
    case BOOT_UNMAPPED_ACCESS:
        reason = L"Unsupported bus access";
        address = cpu.unmapped_address;
        break;
    case BOOT_AUDIO_FAULT: reason = L"Sound driver stopped"; break;
    case BOOT_STEP_LIMIT: reason = L"Frame step limit reached"; break;
    case BOOT_FRAME: break;
    }
    wchar_t message[96];
    std::swprintf(message, std::size(message), L"%ls at 0x%06X | F5: Restart",
                  reason, static_cast<unsigned>(address));
    return message;
}

void paint_frame(HWND window, const SnesFrame &frame) {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window, &paint);
    RECT client{};
    GetClientRect(window, &client);
    const int width = std::min(client.right, client.bottom * 8 / 7);
    const int height = width * 7 / 8;
    if (width > 0 && height > 0) {
        const int left = (client.right - width) / 2;
        const int top = (client.bottom - height) / 2;
        const int right = left + width;
        const int bottom = top + height;

        BITMAPINFO bitmap{};
        bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap.bmiHeader.biWidth = SnesFrame::width;
        bitmap.bmiHeader.biHeight = -SnesFrame::height;
        bitmap.bmiHeader.biPlanes = 1;
        bitmap.bmiHeader.biBitCount = 32;
        bitmap.bmiHeader.biCompression = BI_RGB;
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchDIBits(dc, left, top, width, height,
                      0, 0, SnesFrame::width, SnesFrame::height, frame.pixels.data(), &bitmap,
                      DIB_RGB_COLORS, SRCCOPY);

        // Repaint the letterbox without clearing the image area between frames.
        const RECT margins[] = {
            {0, 0, client.right, top},
            {0, bottom, client.right, client.bottom},
            {0, top, left, bottom},
            {right, top, client.right, bottom},
        };
        const auto black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        for (const auto &margin : margins) {
            FillRect(dc, &margin, black);
        }
    }
    EndPaint(window, &paint);
}

LRESULT CALLBACK preview_window(HWND window, UINT message, WPARAM key, LPARAM data) {
    if (message == WM_NCCREATE) {
        const auto *event = reinterpret_cast<CREATESTRUCTW *>(data);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(event->lpCreateParams));
    }
    if (message == WM_DESTROY) {
        KillTimer(window, frame_timer);
        PostQuitMessage(0);
        return 0;
    }
    auto *session = reinterpret_cast<PreviewSession *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (session == nullptr) {
        return DefWindowProcW(window, message, key, data);
    }
    switch (message) {
    case WM_PAINT:
        paint_frame(window, session->frame);
        return 0;
    case WM_TIMER: {
        if (key != frame_timer) {
            break;
        }
        const auto now = PreviewClock::now();
        if (now < session->next_frame) {
            return 0;
        }
        // Keep audio and video on the same clock even if Windows batches timer messages.
        session->next_frame = std::max(session->next_frame, now - 3 * frame_period);
        do {
            const auto result = session->program.advance(&session->cpu);
            if (result != BOOT_FRAME || !snes_render_mode1(session->cpu.ppu, session->frame)) {
                KillTimer(window, frame_timer);
                session->output.close();
                const auto stopped = std::wstring(session->program.name) + L" - " +
                                     stop_message(result, session->cpu);
                SetWindowTextW(window, stopped.c_str());
                return 0;
            }
            session->output.submit(session->audio.take_samples());
            session->next_frame += frame_period;
        } while (session->next_frame <= now);
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        if (key == VK_ESCAPE) {
            DestroyWindow(window);
        } else if (key == VK_F5 && (data & (1LL << 30)) == 0) {
            KillTimer(window, frame_timer);
            if (session->boot() && SetTimer(window, frame_timer, timer_interval_ms, nullptr)) {
                SetWindowTextW(window, session->title.c_str());
                InvalidateRect(window, nullptr, FALSE);
            } else {
                show_preview_error(session->program.name, L"Could not restart the interactive preview.");
            }
        } else {
            session->cpu.joypad_input[0] |= controller_button(key);
        }
        return 0;
    case WM_KEYUP:
        session->cpu.joypad_input[0] &= ~controller_button(key);
        return 0;
    case WM_KILLFOCUS:
        session->cpu.joypad_input[0] = 0;
        break;
    }
    return DefWindowProcW(window, message, key, data);
}

} // namespace

int show_preview_error(const wchar_t *title, const wchar_t *message) {
    MessageBoxW(nullptr, message, title, MB_OK | MB_ICONERROR);
    return 1;
}

std::filesystem::path preview_executable_path() {
    std::wstring executable(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, executable.data(), executable.size());
    if (length == 0 || length >= executable.size()) {
        return {};
    }
    executable.resize(length);
    return executable;
}

int run_snes_preview(HINSTANCE instance, int show_mode, const SnesPreviewProgram &program,
                     std::span<const uint8_t> rom) {
    PreviewSession session{program, {}, {}, {}, {}, {}, {}};
    session.cpu.rom = rom.data();
    session.cpu.rom_size = static_cast<uint32_t>(rom.size());
    if (!session.boot()) {
        return show_preview_error(program.name, L"The recompiled boot did not reach a visible frame.");
    }

    constexpr wchar_t class_name[] = L"SnesRecompPreview";
    WNDCLASSW window_class{};
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = preview_window;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.lpszClassName = class_name;
    if (!RegisterClassW(&window_class)) {
        return show_preview_error(program.name, L"Could not register the preview window.");
    }
    constexpr DWORD style = WS_OVERLAPPEDWINDOW;
    RECT size{0, 0, SnesFrame::width * 3, SnesFrame::height * 3};
    AdjustWindowRect(&size, style, FALSE);
    const auto window = CreateWindowExW(
        0, class_name, session.title.c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT, size.right - size.left, size.bottom - size.top,
        nullptr, nullptr, instance, &session);
    if (!window) {
        return show_preview_error(program.name, L"Could not open the preview window.");
    }
    if (!SetTimer(window, frame_timer, timer_interval_ms, nullptr)) {
        DestroyWindow(window);
        return show_preview_error(program.name, L"Could not start the preview frame timer.");
    }
    ShowWindow(window, show_mode);
    MSG message{};
    int result;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return result < 0 ? 1 : 0;
}
