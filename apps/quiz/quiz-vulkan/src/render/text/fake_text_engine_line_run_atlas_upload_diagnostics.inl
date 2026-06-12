std::optional<std::size_t> find_atlas_materialization_index_for_cluster(
    const std::vector<render_text_glyph_atlas_materialization_snapshot>& materializations,
    const std::size_t cluster_index)
{
    for (std::size_t index = 0; index < materializations.size(); ++index) {
        if (materializations[index].cluster_index == cluster_index) {
            return index;
        }
    }
    return std::nullopt;
}

const render_text_atlas_upload_request_snapshot* find_upload_request_for_materialization(
    const render_text_atlas_upload_request_bridge_snapshot& bridge,
    const std::size_t materialization_index,
    std::size_t& upload_request_index)
{
    for (std::size_t index = 0; index < bridge.requests.size(); ++index) {
        if (bridge.requests[index].materialization_index == materialization_index) {
            upload_request_index = index;
            return &bridge.requests[index];
        }
    }
    upload_request_index = 0;
    return nullptr;
}

render_text_atlas_page line_run_atlas_upload_page_for(
    const render_text_glyph_atlas_materialization_snapshot* materialization,
    const render_text_shaped_atlas_update_trace_snapshot* trace,
    const render_text_atlas_upload_request_snapshot* upload_request)
{
    if (materialization != nullptr && materialization->page.id != 0U) {
        return materialization->page;
    }
    if (trace != nullptr && trace->page.id != 0U) {
        return trace->page;
    }
    if (upload_request != nullptr && upload_request->upload_request.page.id != 0U) {
        return upload_request->upload_request.page;
    }
    return {};
}

std::string line_run_atlas_upload_blocker_reason_for(
    const fake_text_engine_shaping_line_run_evidence_snapshot& line_evidence,
    const render_text_rasterized_glyph_atlas_payload_snapshot* payload,
    const render_text_glyph_atlas_materialization_snapshot* materialization,
    const render_text_atlas_upload_request_snapshot* upload_request)
{
    if (upload_request != nullptr && upload_request->ok()) {
        return {};
    }
    if (!line_evidence.glyph_supported) {
        return "line/run cluster glyph is unsupported";
    }
    if (!line_evidence.cacheable) {
        return "line/run cluster is not cacheable";
    }
    if (materialization == nullptr) {
        return "line/run cluster has no atlas materialization";
    }
    if (!materialization->has_cache_key) {
        return "line/run cluster has no atlas cache key";
    }
    if (payload == nullptr) {
        return "line/run cluster has no raster payload record";
    }
    if (payload->skipped || payload->rgba_bytes == 0U) {
        return payload->diagnostic.empty()
            ? "line/run cluster has no uploadable raster payload"
            : payload->diagnostic;
    }
    if (!materialization->payload_byte_count_matches) {
        return "line/run cluster payload byte count mismatch";
    }
    if (!materialization->payload_upload_ready) {
        return "line/run cluster raster payload is not upload-ready";
    }
    if (!materialization->has_atlas_placement) {
        return "line/run cluster has no resident atlas slot";
    }
    if (!materialization->has_atlas_update && !materialization->clean_reuse) {
        return "line/run cluster has no atlas update";
    }
    if (!materialization->diagnostic.empty()) {
        return materialization->diagnostic;
    }
    if (upload_request != nullptr && !upload_request->diagnostic.empty()) {
        return upload_request->diagnostic;
    }
    if (!line_evidence.fallback_reason.empty()) {
        return line_evidence.fallback_reason;
    }
    return "line/run atlas upload is blocked";
}

