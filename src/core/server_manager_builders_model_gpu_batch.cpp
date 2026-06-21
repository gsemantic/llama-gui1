#include "../include/core/server_manager.h"
#include "../include/core/settings.h"
#include <sstream>
#include <iostream>

namespace llama_gui {
namespace core {

// =========================================================================
// Helper functions
// =========================================================================

std::string ServerManager::format_tensor_split(const std::string& split) const {
    return split;
}

std::string ServerManager::format_cache_type(llama_gui::core::CacheSettings::CacheType type) const {
    switch (type) {
        case CacheSettings::CacheType::F32:    return "f32";
        case CacheSettings::CacheType::F16:    return "f16";
        case CacheSettings::CacheType::BF16:   return "bf16";
        case CacheSettings::CacheType::Q8_0:   return "q8_0";
        case CacheSettings::CacheType::Q4_0:   return "q4_0";
        case CacheSettings::CacheType::Q4_1:   return "q4_1";
        case CacheSettings::CacheType::IQ4_NL: return "iq4_nl";
        case CacheSettings::CacheType::Q5_0:   return "q5_0";
        case CacheSettings::CacheType::Q5_1:   return "q5_1";
        default: return "f16";
    }
}

std::string ServerManager::format_lora_adapters(
    const std::vector<llama_gui::core::ModelLoadingSettings::LoRAAdapter>& adapters) const {
    
    std::ostringstream cmd;
    for (const auto& adapter : adapters) {
        if (adapter.scale != 1.0f) {
            cmd << " --lora-scaled " << adapter.path << " " << adapter.scale;
        } else {
            cmd << " --lora " << adapter.path;
        }
    }
    return cmd.str();
}

std::string ServerManager::format_control_vectors(
    const std::vector<llama_gui::core::ControlVectorSettings::ControlVector>& vectors) const {
    
    std::ostringstream cmd;
    for (const auto& cv : vectors) {
        if (cv.scale != 1.0f) {
            cmd << " --control-vector-scaled " << cv.path << " " << cv.scale;
        } else {
            cmd << " --control-vector " << cv.path;
        }
    }
    return cmd.str();
}

std::string ServerManager::format_dry_breakers(const std::vector<std::string>& breakers) const {
    std::ostringstream cmd;
    for (const auto& breaker : breakers) {
        cmd << " --dry-sequence-breaker \"" << breaker << "\"";
    }
    return cmd.str();
}

// =========================================================================
// Model arguments
// =========================================================================

std::string ServerManager::build_model_args() const {
    std::ostringstream cmd;
    const auto& model = settings_.model_loading();

    // Model path -优先使用 ServerManager 的 model_path_
    std::string model_path = model_path_;
    if (model_path.empty()) {
        model_path = model.model_path;
    }
    if (model_path.empty()) {
        model_path = settings_.get_model_path();
    }
    
    // Debug output
    std::cerr << "DEBUG build_model_args: model_path_='" << model_path_ << "', model.model_path='" << model.model_path << "', get_model_path()='" << settings_.get_model_path() << "', final='" << model_path << "'" << std::endl;
    
    if (!model_path.empty()) {
        cmd << " --model " << model_path;
    }

    // Model URL
    if (!model.model_url.empty()) {
        cmd << " --model-url " << model.model_url;
    }

    // Hugging Face
    if (!model.hf_repo.empty()) {
        cmd << " --hf-repo " << model.hf_repo;
    }
    if (!model.hf_file.empty()) {
        cmd << " --hf-file " << model.hf_file;
    }
    if (!model.hf_token.empty()) {
        cmd << " --hf-token " << model.hf_token;
    }

    // Alias
    if (!model.model_alias.empty()) {
        cmd << " --alias " << model.model_alias;
    }

    // Draft model
    bool has_draft = !model.model_draft.empty();
    if (has_draft) {
        cmd << " --model-draft " << model.model_draft;
    }
    if (!model.hf_repo_draft.empty()) {
        cmd << " --hf-repo-draft " << model.hf_repo_draft;
    }

    // Draft parameters (только если есть draft-модель)
    if (has_draft && model.draft_max > 0) {
        cmd << " --draft-max " << model.draft_max;
    }
    if (has_draft && model.draft_min > 0) {
        cmd << " --draft-min " << model.draft_min;
    }
    if (has_draft && model.draft_p_min > 0.0f && model.draft_p_min < 1.0f) {
        cmd << " --draft-p-min " << model.draft_p_min;
    }

    // Vocoder model
    if (!model.model_vocoder.empty()) {
        cmd << " --model-vocoder " << model.model_vocoder;
    }

    // LoRA adapters
    if (!model.lora_adapters.empty()) {
        cmd << format_lora_adapters(model.lora_adapters);
    }
    if (!model.lora_base.empty()) {
        cmd << " --lora-base " << model.lora_base;
    }

    // Multimodal
    if (!model.mmproj.empty()) {
        cmd << " --mmproj " << model.mmproj;
    }
    if (!model.mmproj_url.empty()) {
        cmd << " --mmproj-url " << model.mmproj_url;
    }
    if (model.no_mmproj) {
        cmd << " --no-mmproj";
    }
    if (model.no_mmproj_offload) {
        cmd << " --no-mmproj-offload";
    }

    // Device
    if (!model.device.empty()) {
        cmd << " --device " << model.device;
    }
    if (!model.device_draft.empty()) {
        cmd << " --device-draft " << model.device_draft;
    }

    // Validation
    if (model.check_tensors) {
        cmd << " --check-tensors";
    }

    return cmd.str();
}

// =========================================================================
// GPU arguments
// =========================================================================

std::string ServerManager::build_gpu_args() const {
    std::ostringstream cmd;
    const auto& gpu = settings_.gpu();

    // GPU layers
    if (gpu.n_gpu_layers > 0) {
        cmd << " --n-gpu-layers " << gpu.n_gpu_layers;
    }
    if (gpu.n_gpu_layers_draft > 0) {
        cmd << " --n-gpu-layers-draft " << gpu.n_gpu_layers_draft;
    }

    // Split mode
    if (gpu.split_mode != GPUSettings::SplitMode::Layer) {
        cmd << " --split-mode " << gpu.get_split_mode_string();
    }

    // Tensor split
    if (!gpu.tensor_split.empty()) {
        cmd << " --tensor-split " << gpu.tensor_split;
    }

    // Main GPU
    if (gpu.main_gpu > 0) {
        cmd << " --main-gpu " << gpu.main_gpu;
    }

    // Offload options
    if (gpu.no_op_offload) {
        cmd << " --no-op-offload";
    }
    if (gpu.no_kv_offload) {
        cmd << " --no-kv-offload";
    }
    if (gpu.no_warmup) {
        cmd << " --no-warmup";
    }

    // Flash Attention
    if (gpu.flash_attn != GPUSettings::FlashAttention::Auto) {
        if (gpu.flash_attn == GPUSettings::FlashAttention::Enabled) {
            cmd << " --flash-attn";
        } else {
            cmd << " --no-flash-attn";
        }
    }

    // Defrag (DEPRECATED, не добавляем если значение по умолчанию)
    if (gpu.defrag_thold > 0.0f && gpu.defrag_thold != 0.1f) {
        cmd << " --defrag-thold " << gpu.defrag_thold;
    }

    return cmd.str();
}

// =========================================================================
// Batch arguments
// =========================================================================

std::string ServerManager::build_batch_args() const {
    std::ostringstream cmd;
    const auto& batch = settings_.batch();

    // Batch sizes
    if (batch.batch_size != 2048) {
        cmd << " --batch-size " << batch.batch_size;
    }
    if (batch.ubatch_size != 512) {
        cmd << " --ubatch-size " << batch.ubatch_size;
    }

    // Context - always specify explicitly to override model's n_ctx_train
    cmd << " --ctx-size " << batch.ctx_size;
    if (batch.ctx_size_draft > 0) {
        cmd << " --ctx-size-draft " << batch.ctx_size_draft;
    }

    // Threading
    if (batch.threads > 0) {
        cmd << " --threads " << batch.threads;
    }
    if (batch.threads_batch > 0) {
        cmd << " --threads-batch " << batch.threads_batch;
    }

    // CPU affinity (main)
    if (!batch.cpu_mask.empty()) {
        cmd << " --cpu-mask " << batch.cpu_mask;
    }
    if (!batch.cpu_range.empty()) {
        cmd << " --cpu-range " << batch.cpu_range;
    }
    if (batch.cpu_strict) {
        cmd << " --cpu-strict";
    }
    if (batch.priority != 0) {
        cmd << " --prio " << batch.priority;
    }
    if (batch.poll_level != 50) {
        cmd << " --poll " << batch.poll_level;
    }

    // CPU affinity (batch)
    if (!batch.cpu_mask_batch.empty()) {
        cmd << " --cpu-mask-batch " << batch.cpu_mask_batch;
    }
    if (!batch.cpu_range_batch.empty()) {
        cmd << " --cpu-range-batch " << batch.cpu_range_batch;
    }
    if (batch.cpu_strict_batch) {
        cmd << " --cpu-strict-batch";
    }
    if (batch.priority_batch != 0) {
        cmd << " --prio-batch " << batch.priority_batch;
    }
    if (batch.poll_batch != 50) {
        cmd << " --poll-batch " << batch.poll_batch;
    }

    // Options
    if (!batch.cont_batching) {
        cmd << " --no-cont-batching";
    }
    if (batch.no_perf) {
        cmd << " --no-perf";
    }

    return cmd.str();
}

} // namespace core
} // namespace llama_gui
