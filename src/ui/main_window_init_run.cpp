#include "../include/ui/main_window.h"
#include <iostream>
#include <thread>
#include <chrono>

#ifdef USE_SDL2
#include <SDL.h>
#endif

#include "../external/imgui/imgui.h"
#include "../external/imgui/backends/imgui_impl_sdl2.h"
#include "../external/imgui/backends/imgui_impl_opengl3.h"

namespace llama_gui {
namespace ui {

void MainWindow::run() {
    if (!is_initialized_) {
        std::cerr << "MainWindow not initialized" << std::endl;
        return;
    }

    is_running_ = true;
    std::cout << "MainWindow: Starting GUI main loop" << std::endl;

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    setup_imgui_style();

#ifdef USE_SDL2
    if (sdl_window_ && gl_context_) {
        if (!ImGui_ImplSDL2_InitForOpenGL(sdl_window_, gl_context_)) {
            std::cerr << "❌ Failed to initialize ImGui SDL2 backend!" << std::endl;
            return;
        }
    }
#endif

#ifdef USE_OPENGL
    if (gl_context_) {
        if (!ImGui_ImplOpenGL3_Init("#version 130")) {
            std::cerr << "❌ Failed to initialize ImGui OpenGL backend!" << std::endl;
            return;
        }
    }
#endif

    // Load fonts with Cyrillic support (AFTER backend initialization)
    load_fonts_with_cyrillic();

    // Main loop
    while (is_running_) {
#ifdef USE_SDL2
        if (sdl_window_) {
            handle_sdl_events();
        }
#else
        if (!is_running_) break;
#endif

        // Process async HTTP requests
        llama_interface_.process_async_requests();

        // Process pending language change BEFORE ImGui NewFrame
        if (pending_language_change_) {
            std::cout << "Processing pending language change before NewFrame..." << std::endl;
            reload_fonts();
            advanced_menu_system_.rebuildModernMenu();
            pending_language_change_ = false;
            ui_dirty_menu_ = true;
        }

#ifdef USE_OPENGL
        if (gl_context_) {
            ImGui_ImplOpenGL3_NewFrame();
        }
#endif
#ifdef USE_SDL2
        if (sdl_window_ && gl_context_) {
            ImGui_ImplSDL2_NewFrame();
        }
#endif
        ImGui::NewFrame();

        // Update current time for performance tracking
        uint32_t current_time = 0;
#ifdef USE_SDL2
        current_time = SDL_GetTicks();
#else
        current_time = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
#endif

        // Check for user activity
        if (ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered() || ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
            ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            last_user_activity_time_ = current_time;
            is_idle_ = false;
        }

        // Check if we should switch to idle mode
        if (!is_idle_ && (current_time - last_user_activity_time_) > settings_.performance().idle_timeout_ms) {
            is_idle_ = true;
        }

        // Update performance metrics
        if (current_time - last_performance_update_time_ >= settings_.performance().performance_update_interval_ms) {
            update_performance_metrics();
            last_performance_update_time_ = current_time;
        }

        // Check if we need to force UI update
        if (force_ui_update_) {
            ImGui::SetWindowFocus("Chat");
            force_ui_update_ = false;
        }

        // Render UI (includes status bar)
        render();

        // Rendering
        ImGui::Render();

#ifdef USE_OPENGL
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif

#ifdef USE_SDL2
        SDL_GL_SwapWindow(sdl_window_);
#endif

        // Frame rate limiting
        static auto last_frame_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto frame_duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame_time);

        int target_frame_ms = is_idle_ ?
            (1000 / settings_.performance().idle_fps) :
            (1000 / settings_.performance().target_fps);

        if (!settings_.performance().enable_vsync) {
            if (frame_duration.count() < target_frame_ms) {
                std::this_thread::sleep_for(std::chrono::milliseconds(target_frame_ms - frame_duration.count()));
            }
        }

        last_frame_time = now;
    }

    // Cleanup
#ifdef USE_OPENGL
    ImGui_ImplOpenGL3_Shutdown();
#endif
#ifdef USE_SDL2
    ImGui_ImplSDL2_Shutdown();
#endif
    ImGui::DestroyContext();

    std::cout << "MainWindow: GUI main loop ended" << std::endl;
}

} // namespace ui
} // namespace llama_gui
