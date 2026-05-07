# Desktop C++ External Dependencies

This directory stores isolated source snapshots and single-header libraries for the quiz native C++/Vulkan remake.

- `native-deps-manifest.md` records source URLs, tags/refs, checksums, licenses, local paths, reasons, and integration notes.
- Library directories are named by project and version/tag.
- Downloaded source directories are intentionally ignored by Git; keep `native-deps-manifest.md` tracked as the reproducible inventory before committing any large third-party source.
- Temporary download archives should go under `_tmp` and be removed after extraction unless the manifest documents why they remain.
- Do not place app build outputs, generated CMake files, or package-manager caches here.
