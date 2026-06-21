#include "../include/ui/main_window.h"
#include <iostream>
#include <unistd.h>

#ifdef USE_OPENGL
#include "../external/imgui/backends/imgui_impl_opengl3.h"
#include <GL/gl.h>
#endif

#ifdef USE_SDL2
#include <SDL.h>
#endif

#include "../external/imgui/imgui.h"

namespace llama_gui {
namespace ui {

void MainWindow::load_fonts_with_cyrillic() {
    ImGuiIO& io = ImGui::GetIO();

    // Get executable directory for font paths
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    std::string exe_dir = (len > 0) ? std::string(exe_path, len) : ".";
    exe_dir = exe_dir.substr(0, exe_dir.find_last_of('/'));

    // Try multiple font paths for FontAwesome
    std::vector<std::string> fontawesome_paths = {
        exe_dir + "/fonts/FontAwesomeSolid.otf",
        exe_dir + "/../fonts/FontAwesomeSolid.otf",
        "/usr/share/fonts/font-awesome/FontAwesomeSolid.otf",
        "/usr/share/fonts/truetype/font-awesome/FontAwesomeSolid.otf"
    };

    std::string fontawesome_path;
    for (const auto& path : fontawesome_paths) {
        if (access(path.c_str(), F_OK) == 0) {
            fontawesome_path = path;
            break;
        }
    }

    // NOTE: Clear() is called in reload_fonts() before calling this function
    // Don't call Clear() here to allow using this function at initialization too

    // Load fonts with Cyrillic support
    ImFont* default_font = io.Fonts->AddFontDefault();

    // Try to load system fonts with Cyrillic support
    ImFont* cyrillic_font = nullptr;
    float font_size = 16.0f * settings_.get_dpi_scale();

    // Настраиваем диапазон символов для кириллицы и расширенного Unicode
    static const ImWchar cyrillic_ranges[] = {
        0x0020, 0x00FF,  // Basic Latin + Latin-1 Supplement
        0x0400, 0x04FF,  // Cyrillic
        0x0500, 0x052F,  // Cyrillic Supplement
        0x2DE0, 0x2DFF,  // Cyrillic Extended-A
        0xA640, 0xA69F,  // Cyrillic Extended-B
        0x1F00, 0x1FFF,  // Greek Extended
        0x2000, 0x206F,  // General Punctuation
        0x2100, 0x214F,  // Letterlike Symbols
        0x2190, 0x21FF,  // Arrows
        0x2200, 0x22FF,  // Mathematical Operators
        0x2500, 0x257F,  // Box Drawing
        0x2580, 0x259F,  // Block Elements
        0x2600, 0x26FF,  // Miscellaneous Symbols
        0x2700, 0x27BF,  // Dingbats
        0,
    };

    ImFontConfig font_config;
    font_config.GlyphRanges = cyrillic_ranges;
    font_config.PixelSnapH = true;
    font_config.OversampleH = 3;
    font_config.OversampleV = 3;

    // Try common Linux font paths with Cyrillic support
    std::vector<std::string> cyrillic_font_paths = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/google-noto-cyrillic/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/noto-sans/NotoSans-Regular.ttf"
    };

    for (const auto& font_path : cyrillic_font_paths) {
        if (access(font_path.c_str(), F_OK) == 0) {
            cyrillic_font = io.Fonts->AddFontFromFileTTF(
                font_path.c_str(), 
                font_size,
                &font_config,
                cyrillic_ranges  // Явно указываем диапазон символов
            );
            if (cyrillic_font) {
                std::cout << "✓ Cyrillic font loaded: " << font_path << std::endl;
                break;
            }
        }
    }

