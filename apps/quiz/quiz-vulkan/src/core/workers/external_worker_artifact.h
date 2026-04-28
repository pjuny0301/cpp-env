#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace quiz_vulkan::workers {

enum class external_worker_kind {
    pdf_import,
    ocr,
    ai_quiz_generation,
    proof_grading,
    code_execution,
};

struct worker_artifact_ref {
    std::string uri;
    std::string media_type;
    std::string sha256;
    std::uint64_t byte_size = 0;

    [[nodiscard]] bool valid() const noexcept
    {
        return !uri.empty();
    }
};

struct external_worker_artifact_manifest {
    std::string schema_version = "quiz-worker-artifact-v1";
    std::string id;
    external_worker_kind kind = external_worker_kind::ai_quiz_generation;
    std::string source_uri;
    std::string cache_key;
    std::uint64_t created_at_ms = 0;
    std::vector<worker_artifact_ref> inputs;
    std::vector<worker_artifact_ref> outputs;

    [[nodiscard]] bool valid() const noexcept
    {
        return schema_version == "quiz-worker-artifact-v1"
            && !id.empty()
            && !outputs.empty();
    }
};

struct worker_manifest_load_request {
    std::string manifest_uri;
};

enum class worker_manifest_status {
    loaded,
    not_found,
    invalid_manifest,
    unsupported_schema,
    unavailable,
};

struct worker_manifest_load_result {
    worker_manifest_status status = worker_manifest_status::unavailable;
    external_worker_artifact_manifest manifest;
    std::string diagnostic;

    [[nodiscard]] bool ok() const noexcept
    {
        return status == worker_manifest_status::loaded;
    }
};

class worker_artifact_store_interface {
public:
    virtual ~worker_artifact_store_interface() = default;

    virtual worker_manifest_load_result load_manifest(
        const worker_manifest_load_request& request) const = 0;
    virtual std::vector<external_worker_artifact_manifest> list_manifests(
        external_worker_kind kind) const = 0;
};

} // namespace quiz_vulkan::workers
