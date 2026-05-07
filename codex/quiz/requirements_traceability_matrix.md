# Quiz 요구사항 추적 매트릭스

마지막 갱신: 2026-05-08

이 문서는 요구사항 번호를 실행 순서가 아니라 추적 ID로 관리한다. 실제 구현 순서는 `big_plan.md`의 의존관계 기반 단계가 기준이며, 각 행은 루트 구현 문서, 하위 프로젝트 문서, 현재 C++/문서 증거를 연결한다.

최근 baseline 통합 증거:

- current: Vulkan command packet bridge를 통합하고 Windows MinGW focused Vulkan CTest 3/3 확인.
- `81514a6`: Vulkan backend에 draw-list batch를 command packet diagnostics로 변환하는 bridge, packet category counts, pipeline/resource prerequisite fallback tests 추가.
- current: input gesture/focus diagnostics와 optional decoder pipeline integration을 통합하고 Windows MinGW focused input CTest 6/6, focused image CTest 3/3 확인.
- `77ecf32`: optional third-party image decoder adapter를 `image_texture_pipeline` 경계로 연결해 fake decoder upload/cache diagnostics, adapter failure fallback, unavailable adapter diagnostics tests 추가.
- `5b080c8`: input engine에 wheel/touch cancel/focus traversal/pointer capture release-restart route diagnostics tests와 input-owned policy fields 추가.
- current: Windows MinGW full CTest 81/81 통과로 Vulkan frame handoff 포함 baseline 기준선 갱신.
- current: Vulkan frame pipeline handoff를 통합하고 Windows MinGW focused Vulkan CTest 5/5 및 `ctest -N` 81개 확인.
- `2656970`: Vulkan backend adapter에 loader/instance/device/swapchain/render-pass/pipeline/resource-binding/command-recording/submit/present readiness를 묶는 frame pipeline handoff와 fallback diagnostics tests 추가.
- current: font backend selection layer, optional third-party image decoder adapter, input IME/focus/caret hardening을 통합하고 Windows MinGW focused text/image/input/architecture CTest 8/8 및 `ctest -N` 80개 확인.
- `bad1552`: input engine/text input model에 IME composition_start/restart diagnostics, focus/caret/selection stale-preedit clearing, invalid IME UTF-8 rejection tests 추가.
- `c662239`: image engine에 optional third-party decoder adapter boundary, fake backend, standard decoder fallback, host-path/include guard tests 추가.
- `45fec4e`: text engine에 FreeType/HarfBuzz/utf8proc 후보를 logical metadata와 capability/fallback selection으로 표현하는 font backend selection layer 추가.
- `15d77ce`: app render frame report에 scene modifier error count/first error를 추가해 modifier stack failure가 조용히 사라지지 않도록 보강.
- `0a721e2`: architecture boundary test에 host-specific source path와 direct `build/external/lib/cpp/desktop` reference 차단 규칙 추가.
- `7505a63`: native desktop dependency manifest/README를 추적하고, FreeType/HarfBuzz/Vulkan/stb/miniaudio 등 다운로드된 대용량 source snapshots는 `build/external/lib/cpp/desktop/*/` ignore 규칙으로 Git 커밋 대상에서 분리.
- current: fake text backend-adapter injection, Vulkan pipeline readiness summary, image manifest boundary hardening을 통합하고 Windows MinGW focused text/Vulkan/image CTest 13/13 및 `ctest -N` 79개 확인.
- `96e5f31`: image manifest texture pipeline boundary tests를 강화해 manifest-owned snapshots와 include/dependency guard를 고정.
- `ae37ddf`: Vulkan shader/layout/graphics pipeline readiness를 함께 볼 수 있는 pipeline readiness summary와 tests 추가.
- `418f58c`: fake text engine에 optional font backend adapter injection path와 adapter outcome diagnostics tests 추가.
- current: platform input boundary contract tests를 통합하고 Windows MinGW focused input CTest 10/10 및 `ctest -N` 78개 확인.
- `e5042ec`: input translator/adapter path가 renderer picking, app actions, domain semantics를 노출하지 않는 boundary contract tests 추가.
- current: real font backend adapter와 Vulkan graphics pipeline readiness headers를 CMake public FILE_SET에 등록하고 Windows MinGW focused text/Vulkan CTest 5/5 및 `ctest -N` 77개 확인.
- `3b32db2`: Vulkan graphics pipeline readiness boundary, shader/layout prerequisite validation, vertex/raster/blend/depth state diagnostics tests 추가.
- `0287ddb`: text engine에 HarfBuzz/FreeType/DirectWrite-style backend adapter boundary와 fake adapter diagnostics tests 추가.
- current: Windows MinGW full CTest 76/76 통과. 현재 `ctest -N` 76개 확인.
- current: platform input engine adapter header를 CMake public FILE_SET에 등록하고 image manifest diagnostics와 함께 Windows MinGW focused input/image CTest 13/13 및 `ctest -N` 76개 확인.
- `f879a62`: image manifest texture pipeline에 source resolution, normalized key, cache/revision, placeholder, decode/upload status diagnostics 추가.
- `3db2baf`: translated platform input을 input_engine/gesture/text handling에 공급하는 input-only adapter와 tests 추가.
- current: Vulkan pipeline layout readiness header를 CMake public FILE_SET에 등록하고 font backend capability diagnostics와 함께 Windows MinGW focused text/Vulkan CTest 6/6 및 `ctest -N` 75개 확인.
- `6fd844d`: Vulkan descriptor-set/pipeline-layout readiness boundary, fake create/destroy diagnostics, push constant/binding validation tests 추가.
- `5f02ac7`: fake text engine diagnostics에 font backend capability mode를 연결하고 complex-script fallback/backend-supported behavior tests 추가.
- current: image manifest texture pipeline header를 CMake public FILE_SET에 등록하고 Windows MinGW focused image CTest 4/4 및 `ctest -N` 74개 확인.
- `a6645a8`: image manifest texture pipeline boundary를 `image_manifest_texture_pipeline.h`로 분리해 decode/upload/cache core header 부담을 낮춤.
- current: platform input translator header를 CMake public FILE_SET에 등록하고 Windows MinGW focused input CTest 8/8 및 `ctest -N` 74개 확인.
- `f161648`: input core에 raw-ish mouse/touch/key/wheel/char/IME samples를 기존 normalized `raw_platform_input_event`로 변환하는 platform input translator boundary와 tests 추가.
- current: font backend capability probe header를 CMake public FILE_SET에 등록하고 image manifest texture pipeline boundary와 함께 Windows MinGW focused text/image CTest 5/5 및 `ctest -N` 73개 확인.
- `56e3d0d`: image manifest/source-to-texture pipeline adapter, normalized source key, source revision invalidation, missing-source placeholder, path traversal rejection tests 추가.
- `7058a02`: text engine에 FreeType/HarfBuzz/DirectWrite 가능성을 data-only로 표현하는 font backend capability probe와 fallback-mode contract tests 추가.
- current: Vulkan shader module readiness header를 CMake public FILE_SET에 등록하고 Windows MinGW focused Vulkan CTest 3/3 및 `ctest -N` 72개 확인.
- `516c4bc`: Vulkan pipeline/cache 뒤에 data-only SPIR-V shader module readiness boundary, fake create/destroy diagnostics, shader stage/entry validation tests 추가.
- current: input gesture diagnostics split tests를 통합하고 Windows MinGW focused input CTest 7/7 및 `ctest -N` 71개 확인.
- `8f6b959`: input engine gesture diagnostics tests를 전용 파일로 추가해 swipe threshold, mouse/touch parity, drag cancel, long-press timing, wheel delta normalization을 고정.
- current: standard image pipeline cache-reuse diagnostics와 resolved glyph-id atlas diagnostics를 통합하고 Windows MinGW focused text/image CTest 7/7 및 `ctest -N` 70개 확인.
- `60c92fb`: text engine의 resolved glyph id를 shaping selection, glyph atlas key, raster payload, shaped-atlas update trace diagnostics로 일관되게 연결.
- `7a4c528`: standard image texture pipeline에 normalized cache key 재사용, revision invalidation, decode/upload attempt diagnostics tests 추가.
- current: standard decoder-backed image texture pipeline을 통합하고 Windows MinGW focused image CTest 2/2 및 `ctest -N` 70개 확인.
- `aa4ce88`: image texture pipeline에 standard decoder chain 기반 encoded bytes decode-to-upload wrapper와 decode failure diagnostics tests 추가.
- current: Vulkan queue submit adapter summary diagnostics를 통합하고 Windows MinGW focused Vulkan CTest 2/2 및 `ctest -N` 69개 확인.
- `b4b04fc`: Vulkan backend frame result와 renderer summary에 queue submit/present adapter checked/status/order/failure diagnostics 추가.
- current: text glyph-id resolver와 image standard decoder chain headers를 CMake public FILE_SET에 등록하고 Windows MinGW focused text/image CTest 6/6 및 `ctest -N` 69개 확인.
- `8d808cd`: image engine에 BMP/PPM-style decoder behavior와 PNG zlib inflater를 묶는 standard decoder chain/factory와 focused/contract tests 추가.
- `81cb2a8`: text engine에 deterministic glyph-id resolver, shaping selection glyph-id bridge, fake-engine diagnostics와 focused/contract tests 추가.
- current: Vulkan queue submit/present adapter header를 CMake public FILE_SET에 등록하고 Windows MinGW focused Vulkan CTest 2/2 및 `ctest -N` 67개 확인.
- `6c25c12`: Vulkan command-submit readiness 뒤에 queue submit/present adapter boundary, fake function table, submit-before-present ordering/failure mapping tests 추가.
- current: PNG zlib stored-block inflater header를 CMake public FILE_SET에 등록하고 Windows MinGW focused image CTest 2/2 및 `ctest -N` 66개 확인.
- `4695d21`: image engine에 zlib-wrapped stored/no-compression deflate inflater, Adler32 validation, PNG decoder stored-IDAT integration tests 추가.
- current: shaped glyph atlas-update tracing, PNG decoder-chain wrapper, Vulkan command-submit readiness headers를 CMake public FILE_SET에 등록하고 Windows MinGW focused CTest 7/7 및 `ctest -N` 65개 확인.
- `710c928`: Vulkan command-recording 다음 단계로 command-submit readiness boundary, sync/present precondition diagnostics와 focused/contract tests 추가.
- `2356495`: image engine에 PNG decoder-chain wrapper, injected inflater contract, deterministic PNG decode failure diagnostics와 focused/contract tests 추가.
- `e8f148a`: text engine에 shaped glyph/raster payload/atlas update trace diagnostics와 focused/contract tests 추가.
- current: PNG RGBA8 unfilter boundary를 CMake public FILE_SET에 등록하고 focused image CTest 2/2 및 `ctest -N` 62개 확인.
- `e2dc64a`: image engine에 PNG filter-none RGBA8 scanline unfilter boundary, unsupported filter/truncated/stride diagnostics와 focused/contract tests 추가.
- current: text shaping backend header를 CMake public FILE_SET에 등록하고 focused text CTest 3/3 및 `ctest -N` 61개 확인.
- `2751777`: text engine에 deterministic shaping backend boundary, shaped glyph diagnostics, fake_text_engine bridge와 focused/contract tests 추가.
- current: Vulkan command-recording readiness header를 CMake public FILE_SET에 등록하고 focused Vulkan CTest 4/4, full CTest 60/60, `ctest -N` 60개 확인.
- `ddf7271`: Vulkan render pass 다음 단계로 command-recording readiness, pipeline/framebuffer/command-buffer diagnostics와 focused/contract tests 추가.
- current: PNG decode boundary를 CMake public FILE_SET에 등록하고 focused image CTest 2/2 및 `ctest -N` 59개 확인.
- `ce93465`: image engine에 PNG decode plan/inflater boundary, row-byte validation, inflater unavailable/failure diagnostics와 focused/contract tests 추가.
- current: text rasterizer atlas-readiness integration focused text CTest 3/3 및 `ctest -N` 58개 확인.
- `ecacbb9`: fake text engine에 deterministic font rasterizer atlas payload diagnostics와 skipped/missing-byte policy tests 추가.
- current: PNG chunk scanner를 CMake public FILE_SET에 등록하고 focused image CTest 2/2 및 `ctest -N` 58개 확인.
- `f9205e6`: image engine에 PNG chunk scanner, IDAT byte/count metadata, chunk validity diagnostics와 focused/contract tests 추가.
- current: Vulkan render-pass/framebuffer readiness header를 CMake public FILE_SET에 등록하고 focused Vulkan CTest 6/6, full CTest 57/57, `ctest -N` 57개 확인.
- `34af925`: Vulkan swapchain 다음 단계로 render-pass/framebuffer readiness boundary와 focused/contract tests 추가.
- current: PNG header inspector를 CMake public FILE_SET에 등록하고 focused image CTest 1/1 및 `ctest -N` 56개 확인.
- `f668eda`: image engine에 PNG signature/IHDR metadata inspector와 focused/contract tests 추가.
- current: text font rasterizer header를 CMake public FILE_SET에 등록하고 focused text CTest 1/1 및 `ctest -N` 55개 확인.
- `0c0917f`: text engine에 deterministic fake font rasterizer boundary와 atlas-ready glyph payload helper/focused tests 추가.
- current: Windows MinGW full CTest 54/54 통과로 text/image/Vulkan worker 통합 라운드 기준선 갱신.
- current: Vulkan swapchain readiness header를 CMake public FILE_SET에 등록하고 focused Vulkan CTest 5/5 및 `ctest -N` 54개 확인.
- `d229547`: Vulkan device 다음 단계로 swapchain/surface extent/present-mode readiness boundary와 focused/contract tests 추가.
- current: image texture cache snapshot header를 CMake public FILE_SET에 등록하고 focused image CTest 3/3 통과 확인.
- `f7c5bf1`: image texture cache snapshot/residency diagnostics를 전용 header로 분리하면서 `image_texture_cache.h` 안정 include surface 유지.
- current: text coverage run segmentation header를 CMake public FILE_SET에 등록하고 focused text CTest 2/2 통과 확인.
- `d144683`: text engine에 coverage 기반 run segmentation helper와 focused/contract tests 추가.
- current: render image/text/Vulkan split headers를 CMake public FILE_SET에 등록해 worker가 추가한 interface headers를 target metadata에도 반영.
- `c12ec31`: Vulkan device readiness와 image texture upload split 결과를 RTM/current handoff 문서에 반영.
- `31275c0`: image texture upload/uploader helpers를 `image_texture_upload.h`로 분리해 `image_texture_cache.h` 토큰 비용 축소.
- `8f80b6e`: Vulkan instance 다음 단계로 device/queue readiness boundary와 focused tests 추가.
- `87935ed`: font Unicode coverage 결과를 `font_face_descriptor`/`font_face_catalog` coverage 범위로 연결하는 text catalog adapter 추가.
- `c83ed69`: text engine에 font Unicode coverage resolver와 contract/focused tests 추가.
- `20fc3e6`: Vulkan instance creation boundary를 전용 header로 분리하고 loader 역방향 include 없이 직접 include 계약으로 정리.
- `4f6af6e`: text engine에 font cmap Unicode coverage inspector와 contract/focused tests 추가.
- `e668cca`: Vulkan instance creation boundary, fake instance factory, lifecycle gate, focused tests 추가.
- `98a5e3d`: text engine에 SFNT/TrueType/OpenType font bytes inspector와 contract/focused tests 추가.
- `554ea90`: image texture placeholder policy helpers를 전용 header로 분리.
- `977e907`: Vulkan loader probe와 deterministic loader tests 추가.
- `4233267`: BMP decoder 구현을 image decoder contract header에서 분리.
- `7e62bc8`: text font/source initializer를 명시화해 MinGW warning 제거.
- `bbd8ecf`: 실제 uncompressed BMP image decoder와 BMP decoder focused tests 추가.
- `c091b53`: Vulkan resource binding diagnostics implementation을 전용 source 파일로 분리.
- `bdc9d21`: input keyboard/text-edit tests를 전용 파일로 분리.
- `964f515`: Vulkan diagnostic pipeline cache implementation을 전용 source 파일로 분리.
- `53bcc29`: input text route policy helpers를 전용 header로 분리.
- `8fd8c5a`: Vulkan pipeline cache diagnostics tests를 전용 파일로 분리.
- `0d865fd`: input pointer policy helpers를 전용 header로 분리.
- `ca4795e`: image upload retry policy를 전용 header로 분리.
- `73bb74b`: image engine architecture boundary lock 추가.
- `eadd012`: input touch arbitration tests를 전용 파일로 분리.
- `d222206`: image texture uploader tests를 전용 파일로 분리.
- `dc8e9f8`: input key policy helpers를 전용 header로 분리.
- `1ecdd14`: Vulkan frame lifecycle helpers를 전용 source/header로 분리.
- `0b94ca8`: input core architecture boundary lock 추가.
- `4411467`: fake text engine line layout helpers를 전용 header로 분리.
- `8ae5a73`: asset manifest version policy를 전용 header로 분리.
- `bc27039`: asset/text engine core boundary locks and explicit bridge exceptions 추가.
- `7dc9932`: Vulkan adapter name diagnostics를 전용 source 파일로 분리.
- `d0aee07`: image texture diagnostics helpers를 전용 header로 분리.
- `9dea60a`: Vulkan resource binding diagnostics tests를 전용 파일로 분리.
- `6561069`: image texture cache residency tests를 전용 파일로 분리.
- `81d5c03`: fake text layout diagnostics tests를 전용 파일로 분리.
- `4494d54`: input engine IME tests를 전용 파일로 분리.
- `afcb47d`: filesystem image pipeline fixture/cache/failure tests 추가.
- `48d1565`: Vulkan command recorder gate tests를 전용 파일로 분리.
- `bf35ad8`: materialized asset byte integrity e2e tests 추가.
- `795e210`: text font source byte loader tests를 전용 파일로 분리.
- `ee94009`: text font source file byte loader와 loader interface contract 추가.
- `d825a15`: asset byte integrity diagnostics 추가.
- `36b98db`: Vulkan command recorder gate diagnostics 추가.
- `e83ca14`: materialized asset byte load diagnostics와 provider gate 추가.
- `049bc0b`: text font source byte readiness/cache diagnostics 추가.
- `02ec079`: Vulkan descriptor/resource binding validation diagnostics 추가.
- `5fc2ea9`: runtime asset materialization diagnostics와 rooted local path policy 추가.
- `69ac57d`: filesystem image source byte loader와 image engine interface lock 추가.
- `8976633`: Vulkan frame resource initialization 정리.
- `66242e9`: Vulkan frame resource lifetime diagnostics 추가.
- `caf4504`: text font source diagnostics와 font source resolver 추가.
- `0c4dd6a`: asset cache key classification policy diagnostics 추가.
- `d132984`: text input IME selection range를 UTF-8-safe 경계로 보강.
- `8470a52`: worker build command serialization wrapper 추가.
- `3b1a3c2`: layout/modifier/render boundary 규칙 문서화.
- `6909c05`: worker follow-up prompt의 folder ownership과 검증 규칙 보강.
- `8df41ac`: fake texture residency/pinning/eviction diagnostics 추가.
- `949a2ad`: asset policy catalog duplicate/count/sorted snapshot diagnostics 추가.
- `179cf02`: fake text UTF-8 cluster, line break, line metrics diagnostics 추가.
- `30de379`: IME composition snapshot routing을 input event 계약에 반영.
- `7e2bb10`: Vulkan resource binding diagnostics 추가.
- `d586894`: fake image decoder format/size validation diagnostics 추가.
- `b9e44cc`: Vulkan resource registry diagnostics 추가.
- `c6688df`: asset manifest policy validation diagnostics 추가.
- `36e1a7f`: fake text glyph cache policy diagnostics 추가.
- `3d8dffe`: procedural audio mixer event unification 추가.
- `e851ec1`: input routing/capture diagnostics 추가.
- `3ef5c81`: asset runtime resolver policy diagnostics 추가.
- `8eb428a`: renderer architecture boundary lock 강화.
- `c755f1a`: text line-break policy diagnostics 추가.
- `89a4515`: image sampler/cache policy diagnostics 추가.
- `5400a28`: asset pack validation diagnostics 추가.
- `8b61a9a`: Vulkan frame lifecycle policy diagnostics 추가.
- `8db41dd`: input routing action policy diagnostics 추가.
- `b58c163`: text font resolution/readiness diagnostics 추가.
- `ebae9a5`: asset pack index diagnostics 추가.
- `09dfd97`: image texture lifetime/eviction diagnostics 추가.
- `c60f719`: Vulkan pipeline lifecycle diagnostics 추가.
- `e26950e`: input text edit policy diagnostics 추가.
- `f18523c`: IME preedit routing diagnostics 추가.
- `78e710d`: text atlas page diagnostics 추가.
- `fc76684`: asset pack lookup policy diagnostics 추가.
- `be85368`: image upload queue diagnostics 추가.
- `e0d7f4f`: Vulkan command-buffer submit diagnostics 추가.
- `8b14e43`: input gesture cancel/pointer-capture reset diagnostics 추가.
- `faff5a0`: text atlas upload-policy diagnostics 추가.
- `e2fab0f`: image upload retry/backoff diagnostics 추가.
- `bb2d517`: asset pack fallback/root-selection diagnostics 추가.
- `d3f6548`: input keyboard focus traversal/caret-edge diagnostics 추가.
- `db74963`: text atlas eviction/reuse diagnostics 추가.
- `1403fd4`: input pointer capture arbitration diagnostics 추가.
- `935231c`: asset catalog cache-policy diagnostics 추가.
- `725d805`: image placeholder texture-policy diagnostics 추가.
- `0c13cc8`: Vulkan frame-present/acquire backpressure diagnostics 추가.
- `1fe6490`: text font-face catalog/fallback diagnostics 추가.
- `d1300f8`: aggregate interface contract에 asset pack/runtime resolver policy locks 포함.
- `6f49fe4`: image decoder-chain candidate/source/support/failure diagnostics 추가.
- `360975a`: text line-layout policy, run-box, caret hit-test diagnostics 추가.
- `267f31d`: input gesture route-policy diagnostics와 rejected swipe/drag route snapshots 추가.
- `5b8b3d6`: asset manifest schema version/feature compatibility policy diagnostics 추가.
- `8710fa0`: image data URI source-byte loading and malformed data diagnostics 추가.
- `5965a4b`: Vulkan swapchain policy/pipeline compatibility/shader binding/fallback diagnostics 추가.
- `244d5b7`: input multipointer touch-like arbitration diagnostics 추가.
- `94a922f`: asset manifest integrity diagnostics 추가.
- 기준 검증 예시: 2026-05-07 Windows MinGW focused image CTest 2/2, focused Vulkan CTest 2/2, focused text/image CTest 6/6, focused input/image CTest 13/13, 최근 full CTest 76/76 통과. 현재 `ctest -N`이 76개 테스트를 보고한다. 권위 있는 테스트 목록은 항상 실행 시점의 `ctest -N`으로 확인한다.
- 기준 검증은 고정 개수로 적지 않는다. 현재 전체 테스트 수는 `ctest -N`이 권위이며, handoff에는 실행한 focused target만 기록한다.