    // Попытка загрузить Noto Sans как fallback для специальных символов
    if (!cyrillic_font) {
        std::vector<std::string> fallback_fonts = {
            "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/noto-sans/NotoSans-Regular.ttf",
            "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc"
        };
        
        for (const auto& font_path : fallback_fonts) {
            if (access(font_path.c_str(), F_OK) == 0) {
                cyrillic_font = io.Fonts->AddFontFromFileTTF(
                    font_path.c_str(), 
                    font_size,
                    &font_config,
                    cyrillic_ranges
                );
                if (cyrillic_font) {
                    std::cout << "✓ Fallback font loaded: " << font_path << std::endl;
                    break;
                }
            }
        }
    }

    // Load FontAwesome icons with MergeMode
    if (cyrillic_font && !fontawesome_path.empty()) {
        ImFontConfig icon_config;
        icon_config.MergeMode = true;
        icon_config.PixelSnapH = true;
        icon_config.GlyphOffset.y = font_size * 0.1f;

        // FontAwesome icon range (0xF000-0xF8FF)
        static const ImWchar icon_ranges[] = { 0xF000, 0xF8FF, 0 };

        if (io.Fonts->AddFontFromFileTTF(fontawesome_path.c_str(), font_size, &icon_config, icon_ranges)) {
            std::cout << "✓ FontAwesome icons loaded from: " << fontawesome_path << std::endl;
        } else {
            std::cout << "⚠ Failed to load FontAwesome from " << fontawesome_path << std::endl;
        }

        io.FontDefault = cyrillic_font;
        std::cout << "✓ Fonts initialized with Cyrillic support" << std::endl;
    } else {
        if (fontawesome_path.empty()) {
            std::cout << "⚠ FontAwesome not found" << std::endl;
        }
        if (!cyrillic_font) {
            std::cout << "⚠ Cyrillic font not found, using default font" << std::endl;
        }
    }

    if (!io.Fonts->Build()) {
        std::cerr << "❌ Failed to build fonts" << std::endl;
    }
}

void MainWindow::reload_fonts() {
    std::cout << "Reloading fonts for language change..." << std::endl;

    ImGuiIO& io = ImGui::GetIO();

    // Clear fonts before reloading
    io.Fonts->Clear();
    
    // Load new fonts (Clear() is already called, so just load and build)
    load_fonts_with_cyrillic();
    
    // Font texture will be rebuilt automatically on next NewFrame()
    // The fonts are already built in load_fonts_with_cyrillic()
}

bool MainWindow::init_opengl() {
#ifdef USE_OPENGL
    std::cout << "MainWindow: Initializing OpenGL..." << std::endl;

    if (SDL_GL_MakeCurrent(sdl_window_, gl_context_) != 0) {
        std::cerr << "SDL_GL_MakeCurrent Error: " << SDL_GetError() << std::endl;
        return false;
    }

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindow(sdl_window_);

    std::cout << "✓ OpenGL initialized successfully" << std::endl;
    return true;
#else
    std::cout << "MainWindow: OpenGL initialization (stub)" << std::endl;
    return true;
#endif
}

bool MainWindow::init_sdl2() {
#ifdef USE_SDL2
    std::cout << "MainWindow: Initializing SDL2..." << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    sdl_window_ = SDL_CreateWindow(
        title_.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width_, height_,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (sdl_window_ == nullptr) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        return false;
    }

    gl_context_ = SDL_GL_CreateContext(sdl_window_);
    if (gl_context_ == nullptr) {
        std::cerr << "SDL_GL_CreateContext Error: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_SetSwapInterval(1);

    std::cout << "✓ SDL2 initialized successfully" << std::endl;
    return true;
#else
    std::cerr << "SDL2 support not enabled" << std::endl;
    return false;
#endif
}

void MainWindow::cleanup_opengl() {
    // Stub implementation
}

void MainWindow::cleanup_sdl2() {
    // Stub implementation
}

void MainWindow::setup_imgui_style() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    
    // Включаем кнопку сворачивания в заголовке окна (справа)
    style.WindowMenuButtonPosition = ImGuiDir_Right;

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
}

} // namespace ui
} // namespace llama_gui
