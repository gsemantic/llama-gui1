cmake_minimum_required(VERSION 3.14)
project("llama-gui-core" CXX)

# ============================================================================
# Version Information (SemVer 2.0.0)
# ============================================================================
set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 29)
set(VERSION_SUFFIX "alpha.2")
set(VERSION_CODENAME "Archimedes")

# Full version string: 0.1.0-alpha.2 Archimedes
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}-${VERSION_SUFFIX}")
set(VERSION_FULL "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}-${VERSION_SUFFIX} ${VERSION_CODENAME}")

# Build date and time
string(TIMESTAMP BUILD_DATE "%Y-%m-%d")
string(TIMESTAMP BUILD_TIME "%H:%M:%S")
string(TIMESTAMP BUILD_YEAR "%Y")

# Git information (if available)
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
else()
    set(GIT_COMMIT_HASH "unknown")
    set(GIT_DESCRIBE "unknown")
endif()

message(STATUS "=== Llama GUI Core Build Information ===")
message(STATUS "Version: ${VERSION_FULL}")
message(STATUS "Build Date: ${BUILD_DATE} ${BUILD_TIME}")
message(STATUS "Git Commit: ${GIT_COMMIT_HASH}")
message(STATUS "Git Describe: ${GIT_DESCRIBE}")
message(STATUS "========================================")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find nlohmann_json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/deps/include)