상태 기준:

- `부분 구현`: native C++/Vulkan 또는 테스트/계약 코드에 실행 가능한 일부가 들어간 상태
- `계획 문서화`: 구현 방식 문서와 하위 프로젝트 연결은 있으나 앱 코드 증거는 아직 없는 상태
- `예약`: 원문 요구가 비어 있어 구현하지 않는 상태
- `원본 없음`: 원본 요구사항 번호가 존재하지 않는 상태

| ID | 단계 | 요구사항 요약 | 담당/하위 문서 | 현재 증거 | 상태 | 다음 확인 |
| --- | --- | --- | --- | --- | --- | --- |
| 01 | 0. 작업 기준 고정<br>3. Editor 데이터 제작 | 1\. 고성능 AI를 활용해 pdf에 있는 문제만 뽑아다가 챕터별 퀴즈형식으로 만드는 자동화프로그램 | [01](구현/01.md); [quiz-editor](quiz-editor/구현/01.md) | `src/core/domain/deck_artifact_loader.*`, `src/app/app.*`, `tests/domain/deck_artifact_loader_tests.cpp` | 부분 구현 | editor/worker 산출물 fixture를 `quiz_vulkan --deck` 실행 입력으로 연결 |
| 02 | 0. 작업 기준 고정<br>4. Vulkan 플레이어 재작성 | 2\. 퀴즈앱 퀴즈마다 디자인 업그레이드 | [02](구현/02.md); [quiz-vulkan](quiz-vulkan/구현/02.md) | `tests/render/vulkan_renderer_tests.cpp`, `tests/render/vulkan/vulkan_command_recorder_gate_tests.cpp`, `tests/render/text/font_source_bytes_loader_tests.cpp`, `tests/render/image/image_texture_pipeline_tests.cpp`, `src/render/vulkan/*`, `src/render/text/*`, `src/render/image/*`, fake text/image/vulkan diagnostics, texture sampler/cache/upload queue/retry/residency/lifetime eviction/decoder/decoder-chain/data-URI/filesystem source-byte/filesystem pipeline/placeholder texture-policy diagnostics, Vulkan frame/pipeline/command-buffer-submit/command-recorder-gate/frame-present/frame-resource/descriptor-validation/swapchain policy/pipeline compatibility/shader binding/fallback lifecycle/resource binding/registry diagnostics, text line-break/glyph cache/font-resolution readiness/font-face catalog/source/source-byte/file-byte-loader/line-layout/run-box/caret-hit-test/atlas-page/upload-policy/eviction diagnostics, asset pack lookup/fallback/cache-key/cache-policy/materialization/byte-provider/byte-integrity diagnostics, `tests/architecture/architecture_boundary_tests.cpp` | 부분 구현 | renderer backend, text atlas, image texture path를 실제 GPU 리소스 경로로 확대 |
| 03 | 0. 작업 기준 고정<br>3. Editor 데이터 제작 | 3\. 퀴즈에디터 AI 실시간 연동(퀴즈에디터에서 AI를 부르면 퀴즈에디터 위쪽으로 실시간 연동 | [03](구현/03.md); [quiz-editor](quiz-editor/구현/03.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 04 | 0. 작업 기준 고정<br>3. Editor 데이터 제작<br>6. 본문 학습/카툰/Codexbot | 4\. 퀴즈앱으로 본문학습까지 할 수 있게(pdf 원문, 번역본(claude 생성, git에 캐싱하여 토... | [04](구현/04.md); [android-quiz-app](android-quiz-app/구현/04.md), [quiz-editor](quiz-editor/구현/04.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 05 | 0. 작업 기준 고정<br>1. 공통 데이터 계약/도메인 | 5\. 위 프로젝트를 만드는데 필요한 요구사항 트리로 표현(블로그에 있는 기법과 관련시킴) | [05](구현/05.md); [quiz-vulkan](quiz-vulkan/구현/05.md) | `codex/quiz/big_plan.md`, this RTM | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 06 | 0. 작업 기준 고정<br>2. Android UX 기준/FSM | 6.퀴즈를 옆으로 튕기듯 슬라이드시키면 정답표시같은 거 없이 다음 문제로 바로 넘어감. 몇번이상 문제를... | [06](구현/06.md); [android-quiz-app](android-quiz-app/구현/06.md), [quiz-vulkan](quiz-vulkan/구현/06.md) | `tests/app/app_state_tests.cpp`, `src/core/domain/learning_state.*`, `src/core/input/gesture_recognizer.*`, `src/core/input/input_engine.*`, `tests/input/*`, gesture route-policy/rejected swipe/drag/multipointer touch diagnostics | 부분 구현 | 실제 Win32/touch pointer 이벤트와 모바일 surface 검증 |
| 07 | 0. 작업 기준 고정<br>2. Android UX 기준/FSM | 7\. select 유형에서 선택하면 정답 빨간박스를 따로 띄우지 않음 | [07](구현/07.md); [android-quiz-app](android-quiz-app/구현/07.md), [quiz-vulkan](quiz-vulkan/구현/07.md) | `tests/app/app_state_tests.cpp`, `tests/app/app_quiz_screens_tests.cpp` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 08 | 0. 작업 기준 고정<br>2. Android UX 기준/FSM | 8\. select 유형에서 OX표시 제거. multiselect multiblank blank 유형에선... | [08](구현/08.md); [android-quiz-app](android-quiz-app/구현/08.md), [quiz-vulkan](quiz-vulkan/구현/08.md) | `tests/app/app_quiz_screens_tests.cpp` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 09 | 0. 작업 기준 고정<br>2. Android UX 기준/FSM | 9\. multiselect 유형은 정답을 여러번 선택해야하는 유형, 빈칸에 차례대로 표시되게 함. 속도... | [09](구현/09.md); [android-quiz-app](android-quiz-app/구현/09.md), [quiz-vulkan](quiz-vulkan/구현/09.md) | `src/core/domain/quiz_session.*`, `tests/domain/domain_smoke_test.cpp` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 10 | 0. 작업 기준 고정<br>1. 공통 데이터 계약/도메인 | 리팩토링 | [10](구현/10.md); [quiz-vulkan](quiz-vulkan/구현/10.md) | `src/core/domain/*`, `src/app/app_state.*` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 11 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | 수학 증명 쓰면 AI가 채점 | [11](구현/11.md); [quiz-editor](quiz-editor/구현/11.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 12 | 0. 작업 기준 고정<br>3. Editor 데이터 제작 | pdf 등록하면 AI가 넘기는 장마다 텍스트로 추출, 사진과 인덱싱 AI 에이전트가 학생들이 헷갈릴만한... | [12](구현/12.md); [quiz-editor](quiz-editor/구현/12.md) | `src/core/domain/deck_artifact_loader.*`, `src/app/app.*`, `tests/domain/deck_artifact_loader_tests.cpp` | 부분 구현 | PDF/AI worker artifact fixture를 `--deck` 앱 실행으로 검증 |
| 13 | 0. 작업 기준 고정<br>3. Editor 데이터 제작 | 정답일 때 왜 이게 정답인지 적게한다 | [13](구현/13.md); [quiz-editor](quiz-editor/구현/13.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 14 | 0. 작업 기준 고정<br>3. Editor 데이터 제작 | 정답 사유가 틀렸거나 오답이면 AI와 토론할 수 있게 한다 | [14](구현/14.md); [quiz-editor](quiz-editor/구현/14.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 15 | 0. 작업 기준 고정<br>3. Editor 데이터 제작 | 텍스트로 추출하고 텍스트문서에 파일경로가 적힌 텍스트 사진과 인덱싱하는 프로그램 개발 | [15](구현/15.md); [quiz-editor](quiz-editor/구현/15.md) | `src/core/domain/deck_artifact_loader.*`, `src/app/app.*`, `tests/domain/deck_artifact_loader_tests.cpp` | 부분 구현 | source page/paragraph/image path fixture를 앱 실행 입력까지 확대 |
| 16 | 0. 작업 기준 고정<br>1. 공통 데이터 계약/도메인 | 기능을 모듈화(프로그램으로 나누고, 각 기능이 공유하는 일반화된 표준 개발 | [16](구현/16.md); [quiz-vulkan](quiz-vulkan/구현/16.md) | `src/core/domain/*`, `tests/domain/domain_smoke_test.cpp` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 17 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | **수학 수식 필기 인식 (OCR):** 11번(수학 증명) 기능과 연동하여, 태블릿에서 직접 쓴 수식을... | [17](구현/17.md); [quiz-editor](quiz-editor/구현/17.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 18 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | 수학 수식 필기 인식 (OCR): 11번(수학 증명) 기능과 연동하여, 태블릿에서 직접 쓴 수식을 텍스트... | [18](구현/18.md); [quiz-editor](quiz-editor/구현/18.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 19 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | **포커스 모드 (Zettelkasten 연동):** 퀴즈를 풀다 막히면 해당 개념과 연결된 자신의 과거... | [19](구현/19.md); [quiz-editor](quiz-editor/구현/19.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 20 | 0. 작업 기준 고정<br>2. Android UX 기준/FSM | **망각 곡선 기반 에이징 (Spaced Repetition System, SRS):** 6번의 "이미... | [20](구현/20.md); [android-quiz-app](android-quiz-app/구현/20.md), [quiz-vulkan](quiz-vulkan/구현/20.md) | `src/core/domain/learning_state.*`, `tests/app/app_state_tests.cpp` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 21 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | 개념 의존성 그래프 트래킹: AI가 PDF 지문을 추출할 때(12번), 단순히 텍스트만 나누는 것이 아니... | [21](구현/21.md); [quiz-editor](quiz-editor/구현/21.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 22 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | 코드/로직 스니펫 샌드박스 채점: 수학 증명(11번)과 결합하여, 프로그래밍이나 알고리즘 문제의 경우 학... | [22](구현/22.md); [quiz-editor](quiz-editor/구현/22.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 23 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | ​리버스 엔지니어링 / 디버깅 퀴즈: AI가 의도적으로 논리적 비약이 있는 수학 증명이나, 미세한 버그(... | [23](구현/23.md); [quiz-editor](quiz-editor/구현/23.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 24 | 0. 작업 기준 고정<br>1. 공통 데이터 계약/도메인<br>4. Vulkan 플레이어 재작성 | ​**오토마타(FSM) 기반 퀴즈 플로우 및 상태 관리:** 6번(슬라이드), 14번(AI 토론), 19... | [24](구현/24.md); [quiz-vulkan](quiz-vulkan/구현/24.md) | `src/app/app_state.*`, `src/app/app_action_router.*`, `src/core/layout/input_hit_test.h`, `src/platform/platform_shell.h`, `src/core/input/*`, `tests/app/app_action_router_tests.cpp`, `tests/layout/input_hit_test_tests.cpp`, input/IME composition snapshot and input routing/capture/action/text-edit/UTF-8-safe IME preedit/selection/gesture-cancel/focus-traversal/pointer-capture arbitration/gesture route-policy/multipointer touch diagnostics focused tests | 부분 구현 | normalized input/IME route를 app action과 창 실행까지 연결 |
| 26 | 0. 작업 기준 고정<br>1. 공통 데이터 계약/도메인<br>3. Editor 데이터 제작 | 책에서 주요개념과 책에 나온 관련 설명 퀴즈앱에 MultiSelect 유형으로 그대로 매핑(인덱스로 해서... | [26](구현/26.md); [android-quiz-app](android-quiz-app/구현/26.md), [quiz-editor](quiz-editor/구현/26.md) | `src/core/domain/quiz_model.*`, `src/core/domain/deck_artifact_loader.*` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 27 | 0. 작업 기준 고정<br>2. Android UX 기준/FSM | 유저가 빈캰뚫을 수 있게 | [27](구현/27.md); [android-quiz-app](android-quiz-app/구현/27.md), [quiz-editor](quiz-editor/구현/27.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 28 | 0. 작업 기준 고정<br>1. 공통 데이터 계약/도메인<br>3. Editor 데이터 제작 | 문단 / 문제 개념 / 정답 | [28](구현/28.md); [android-quiz-app](android-quiz-app/구현/28.md), [quiz-editor](quiz-editor/구현/28.md) | `src/core/domain/quiz_model.*`, `src/core/domain/deck_artifact_loader.*` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 29 | 0. 작업 기준 고정<br>3. Editor 데이터 제작 | 선지의 오답들은 정답인 개념과 헷갈리되 절대 아닌 것들로 구성 | [29](구현/29.md); [android-quiz-app](android-quiz-app/구현/29.md), [quiz-editor](quiz-editor/구현/29.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 30 | 0. 작업 기준 고정<br>2. Android UX 기준/FSM | 왼쪽으로 슬라이드하면 이전 퀴즈 볼 수 있게 | [30](구현/30.md); [android-quiz-app](android-quiz-app/구현/30.md), [quiz-vulkan](quiz-vulkan/구현/30.md) | `tests/app/app_state_tests.cpp` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 31 | 0. 작업 기준 고정<br>2. Android UX 기준/FSM<br>4. Vulkan 플레이어 재작성 | 빈칸유형에 입력하기 위해서 입력창 터치하면 키보드가 위로 올라오면서 문제가 위로 밀리고 화면밖으로 나가서... | [31](구현/31.md); [android-quiz-app](android-quiz-app/구현/31.md), [quiz-vulkan](quiz-vulkan/구현/31.md) | `src/core/layout/layout_placer.h`, `src/app/app_quiz_screens.h`, `src/app/app.cpp`, `src/app/app_demo.cpp`, `src/core/input/text_input_model.*`, `src/core/input/text_input_types.h`, `src/core/input/input_engine.*`, `tests/app/app_demo_tests.cpp`, `tests/app/app_quiz_screens_tests.cpp`, `tests/input/text_input_model_tests.cpp`, `tests/input/input_engine_tests.cpp`, input routing/text-edit/UTF-8-safe IME preedit/selection/gesture-cancel/focus-traversal/pointer-capture arbitration/gesture route-policy/multipointer touch diagnostics | 부분 구현 | 실제 OS 키보드/IME 입력과 blank 답안 제출을 창 실행으로 재확인 |
| 32 | 0. 작업 기준 고정<br>2. Android UX 기준/FSM | 유저가 옵션에서 허용시 한 번 틀리면 그 문제가 오답노트로 이동하는 기능도 추가 유저가 설정한 숫자만큼... | [32](구현/32.md); [android-quiz-app](android-quiz-app/구현/32.md), [quiz-vulkan](quiz-vulkan/구현/32.md) | `src/app/app_state.cpp`, `src/core/domain/learning_state.*`, `tests/app/app_state_tests.cpp` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 33 | 0. 작업 기준 고정<br>7. 배포/동기화/QA | 지금 하고있는 배포/테스트/디자인파이프라인 문제점 분석 -> 제언 | [33](구현/33.md); [quiz-editor](quiz-editor/구현/33.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 34 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | 수학증명을 태블릿으로 쓰거나 찍어서 바로 앱으로 전달할 수 있게 함. 카메라모드로 쓸지 태블릿증명모드로... | [34](구현/34.md); [quiz-editor](quiz-editor/구현/34.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 35 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | 옵션에 고급옵션 추가 | [35](구현/35.md); [android-quiz-app](android-quiz-app/구현/35.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 36 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | 고급옵션에 카메라로 쓸지 태블릿으로 쓸지 선택하는 창 뜨게 할 수 있음 | [36](구현/36.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 37 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | 옵션에서 태블릿에 뭐 안적혀있으면 제출할 때 바로 카메라로 이동하게 설정가능 | [37](구현/37.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 38 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | 이미지분석 -> 텍스트 변환 | [38](구현/38.md); [quiz-editor](quiz-editor/구현/38.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 39 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | 텍스트 -> LaTex 변환 | [39](구현/39.md); [quiz-editor](quiz-editor/구현/39.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 40 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | LaTex 변환하는 AI에게 자신만의 표기법이 뭔지 설명해서 LaTex 변환에 뱐영하게 할 수 있음. | [40](구현/40.md); [quiz-editor](quiz-editor/구현/40.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 41 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | LaTex 채점하는 AI | [41](구현/41.md); [quiz-editor](quiz-editor/구현/41.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 42 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | 그 AI가 왜 틀렸는지 설명하는 걸 표기법대로 고쳐서 설명함 | [42](구현/42.md); [quiz-editor](quiz-editor/구현/42.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 43 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | 정답만 채점 / 과정도 채점 옵션에서 선택가능 | [43](구현/43.md); [quiz-editor](quiz-editor/구현/43.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 44 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | 정답 채점시 태블릿에서 어느 부분이 왜 틀렸는지 AI가 직접 스크롤해서 그 부분을 가리키면서 설명 | [44](구현/44.md); [quiz-editor](quiz-editor/구현/44.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 45 | 0. 작업 기준 고정<br>3. Editor 데이터 제작 | 오답 생성 | [45](구현/45.md); [quiz-editor](quiz-editor/구현/45.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 46 | 0. 작업 기준 고정<br>4. Vulkan 플레이어 재작성<br>6. 본문 학습/카툰/Codexbot | pdf 텍스트 불러올시 오른쪽에 흐릿한 번역본 창 띄움. 움직이지 않으며 테두리가 없고 옆에 있는 본문이... | [46](구현/46.md); [android-quiz-app](android-quiz-app/구현/46.md), [quiz-vulkan](quiz-vulkan/구현/46.md) | `src/core/scene/*`, `src/core/ui/*` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 47 | 0. 작업 기준 고정<br>4. Vulkan 플레이어 재작성 | 단답형에 OX 표시 추가할 땐 화면과 같은 색계열에서 흐리게 투명도 높게 한다 또한 답은 퀴즈 바로 아래... | [47](구현/47.md); [android-quiz-app](android-quiz-app/구현/47.md), [quiz-vulkan](quiz-vulkan/구현/47.md) | `src/app/app_quiz_screens.h` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 48 | 0. 작업 기준 고정 | 예약/미정 | [48](구현/48.md) | - | 예약 | 요구 원문 확정 후 프로젝트 배정 |
| 49 | 0. 작업 기준 고정<br>4. Vulkan 플레이어 재작성 | 장문 문제인 경우와 짧은 문제를 나눈다(이미 나눠진다면 장문 기준을 더 짧게 한다. \[50번부터 56번... | [49](구현/49.md); [android-quiz-app](android-quiz-app/구현/49.md), [quiz-vulkan](quiz-vulkan/구현/49.md) | `src/core/layout/layout_placer.h` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 50 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | 문제 전환하는 애니메이션 없앤다 | [50](구현/50.md); [quiz-vulkan](quiz-vulkan/구현/50.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 51 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | 선지 유형에서 정답 맞았을 때 애니메이션 없앤다 | [51](구현/51.md); [quiz-vulkan](quiz-vulkan/구현/51.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 52 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | 정답 맞았을 때 나머지 흐려지는 거 없앤다 | [52](구현/52.md); [quiz-vulkan](quiz-vulkan/구현/52.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 53 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | 정답 틀렸을 때 나머지 | [53](구현/53.md); [quiz-vulkan](quiz-vulkan/구현/53.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 54 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | 선지를 직사각형으로 만든다 | [54](구현/54.md); [quiz-vulkan](quiz-vulkan/구현/54.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 55 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | 선지에서 번호를 없앤다 | [55](구현/55.md); [quiz-vulkan](quiz-vulkan/구현/55.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 56 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | PASS를 오른쪽 끝에 놓고 틀렸으면 다음으로 바꾼다 | [56](구현/56.md); [quiz-vulkan](quiz-vulkan/구현/56.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 57 | 0. 작업 기준 고정<br>2. Android UX 기준/FSM | "이미 앎" 문제는 정답이 아닌 스와이프를 통해서만 걀 수 있도록 한다 | [57](구현/57.md); [android-quiz-app](android-quiz-app/구현/57.md), [quiz-vulkan](quiz-vulkan/구현/57.md) | `src/core/domain/quiz_session.*` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 58 | 0. 작업 기준 고정<br>2. Android UX 기준/FSM | "이미 앎" 탭으로는 하단에 있는 바를 통해서 이동할 수 있도록 한다. | [58](구현/58.md); [android-quiz-app](android-quiz-app/구현/58.md), [quiz-vulkan](quiz-vulkan/구현/58.md) | `src/app/app_quiz_screens.h`, `tests/app/app_quiz_screens_tests.cpp`, `src/core/domain/quiz_session.*` | 부분 구현 | bottom navigation을 실제 input route와 연결 |
| 59 | 0. 작업 기준 고정<br>2. Android UX 기준/FSM | "오답노트"도 하단에 있는 바를 통해서 이동할 수 있도록 하는데 오답노트가 적용된 덱만 이동할 수 있도록... | [59](구현/59.md); [android-quiz-app](android-quiz-app/구현/59.md), [quiz-vulkan](quiz-vulkan/구현/59.md) | `src/core/domain/quiz_session.*`, `src/app/app_quiz_screens.h`, `tests/app/app_state_tests.cpp`, `tests/app/app_quiz_screens_tests.cpp` | 부분 구현 | wrong_note mode를 renderer/app interaction까지 연결 |
| 60 | 0. 작업 기준 고정<br>2. Android UX 기준/FSM | 퀴즈 위에 어떤 유형인지는 표시하지 않음 | [60](구현/60.md); [android-quiz-app](android-quiz-app/구현/60.md), [quiz-vulkan](quiz-vulkan/구현/60.md) | `src/core/domain/quiz_session.*` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 61 | 0. 작업 기준 고정<br>2. Android UX 기준/FSM | 다중빈칸카드유형 주가, 위쪽에 카드를 많이 띄워놓고 전부 배치하면 어느 부분이 오답인지 표기 | [61](구현/61.md); [android-quiz-app](android-quiz-app/구현/61.md), [quiz-vulkan](quiz-vulkan/구현/61.md) | `src/core/domain/quiz_session.*` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 62 | 0. 작업 기준 고정<br>7. 배포/동기화/QA | 달력앱과 연동기능 추가 | [62](구현/62.md); [android-quiz-app](android-quiz-app/구현/62.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 63 | 0. 작업 기준 고정<br>3. Editor 데이터 제작 | git repo 옵션으로 빼기 | [63](구현/63.md); [android-quiz-app](android-quiz-app/구현/63.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 64 | 0. 작업 기준 고정<br>4. Vulkan 플레이어 재작성 | Deck은 마치 첨부할 암기고래 앱같이 3 x n걔 박스가 늘어선 형태로 하고 Day는 마찬가지로 암기고... | [64](구현/64.md); [android-quiz-app](android-quiz-app/구현/64.md), [quiz-vulkan](quiz-vulkan/구현/64.md) | `src/app/app_quiz_screens.h` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 65 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker | 그 쟝에서 나오는 개념을 보고 백지에 요약하면 AI가 그 내용이 요약인지, 빠뜨린 건 없는지 판단 | [65](구현/65.md); [quiz-editor](quiz-editor/구현/65.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 66 | 0. 작업 기준 고정<br>4. Vulkan 플레이어 재작성 | UI 세련되고 통일성있게 | [66](구현/66.md); [android-quiz-app](android-quiz-app/구현/66.md), [quiz-vulkan](quiz-vulkan/구현/66.md) | `src/core/layout/layout_placer.h` | 부분 구현 | 프로젝트 구현 문서의 검증 기준을 native 테스트로 확대 |
| 67 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | pdf 본문 번역본 따로 표시되는 화면 | [67](구현/67.md); [android-quiz-app](android-quiz-app/구현/67.md), [quiz-vulkan](quiz-vulkan/구현/67.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 68 | 0. 작업 기준 고정<br>3. Editor 데이터 제작 | 풀면서 문제 보이면 폰에서 즉시 수정할 수 있게 | [68](구현/68.md); [android-quiz-app](android-quiz-app/구현/68.md), [quiz-editor](quiz-editor/구현/68.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 69 | 0. 작업 기준 고정<br>5. AI/OCR/Proof/Code worker<br>7. 배포/동기화/QA | AI 구현할 때 토큰비용 과도해지지 않게 모듈화 | [69](구현/69.md); [quiz-editor](quiz-editor/구현/69.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 70 | 0. 작업 기준 고정<br>3. Editor 데이터 제작<br>6. 본문 학습/카툰/Codexbot | pdf가 본문 + 문제 형식인 경우에는 문제는 퀴즈로 보내고 본문은 앱에다 배치함. 특정 본문을 설명하기... | [70](구현/70.md); [android-quiz-app](android-quiz-app/구현/70.md), [quiz-editor](quiz-editor/구현/70.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 71 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | 데톱앱으로 학습시 코덱스봇 캐릭터 왼쪽에 코덱스봇과 세로로 비슷한 크기의 말풍선에 설명 씌여있는 형식도... | [71](구현/71.md); [quiz-vulkan](quiz-vulkan/구현/71.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 72 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | 데톱앱으로 코덱스봇 캐릭터와 함께 학습시 연필이 종이에 쓰는 이모티콘이 각 이모티콘마다 달린 왼쪽의 말풍... | [72](구현/72.md); [quiz-vulkan](quiz-vulkan/구현/72.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 73a | 6. 본문 학습/카툰/Codexbot | 말풍선은 마치 게임의 npc대화창처럼 구성되어있고 뒤로 넘기는 식으로 대화함. 코덱스봇과 대화하다보면 연... | [73a](구현/73a.md); [quiz-vulkan](quiz-vulkan/구현/73a.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 73b | 6. 본문 학습/카툰/Codexbot | AI가 배정된 챕터의 텍스트를 받고 71 72를 짜되 앞으로 다시 생성하는 일 없게 파일로 생성결과물은... | [73b](구현/73b.md); [quiz-editor](quiz-editor/구현/73b.md), [quiz-vulkan](quiz-vulkan/구현/73b.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 74 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | 말풍선은 흐린(파스텔 계열) 노란색, 코덱스봇 역시 비슷한 파란색, 연필, 코덱스봇은 카툰형, 글꼴은 둥... | [74](구현/74.md); [quiz-vulkan](quiz-vulkan/구현/74.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 75 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | 선지 애니메이션은 76의 테마구현자 자유 | [75](구현/75.md); [quiz-vulkan](quiz-vulkan/구현/75.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 76 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | 퀴즈앱의 디자인(덱선택창, 메인화면, 선택형문제선지 디자인) 등을 마음대로 제작하고 적용할 수 있는 테마... | [76](구현/76.md); [quiz-vulkan](quiz-vulkan/구현/76.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 77 | 0. 작업 기준 고정<br>3. Editor 데이터 제작 | Deck에 pdf 넣으면 서버로 전송 -> 텍스트화, 퀴즈화후 다시 폰으로 전송 -> 다음 앱에 접속시... | [77](구현/77.md); [quiz-editor](quiz-editor/구현/77.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 78 | 0. 작업 기준 고정<br>4. Vulkan 플레이어 재작성 | 앱 자체가 굉장히 가볍고 빠르게 | [78](구현/78.md); [quiz-vulkan](quiz-vulkan/구현/78.md) | `src/render/render_draw_list.h`, `src/render/vulkan/vulkan_renderer.*`, CPU fallback RGBA framebuffer, `src/render/text/fake_text_engine.*`, `src/render/image/image_texture_pipeline.h`, render-only text/image/vulkan tests, `tests/render/vulkan/vulkan_command_recorder_gate_tests.cpp`, `tests/render/text/font_source_bytes_loader_tests.cpp`, `tests/render/image/image_texture_pipeline_tests.cpp`, frame/pipeline/command-buffer-submit/command-recorder-gate/frame-present/frame-resource/descriptor-validation/swapchain policy/pipeline compatibility/shader binding/fallback lifecycle/sync/resource binding/resource registry/text line/glyph cache/font-resolution/font-face catalog/source/source-byte/file-byte-loader/line-layout/run-box/caret-hit-test/atlas-page/upload-policy/eviction and image sampler/cache/upload queue/retry/residency/lifetime eviction/decoder/decoder-chain/data-URI/filesystem source-byte/filesystem pipeline/placeholder texture diagnostics | 부분 구현 | renderer backend 교체 전까지 CPU fallback diagnostics와 framebuffer smoke 유지 |
| 79 | 0. 작업 기준 고정<br>1. 공통 데이터 계약/도메인<br>7. 배포/동기화/QA | 위 계획들을 일관적으로 시행하기 위해서는 어찌해야하는지? | [79](구현/79.md); [quiz-vulkan](quiz-vulkan/구현/79.md) | `tests/test_manifest.json`, `tools/run_windows_mingw_ascii.ps1`, `codex-workers/with-build-lock.sh`, `CMakeLists.txt`, `quiz_vulkan.exe --help`, C++23 compile features, aggregate interface lock `tests/contracts/interface_contract_compile_tests.cpp`, architecture boundary lock, asset pack index/lookup/fallback/validation/catalog cache-key/cache-policy/runtime resolver/materialization/byte-provider/byte-integrity/version/integrity policy diagnostics, 앱 렌더 경계 `layout_placer -> ui_renderer -> render_draw_list -> vulkan_renderer`, large-file split policy in `codex/quiz/current.md`, `quiz_vulkan_render_contract` CMake target, latest full CTest handoff evidence | 부분 구현 | CI에서도 MinGW runtime DLL 복사와 direct exe 실행 검증 |
| 80 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | 퀴즈앱 전반을 카툰렌더링게임처렁 만들고싶음. 아까 코덱스갸 나와서 설명하는 것도 카툰렌더링게임의 npc가... | [80](구현/80.md); [quiz-vulkan](quiz-vulkan/구현/80.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 81 | 0. 작업 기준 고정<br>6. 본문 학습/카툰/Codexbot | 토큰관리햐기 위해선 어찌해야하는지? | [81](구현/81.md); [quiz-vulkan](quiz-vulkan/구현/81.md) | - | 계획 문서화 | 하위 프로젝트 구현 착수 시 fixture/test 추가 |
| 82 | 0. 작업 기준 고정<br>4. Vulkan 플레이어 재작성 | vulkan 렌더러로 구현 - 순수 c++ | [82](구현/82.md); [quiz-vulkan](quiz-vulkan/구현/82.md) | `src/app/app_demo.*`, `src/app/app.cpp`, `src/app/main.cpp`, `src/platform/windows_platform_shell.cpp`, `tests/app/app_demo_tests.cpp`, `src/render/render_draw_list.h`, `src/render/vulkan/*`, `src/render/text/*`, `src/render/image/*`, `tests/render/vulkan/vulkan_command_recorder_gate_tests.cpp`, `tests/render/text/font_source_bytes_loader_tests.cpp`, `tests/render/image/image_texture_pipeline_tests.cpp`, renderer가 `core/scene`, `core/ui`, `core/domain` 없이 render-only draw list 소비, CPU fallback framebuffer present path, Vulkan frame sync/pipeline/command-buffer-submit/command-recorder-gate/frame-present/frame-resource/descriptor-validation/swapchain policy/pipeline compatibility/shader binding/fallback lifecycle/resource binding/resource registry diagnostics, text font source file-byte loader, image data URI/filesystem source-byte/filesystem pipeline/placeholder texture diagnostics | 부분 구현 | swapchain/text backend 추가 전까지 app snapshot render path와 입력 route 확대 |
| 25 | 0. 작업 기준 고정 | 원본 요구사항 목록에 25번 없음 | - | - | 원본 없음 | 번호 재사용 금지, 새 요구는 새 ID로 추가 |

## 코드 증거 묶음

- Domain/FSM: `apps/quiz/quiz-vulkan/src/core/domain/*`, `apps/quiz/quiz-vulkan/src/app/app_state.*`
- Artifact loading: `apps/quiz/quiz-vulkan/src/core/domain/deck_artifact_loader.*`, `apps/quiz/quiz-vulkan/src/app/app.*`, `apps/quiz/quiz-vulkan/tests/domain/deck_artifact_loader_tests.cpp`
- Scene/Layout/UI: `apps/quiz/quiz-vulkan/src/core/scene/*`, `apps/quiz/quiz-vulkan/src/core/layout/layout_placer.h`, `apps/quiz/quiz-vulkan/src/core/layout/input_hit_test.h`, `apps/quiz/quiz-vulkan/src/core/ui/*`
- Renderer/App smoke: `apps/quiz/quiz-vulkan/src/render/render_draw_list.h`, `apps/quiz/quiz-vulkan/src/render/vulkan/*`, `apps/quiz/quiz-vulkan/src/app/app_demo.*`, `apps/quiz/quiz-vulkan/src/app/app.cpp`, `apps/quiz/quiz-vulkan/src/app/app_action_router.*`, `apps/quiz/quiz-vulkan/src/platform/*`, CPU fallback framebuffer present path
- Text/Image engines: `apps/quiz/quiz-vulkan/src/render/text/*`, `apps/quiz/quiz-vulkan/src/render/image/*`, `apps/quiz/quiz-vulkan/tests/render/fake_text_engine_tests.cpp`, `apps/quiz/quiz-vulkan/tests/render/fake_image_texture_cache_tests.cpp`, line-break/glyph cache/font-resolution/font-face catalog/font-source/font-source-byte/file-byte-loader/line-layout/run-box/caret-hit-test/atlas-page upload-policy and sampler/cache/upload queue/retry/lifetime eviction/decoder format/decoder-chain/data-URI/filesystem source-byte/placeholder texture diagnostics
- Asset system: `apps/quiz/quiz-vulkan/src/assets/*`, `apps/quiz/quiz-vulkan/tests/assets/*`, asset manifest/runtime resolver/materialization/byte-provider/byte-integrity/pack index/pack lookup/fallback/pack validation/catalog cache-key/cache-policy diagnostics
- Input/Audio engines: `apps/quiz/quiz-vulkan/src/core/input/*`, `apps/quiz/quiz-vulkan/tests/input/*`, input routing/action/text-edit/UTF-8-safe IME preedit/selection/gesture-cancel/focus-traversal/pointer-capture arbitration policy diagnostics. Audio files exist but audio is deferred from the current engine priority.
- Tests: `apps/quiz/quiz-vulkan/tests/**`, `apps/quiz/quiz-vulkan/tests/test_manifest.json`
- Build wrapper/runtime: `apps/quiz/quiz-vulkan/CMakeLists.txt`, `tools/run_windows_mingw_ascii.ps1`

## 갱신 규칙

1. 요구사항을 구현할 때 먼저 해당 루트 `구현/NN.md`와 프로젝트 `구현/NN.md`를 확인한다.
2. 코드나 테스트가 추가되면 이 문서의 `현재 증거`, `상태`, `다음 확인`만 갱신한다.
3. 요구사항 원문이나 waterfall 단계가 바뀌면 `big_plan.md`를 먼저 고친 뒤 이 문서를 맞춘다.
