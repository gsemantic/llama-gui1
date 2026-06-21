#include "../include/core/server_manager.h"
#include "../include/core/settings.h"
#include <sstream>
#include <iostream>

namespace llama_gui {
namespace core {

// =========================================================================
// Server arguments
// =========================================================================

std::string ServerManager::build_server_args() const {
    std::ostringstream cmd;
    const auto& runtime = settings_.server_runtime();

    // Network - всегда добавляем host и port
    cmd << " --host " << runtime.host;
    cmd << " --port " << runtime.port;

    if (runtime.timeout != 600) {
        cmd << " --timeout " << runtime.timeout;
    }

    // API prefix
    if (!runtime.api_prefix.empty()) {
        cmd << " --api-prefix " << runtime.api_prefix;
    }

    // Offline mode
    if (runtime.offline) {
        cmd << " --offline";
    }

    // HTTP threads
    if (runtime.threads_http > 0) {
        cmd << " --threads-http " << runtime.threads_http;
    }

    // Static files
    if (!runtime.static_path.empty()) {
        cmd << " --path " << runtime.static_path;
    }

    // No WebUI
    if (runtime.no_webui) {
        cmd << " --no-webui";
    }

    // Features
    if (runtime.embeddings_mode) {
        cmd << " --embeddings";
    }
    if (runtime.reranking_mode) {
        cmd << " --reranking";
    }
    if (runtime.metrics_enabled) {
        cmd << " --metrics";
    }

    // N parallel (speculative decoding)
    if (runtime.n_parallel != 4) {
        cmd << " --n-parallel " << runtime.n_parallel;
    }

    return cmd.str();
}

// =========================================================================
// Security arguments
// =========================================================================

std::string ServerManager::build_security_args() const {
    std::ostringstream cmd;
    const auto& runtime = settings_.server_runtime();

    // API keys
    for (const auto& key : runtime.api_keys) {
        cmd << " --api-key " << key;
    }

    // API key file
    if (!runtime.api_key_file.empty()) {
        cmd << " --api-key-file " << runtime.api_key_file;
    }

    // SSL
    if (!runtime.ssl_key_file.empty()) {
        cmd << " --ssl-key-file " << runtime.ssl_key_file;
    }
    if (!runtime.ssl_cert_file.empty()) {
        cmd << " --ssl-cert-file " << runtime.ssl_cert_file;
    }

    return cmd.str();
}

// =========================================================================
// Advanced arguments (Control Vectors, Tensor Override, Speculative)
// =========================================================================

std::string ServerManager::build_advanced_args() const {
    std::ostringstream cmd;
    const auto& cv = settings_.control_vector();
    const auto& to = settings_.tensor_override();
    const auto& model = settings_.model_loading();

    // Control vectors
    if (!cv.control_vectors.empty()) {
        cmd << format_control_vectors(cv.control_vectors);
    }
    if (cv.control_vector_layer_start > 0 || cv.control_vector_layer_end >= 0) {
        cmd << " --control-vector-layer-range " 
            << cv.control_vector_layer_start << " " << cv.control_vector_layer_end;
    }

    // Tensor overrides
    for (const auto& override : to.overrides) {
        cmd << " --override-tensor " << override.pattern << " " << override.buffer_type;
    }

    // Override KV
    for (const auto& [key, value] : model.override_kv) {
        cmd << " --override-kv " << key << "=" << value;
    }

    return cmd.str();
}

// =========================================================================
// Logging arguments
// =========================================================================

std::string ServerManager::build_logging_args() const {
    std::ostringstream cmd;
    const auto& runtime = settings_.server_runtime();

    // Log disabled
    if (runtime.log_disabled) {
        cmd << " --log-disable";
    }

    // Log file
    if (!runtime.log_file.empty()) {
        cmd << " --log-file " << runtime.log_file;
    }

    // Log format
    if (runtime.log_format != "json") {
        cmd << " --log-format " << runtime.log_format;
    }

    // Log options
    if (runtime.log_colors) {
        cmd << " --log-colors";
    }
    if (!runtime.log_prefix) {
        cmd << " --no-log-prefix";
    }
    if (!runtime.log_timestamps) {
        cmd << " --no-log-timestamps";
    }

    // Verbosity
    if (runtime.log_verbose) {
        cmd << " --verbose";
    }
    if (runtime.log_verbosity > 0) {
        cmd << " --verbosity " << runtime.log_verbosity;
    }

    return cmd.str();
}

// =========================================================================
// Grammar arguments
// =========================================================================

std::string ServerManager::build_grammar_args() const {
    std::ostringstream cmd;
    const auto& grammar = settings_.grammar();

    // Grammar
    if (!grammar.grammar.empty()) {
        cmd << " --grammar \"" << grammar.grammar << "\"";
    }
    if (!grammar.grammar_file.empty()) {
        cmd << " --grammar-file " << grammar.grammar_file;
    }

    // JSON Schema
    if (!grammar.json_schema.empty()) {
        cmd << " --json-schema \"" << grammar.json_schema << "\"";
    }
    if (!grammar.json_schema_file.empty()) {
        cmd << " --json-schema-file " << grammar.json_schema_file;
    }

    // Chat template
    if (!grammar.chat_template.empty()) {
        cmd << " --chat-template \"" << grammar.chat_template << "\"";
    }
    if (!grammar.chat_template_file.empty()) {
        cmd << " --chat-template-file " << grammar.chat_template_file;
    }
    if (!grammar.chat_template_kwargs.empty()) {
        cmd << " --chat-template-kwargs \"" << grammar.chat_template_kwargs << "\"";
    }

    // Jinja
    if (grammar.use_jinja) {
        cmd << " --jinja";
    }

    // No prefill assistant
    if (grammar.no_prefill_assistant) {
        cmd << " --no-prefill-assistant";
    }

    // System prompt
    if (!grammar.system_prompt_file.empty()) {
        cmd << " --system-prompt-file " << grammar.system_prompt_file;
    }

    // Reasoning - always specify to override model default
    cmd << " --reasoning-format " << grammar.get_reasoning_format_string();
    // Always specify budget: 0 disables thinking, -1 means unlimited
    cmd << " --reasoning-budget " << grammar.reasoning_budget;

    return cmd.str();
}

// =========================================================================
// Output arguments
// =========================================================================

std::string ServerManager::build_output_args() const {
    std::ostringstream cmd;
    const auto& output = settings_.output();

    // N predict
    if (output.n_predict >= 0) {
        cmd << " --n-predict " << output.n_predict;
    }

    // Keep
    if (output.keep > 0) {
        cmd << " --keep " << output.keep;
    }

    // Special tokens
    if (output.special_tokens) {
        cmd << " --special";
    }

    // SPM infill
    if (output.spm_infill) {
        cmd << " --spm-infill";
    }

    // Verbose prompt
    if (output.verbose_prompt) {
        cmd << " --verbose-prompt";
    }

    // Escape
    if (!output.escape_sequences) {
        cmd << " --no-escape";
    }

    // TTS
    if (output.tts_use_guide_tokens) {
        cmd << " --tts-use-guide-tokens";
    }

    // Pooling
    if (output.pooling_type != "model") {
        cmd << " --pooling " << output.pooling_type;
    }

    return cmd.str();
}

} // namespace core
} // namespace llama_gui