void record_line_run_atlas_upload_diagnostics(fake_text_engine_diagnostics& diagnostics)
{
    diagnostics.line_run_atlas_uploads.clear();
    diagnostics.line_run_atlas_upload_policy = {};
    diagnostics.line_run_atlas_uploads.reserve(diagnostics.shaping_line_run_evidence.size());

    std::vector<std::string> unique_line_runs;
    std::vector<glyph_atlas_key> unique_cache_keys;
    std::vector<std::string> unique_page_keys;
    unique_line_runs.reserve(diagnostics.shaping_line_run_evidence.size());
    unique_cache_keys.reserve(diagnostics.shaping_line_run_evidence.size());
    unique_page_keys.reserve(diagnostics.shaping_line_run_evidence.size());

    for (const fake_text_engine_shaping_line_run_evidence_snapshot& line_evidence :
         diagnostics.shaping_line_run_evidence) {
        const std::optional<std::size_t> materialization_index =
            find_atlas_materialization_index_for_cluster(
                diagnostics.glyph_atlas_materializations,
                line_evidence.cluster_index);
        const render_text_glyph_atlas_materialization_snapshot* materialization =
            materialization_index.has_value()
                ? &diagnostics.glyph_atlas_materializations[*materialization_index]
                : nullptr;
        const render_text_rasterized_glyph_atlas_payload_snapshot* payload =
            find_rasterized_payload_for_cluster(
                diagnostics.rasterized_glyph_atlas_payloads,
                line_evidence.cluster_index);
        const render_text_shaped_atlas_update_trace_snapshot* trace =
            find_shaped_atlas_trace_for_cluster(
                diagnostics.shaped_atlas_update_traces,
                line_evidence.cluster_index);
        std::size_t upload_request_index = 0;
        const render_text_atlas_upload_request_snapshot* upload_request =
            materialization_index.has_value()
                ? find_upload_request_for_materialization(
                    diagnostics.atlas_upload_request_bridge,
                    *materialization_index,
                    upload_request_index)
                : nullptr;

        const render_text_atlas_page page =
            line_run_atlas_upload_page_for(materialization, trace, upload_request);
        const bool upload_ready = upload_request != nullptr
            && upload_request->status == render_text_atlas_upload_request_status::upload_ready;
        const bool clean_reuse = upload_request != nullptr
            && upload_request->status == render_text_atlas_upload_request_status::clean_reuse;
        const bool duplicate_suppressed = upload_request != nullptr
            && upload_request->status == render_text_atlas_upload_request_status::duplicate_suppressed;
        const bool reused = clean_reuse || duplicate_suppressed;
        const bool reused_atlas_payload = payload != nullptr && payload->reused_atlas_payload;
        const bool has_raster_payload =
            payload != nullptr && ((!payload->skipped && payload->rgba_bytes > 0U) || reused_atlas_payload);
        const bool raster_payload_upload_ready =
            payload != nullptr && (payload->upload_ready || reused_atlas_payload);
        const bool blocked = !upload_ready && !reused;
        const std::string blocker_reason = blocked
            ? line_run_atlas_upload_blocker_reason_for(
                line_evidence,
                payload,
                materialization,
                upload_request)
            : std::string{};
        const glyph_atlas_key cache_key = materialization != nullptr
            ? materialization->cache_key
            : (upload_request == nullptr ? glyph_atlas_key{} : upload_request->cache_key);
        const std::string stable_page_key = page.id == 0U
            ? std::string{}
            : render_text_glyph_atlas_upload_operation_stable_page_id_for(page);

        fake_text_engine_line_run_atlas_upload_snapshot snapshot{
            .cluster_index = line_evidence.cluster_index,
            .line_index = line_evidence.line_index,
            .run_index = line_evidence.run_index,
            .style_token = line_evidence.style_token,
            .cluster_byte_offset = line_evidence.cluster_byte_offset,
            .cluster_byte_count = line_evidence.cluster_byte_count,
            .cluster_codepoint_offset = line_evidence.cluster_codepoint_offset,
            .cluster_codepoint_count = line_evidence.cluster_codepoint_count,
            .shaped_glyph_ids = line_evidence.shaped_glyph_ids,
            .resolved_glyph_id = materialization == nullptr
                ? line_evidence.resolved_glyph_id
                : materialization->resolved_glyph_id,
            .resolved_face_id = materialization == nullptr
                ? line_evidence.resolved_face_id
                : materialization->resolved_face_id,
            .cluster_advance = line_evidence.cluster_advance,
            .line_advance = line_evidence.line_advance,
            .run_box_advance = line_evidence.run_box_advance,
            .cache_key = cache_key,
            .has_cache_key = materialization == nullptr
                ? (cache_key.face_id != 0U && cache_key.glyph_id != 0U && cache_key.pixel_size != 0U)
                : materialization->has_cache_key,
            .page = page,
            .stable_page_key = stable_page_key,
            .atlas_bounds = materialization == nullptr ? render_rect{} : materialization->atlas_bounds,
            .atlas_update_bounds =
                materialization == nullptr ? render_rect{} : materialization->atlas_update_bounds,
            .materialization_index = materialization_index.value_or(0U),
            .upload_request_index = upload_request == nullptr ? 0U : upload_request_index,
            .upload_request_id = upload_request == nullptr ? std::string{} : upload_request->request_id,
            .shaping_backend_library = line_evidence.backend_library,
            .shaping_backend_label = line_evidence.backend_label,
            .raster_backend_library = materialization == nullptr
                ? (payload == nullptr ? render_text_font_backend_library::deterministic_fake
                                      : payload->font_backend_library)
                : materialization->raster_font_backend_library,
            .raster_backend_label = materialization == nullptr
                ? (payload == nullptr ? std::string{} : payload->font_backend_label)
                : materialization->raster_font_backend_label,
            .adapter_status = line_evidence.adapter_status,
            .capability_status = line_evidence.capability_status,
            .source_bytes_status = line_evidence.source_bytes_status,
            .rasterizer_status = materialization == nullptr
                ? (payload == nullptr ? render_text_font_rasterizer_status::missing_font_source
                                      : payload->status)
                : materialization->rasterizer_status,
            .materialization_status = materialization == nullptr
                ? render_text_glyph_atlas_materialization_status::skipped_missing_cache_key
                : materialization->status,
            .upload_status = upload_request == nullptr
                ? render_text_atlas_upload_request_status::skipped_materialization
                : upload_request->status,
            .materialized_font_bytes = line_evidence.materialized_font_bytes,
            .used_harfbuzz = line_evidence.used_harfbuzz,
            .used_deterministic_fallback = line_evidence.used_deterministic_fallback,
            .has_line_run_evidence = true,
            .has_materialization = materialization != nullptr,
            .has_raster_payload = has_raster_payload,
            .raster_payload_upload_ready = raster_payload_upload_ready,
            .has_atlas_placement = materialization != nullptr && materialization->has_atlas_placement,
            .has_atlas_update = (materialization != nullptr && materialization->has_atlas_update)
                || (upload_request != nullptr && upload_request->has_upload_request),
            .has_upload_request = upload_request != nullptr && upload_request->has_upload_request,
            .upload_ready = upload_ready,
            .clean_reuse = clean_reuse,
            .duplicate_suppressed = duplicate_suppressed,
            .reused = reused,
            .skipped = upload_request == nullptr || upload_request->skipped,
            .blocked = blocked,
            .payload_byte_count_matches = upload_request == nullptr
                ? (materialization == nullptr || materialization->payload_byte_count_matches)
                : upload_request->payload_byte_count_matches,
            .payload_rgba_bytes = upload_request == nullptr
                ? (materialization == nullptr ? (payload == nullptr ? 0U : payload->rgba_bytes)
                                              : materialization->payload_rgba_bytes)
                : upload_request->payload_rgba_bytes,
            .upload_rgba_bytes = upload_request == nullptr
                ? (materialization == nullptr ? 0U : materialization->atlas_update_rgba_bytes)
                : upload_request->actual_upload_rgba_bytes,
            .fallback_reason = line_evidence.fallback_reason,
            .blocker_reason = blocker_reason,
            .diagnostic = blocked
                ? blocker_reason
                : (upload_request == nullptr ? std::string{} : upload_request->diagnostic),
        };

        ++diagnostics.line_run_atlas_upload_policy.cluster_count;
        const std::string line_run_key =
            std::to_string(snapshot.line_index) + ":" + std::to_string(snapshot.run_index);
        if (!contains_string(unique_line_runs, line_run_key)) {
            unique_line_runs.push_back(line_run_key);
        }
        if (snapshot.used_harfbuzz) {
            ++diagnostics.line_run_atlas_upload_policy.harfbuzz_cluster_count;
        }
        if (snapshot.used_deterministic_fallback) {
            ++diagnostics.line_run_atlas_upload_policy.deterministic_fallback_cluster_count;
        }
        if (snapshot.materialized_font_bytes) {
            ++diagnostics.line_run_atlas_upload_policy.materialized_font_byte_cluster_count;
        } else {
            ++diagnostics.line_run_atlas_upload_policy.missing_font_byte_cluster_count;
        }
        if (snapshot.has_materialization) {
            ++diagnostics.line_run_atlas_upload_policy.materialized_cluster_count;
        } else {
            ++diagnostics.line_run_atlas_upload_policy.missing_materialization_cluster_count;
        }
        if (snapshot.has_raster_payload) {
            ++diagnostics.line_run_atlas_upload_policy.raster_payload_cluster_count;
        } else {
            ++diagnostics.line_run_atlas_upload_policy.missing_raster_payload_cluster_count;
        }
        if (snapshot.raster_payload_upload_ready) {
            ++diagnostics.line_run_atlas_upload_policy.raster_payload_upload_ready_cluster_count;
        }
        if (snapshot.upload_ready) {
            ++diagnostics.line_run_atlas_upload_policy.upload_ready_cluster_count;
        }
        if (snapshot.clean_reuse) {
            ++diagnostics.line_run_atlas_upload_policy.clean_reuse_cluster_count;
        }
        if (snapshot.duplicate_suppressed) {
            ++diagnostics.line_run_atlas_upload_policy.duplicate_suppressed_cluster_count;
        }
        if (snapshot.reused) {
            ++diagnostics.line_run_atlas_upload_policy.reused_cluster_count;
        }
        if (snapshot.blocked) {
            ++diagnostics.line_run_atlas_upload_policy.blocked_cluster_count;
        }
        if (snapshot.skipped) {
            ++diagnostics.line_run_atlas_upload_policy.skipped_cluster_count;
        }
        if (!snapshot.payload_byte_count_matches) {
            ++diagnostics.line_run_atlas_upload_policy.payload_byte_count_mismatch_cluster_count;
        }
        if (!snapshot.fallback_reason.empty()) {
            ++diagnostics.line_run_atlas_upload_policy.fallback_reason_cluster_count;
        }
        if (snapshot.has_cache_key && !contains_glyph_atlas_key(unique_cache_keys, snapshot.cache_key)) {
            unique_cache_keys.push_back(snapshot.cache_key);
        }
        if (!snapshot.stable_page_key.empty() && !contains_string(unique_page_keys, snapshot.stable_page_key)) {
            unique_page_keys.push_back(snapshot.stable_page_key);
        }
        diagnostics.line_run_atlas_upload_policy.total_payload_rgba_bytes += snapshot.payload_rgba_bytes;
        diagnostics.line_run_atlas_upload_policy.total_upload_rgba_bytes += snapshot.upload_rgba_bytes;

        diagnostics.line_run_atlas_uploads.push_back(std::move(snapshot));
    }

    diagnostics.line_run_atlas_upload_policy.line_count =
        diagnostics.shaping_line_run_evidence_policy.line_count;
    diagnostics.line_run_atlas_upload_policy.run_count = unique_line_runs.size();
    diagnostics.line_run_atlas_upload_policy.unique_cache_key_count = unique_cache_keys.size();
    diagnostics.line_run_atlas_upload_policy.unique_page_key_count = unique_page_keys.size();
}