# Source files - include all necessary files
set(SOURCES
    src/main.cpp
    src/core/llama_interface_main.cpp
    src/core/llama_interface_http.cpp
    src/core/llama_interface_streaming.cpp
    src/core/llama_interface_utils.cpp
    src/core/llama_interface_async.cpp
    src/core/state_manager.cpp
    src/core/state_manager_conversation.cpp
    src/core/state_manager_persistence.cpp
    src/core/settings_core.cpp
    src/core/settings_window.cpp
    src/core/settings_model.cpp
    src/core/settings_serialization.cpp
    src/core/settings_serialization_basic.cpp
    src/core/settings_serialization_advanced.cpp
    src/core/settings_serialization_server.cpp
    src/core/settings_ini.cpp
    src/core/settings_profiles.cpp
    src/core/settings_accessors.cpp
    src/core/settings_validation.cpp
    src/core/env_file_handler.cpp
    src/core/config_manager.cpp
    src/core/server_manager_core.cpp
    src/core/server_manager_utils.cpp
    src/core/server_manager_builders_model_gpu_batch.cpp
    src/core/server_manager_builders_sampling_cache_rope.cpp
    src/core/server_manager_builders_server_security.cpp
    src/core/model_manager.cpp
    src/core/rag_manager_core.cpp
    src/core/rag_manager_documents.cpp
    src/core/rag_manager_search.cpp
    src/core/rag_manager_embedding.cpp
    src/core/rag_manager_utils.cpp
    src/core/rag_manager_cache.cpp
    src/core/rag_manager_persistence.cpp
    src/core/rag_manager_hybrid.cpp
    src/core/rag_manager_deep_analysis.cpp
    src/core/rag_manager_model_selection.cpp
    src/core/rag_index_profile.cpp
    src/core/kv_cache_settings.cpp
    src/core/kv_cache_api_impl.cpp
    src/core/kv_cache_storage.cpp
    src/core/kv_cache_slot_manager.cpp
    src/core/kv_cache_metrics.cpp
    src/core/model_performance_stats.cpp
    src/core/chat_template_manager.cpp
    src/core/chat_cache_manager.cpp
    src/core/embedding_generator.cpp
    src/core/document_parser.cpp
    src/core/file_dialog_filters.cpp
    src/ui/main_window_init_constructor.cpp
    src/ui/main_window_init_run.cpp
    src/ui/main_window_init_sdl_gl.cpp
    src/ui/main_window_init_utils.cpp
    src/ui/main_window_events.cpp
    src/ui/main_window_rendering_core.cpp
    src/ui/main_window_rendering_debug.cpp
    src/ui/main_window_file_dialogs_core.cpp
    src/ui/main_window_file_dialogs_models.cpp
    src/ui/main_window_commands.cpp
    src/ui/main_window_conversation_files.cpp
    src/ui/main_window_model.cpp
    src/ui/main_window_model_load.cpp
#ifdef ENABLE_LLAMA_BENCH
    src/ui/main_window_llama_bench.cpp
    src/ui/llama_bench_dialog.cpp
    src/bench/profile_adapter.cpp
#endif
    src/ui/window_manager.cpp
    src/ui/chat_interface_main.cpp
    src/ui/chat_interface_rendering_core.cpp
    src/ui/chat_interface_input.cpp
    src/ui/chat_interface_streaming.cpp
    src/ui/chat_input_filter.cpp
    src/ui/chat_message_send.cpp
    src/ui/chat_text_cache.cpp
    src/ui/conversation_manager.cpp
    src/ui/file_manager.cpp
    src/ui/file_dialog_helper.cpp
    src/ui/file_browser.cpp
    src/ui/recent_files_manager.cpp
    src/ui/settings_dialog_main.cpp
    src/ui/settings_dialog_server_chat.cpp
    src/ui/settings_dialog_performance_model.cpp
    src/ui/settings_dialog_ui_files_security.cpp
    src/ui/settings_dialog_model.cpp
    src/ui/settings_dialog_gpu.cpp
    src/ui/settings_dialog_sampling_basic.cpp
    src/ui/settings_dialog_sampling_advanced.cpp
    src/ui/settings_dialog_context.cpp
    src/ui/settings_dialog_rope.cpp
    src/ui/settings_dialog_advanced.cpp
    src/ui/settings_dialog_grammar.cpp
    src/ui/settings_dialog_server_runtime.cpp
    src/ui/settings_dialog_batch.cpp
    src/ui/settings_dialog_logging.cpp
    src/ui/settings_viewer_dialog.cpp
    src/ui/command_manager.cpp
    src/ui/advanced_menu_system_core.cpp
    src/ui/advanced_menu_system_factories.cpp
    src/ui/advanced_menu_system_rendering.cpp
    src/ui/advanced_menu_system_workspace.cpp
    src/ui/dialog_manager_core.cpp
    src/ui/dialog_manager_render.cpp
    src/ui/dialog_manager_factory.cpp
    src/ui/dialog_manager_utils.cpp
    src/ui/localization_manager.cpp
    src/ui/localization_menu_translations.cpp
    src/ui/localization_settings_translations.cpp
    src/ui/localization_common_translations.cpp
    src/ui/language_selector.cpp
    src/ui/system_prompt_settings.cpp
    src/ui/rag_interface.cpp
    src/ui/rag_settings_dialog.cpp
    # Agent System UI - ОТКЛЮЧЕНО: агенты временно отключены
    # src/ui/agent_panel.cpp
    # src/ui/agent_commands.cpp
    # src/ui/agent_status_widget.cpp
    # src/ui/agent_chat_integration.cpp
    # Workspace Manager
    src/ui/workspace_manager.cpp
    # Quick Settings Dialog
    src/ui/quick_settings_dialog.cpp
    # Advanced Settings Dialog
    src/ui/advanced_settings_dialog.cpp
    # Profile Manager Dialog
    src/ui/profile_manager_dialog.cpp
    # Backup Manager Dialog
    src/ui/backup_manager_dialog.cpp
    # OpenRouter Client
    src/core/openrouter_client.cpp
    # KiloCode Client
    src/core/kilocode_client.cpp
    # Universal OpenAI Client
    src/core/universal_openai_client.cpp
    # OpenRouter Models Dialog
    src/ui/openrouter_models_dialog.cpp
    # Cloud Services Dialog
    src/ui/cloud_services_dialog.cpp
    # Universal OpenAI Dialog
    src/ui/universal_openai_dialog.cpp
    # Embedding Settings Dialog
    src/ui/embedding_settings_dialog.cpp
    # Workspace-Settings Integration
    src/core/workspace_settings_integration.cpp
    # Grid Snapping System
    src/ui/grid_snapping_system.cpp
    src/ui/grid_snapping_dialog.cpp
)

