#include "../include/core/server_manager.h"
#include "../include/core/settings.h"
#include <sstream>
#include <iostream>

namespace llama_gui {
namespace core {

// =========================================================================
// Sampling arguments
// =========================================================================

std::string ServerManager::build_sampling_args() const {
    std::ostringstream cmd;
    const auto& sampling = settings_.sampling();

    // Basic sampling
    if (sampling.temperature != 0.8f) {
        cmd << " --temp " << sampling.temperature;
    }
    if (sampling.top_k > 0) {
        cmd << " --top-k " << sampling.top_k;
    }
    if (sampling.top_p < 1.0f) {
        cmd << " --top-p " << sampling.top_p;
    }
    if (sampling.min_p > 0.0f) {
        cmd << " --min-p " << sampling.min_p;
    }

    // Advanced sampling
    if (sampling.typical_p < 1.0f) {
        cmd << " --typical " << sampling.typical_p;
    }
    if (sampling.tfs_z < 1.0f) {
        cmd << " --tfs-z " << sampling.tfs_z;
    }

    // XTC
    if (sampling.xtc_probability > 0.0f) {
        cmd << " --xtc-probability " << sampling.xtc_probability;
        cmd << " --xtc-threshold " << sampling.xtc_threshold;
    }

    // DRY
    if (sampling.dry_multiplier > 0.0f) {
        cmd << " --dry-multiplier " << sampling.dry_multiplier;
        cmd << " --dry-base " << sampling.dry_base;
        cmd << " --dry-allowed-length " << sampling.dry_allowed_length;
        if (sampling.dry_penalty_last_n >= 0) {
            cmd << " --dry-penalty-last-n " << sampling.dry_penalty_last_n;
        }
        cmd << format_dry_breakers(sampling.dry_sequence_breakers);
    }

    // Dynamic temperature
    if (sampling.dynatemp_range > 0.0f) {
        cmd << " --dynatemp-range " << sampling.dynatemp_range;
        cmd << " --dynatemp-exp " << sampling.dynatemp_exp;
    }

    // Penalties
    if (sampling.repeat_penalty != 1.1f) {
        cmd << " --repeat-penalty " << sampling.repeat_penalty;
    }
    if (sampling.presence_penalty != 0.0f) {
        cmd << " --presence-penalty " << sampling.presence_penalty;
    }
    if (sampling.frequency_penalty != 0.0f) {
        cmd << " --frequency-penalty " << sampling.frequency_penalty;
    }
    if (sampling.repeat_last_n != 64) {
        cmd << " --repeat-last-n " << sampling.repeat_last_n;
    }
    if (!sampling.penalize_nl) {
        cmd << " --no-penalize-nl";
    }
    if (sampling.ignore_eos) {
        cmd << " --ignore-eos";
    }

    // Mirostat
    if (sampling.mirostat_mode > 0) {
        cmd << " --mirostat " << sampling.mirostat_mode;
        cmd << " --mirostat-tau " << sampling.mirostat_tau;
        cmd << " --mirostat-eta " << sampling.mirostat_eta;
    }

    // Sampler order
    if (sampling.use_custom_sampler_order) {
        cmd << " --sampling-seq \"" << sampling.samplers_order << "\"";
    }

    return cmd.str();
}

// =========================================================================
// Cache arguments
// =========================================================================

std::string ServerManager::build_cache_args() const {
    std::ostringstream cmd;
    const auto& cache = settings_.cache();

    // Cache types
    if (cache.cache_type_k != CacheSettings::CacheType::F16) {
        cmd << " --cache-type-k " << format_cache_type(cache.cache_type_k);
    }
    if (cache.cache_type_v != CacheSettings::CacheType::F16) {
        cmd << " --cache-type-v " << format_cache_type(cache.cache_type_v);
    }

    // Draft cache types
    if (cache.cache_type_k_draft != CacheSettings::CacheType::F16) {
        cmd << " --cache-type-k-draft " << format_cache_type(cache.cache_type_k_draft);
    }
    if (cache.cache_type_v_draft != CacheSettings::CacheType::F16) {
        cmd << " --cache-type-v-draft " << format_cache_type(cache.cache_type_v_draft);
    }

    // Cache options
    if (cache.cache_reuse > 0) {
        cmd << " --cache-reuse " << cache.cache_reuse;
    }
    if (cache.swa_full) {
        cmd << " --swa-full";
    }
    if (cache.no_context_shift) {
        cmd << " --no-context-shift";
    }

    // Slot management
    if (!cache.slot_save_path.empty()) {
        cmd << " --slot-save-path " << cache.slot_save_path;
    }
    if (cache.slot_prompt_similarity != 0.5f) {
        cmd << " --slot-prompt-similarity " << cache.slot_prompt_similarity;
    }
    if (!cache.slots_endpoint_enabled) {
        cmd << " --no-slots";
    }

    return cmd.str();
}

// =========================================================================
// RoPE arguments
// =========================================================================

std::string ServerManager::build_rope_args() const {
    std::ostringstream cmd;
    const auto& rope = settings_.rope();

    // RoPE scaling
    if (rope.rope_scaling != RoPESettings::RopeScaling::Linear) {
        cmd << " --rope-scaling " << rope.get_scaling_string();
    }

    // RoPE parameters
    if (rope.rope_scale != 1.0f) {
        cmd << " --rope-scale " << rope.rope_scale;
    }
    if (rope.rope_freq_base > 0.0f) {
        cmd << " --rope-freq-base " << rope.rope_freq_base;
    }
    if (rope.rope_freq_scale != 1.0f) {
        cmd << " --rope-freq-scale " << rope.rope_freq_scale;
    }

    // YaRN
    if (rope.rope_scaling == RoPESettings::RopeScaling::Yarn) {
        if (rope.yarn_orig_ctx > 0) {
            cmd << " --yarn-orig-ctx " << rope.yarn_orig_ctx;
        }
        if (rope.yarn_ext_factor != -1.0f) {
            cmd << " --yarn-ext-factor " << rope.yarn_ext_factor;
        }
        if (rope.yarn_attn_factor != 1.0f) {
            cmd << " --yarn-attn-factor " << rope.yarn_attn_factor;
        }
        if (rope.yarn_beta_slow != 1.0f) {
            cmd << " --yarn-beta-slow " << rope.yarn_beta_slow;
        }
        if (rope.yarn_beta_fast != 32.0f) {
            cmd << " --yarn-beta-fast " << rope.yarn_beta_fast;
        }
    }

    return cmd.str();
}

} // namespace core
} // namespace llama_gui
