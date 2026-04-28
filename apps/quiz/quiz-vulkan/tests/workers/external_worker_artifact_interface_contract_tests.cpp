#include "core/workers/external_worker_artifact.h"

#include <concepts>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept WorkerArtifactStoreInterface = requires(
    const T& store,
    const workers::worker_manifest_load_request& request,
    workers::external_worker_kind kind) {
    { store.load_manifest(request) } -> std::same_as<workers::worker_manifest_load_result>;
    { store.list_manifests(kind) } -> std::same_as<std::vector<workers::external_worker_artifact_manifest>>;
};

static_assert(WorkerArtifactStoreInterface<workers::worker_artifact_store_interface>);

static_assert(requires(workers::worker_artifact_ref artifact) {
    { artifact.uri } -> std::same_as<std::string&>;
    { artifact.media_type } -> std::same_as<std::string&>;
    { artifact.sha256 } -> std::same_as<std::string&>;
    { artifact.byte_size } -> std::same_as<std::uint64_t&>;
    { artifact.valid() } -> std::same_as<bool>;
});

static_assert(requires(workers::external_worker_artifact_manifest manifest) {
    { manifest.schema_version } -> std::same_as<std::string&>;
    { manifest.id } -> std::same_as<std::string&>;
    { manifest.kind } -> std::same_as<workers::external_worker_kind&>;
    { manifest.source_uri } -> std::same_as<std::string&>;
    { manifest.cache_key } -> std::same_as<std::string&>;
    { manifest.created_at_ms } -> std::same_as<std::uint64_t&>;
    { manifest.inputs } -> std::same_as<std::vector<workers::worker_artifact_ref>&>;
    { manifest.outputs } -> std::same_as<std::vector<workers::worker_artifact_ref>&>;
    { manifest.valid() } -> std::same_as<bool>;
});

static_assert(requires(workers::worker_manifest_load_result result) {
    { result.status } -> std::same_as<workers::worker_manifest_status&>;
    { result.manifest } -> std::same_as<workers::external_worker_artifact_manifest&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ok() } -> std::same_as<bool>;
});

} // namespace