# Header files
set(HEADERS
    include/core/llama_interface.h
    include/core/llama_interface_impl.h
    include/core/state_manager.h
    include/core/state_manager_conversation.h
    include/core/state_manager_persistence.h
    include/core/settings.h
    include/core/config_manager.h
    include/core/version.h
    include/core/settings_sampling.h
    include/core/settings_model_loading.h
    include/core/settings_gpu.h
    include/core/settings_cache.h
    include/core/settings_rope.h
    include/core/settings_control_vector.h
    include/core/settings_tensor_override.h
    include/core/settings_server_runtime.h
    include/core/settings_batch.h
    include/core/settings_grammar.h
    include/core/settings_output.h
    include/core/model_manager.h
    include/core/chat_template_manager.h
    include/core/rag_manager.h
    include/core/chat_cache_manager.h
    include/core/embedding_generator.h
    include/core/document_parser.h
    include/core/file_dialog_filters.h
    include/core/rag_settings.h
    include/core/kv_cache_types.h
    include/core/kv_cache_api.h
    include/core/kv_cache_settings.h
    include/core/kv_cache_storage.h
    include/core/kv_cache_slot_manager.h
    include/core/kv_cache_metrics.h
    include/ui/main_window.h
    # include/ui/main_window_core.h              # TODO: Создать в этапе 5
    # include/ui/main_window_rendering.h         # TODO: Создать в этапе 5
    # include/ui/main_window_dialogs.h           # TODO: Создать в этапе 5
    include/ui/chat_interface.h
    # include/ui/chat_interface_rendering.h      # TODO: Создать в этапе 2
    include/ui/conversation_manager.h
    include/ui/file_manager.h
    include/ui/file_dialog_helper.h
    include/ui/file_browser.h
    include/ui/recent_files_manager.h
    include/ui/settings_dialog.h
    include/ui/settings_dialog_model.h
    include/ui/settings_dialog_gpu.h
    include/ui/settings_dialog_sampling_basic.h
    include/ui/settings_dialog_sampling_advanced.h
    include/ui/settings_dialog_context.h
    include/ui/settings_dialog_rope.h
    include/ui/settings_dialog_advanced.h
    include/ui/settings_dialog_grammar.h
    include/ui/settings_dialog_server_runtime.h
    include/ui/settings_dialog_batch.h
    include/ui/settings_dialog_logging.h
    include/ui/command.h
    include/ui/command_base.h
    include/ui/commands_file.h
    include/ui/commands_settings.h
    include/ui/commands_workspace.h
    include/ui/commands_help.h
    include/ui/command_manager.h
    include/ui/dialog_manager.h
    include/ui/localization_manager.h
    include/ui/language_selector.h
    include/ui/system_prompt_settings.h
    include/ui/rag_interface.h
    include/ui/rag_settings_dialog.h
    include/ui/embedding_settings_dialog.h
    include/ui/settings_viewer_dialog.h
    # Agent System UI
    include/ui/agent_panel.h
    include/ui/agent_status_widget.h
    include/ui/agent_commands.h
    include/ui/agent_chat_integration.h
    # Workspace Manager
    include/ui/workspace_manager.h
    # Quick Settings Dialog
    include/ui/quick_settings_dialog.h
    # Advanced Settings Dialog
    include/ui/advanced_settings_dialog.h
    # Profile Manager Dialog
    include/ui/profile_manager_dialog.h
    # Backup Manager Dialog
    include/ui/backup_manager_dialog.h
    # OpenRouter Client
    include/core/openrouter_client.h
    include/core/openrouter_types.h
    # KiloCode Client
    include/core/kilocode_client.h
    include/core/kilocode_types.h
    # Universal OpenAI Client
    include/core/universal_openai_client.h
    include/core/universal_openai_types.h
    include/core/universal_openai_settings.h
    # OpenRouter Models Dialog
    include/ui/openrouter_models_dialog.h
    # Cloud Services Dialog
    include/ui/cloud_services_dialog.h
    # Universal OpenAI Dialog
    include/ui/universal_openai_dialog.h
    # Workspace-Settings Integration
    include/core/workspace_settings_integration.h
    # Grid Snapping System
    include/ui/grid_snapping_system.h
    include/ui/grid_snapping_dialog.h
    # include/ui/window_manager.h                # TODO: Создать в этапе 4
)

