#include "../include/core/settings.h"
#include "../include/core/ini_parser.h"
#include <iostream>
#include <sstream>
#include <filesystem>

namespace llama_gui {
namespace core {

using IniDoc = IniParser::Document;

// =========================================================================
// Вспомогательные функции для конвертации типов
// =========================================================================

namespace {

template<typename T>
std::string to_string_impl(const T& value) {
    if constexpr (std::is_same_v<T, std::string>) {
        return value;
    } else if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_same_v<T, float>) {
        return std::to_string(value);
    } else if constexpr (std::is_same_v<T, int>) {
        return std::to_string(value);
    } else {
        return std::to_string(value);
    }
}

template<typename T>
void set_from_ini(const IniDoc& doc, const std::string& section, 
                  const std::string& key, T& value) {
    // Реализация будет специализирована ниже
}

template<>
void set_from_ini<int>(const IniDoc& doc, const std::string& section,
                       const std::string& key, int& value) {
    int val = IniParser::get_int(doc, section, key, value);
    value = val;
}

template<>
void set_from_ini<float>(const IniDoc& doc, const std::string& section,
                         const std::string& key, float& value) {
    float val = IniParser::get_float(doc, section, key, value);
    value = val;
}

template<>
void set_from_ini<bool>(const IniDoc& doc, const std::string& section,
                        const std::string& key, bool& value) {
    bool val = IniParser::get_bool(doc, section, key, value);
    value = val;
}

template<>
void set_from_ini<std::string>(const IniDoc& doc, const std::string& section,
                               const std::string& key, std::string& value) {
    std::string val = IniParser::get(doc, section, key, value);
    value = val;
}

} // anonymous namespace

// =========================================================================
// Загрузка из INI
// =========================================================================

bool Settings::load_from_ini(const std::string& file_path) {
    IniDoc doc;
    
    if (!IniParser::load(file_path, doc)) {
        std::cerr << "Failed to load INI file: " << file_path << std::endl;
        return false;
    }

    std::cout << "Loading settings from INI: " << file_path << std::endl;

    // =========================================================================
    // [DISPLAY]
    // =========================================================================
    set_from_ini(doc, "display", "window_width", display_settings_.window_width);
    set_from_ini(doc, "display", "window_height", display_settings_.window_height);
    set_from_ini(doc, "display", "window_maximized", display_settings_.window_maximized);
    set_from_ini(doc, "display", "window_x", display_settings_.window_x);
    set_from_ini(doc, "display", "window_y", display_settings_.window_y);
    set_from_ini(doc, "display", "use_dark_theme", display_settings_.use_dark_theme);
    set_from_ini(doc, "display", "font_size", display_settings_.font_size);
    set_from_ini(doc, "display", "font_family", display_settings_.font_family);
    set_from_ini(doc, "display", "enable_animation", display_settings_.enable_animation);
    set_from_ini(doc, "display", "frame_rate_limit", display_settings_.frame_rate_limit);
    set_from_ini(doc, "display", "screen_width", display_settings_.screen_width);
    set_from_ini(doc, "display", "screen_height", display_settings_.screen_height);
    set_from_ini(doc, "display", "dpi_scale", display_settings_.dpi_scale);
    set_from_ini(doc, "display", "auto_resize", display_settings_.auto_resize);
    set_from_ini(doc, "display", "min_window_width", display_settings_.min_window_width);
    set_from_ini(doc, "display", "min_window_height", display_settings_.min_window_height);
    set_from_ini(doc, "display", "center_window", display_settings_.center_window);
    set_from_ini(doc, "display", "margin", display_settings_.margin);

    // =========================================================================
    // [PERFORMANCE]
    // =========================================================================
    set_from_ini(doc, "performance", "enable_vsync", performance_settings_.enable_vsync);
    set_from_ini(doc, "performance", "target_fps", performance_settings_.target_fps);
    set_from_ini(doc, "performance", "idle_fps", performance_settings_.idle_fps);
    set_from_ini(doc, "performance", "idle_timeout_ms", performance_settings_.idle_timeout_ms);
    set_from_ini(doc, "performance", "enable_smart_redraw", performance_settings_.enable_smart_redraw);
    set_from_ini(doc, "performance", "show_performance_overlay", performance_settings_.show_performance_overlay);
    set_from_ini(doc, "performance", "performance_update_interval_ms", performance_settings_.performance_update_interval_ms);
    set_from_ini(doc, "performance", "enable_logging", performance_settings_.enable_logging);
    set_from_ini(doc, "performance", "log_level", performance_settings_.log_level);
    set_from_ini(doc, "performance", "log_to_file", performance_settings_.log_to_file);
    set_from_ini(doc, "performance", "log_file_path", performance_settings_.log_file_path);
    set_from_ini(doc, "performance", "log_flush_policy", performance_settings_.log_flush_policy);
    set_from_ini(doc, "performance", "debug_mode", performance_settings_.debug_mode);

    // =========================================================================
    // [SERVER]
    // =========================================================================
    set_from_ini(doc, "server", "host", server_settings_.host);
    set_from_ini(doc, "server", "port", server_settings_.port);
    set_from_ini(doc, "server", "api_url", server_settings_.api_url);
    set_from_ini(doc, "server", "connection_timeout", server_settings_.connection_timeout);
    set_from_ini(doc, "server", "request_timeout", server_settings_.request_timeout);
    set_from_ini(doc, "server", "max_retries", server_settings_.max_retries);
    set_from_ini(doc, "server", "verify_ssl", server_settings_.verify_ssl);
    set_from_ini(doc, "server", "auth_token", server_settings_.auth_token);

    // =========================================================================
    // [CHAT]
    // =========================================================================
    set_from_ini(doc, "chat", "auto_scroll", chat_settings_.auto_scroll);
    set_from_ini(doc, "chat", "max_messages_display", chat_settings_.max_messages_display);
    set_from_ini(doc, "chat", "show_timestamps", chat_settings_.show_timestamps);
    set_from_ini(doc, "chat", "show_system_messages", chat_settings_.show_system_messages);
    set_from_ini(doc, "chat", "preserve_formatting", chat_settings_.preserve_formatting);
    set_from_ini(doc, "chat", "default_system_prompt", chat_settings_.default_system_prompt);
    set_from_ini(doc, "chat", "max_tokens", chat_settings_.max_tokens);
    set_from_ini(doc, "chat", "temperature", chat_settings_.temperature);
    set_from_ini(doc, "chat", "top_p", chat_settings_.top_p);
    set_from_ini(doc, "chat", "top_k", chat_settings_.top_k);
    set_from_ini(doc, "chat", "min_p", chat_settings_.min_p);
    set_from_ini(doc, "chat", "repeat_penalty", chat_settings_.repeat_penalty);
    set_from_ini(doc, "chat", "presence_penalty", chat_settings_.presence_penalty);
    set_from_ini(doc, "chat", "frequency_penalty", chat_settings_.frequency_penalty);
    set_from_ini(doc, "chat", "mirostat_mode", chat_settings_.mirostat_mode);
    set_from_ini(doc, "chat", "mirostat_tau", chat_settings_.mirostat_tau);
    set_from_ini(doc, "chat", "mirostat_eta", chat_settings_.mirostat_eta);
    set_from_ini(doc, "chat", "stop_on_newline", chat_settings_.stop_on_newline);
    set_from_ini(doc, "chat", "threads", chat_settings_.threads);
    set_from_ini(doc, "chat", "n_ctx", chat_settings_.n_ctx);
    set_from_ini(doc, "chat", "seed", chat_settings_.seed);
    set_from_ini(doc, "chat", "tfs_z", chat_settings_.tfs_z);
    set_from_ini(doc, "chat", "typical_p", chat_settings_.typical_p);
    set_from_ini(doc, "chat", "n_gpu_layers", chat_settings_.n_gpu_layers);
    set_from_ini(doc, "chat", "tensor_split", chat_settings_.tensor_split);
    set_from_ini(doc, "chat", "numa", chat_settings_.numa);
    set_from_ini(doc, "chat", "lora_base", chat_settings_.lora_base);
    set_from_ini(doc, "chat", "mmproj", chat_settings_.mmproj);
    set_from_ini(doc, "chat", "grammar", chat_settings_.grammar);
    set_from_ini(doc, "chat", "chat_template", chat_settings_.chat_template);
    set_from_ini(doc, "chat", "embedding", chat_settings_.embedding);
    set_from_ini(doc, "chat", "log_format", chat_settings_.log_format);
    set_from_ini(doc, "chat", "verbosity", chat_settings_.verbosity);

    // =========================================================================
    // [FILES]
    // =========================================================================
    set_from_ini(doc, "files", "default_save_path", file_settings_.default_save_path);
    set_from_ini(doc, "files", "default_export_path", file_settings_.default_export_path);
    set_from_ini(doc, "files", "auto_save_path", file_settings_.auto_save_path);
    set_from_ini(doc, "files", "auto_save_enabled", file_settings_.auto_save_enabled);
    set_from_ini(doc, "files", "auto_save_interval", file_settings_.auto_save_interval);
    set_from_ini(doc, "files", "max_file_size", file_settings_.max_file_size);

    // =========================================================================
    // [RAG]
    // =========================================================================
    set_from_ini(doc, "rag", "embedding_model_path", rag_settings_.embedding_model_path);
    set_from_ini(doc, "rag", "max_chunks_in_memory", rag_settings_.max_chunks_in_memory);
    set_from_ini(doc, "rag", "similarity_threshold", rag_settings_.similarity_threshold);
    set_from_ini(doc, "rag", "max_embedding_cache_size", rag_settings_.max_embedding_cache_size);
    set_from_ini(doc, "rag", "embedding_dimension", rag_settings_.embedding_dimension);
    set_from_ini(doc, "rag", "max_sequence_length", rag_settings_.max_sequence_length);
    set_from_ini(doc, "rag", "max_tokens_per_chunk", rag_settings_.max_tokens_per_chunk);
    set_from_ini(doc, "rag", "search_k", rag_settings_.search_k);
    set_from_ini(doc, "rag", "mmr_lambda", rag_settings_.mmr_lambda);
    set_from_ini(doc, "rag", "enable_mmr", rag_settings_.enable_mmr);
    set_from_ini(doc, "rag", "enable_rag", rag_settings_.enable_rag);
    set_from_ini(doc, "rag", "enable_caching", rag_settings_.enable_caching);
    int rag_mode_val = IniParser::get_int(doc, "rag", "rag_mode", static_cast<int>(rag_settings_.rag_mode));
    rag_settings_.rag_mode = static_cast<RagMode>(rag_mode_val);

    // Параметры гибридного поиска
    set_from_ini(doc, "rag", "enable_hybrid_search", rag_settings_.enable_hybrid_search);
    set_from_ini(doc, "rag", "keyword_boost_weight", rag_settings_.keyword_boost_weight);
    set_from_ini(doc, "rag", "enable_query_expansion", rag_settings_.enable_query_expansion);

    // Настройки глубокого анализа документа
    set_from_ini(doc, "rag", "deep_analysis_mode", rag_settings_.deep_analysis.mode);
    set_from_ini(doc, "rag", "deep_analysis_chunks_per_batch", rag_settings_.deep_analysis.chunks_per_batch);
    set_from_ini(doc, "rag", "deep_analysis_max_iterations", rag_settings_.deep_analysis.max_iterations);
    set_from_ini(doc, "rag", "deep_analysis_enable_progressive_summary", rag_settings_.deep_analysis.enable_progressive_summary);
    set_from_ini(doc, "rag", "deep_analysis_final_synthesis_chunks", rag_settings_.deep_analysis.final_synthesis_chunks);
    set_from_ini(doc, "rag", "deep_analysis_auto_adjust_context_size", rag_settings_.deep_analysis.auto_adjust_context_size);
    set_from_ini(doc, "rag", "deep_analysis_target_context_size", rag_settings_.deep_analysis.target_context_size);

    // =========================================================================
    // [SAMPLING]
    // =========================================================================
    set_from_ini(doc, "sampling", "temperature", sampling_settings_.temperature);
    set_from_ini(doc, "sampling", "top_k", sampling_settings_.top_k);
    set_from_ini(doc, "sampling", "top_p", sampling_settings_.top_p);
    set_from_ini(doc, "sampling", "min_p", sampling_settings_.min_p);
    set_from_ini(doc, "sampling", "typical_p", sampling_settings_.typical_p);
    set_from_ini(doc, "sampling", "tfs_z", sampling_settings_.tfs_z);
    set_from_ini(doc, "sampling", "xtc_probability", sampling_settings_.xtc_probability);
    set_from_ini(doc, "sampling", "xtc_threshold", sampling_settings_.xtc_threshold);
    set_from_ini(doc, "sampling", "dry_multiplier", sampling_settings_.dry_multiplier);
    set_from_ini(doc, "sampling", "dry_base", sampling_settings_.dry_base);
    set_from_ini(doc, "sampling", "dry_allowed_length", sampling_settings_.dry_allowed_length);
    set_from_ini(doc, "sampling", "dry_penalty_last_n", sampling_settings_.dry_penalty_last_n);
    set_from_ini(doc, "sampling", "dynatemp_range", sampling_settings_.dynatemp_range);
    set_from_ini(doc, "sampling", "dynatemp_exp", sampling_settings_.dynatemp_exp);
    set_from_ini(doc, "sampling", "repeat_penalty", sampling_settings_.repeat_penalty);
    set_from_ini(doc, "sampling", "presence_penalty", sampling_settings_.presence_penalty);
    set_from_ini(doc, "sampling", "frequency_penalty", sampling_settings_.frequency_penalty);
    set_from_ini(doc, "sampling", "repeat_last_n", sampling_settings_.repeat_last_n);
    set_from_ini(doc, "sampling", "penalize_nl", sampling_settings_.penalize_nl);
    set_from_ini(doc, "sampling", "ignore_eos", sampling_settings_.ignore_eos);
    set_from_ini(doc, "sampling", "mirostat_mode", sampling_settings_.mirostat_mode);
    set_from_ini(doc, "sampling", "mirostat_tau", sampling_settings_.mirostat_tau);
    set_from_ini(doc, "sampling", "mirostat_eta", sampling_settings_.mirostat_eta);
    set_from_ini(doc, "sampling", "samplers_order", sampling_settings_.samplers_order);
    set_from_ini(doc, "sampling", "use_custom_sampler_order", sampling_settings_.use_custom_sampler_order);

    // =========================================================================
    // [MODEL_LOADING]
    // =========================================================================
    set_from_ini(doc, "model_loading", "model_path", model_loading_settings_.model_path);
    set_from_ini(doc, "model_loading", "model_url", model_loading_settings_.model_url);
    set_from_ini(doc, "model_loading", "hf_repo", model_loading_settings_.hf_repo);
    set_from_ini(doc, "model_loading", "hf_file", model_loading_settings_.hf_file);
    set_from_ini(doc, "model_loading", "hf_token", model_loading_settings_.hf_token);
    set_from_ini(doc, "model_loading", "model_alias", model_loading_settings_.model_alias);
    set_from_ini(doc, "model_loading", "model_draft", model_loading_settings_.model_draft);
    set_from_ini(doc, "model_loading", "hf_repo_draft", model_loading_settings_.hf_repo_draft);
    set_from_ini(doc, "model_loading", "draft_max", model_loading_settings_.draft_max);
    set_from_ini(doc, "model_loading", "draft_min", model_loading_settings_.draft_min);
    set_from_ini(doc, "model_loading", "draft_p_min", model_loading_settings_.draft_p_min);
    set_from_ini(doc, "model_loading", "model_vocoder", model_loading_settings_.model_vocoder);
    set_from_ini(doc, "model_loading", "hf_repo_vocoder", model_loading_settings_.hf_repo_vocoder);
    set_from_ini(doc, "model_loading", "hf_file_vocoder", model_loading_settings_.hf_file_vocoder);
    set_from_ini(doc, "model_loading", "lora_base", model_loading_settings_.lora_base);
    set_from_ini(doc, "model_loading", "lora_init_without_apply", model_loading_settings_.lora_init_without_apply);
    set_from_ini(doc, "model_loading", "mmproj", model_loading_settings_.mmproj);
    set_from_ini(doc, "model_loading", "mmproj_url", model_loading_settings_.mmproj_url);
    set_from_ini(doc, "model_loading", "no_mmproj", model_loading_settings_.no_mmproj);
    set_from_ini(doc, "model_loading", "no_mmproj_offload", model_loading_settings_.no_mmproj_offload);
    set_from_ini(doc, "model_loading", "check_tensors", model_loading_settings_.check_tensors);
    set_from_ini(doc, "model_loading", "device", model_loading_settings_.device);
    set_from_ini(doc, "model_loading", "device_draft", model_loading_settings_.device_draft);
    set_from_ini(doc, "model_loading", "list_devices", model_loading_settings_.list_devices);

    // =========================================================================
    // [GPU]
    // =========================================================================
    set_from_ini(doc, "gpu", "n_gpu_layers", gpu_settings_.n_gpu_layers);
    set_from_ini(doc, "gpu", "n_gpu_layers_draft", gpu_settings_.n_gpu_layers_draft);
    
    std::string split_mode_str = IniParser::get(doc, "gpu", "split_mode", "layer");
    gpu_settings_.set_split_mode(split_mode_str);
    
    set_from_ini(doc, "gpu", "tensor_split", gpu_settings_.tensor_split);
    set_from_ini(doc, "gpu", "main_gpu", gpu_settings_.main_gpu);
    set_from_ini(doc, "gpu", "no_op_offload", gpu_settings_.no_op_offload);
    set_from_ini(doc, "gpu", "no_kv_offload", gpu_settings_.no_kv_offload);
    set_from_ini(doc, "gpu", "no_warmup", gpu_settings_.no_warmup);
    set_from_ini(doc, "gpu", "mlock", gpu_settings_.mlock);
    set_from_ini(doc, "gpu", "no_mmap", gpu_settings_.no_mmap);
    
    int flash_attn_val = IniParser::get_int(doc, "gpu", "flash_attn", 0);
    if (flash_attn_val == 0) {
        gpu_settings_.flash_attn = GPUSettings::FlashAttention::Auto;
    } else if (flash_attn_val == 1) {
        gpu_settings_.flash_attn = GPUSettings::FlashAttention::Enabled;
    } else {
        gpu_settings_.flash_attn = GPUSettings::FlashAttention::Disabled;
    }
    
    set_from_ini(doc, "gpu", "defrag_thold", gpu_settings_.defrag_thold);

    // =========================================================================
    // [CACHE]
    // =========================================================================
    std::string cache_type_k_str = IniParser::get(doc, "cache", "cache_type_k", "f16");
    cache_settings_.cache_type_k = CacheSettings::cache_type_from_string(cache_type_k_str);
    
    std::string cache_type_v_str = IniParser::get(doc, "cache", "cache_type_v", "f16");
    cache_settings_.cache_type_v = CacheSettings::cache_type_from_string(cache_type_v_str);
    
    std::string cache_type_k_draft_str = IniParser::get(doc, "cache", "cache_type_k_draft", "f16");
    cache_settings_.cache_type_k_draft = CacheSettings::cache_type_from_string(cache_type_k_draft_str);
    
    std::string cache_type_v_draft_str = IniParser::get(doc, "cache", "cache_type_v_draft", "f16");
    cache_settings_.cache_type_v_draft = CacheSettings::cache_type_from_string(cache_type_v_draft_str);
    
    set_from_ini(doc, "cache", "cache_prompt", cache_settings_.cache_prompt);
    set_from_ini(doc, "cache", "cache_reuse", cache_settings_.cache_reuse);
    set_from_ini(doc, "cache", "swa_full", cache_settings_.swa_full);
    set_from_ini(doc, "cache", "no_context_shift", cache_settings_.no_context_shift);
    set_from_ini(doc, "cache", "slot_save_path", cache_settings_.slot_save_path);
    set_from_ini(doc, "cache", "slot_prompt_similarity", cache_settings_.slot_prompt_similarity);
    set_from_ini(doc, "cache", "slots_endpoint_enabled", cache_settings_.slots_endpoint_enabled);

    // =========================================================================
    // [ROPE]
    // =========================================================================
    std::string rope_scaling_str = IniParser::get(doc, "rope", "rope_scaling", "linear");
    rope_settings_.set_scaling(rope_scaling_str);
    
    set_from_ini(doc, "rope", "rope_scale", rope_settings_.rope_scale);
    set_from_ini(doc, "rope", "rope_freq_base", rope_settings_.rope_freq_base);
    set_from_ini(doc, "rope", "rope_freq_scale", rope_settings_.rope_freq_scale);
    set_from_ini(doc, "rope", "yarn_orig_ctx", rope_settings_.yarn_orig_ctx);
    set_from_ini(doc, "rope", "yarn_ext_factor", rope_settings_.yarn_ext_factor);
    set_from_ini(doc, "rope", "yarn_attn_factor", rope_settings_.yarn_attn_factor);
    set_from_ini(doc, "rope", "yarn_beta_slow", rope_settings_.yarn_beta_slow);
    set_from_ini(doc, "rope", "yarn_beta_fast", rope_settings_.yarn_beta_fast);

    // =========================================================================
    // [CONTROL_VECTOR]
    // =========================================================================
    set_from_ini(doc, "control_vector", "control_vector_layer_start", control_vector_settings_.control_vector_layer_start);
    set_from_ini(doc, "control_vector", "control_vector_layer_end", control_vector_settings_.control_vector_layer_end);

    // =========================================================================
    // [TENSOR_OVERRIDE]
    // =========================================================================
    // Переопределения тензоров загружаются из JSON профиля (сложная структура)

    // =========================================================================
    // [SERVER_RUNTIME]
    // =========================================================================
    set_from_ini(doc, "server_runtime", "host", server_runtime_settings_.host);
    set_from_ini(doc, "server_runtime", "port", server_runtime_settings_.port);
    set_from_ini(doc, "server_runtime", "timeout", server_runtime_settings_.timeout);
    set_from_ini(doc, "server_runtime", "api_prefix", server_runtime_settings_.api_prefix);
    set_from_ini(doc, "server_runtime", "offline", server_runtime_settings_.offline);
    set_from_ini(doc, "server_runtime", "threads_http", server_runtime_settings_.threads_http);
    set_from_ini(doc, "server_runtime", "static_path", server_runtime_settings_.static_path);
    set_from_ini(doc, "server_runtime", "no_webui", server_runtime_settings_.no_webui);
    set_from_ini(doc, "server_runtime", "api_key_file", server_runtime_settings_.api_key_file);
    set_from_ini(doc, "server_runtime", "ssl_key_file", server_runtime_settings_.ssl_key_file);
    set_from_ini(doc, "server_runtime", "ssl_cert_file", server_runtime_settings_.ssl_cert_file);
    set_from_ini(doc, "server_runtime", "embeddings_mode", server_runtime_settings_.embeddings_mode);
    set_from_ini(doc, "server_runtime", "reranking_mode", server_runtime_settings_.reranking_mode);
    set_from_ini(doc, "server_runtime", "metrics_enabled", server_runtime_settings_.metrics_enabled);
    set_from_ini(doc, "server_runtime", "slot_save_path", server_runtime_settings_.slot_save_path);
    set_from_ini(doc, "server_runtime", "n_parallel", server_runtime_settings_.n_parallel);
    set_from_ini(doc, "server_runtime", "cache_type_k", server_runtime_settings_.cache_type_k);
    set_from_ini(doc, "server_runtime", "cache_type_v", server_runtime_settings_.cache_type_v);
    set_from_ini(doc, "server_runtime", "cache_reuse", server_runtime_settings_.cache_reuse);
    set_from_ini(doc, "server_runtime", "log_disabled", server_runtime_settings_.log_disabled);
    set_from_ini(doc, "server_runtime", "log_file", server_runtime_settings_.log_file);
    set_from_ini(doc, "server_runtime", "log_colors", server_runtime_settings_.log_colors);
    set_from_ini(doc, "server_runtime", "log_verbose", server_runtime_settings_.log_verbose);
    set_from_ini(doc, "server_runtime", "log_verbosity", server_runtime_settings_.log_verbosity);
    set_from_ini(doc, "server_runtime", "log_format", server_runtime_settings_.log_format);
    set_from_ini(doc, "server_runtime", "log_prefix", server_runtime_settings_.log_prefix);
    set_from_ini(doc, "server_runtime", "log_timestamps", server_runtime_settings_.log_timestamps);

    // =========================================================================
    // [BATCH]
    // =========================================================================
    set_from_ini(doc, "batch", "batch_size", batch_settings_.batch_size);
    set_from_ini(doc, "batch", "ubatch_size", batch_settings_.ubatch_size);
    set_from_ini(doc, "batch", "ctx_size", batch_settings_.ctx_size);
    set_from_ini(doc, "batch", "ctx_size_draft", batch_settings_.ctx_size_draft);
    set_from_ini(doc, "batch", "threads", batch_settings_.threads);
    set_from_ini(doc, "batch", "threads_batch", batch_settings_.threads_batch);
    set_from_ini(doc, "batch", "cpu_mask", batch_settings_.cpu_mask);
    set_from_ini(doc, "batch", "cpu_range", batch_settings_.cpu_range);
    set_from_ini(doc, "batch", "cpu_strict", batch_settings_.cpu_strict);
    set_from_ini(doc, "batch", "priority", batch_settings_.priority);
    set_from_ini(doc, "batch", "poll_level", batch_settings_.poll_level);
    set_from_ini(doc, "batch", "cpu_mask_batch", batch_settings_.cpu_mask_batch);
    set_from_ini(doc, "batch", "cpu_range_batch", batch_settings_.cpu_range_batch);
    set_from_ini(doc, "batch", "cpu_strict_batch", batch_settings_.cpu_strict_batch);
    set_from_ini(doc, "batch", "priority_batch", batch_settings_.priority_batch);
    set_from_ini(doc, "batch", "poll_batch", batch_settings_.poll_batch);
    set_from_ini(doc, "batch", "cont_batching", batch_settings_.cont_batching);
    set_from_ini(doc, "batch", "no_perf", batch_settings_.no_perf);

    // =========================================================================
    // [GRAMMAR]
    // =========================================================================
    set_from_ini(doc, "grammar", "grammar", grammar_settings_.grammar);
    set_from_ini(doc, "grammar", "grammar_file", grammar_settings_.grammar_file);
    set_from_ini(doc, "grammar", "json_schema", grammar_settings_.json_schema);
    set_from_ini(doc, "grammar", "json_schema_file", grammar_settings_.json_schema_file);
    set_from_ini(doc, "grammar", "chat_template", grammar_settings_.chat_template);
    set_from_ini(doc, "grammar", "chat_template_file", grammar_settings_.chat_template_file);
    set_from_ini(doc, "grammar", "chat_template_kwargs", grammar_settings_.chat_template_kwargs);
    set_from_ini(doc, "grammar", "use_jinja", grammar_settings_.use_jinja);
    set_from_ini(doc, "grammar", "no_prefill_assistant", grammar_settings_.no_prefill_assistant);
    set_from_ini(doc, "grammar", "system_prompt_file", grammar_settings_.system_prompt_file);
    set_from_ini(doc, "grammar", "default_system_prompt", grammar_settings_.default_system_prompt);
    
    std::string reasoning_format_str = IniParser::get(doc, "grammar", "reasoning_format", "none");
    if (reasoning_format_str == "deepseek") {
        grammar_settings_.reasoning_format = GrammarSettings::ReasoningFormat::Deepseek;
    } else {
        grammar_settings_.reasoning_format = GrammarSettings::ReasoningFormat::None;
    }
    
    set_from_ini(doc, "grammar", "reasoning_budget", grammar_settings_.reasoning_budget);

    // =========================================================================
    // [OUTPUT]
    // =========================================================================
    set_from_ini(doc, "output", "n_predict", output_settings_.n_predict);
    set_from_ini(doc, "output", "keep", output_settings_.keep);
    set_from_ini(doc, "output", "special_tokens", output_settings_.special_tokens);
    set_from_ini(doc, "output", "spm_infill", output_settings_.spm_infill);
    set_from_ini(doc, "output", "verbose_prompt", output_settings_.verbose_prompt);
    set_from_ini(doc, "output", "escape_sequences", output_settings_.escape_sequences);
    set_from_ini(doc, "output", "tts_use_guide_tokens", output_settings_.tts_use_guide_tokens);
    set_from_ini(doc, "output", "pooling_type", output_settings_.pooling_type);

    // =========================================================================
    // [CUSTOM]
    // =========================================================================
    // Загрузка пользовательских настроек будет реализована отдельно

    std::cout << "Settings successfully loaded from INI file" << std::endl;

    // Синхронизируем n_ctx и ctx_size после загрузки из INI
    std::cout << "load_from_ini: Перед синхронизацией n_ctx=" << chat_settings_.n_ctx 
              << ", ctx_size=" << batch_settings_.ctx_size << std::endl;
    sync_ctx_size();
    std::cout << "load_from_ini: После sync_ctx_size n_ctx=" << chat_settings_.n_ctx 
              << ", ctx_size=" << batch_settings_.ctx_size << std::endl;

    // Синхронизируем max_tokens и n_predict
    sync_max_tokens();

    // Сохраняем синхронизированные значения обратно в INI-файл
    std::cout << "load_from_ini: Вызов save_to_ini для сохранения синхронизированных значений" << std::endl;
    save_to_ini(file_path);

    return true;
}

// =========================================================================
// Сохранение в INI
// =========================================================================

bool Settings::save_to_ini(const std::string& file_path) const {
    IniDoc doc;

    std::cout << "save_to_ini: Сохранение в файл: " << file_path << std::endl;
    std::cout << "save_to_ini: chat.n_ctx = " << chat_settings_.n_ctx 
              << ", batch.ctx_size = " << batch_settings_.ctx_size << std::endl;

    // =========================================================================
    // [DISPLAY]
    // =========================================================================
    IniParser::set(doc, "display", "window_width", std::to_string(display_settings_.window_width));
    IniParser::set(doc, "display", "window_height", std::to_string(display_settings_.window_height));
    IniParser::set(doc, "display", "window_maximized", display_settings_.window_maximized ? "true" : "false");
    IniParser::set(doc, "display", "window_x", std::to_string(display_settings_.window_x));
    IniParser::set(doc, "display", "window_y", std::to_string(display_settings_.window_y));
    IniParser::set(doc, "display", "use_dark_theme", display_settings_.use_dark_theme ? "true" : "false");
    IniParser::set(doc, "display", "font_size", std::to_string(display_settings_.font_size));
    IniParser::set(doc, "display", "font_family", display_settings_.font_family);
    IniParser::set(doc, "display", "enable_animation", display_settings_.enable_animation ? "true" : "false");
    IniParser::set(doc, "display", "frame_rate_limit", std::to_string(display_settings_.frame_rate_limit));
    IniParser::set(doc, "display", "screen_width", std::to_string(display_settings_.screen_width));
    IniParser::set(doc, "display", "screen_height", std::to_string(display_settings_.screen_height));
    IniParser::set(doc, "display", "dpi_scale", std::to_string(display_settings_.dpi_scale));
    IniParser::set(doc, "display", "auto_resize", display_settings_.auto_resize ? "true" : "false");
    IniParser::set(doc, "display", "min_window_width", std::to_string(display_settings_.min_window_width));
    IniParser::set(doc, "display", "min_window_height", std::to_string(display_settings_.min_window_height));
    IniParser::set(doc, "display", "center_window", display_settings_.center_window ? "true" : "false");
    IniParser::set(doc, "display", "margin", std::to_string(display_settings_.margin));

    // =========================================================================
    // [PERFORMANCE]
    // =========================================================================
    IniParser::set(doc, "performance", "enable_vsync", performance_settings_.enable_vsync ? "true" : "false");
    IniParser::set(doc, "performance", "target_fps", std::to_string(performance_settings_.target_fps));
    IniParser::set(doc, "performance", "idle_fps", std::to_string(performance_settings_.idle_fps));
    IniParser::set(doc, "performance", "idle_timeout_ms", std::to_string(performance_settings_.idle_timeout_ms));
    IniParser::set(doc, "performance", "enable_smart_redraw", performance_settings_.enable_smart_redraw ? "true" : "false");
    IniParser::set(doc, "performance", "show_performance_overlay", performance_settings_.show_performance_overlay ? "true" : "false");
    IniParser::set(doc, "performance", "performance_update_interval_ms", std::to_string(performance_settings_.performance_update_interval_ms));
    IniParser::set(doc, "performance", "enable_logging", performance_settings_.enable_logging ? "true" : "false");
    IniParser::set(doc, "performance", "log_level", performance_settings_.log_level);
    IniParser::set(doc, "performance", "log_to_file", performance_settings_.log_to_file ? "true" : "false");
    IniParser::set(doc, "performance", "log_file_path", performance_settings_.log_file_path);
    IniParser::set(doc, "performance", "log_flush_policy", performance_settings_.log_flush_policy);
    IniParser::set(doc, "performance", "debug_mode", performance_settings_.debug_mode ? "true" : "false");

    // =========================================================================
    // [SERVER]
    // =========================================================================
    IniParser::set(doc, "server", "host", server_settings_.host);
    IniParser::set(doc, "server", "port", std::to_string(server_settings_.port));
    IniParser::set(doc, "server", "api_url", server_settings_.api_url);
    IniParser::set(doc, "server", "connection_timeout", std::to_string(server_settings_.connection_timeout));
    IniParser::set(doc, "server", "request_timeout", std::to_string(server_settings_.request_timeout));
    IniParser::set(doc, "server", "max_retries", std::to_string(server_settings_.max_retries));
    IniParser::set(doc, "server", "verify_ssl", server_settings_.verify_ssl ? "true" : "false");
    IniParser::set(doc, "server", "auth_token", server_settings_.auth_token);

    // =========================================================================
    // [CHAT]
    // =========================================================================
    IniParser::set(doc, "chat", "auto_scroll", chat_settings_.auto_scroll ? "true" : "false");
    IniParser::set(doc, "chat", "max_messages_display", std::to_string(chat_settings_.max_messages_display));
    IniParser::set(doc, "chat", "show_timestamps", chat_settings_.show_timestamps ? "true" : "false");
    IniParser::set(doc, "chat", "show_system_messages", chat_settings_.show_system_messages ? "true" : "false");
    IniParser::set(doc, "chat", "preserve_formatting", chat_settings_.preserve_formatting ? "true" : "false");
    IniParser::set(doc, "chat", "default_system_prompt", chat_settings_.default_system_prompt);
    IniParser::set(doc, "chat", "max_tokens", std::to_string(chat_settings_.max_tokens));
    IniParser::set(doc, "chat", "temperature", std::to_string(chat_settings_.temperature));
    IniParser::set(doc, "chat", "top_p", std::to_string(chat_settings_.top_p));
    IniParser::set(doc, "chat", "top_k", std::to_string(chat_settings_.top_k));
    IniParser::set(doc, "chat", "min_p", std::to_string(chat_settings_.min_p));
    IniParser::set(doc, "chat", "repeat_penalty", std::to_string(chat_settings_.repeat_penalty));
    IniParser::set(doc, "chat", "presence_penalty", std::to_string(chat_settings_.presence_penalty));
    IniParser::set(doc, "chat", "frequency_penalty", std::to_string(chat_settings_.frequency_penalty));
    IniParser::set(doc, "chat", "mirostat_mode", std::to_string(chat_settings_.mirostat_mode));
    IniParser::set(doc, "chat", "mirostat_tau", std::to_string(chat_settings_.mirostat_tau));
    IniParser::set(doc, "chat", "mirostat_eta", std::to_string(chat_settings_.mirostat_eta));
    IniParser::set(doc, "chat", "stop_on_newline", chat_settings_.stop_on_newline ? "true" : "false");
    IniParser::set(doc, "chat", "threads", std::to_string(chat_settings_.threads));
    IniParser::set(doc, "chat", "n_ctx", std::to_string(chat_settings_.n_ctx));
    IniParser::set(doc, "chat", "seed", std::to_string(chat_settings_.seed));
    IniParser::set(doc, "chat", "tfs_z", std::to_string(chat_settings_.tfs_z));
    IniParser::set(doc, "chat", "typical_p", std::to_string(chat_settings_.typical_p));
    IniParser::set(doc, "chat", "n_gpu_layers", std::to_string(chat_settings_.n_gpu_layers));
    IniParser::set(doc, "chat", "tensor_split", chat_settings_.tensor_split);
    IniParser::set(doc, "chat", "numa", chat_settings_.numa);
    IniParser::set(doc, "chat", "lora_base", chat_settings_.lora_base);
    IniParser::set(doc, "chat", "mmproj", chat_settings_.mmproj);
    IniParser::set(doc, "chat", "grammar", chat_settings_.grammar);
    IniParser::set(doc, "chat", "chat_template", chat_settings_.chat_template);
    IniParser::set(doc, "chat", "embedding", chat_settings_.embedding ? "true" : "false");
    IniParser::set(doc, "chat", "log_format", chat_settings_.log_format);
    IniParser::set(doc, "chat", "verbosity", std::to_string(chat_settings_.verbosity));

    // =========================================================================
    // [FILES]
    // =========================================================================
    IniParser::set(doc, "files", "default_save_path", file_settings_.default_save_path);
    IniParser::set(doc, "files", "default_export_path", file_settings_.default_export_path);
    IniParser::set(doc, "files", "auto_save_path", file_settings_.auto_save_path);
    IniParser::set(doc, "files", "auto_save_enabled", file_settings_.auto_save_enabled ? "true" : "false");
    IniParser::set(doc, "files", "auto_save_interval", std::to_string(file_settings_.auto_save_interval));
    IniParser::set(doc, "files", "max_file_size", std::to_string(file_settings_.max_file_size));

    // =========================================================================
    // [RAG]
    // =========================================================================
    IniParser::set(doc, "rag", "embedding_model_path", rag_settings_.embedding_model_path);
    IniParser::set(doc, "rag", "max_chunks_in_memory", std::to_string(rag_settings_.max_chunks_in_memory));
    IniParser::set(doc, "rag", "similarity_threshold", std::to_string(rag_settings_.similarity_threshold));
    IniParser::set(doc, "rag", "max_embedding_cache_size", std::to_string(rag_settings_.max_embedding_cache_size));
    IniParser::set(doc, "rag", "embedding_dimension", std::to_string(rag_settings_.embedding_dimension));
    IniParser::set(doc, "rag", "max_sequence_length", std::to_string(rag_settings_.max_sequence_length));
    IniParser::set(doc, "rag", "max_tokens_per_chunk", std::to_string(rag_settings_.max_tokens_per_chunk));
    IniParser::set(doc, "rag", "search_k", std::to_string(rag_settings_.search_k));
    IniParser::set(doc, "rag", "mmr_lambda", std::to_string(rag_settings_.mmr_lambda));
    IniParser::set(doc, "rag", "enable_mmr", rag_settings_.enable_mmr ? "true" : "false");
    IniParser::set(doc, "rag", "enable_rag", rag_settings_.enable_rag ? "true" : "false");
    IniParser::set(doc, "rag", "enable_caching", rag_settings_.enable_caching ? "true" : "false");
    IniParser::set(doc, "rag", "rag_mode", std::to_string(static_cast<int>(rag_settings_.rag_mode)));

    // Параметры гибридного поиска
    IniParser::set(doc, "rag", "enable_hybrid_search", rag_settings_.enable_hybrid_search ? "true" : "false");
    IniParser::set(doc, "rag", "keyword_boost_weight", std::to_string(rag_settings_.keyword_boost_weight));
    IniParser::set(doc, "rag", "enable_query_expansion", rag_settings_.enable_query_expansion ? "true" : "false");

    // Настройки глубокого анализа документа
    IniParser::set(doc, "rag", "deep_analysis_mode", std::to_string(static_cast<int>(rag_settings_.deep_analysis.mode)));
    IniParser::set(doc, "rag", "deep_analysis_chunks_per_batch", std::to_string(rag_settings_.deep_analysis.chunks_per_batch));
    IniParser::set(doc, "rag", "deep_analysis_max_iterations", std::to_string(rag_settings_.deep_analysis.max_iterations));
    IniParser::set(doc, "rag", "deep_analysis_enable_progressive_summary", rag_settings_.deep_analysis.enable_progressive_summary ? "true" : "false");
    IniParser::set(doc, "rag", "deep_analysis_final_synthesis_chunks", std::to_string(rag_settings_.deep_analysis.final_synthesis_chunks));
    IniParser::set(doc, "rag", "deep_analysis_auto_adjust_context_size", rag_settings_.deep_analysis.auto_adjust_context_size ? "true" : "false");
    IniParser::set(doc, "rag", "deep_analysis_target_context_size", std::to_string(rag_settings_.deep_analysis.target_context_size));

    // =========================================================================
    // [SAMPLING]
    // =========================================================================
    IniParser::set(doc, "sampling", "temperature", std::to_string(sampling_settings_.temperature));
    IniParser::set(doc, "sampling", "top_k", std::to_string(sampling_settings_.top_k));
    IniParser::set(doc, "sampling", "top_p", std::to_string(sampling_settings_.top_p));
    IniParser::set(doc, "sampling", "min_p", std::to_string(sampling_settings_.min_p));
    IniParser::set(doc, "sampling", "typical_p", std::to_string(sampling_settings_.typical_p));
    IniParser::set(doc, "sampling", "tfs_z", std::to_string(sampling_settings_.tfs_z));
    IniParser::set(doc, "sampling", "xtc_probability", std::to_string(sampling_settings_.xtc_probability));
    IniParser::set(doc, "sampling", "xtc_threshold", std::to_string(sampling_settings_.xtc_threshold));
    IniParser::set(doc, "sampling", "dry_multiplier", std::to_string(sampling_settings_.dry_multiplier));
    IniParser::set(doc, "sampling", "dry_base", std::to_string(sampling_settings_.dry_base));
    IniParser::set(doc, "sampling", "dry_allowed_length", std::to_string(sampling_settings_.dry_allowed_length));
    IniParser::set(doc, "sampling", "dry_penalty_last_n", std::to_string(sampling_settings_.dry_penalty_last_n));
    IniParser::set(doc, "sampling", "dynatemp_range", std::to_string(sampling_settings_.dynatemp_range));
    IniParser::set(doc, "sampling", "dynatemp_exp", std::to_string(sampling_settings_.dynatemp_exp));
    IniParser::set(doc, "sampling", "repeat_penalty", std::to_string(sampling_settings_.repeat_penalty));
    IniParser::set(doc, "sampling", "presence_penalty", std::to_string(sampling_settings_.presence_penalty));
    IniParser::set(doc, "sampling", "frequency_penalty", std::to_string(sampling_settings_.frequency_penalty));
    IniParser::set(doc, "sampling", "repeat_last_n", std::to_string(sampling_settings_.repeat_last_n));
    IniParser::set(doc, "sampling", "penalize_nl", sampling_settings_.penalize_nl ? "true" : "false");
    IniParser::set(doc, "sampling", "ignore_eos", sampling_settings_.ignore_eos ? "true" : "false");
    IniParser::set(doc, "sampling", "mirostat_mode", std::to_string(sampling_settings_.mirostat_mode));
    IniParser::set(doc, "sampling", "mirostat_tau", std::to_string(sampling_settings_.mirostat_tau));
    IniParser::set(doc, "sampling", "mirostat_eta", std::to_string(sampling_settings_.mirostat_eta));
    IniParser::set(doc, "sampling", "samplers_order", sampling_settings_.samplers_order);
    IniParser::set(doc, "sampling", "use_custom_sampler_order", sampling_settings_.use_custom_sampler_order ? "true" : "false");

    // =========================================================================
    // [MODEL_LOADING]
    // =========================================================================
    IniParser::set(doc, "model_loading", "model_path", model_loading_settings_.model_path);
    IniParser::set(doc, "model_loading", "model_url", model_loading_settings_.model_url);
    IniParser::set(doc, "model_loading", "hf_repo", model_loading_settings_.hf_repo);
    IniParser::set(doc, "model_loading", "hf_file", model_loading_settings_.hf_file);
    IniParser::set(doc, "model_loading", "hf_token", model_loading_settings_.hf_token);
    IniParser::set(doc, "model_loading", "model_alias", model_loading_settings_.model_alias);
    IniParser::set(doc, "model_loading", "model_draft", model_loading_settings_.model_draft);
    IniParser::set(doc, "model_loading", "hf_repo_draft", model_loading_settings_.hf_repo_draft);
    IniParser::set(doc, "model_loading", "draft_max", std::to_string(model_loading_settings_.draft_max));
    IniParser::set(doc, "model_loading", "draft_min", std::to_string(model_loading_settings_.draft_min));
    IniParser::set(doc, "model_loading", "draft_p_min", std::to_string(model_loading_settings_.draft_p_min));
    IniParser::set(doc, "model_loading", "model_vocoder", model_loading_settings_.model_vocoder);
    IniParser::set(doc, "model_loading", "hf_repo_vocoder", model_loading_settings_.hf_repo_vocoder);
    IniParser::set(doc, "model_loading", "hf_file_vocoder", model_loading_settings_.hf_file_vocoder);
    IniParser::set(doc, "model_loading", "lora_base", model_loading_settings_.lora_base);
    IniParser::set(doc, "model_loading", "lora_init_without_apply", model_loading_settings_.lora_init_without_apply ? "true" : "false");
    IniParser::set(doc, "model_loading", "mmproj", model_loading_settings_.mmproj);
    IniParser::set(doc, "model_loading", "mmproj_url", model_loading_settings_.mmproj_url);
    IniParser::set(doc, "model_loading", "no_mmproj", model_loading_settings_.no_mmproj ? "true" : "false");
    IniParser::set(doc, "model_loading", "no_mmproj_offload", model_loading_settings_.no_mmproj_offload ? "true" : "false");
    IniParser::set(doc, "model_loading", "check_tensors", model_loading_settings_.check_tensors ? "true" : "false");
    IniParser::set(doc, "model_loading", "device", model_loading_settings_.device);
    IniParser::set(doc, "model_loading", "device_draft", model_loading_settings_.device_draft);
    IniParser::set(doc, "model_loading", "list_devices", model_loading_settings_.list_devices ? "true" : "false");

    // =========================================================================
    // [GPU]
    // =========================================================================
    IniParser::set(doc, "gpu", "n_gpu_layers", std::to_string(gpu_settings_.n_gpu_layers));
    IniParser::set(doc, "gpu", "n_gpu_layers_draft", std::to_string(gpu_settings_.n_gpu_layers_draft));
    IniParser::set(doc, "gpu", "split_mode", gpu_settings_.get_split_mode_string());
    IniParser::set(doc, "gpu", "tensor_split", gpu_settings_.tensor_split);
    IniParser::set(doc, "gpu", "main_gpu", std::to_string(gpu_settings_.main_gpu));
    IniParser::set(doc, "gpu", "no_op_offload", gpu_settings_.no_op_offload ? "true" : "false");
    IniParser::set(doc, "gpu", "no_kv_offload", gpu_settings_.no_kv_offload ? "true" : "false");
    IniParser::set(doc, "gpu", "no_warmup", gpu_settings_.no_warmup ? "true" : "false");
    IniParser::set(doc, "gpu", "mlock", gpu_settings_.mlock ? "true" : "false");
    IniParser::set(doc, "gpu", "no_mmap", gpu_settings_.no_mmap ? "true" : "false");
    IniParser::set(doc, "gpu", "flash_attn", std::to_string(static_cast<int>(gpu_settings_.flash_attn)));
    IniParser::set(doc, "gpu", "defrag_thold", std::to_string(gpu_settings_.defrag_thold));

    // =========================================================================
    // [CACHE]
    // =========================================================================
    IniParser::set(doc, "cache", "cache_type_k", CacheSettings::cache_type_to_string(cache_settings_.cache_type_k));
    IniParser::set(doc, "cache", "cache_type_v", CacheSettings::cache_type_to_string(cache_settings_.cache_type_v));
    IniParser::set(doc, "cache", "cache_type_k_draft", CacheSettings::cache_type_to_string(cache_settings_.cache_type_k_draft));
    IniParser::set(doc, "cache", "cache_type_v_draft", CacheSettings::cache_type_to_string(cache_settings_.cache_type_v_draft));
    IniParser::set(doc, "cache", "cache_prompt", cache_settings_.cache_prompt ? "true" : "false");
    IniParser::set(doc, "cache", "cache_reuse", std::to_string(cache_settings_.cache_reuse));
    IniParser::set(doc, "cache", "swa_full", cache_settings_.swa_full ? "true" : "false");
    IniParser::set(doc, "cache", "no_context_shift", cache_settings_.no_context_shift ? "true" : "false");
    IniParser::set(doc, "cache", "slot_save_path", cache_settings_.slot_save_path);
    IniParser::set(doc, "cache", "slot_prompt_similarity", std::to_string(cache_settings_.slot_prompt_similarity));
    IniParser::set(doc, "cache", "slots_endpoint_enabled", cache_settings_.slots_endpoint_enabled ? "true" : "false");

    // =========================================================================
    // [ROPE]
    // =========================================================================
    IniParser::set(doc, "rope", "rope_scaling", rope_settings_.get_scaling_string());
    IniParser::set(doc, "rope", "rope_scale", std::to_string(rope_settings_.rope_scale));
    IniParser::set(doc, "rope", "rope_freq_base", std::to_string(rope_settings_.rope_freq_base));
    IniParser::set(doc, "rope", "rope_freq_scale", std::to_string(rope_settings_.rope_freq_scale));
    IniParser::set(doc, "rope", "yarn_orig_ctx", std::to_string(rope_settings_.yarn_orig_ctx));
    IniParser::set(doc, "rope", "yarn_ext_factor", std::to_string(rope_settings_.yarn_ext_factor));
    IniParser::set(doc, "rope", "yarn_attn_factor", std::to_string(rope_settings_.yarn_attn_factor));
    IniParser::set(doc, "rope", "yarn_beta_slow", std::to_string(rope_settings_.yarn_beta_slow));
    IniParser::set(doc, "rope", "yarn_beta_fast", std::to_string(rope_settings_.yarn_beta_fast));

    // =========================================================================
    // [CONTROL_VECTOR]
    // =========================================================================
    IniParser::set(doc, "control_vector", "control_vector_layer_start", std::to_string(control_vector_settings_.control_vector_layer_start));
    IniParser::set(doc, "control_vector", "control_vector_layer_end", std::to_string(control_vector_settings_.control_vector_layer_end));

    // =========================================================================
    // [TENSOR_OVERRIDE]
    // =========================================================================
    // Переопределения тензоров сохраняются в JSON профиль (сложная структура)

    // =========================================================================
    // [SERVER_RUNTIME]
    // =========================================================================
    IniParser::set(doc, "server_runtime", "host", server_runtime_settings_.host);
    IniParser::set(doc, "server_runtime", "port", std::to_string(server_runtime_settings_.port));
    IniParser::set(doc, "server_runtime", "timeout", std::to_string(server_runtime_settings_.timeout));
    IniParser::set(doc, "server_runtime", "api_prefix", server_runtime_settings_.api_prefix);
    IniParser::set(doc, "server_runtime", "offline", server_runtime_settings_.offline ? "true" : "false");
    IniParser::set(doc, "server_runtime", "threads_http", std::to_string(server_runtime_settings_.threads_http));
    IniParser::set(doc, "server_runtime", "static_path", server_runtime_settings_.static_path);
    IniParser::set(doc, "server_runtime", "no_webui", server_runtime_settings_.no_webui ? "true" : "false");
    IniParser::set(doc, "server_runtime", "api_key_file", server_runtime_settings_.api_key_file);
    IniParser::set(doc, "server_runtime", "ssl_key_file", server_runtime_settings_.ssl_key_file);
    IniParser::set(doc, "server_runtime", "ssl_cert_file", server_runtime_settings_.ssl_cert_file);
    IniParser::set(doc, "server_runtime", "embeddings_mode", server_runtime_settings_.embeddings_mode ? "true" : "false");
    IniParser::set(doc, "server_runtime", "reranking_mode", server_runtime_settings_.reranking_mode ? "true" : "false");
    IniParser::set(doc, "server_runtime", "metrics_enabled", server_runtime_settings_.metrics_enabled ? "true" : "false");
    IniParser::set(doc, "server_runtime", "slot_save_path", server_runtime_settings_.slot_save_path);
    IniParser::set(doc, "server_runtime", "n_parallel", std::to_string(server_runtime_settings_.n_parallel));
    IniParser::set(doc, "server_runtime", "cache_type_k", server_runtime_settings_.cache_type_k);
    IniParser::set(doc, "server_runtime", "cache_type_v", server_runtime_settings_.cache_type_v);
    IniParser::set(doc, "server_runtime", "cache_reuse", std::to_string(server_runtime_settings_.cache_reuse));
    IniParser::set(doc, "server_runtime", "log_disabled", server_runtime_settings_.log_disabled ? "true" : "false");
    IniParser::set(doc, "server_runtime", "log_file", server_runtime_settings_.log_file);
    IniParser::set(doc, "server_runtime", "log_colors", server_runtime_settings_.log_colors ? "true" : "false");
    IniParser::set(doc, "server_runtime", "log_verbose", server_runtime_settings_.log_verbose ? "true" : "false");
    IniParser::set(doc, "server_runtime", "log_verbosity", std::to_string(server_runtime_settings_.log_verbosity));
    IniParser::set(doc, "server_runtime", "log_format", server_runtime_settings_.log_format);
    IniParser::set(doc, "server_runtime", "log_prefix", server_runtime_settings_.log_prefix ? "true" : "false");
    IniParser::set(doc, "server_runtime", "log_timestamps", server_runtime_settings_.log_timestamps ? "true" : "false");

    // =========================================================================
    // [BATCH]
    // =========================================================================
    IniParser::set(doc, "batch", "batch_size", std::to_string(batch_settings_.batch_size));
    IniParser::set(doc, "batch", "ubatch_size", std::to_string(batch_settings_.ubatch_size));
    std::cout << "save_to_ini: Сохранение batch.ctx_size = " << batch_settings_.ctx_size << std::endl;
    IniParser::set(doc, "batch", "ctx_size", std::to_string(batch_settings_.ctx_size));
    IniParser::set(doc, "batch", "ctx_size_draft", std::to_string(batch_settings_.ctx_size_draft));
    IniParser::set(doc, "batch", "threads", std::to_string(batch_settings_.threads));
    IniParser::set(doc, "batch", "threads_batch", std::to_string(batch_settings_.threads_batch));
    IniParser::set(doc, "batch", "cpu_mask", batch_settings_.cpu_mask);
    IniParser::set(doc, "batch", "cpu_range", batch_settings_.cpu_range);
    IniParser::set(doc, "batch", "cpu_strict", batch_settings_.cpu_strict ? "true" : "false");
    IniParser::set(doc, "batch", "priority", std::to_string(batch_settings_.priority));
    IniParser::set(doc, "batch", "poll_level", std::to_string(batch_settings_.poll_level));
    IniParser::set(doc, "batch", "cpu_mask_batch", batch_settings_.cpu_mask_batch);
    IniParser::set(doc, "batch", "cpu_range_batch", batch_settings_.cpu_range_batch);
    IniParser::set(doc, "batch", "cpu_strict_batch", batch_settings_.cpu_strict_batch ? "true" : "false");
    IniParser::set(doc, "batch", "priority_batch", std::to_string(batch_settings_.priority_batch));
    IniParser::set(doc, "batch", "poll_batch", std::to_string(batch_settings_.poll_batch));
    IniParser::set(doc, "batch", "cont_batching", batch_settings_.cont_batching ? "true" : "false");
    IniParser::set(doc, "batch", "no_perf", batch_settings_.no_perf ? "true" : "false");

    // =========================================================================
    // [GRAMMAR]
    // =========================================================================
    IniParser::set(doc, "grammar", "grammar", grammar_settings_.grammar);
    IniParser::set(doc, "grammar", "grammar_file", grammar_settings_.grammar_file);
    IniParser::set(doc, "grammar", "json_schema", grammar_settings_.json_schema);
    IniParser::set(doc, "grammar", "json_schema_file", grammar_settings_.json_schema_file);
    IniParser::set(doc, "grammar", "chat_template", grammar_settings_.chat_template);
    IniParser::set(doc, "grammar", "chat_template_file", grammar_settings_.chat_template_file);
    IniParser::set(doc, "grammar", "chat_template_kwargs", grammar_settings_.chat_template_kwargs);
    IniParser::set(doc, "grammar", "use_jinja", grammar_settings_.use_jinja ? "true" : "false");
    IniParser::set(doc, "grammar", "no_prefill_assistant", grammar_settings_.no_prefill_assistant ? "true" : "false");
    IniParser::set(doc, "grammar", "system_prompt_file", grammar_settings_.system_prompt_file);
    IniParser::set(doc, "grammar", "default_system_prompt", grammar_settings_.default_system_prompt);
    IniParser::set(doc, "grammar", "reasoning_format", grammar_settings_.get_reasoning_format_string());
    IniParser::set(doc, "grammar", "reasoning_budget", std::to_string(grammar_settings_.reasoning_budget));

    // =========================================================================
    // [OUTPUT]
    // =========================================================================
    IniParser::set(doc, "output", "n_predict", std::to_string(output_settings_.n_predict));
    IniParser::set(doc, "output", "keep", std::to_string(output_settings_.keep));
    IniParser::set(doc, "output", "special_tokens", output_settings_.special_tokens ? "true" : "false");
    IniParser::set(doc, "output", "spm_infill", output_settings_.spm_infill ? "true" : "false");
    IniParser::set(doc, "output", "verbose_prompt", output_settings_.verbose_prompt ? "true" : "false");
    IniParser::set(doc, "output", "escape_sequences", output_settings_.escape_sequences ? "true" : "false");
    IniParser::set(doc, "output", "tts_use_guide_tokens", output_settings_.tts_use_guide_tokens ? "true" : "false");
    IniParser::set(doc, "output", "pooling_type", output_settings_.pooling_type);

    // =========================================================================
    // [CUSTOM]
    // =========================================================================
    for (const auto& [key, value] : custom_settings_) {
        IniParser::set(doc, "custom", key, value);
    }

    // Сохраняем файл с заголовком
    std::string header = 
        "; ============================================================\n"
        "; Llama GUI - Централизованный файл настроек\n"
        "; Аналогия: php.ini для web-сервера\n"
        "; ============================================================\n"
        "; Этот файл содержит все настройки приложения\n"
        "; Формат: INI (секции [section], ключи key=value)\n"
        "; ============================================================\n\n";

    bool result = IniParser::save(file_path, doc, header);
    
    if (result) {
        std::cout << "Settings successfully saved to INI file" << std::endl;
    } else {
        std::cerr << "Failed to save INI file: " << file_path << std::endl;
    }
    
    return result;
}

} // namespace core
} // namespace llama_gui