# Add ImGui source files
set(IMGUI_SOURCES
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_demo.cpp
)

# Add ImGui SDL2 backend files
set(IMGUI_SDL2_SOURCES
    external/imgui/backends/imgui_impl_sdl2.cpp
)

# Add ImGui OpenGL3 backend files
set(IMGUI_OPENGL_SOURCES
    external/imgui/backends/imgui_impl_opengl3.cpp
)

# === ImGui as static library (faster builds) ===
add_library(imgui_lib STATIC ${IMGUI_SOURCES} ${IMGUI_SDL2_SOURCES} ${IMGUI_OPENGL_SOURCES})
target_include_directories(imgui_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
target_include_directories(imgui_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends)
# ==============================================

# Add ServerManager source files
set(SERVER_SOURCES
)

# Add ModelManager source files
# model_manager.cpp уже включён в SOURCES
set(MODEL_SOURCES
)

# ============================================================================
# Agent System - Ядро системы агентов
# ОТКЛЮЧЕНО: агенты временно отключены для упрощения системы конфигурации
# ============================================================================
# set(AGENT_SOURCES
#     src/agents/agent_context.cpp
#     src/agents/agent_registry.cpp
#     src/agents/plugin_loader.cpp
#     src/agents/security_manager.cpp
#     src/agents/sandbox.cpp
#     src/agents/agent_logger.cpp
#     src/agents/agent_config.cpp
# )

# set(AGENT_HEADERS
#     include/agents/i_agent.h
#     include/agents/agent_types.h
#     include/agents/agent_result.h
#     include/agents/agent_request.h
#     include/agents/agent_capabilities.h
#     include/agents/agent_context.h
#     include/agents/agent_registry.h
#     include/agents/plugin_c_api.h
#     include/agents/plugin_loader.h
#     include/agents/agents.h
#     include/agents/security_manager.h
#     include/agents/sandbox.h
#     include/agents/agent_logger.h
#     include/agents/agent_config.h
# )

# # Создаём статическую библиотеку ядра агентов
# add_library(agent_core STATIC ${AGENT_SOURCES} ${AGENT_HEADERS})
# target_include_directories(agent_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# target_link_libraries(agent_core PRIVATE nlohmann_json::nlohmann_json)

# # Добавляем dl для динамической загрузки на Linux
# if(UNIX AND NOT APPLE)
#     target_link_libraries(agent_core PRIVATE ${CMAKE_DL_LIBS})
# endif()

# # Создаём отдельную библиотеку для безопасности и песочницы (опционально)
# option(AGENT_ENABLE_SANDBOX "Enable sandbox support for agents" ON)
# if(AGENT_ENABLE_SANDBOX)
#     add_definitions(-DAGENT_ENABLE_SANDBOX)
# endif()

# Create the main executable with all sources
add_executable(llama-gui-core ${SOURCES} ${HEADERS} ${SERVER_SOURCES} ${MODEL_SOURCES})
target_link_libraries(llama-gui-core PRIVATE imgui_lib)

# ============================================================================
# Version Definitions - Передача версии в код
# ============================================================================

# Build type and compiler information
set(BUILD_TYPE "${CMAKE_BUILD_TYPE}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(COMPILER_VERSION "GCC ${CMAKE_CXX_COMPILER_VERSION}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(COMPILER_VERSION "Clang ${CMAKE_CXX_COMPILER_VERSION}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(COMPILER_VERSION "MSVC ${CMAKE_CXX_COMPILER_VERSION}")
else()
    set(COMPILER_VERSION "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
endif()

target_compile_definitions(llama-gui-core PRIVATE
    LLAMA_GUI_VERSION_MAJOR=${VERSION_MAJOR}
    LLAMA_GUI_VERSION_MINOR=${VERSION_MINOR}
    LLAMA_GUI_VERSION_PATCH=${VERSION_PATCH}
    LLAMA_GUI_VERSION_SUFFIX="${VERSION_SUFFIX}"
    LLAMA_GUI_VERSION_CODENAME="${VERSION_CODENAME}"
    LLAMA_GUI_VERSION_STRING="${VERSION_STRING}"
    LLAMA_GUI_VERSION_FULL="${VERSION_FULL}"
    BUILD_DATE="${BUILD_DATE}"
    BUILD_TIME="${BUILD_TIME}"
    BUILD_YEAR="${BUILD_YEAR}"
    GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
    GIT_DESCRIBE="${GIT_DESCRIBE}"
    BUILD_TYPE="${BUILD_TYPE}"
    COMPILER_VERSION="${COMPILER_VERSION}"
)

# Link agent core to main executable
# ОТКЛЮЧЕНО: агенты временно отключены
# target_link_libraries(llama-gui-core PRIVATE agent_core)

# ============================================================================
# Llama Bench Integration - Опциональная сборка
# ============================================================================
option(ENABLE_LLAMA_BENCH "Enable Llama Bench integration" ON)
if(ENABLE_LLAMA_BENCH)
    add_definitions(-DENABLE_LLAMA_BENCH)

    set(BENCH_SOURCES
        src/bench/llama_bench_runner.cpp
        src/bench/llama_bench_parser.cpp
        src/bench/llama_bench_results.cpp
        src/bench/llama_bench_module.cpp
    )

    set(BENCH_HEADERS
        include/bench/bench_types.h
        include/bench/bench_common.h
        include/bench/llama_bench_runner.h
        include/bench/llama_bench_parser.h
        include/bench/llama_bench_results.h
        include/bench/llama_bench_module.h
    )

    # Добавить исходники bench в основной executable
    target_sources(llama-gui-core PRIVATE ${BENCH_SOURCES} ${BENCH_HEADERS})

    # Копирование директории результатов в build
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/bench_results)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/bench_results/
             DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bench_results/)
        message(STATUS "Bench results directory copied to build directory")
    endif()

    message(STATUS "Llama Bench integration enabled")
else()
    message(STATUS "Llama Bench integration disabled")
endif()

# ============================================================================
# Plugins - Сборка плагинов
# ОТКЛЮЧЕНО: плагины отключены вместе с агентами
# ============================================================================
# option(BUILD_PLUGINS "Build agent plugins" ON)
# if(BUILD_PLUGINS)
#     add_subdirectory(plugins)
# endif()

# Find and link cURL library
find_package(CURL REQUIRED)
if(CURL_FOUND)
    target_link_libraries(llama-gui-core PRIVATE ${CURL_LIBRARIES})
    include_directories(${CURL_INCLUDE_DIRS})
endif()

# Find and link SDL2 library
find_package(SDL2 REQUIRED)
if(SDL2_FOUND)
    target_link_libraries(llama-gui-core PRIVATE SDL2::SDL2)
    include_directories(${SDL2_INCLUDE_DIRS})
    add_definitions(-DUSE_SDL2)
    message(STATUS "SDL2 support enabled")
endif()

# Find and link OpenGL library
find_package(OpenGL REQUIRED)
if(OPENGL_FOUND)
    target_link_libraries(llama-gui-core PRIVATE OpenGL::GL)
    add_definitions(-DUSE_OPENGL)
    message(STATUS "OpenGL support enabled")
endif()

# Link JSON library
if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(llama-gui-core PRIVATE nlohmann_json::nlohmann_json)
endif()

# Define USE_IMGUI for ImGui functionality
add_definitions(-DUSE_IMGUI)

# Include ImGui directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends)

# Install targets
install(TARGETS llama-gui-core RUNTIME DESTINATION bin)
install(FILES fonts/FontAwesomeSolid.otf DESTINATION bin/fonts)

# Test targets
if(BUILD_TESTS)
    # ========================================================================
    # Agent System Tests
    # ОТКЛЮЧЕНО: тесты агентов отключены вместе с агентами
    # ========================================================================
    # add_executable(test_agent_registry tests/agents/test_agent_registry.cpp)
    # target_link_libraries(test_agent_registry PRIVATE agent_core nlohmann_json::nlohmann_json)

    # add_executable(test_security_manager tests/agents/test_security_manager.cpp)
    # target_link_libraries(test_security_manager PRIVATE agent_core nlohmann_json::nlohmann_json)

    # add_executable(test_agent_commands tests/agents/test_agent_commands.cpp
    #     src/ui/agent_commands.cpp
    #     src/ui/agent_panel.cpp
    #     src/ui/agent_status_widget.cpp
    #     src/ui/agent_chat_integration.cpp)
    # target_link_libraries(test_agent_commands PRIVATE agent_core nlohmann_json::nlohmann_json imgui_lib)
    
    # ========================================================================
    # RAG Tests
    # ========================================================================
    # Note: test_rag_comprehensive.cpp and test_rag_simple.cpp need to be created
    # add_executable(test_rag_comprehensive test_rag_comprehensive.cpp ...)
    # add_executable(test_rag_simple test_rag_simple.cpp ...)

    # ========================================================================
    # KV-Cache Tests
    # ========================================================================
    add_executable(test_kv_cache_api tests/core/test_kv_cache_api.cpp
        src/core/kv_cache_metrics.cpp
        src/core/kv_cache_settings.cpp
        include/core/kv_cache_types.h
        include/core/kv_cache_metrics.h
        include/core/kv_cache_settings.h)
    target_link_libraries(test_kv_cache_api PRIVATE nlohmann_json::nlohmann_json)

    add_executable(test_kv_cache_storage tests/core/test_kv_cache_storage.cpp
        src/core/kv_cache_storage.cpp
        include/core/kv_cache_storage.h
        include/core/kv_cache_types.h)
    target_link_libraries(test_kv_cache_storage PRIVATE nlohmann_json::nlohmann_json)

    add_executable(test_kv_cache_integration tests/core/test_kv_cache_integration.cpp
        src/core/kv_cache_storage.cpp
        src/core/kv_cache_metrics.cpp
        src/core/kv_cache_settings.cpp
        include/core/kv_cache_types.h
        include/core/kv_cache_storage.h
        include/core/kv_cache_metrics.h
        include/core/kv_cache_settings.h)
    target_link_libraries(test_kv_cache_integration PRIVATE nlohmann_json::nlohmann_json)

    # ========================================================================
    # Model Performance Stats Tests
    # ========================================================================
    add_executable(test_model_performance_stats tests/core/test_model_performance_stats.cpp
        src/core/model_performance_stats.cpp
        include/core/model_performance_stats.h)
    target_link_libraries(test_model_performance_stats PRIVATE nlohmann_json::nlohmann_json)

    # Link libraries to KV-cache tests
    if(SDL2_FOUND)
        target_link_libraries(test_kv_cache_api PRIVATE SDL2::SDL2)
        target_link_libraries(test_kv_cache_integration PRIVATE SDL2::SDL2)
    endif()

    if(OPENGL_FOUND)
        target_link_libraries(test_kv_cache_api PRIVATE OpenGL::GL)
        target_link_libraries(test_kv_cache_integration PRIVATE OpenGL::GL)
    endif()

    if(FAISS_FOUND)
        target_link_libraries(test_kv_cache_integration PRIVATE ${FAISS_LIBRARIES})
    endif()
endif()

# Find and link FAISS library
# First try pkg-config, then fall back to manual find
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(FAISS faiss)
endif()

# If pkg-config fails, try to find FAISS manually
if(NOT FAISS_FOUND OR NOT FAISS_LIBRARIES)
  # Find FAISS include directory
  find_path(FAISS_INCLUDE_DIR 
    NAMES faiss/Index.h faiss/IndexFlat.h faiss/IndexIVFFlat.h
    PATHS /usr/include /usr/local/include /usr/include/faiss
  )
  
  # Find FAISS library (try shared first, then static)
  find_library(FAISS_LIBRARY 
    NAMES faiss
    PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
  )
  
  if(FAISS_INCLUDE_DIR AND FAISS_LIBRARY)
    set(FAISS_FOUND TRUE)
    set(FAISS_INCLUDE_DIRS ${FAISS_INCLUDE_DIR})
    set(FAISS_LIBRARIES ${FAISS_LIBRARY})
    message(STATUS "FAISS found manually:")
    message(STATUS "  Include: ${FAISS_INCLUDE_DIR}")
    message(STATUS "  Library: ${FAISS_LIBRARY}")
  endif()
endif()

if(FAISS_FOUND)
  # FAISS requires OpenMP and BLAS
  find_package(OpenMP REQUIRED)
  find_package(BLAS REQUIRED)

  target_link_libraries(llama-gui-core PRIVATE
    ${FAISS_LIBRARIES}
    OpenMP::OpenMP_CXX
    BLAS::BLAS
  )
  include_directories(${FAISS_INCLUDE_DIRS})
  add_definitions(-DUSE_FAISS)
  message(STATUS "FAISS support enabled")
else()
  message(WARNING "FAISS library not found. RAG functionality will be limited.")
endif()

# ============================================================================
# Llama.cpp для эмбеддингов
# ============================================================================
set(LLAMA_CPP_DIR "/home/Alex/projects/llama-b7472-bin-ubuntu-x64/llama-b7472" CACHE PATH "Path to llama.cpp")

if(EXISTS "${LLAMA_CPP_DIR}/libllama.so")
  # Используем предкомпилированные библиотеки из llama-b7472
  find_library(LLAMA_LIB llama PATHS "${LLAMA_CPP_DIR}" NO_DEFAULT_PATH)
  find_library(GGML_LIB ggml PATHS "${LLAMA_CPP_DIR}" NO_DEFAULT_PATH)
  find_library(GGML_BASE_LIB ggml-base PATHS "${LLAMA_CPP_DIR}" NO_DEFAULT_PATH)
  
  # Находим все GGML CPU бэкенды
  file(GLOB GGML_CPU_LIBS "${LLAMA_CPP_DIR}/libggml-cpu-*.so")
  
  if(LLAMA_LIB AND GGML_LIB AND GGML_BASE_LIB)
    message(STATUS "Llama.cpp libraries found:")
    message(STATUS "  libllama: ${LLAMA_LIB}")
    message(STATUS "  libggml: ${GGML_LIB}")
    message(STATUS "  libggml-base: ${GGML_BASE_LIB}")
    message(STATUS "  GGML CPU backends: ${GGML_CPU_LIBS}")
    
    # Добавляем include path
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps/include")
    
    # Линкуем все библиотеки
    target_link_libraries(llama-gui-core PRIVATE
      ${LLAMA_LIB}
      ${GGML_LIB}
      ${GGML_BASE_LIB}
      ${GGML_CPU_LIBS}
    )
    
    add_definitions(-DUSE_LLAMA_EMBEDDING)
    message(STATUS "✓ Llama.cpp embedding support enabled")
  else()
    message(WARNING "Llama.cpp libraries not found at ${LLAMA_CPP_DIR}")
    message(WARNING "Embedding generation will use stub implementation")
  endif()
else()
  message(WARNING "Llama.cpp library not found. Embeddings will use stub implementation.")
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=== Llama GUI Core Test Build ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Source files: ${SOURCES}")
message(STATUS "")

# ============================================================================
# Copy resource files to build directory
# ============================================================================

# Очистить старую директорию профилей перед копированием
# Это гарантирует, что удалённые профили не останутся в сборке
if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/profiles)
    file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/profiles)
endif()

# Copy profiles directory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/profiles)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/profiles/
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/profiles/
         PATTERN "*.gitignore" EXCLUDE)
    message(STATUS "Profiles copied to build directory")
endif()

# Copy i18n directory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/i18n)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/i18n/
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/i18n/)
    message(STATUS "i18n translations copied to build directory")
endif()

# Copy fonts directory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fonts)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/fonts/
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fonts/)
    message(STATUS "Fonts copied to build directory")
endif()

# Copy models directory if it exists (for convenience)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/models)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/models/
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/models/)
    message(STATUS "Models copied to build directory")
endif()

# Generate version.json file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.json
    @ONLY
)
message(STATUS "version.json generated to ${CMAKE_CURRENT_BINARY_DIR}/version.json")
