# Vulkan Quiz App Rebuild Plan

작성일: 2026-04-25

## 1. 현재 `aa/apps/quiz` 관찰 요약

- `C:/aa` 루트는 Codex용 Windows-first C++ 환경 저장소다. `build/external/tools/bootstrap/codex_env/tools/cpp-new`가 C++/CMake 프로젝트 스캐폴드 도구다.
- 실제 퀴즈 플랫폼은 `C:/aa/apps/quiz` 아래에 있다.
- 플레이 앱 기준 소스는 `android-quiz-app/src/app`이다.
- 예전 `quizquiz/*` 사본들은 `legacy/` 아래로 보존한다.
- 에디터는 `quiz-editor`에 있고, Tauri + React로 Git 기반 `quiz-data` 편집, JSON export, Codex 호출, 커밋/푸시를 담당한다.
- `figma-make/quiz-web`은 Android 앱 소스를 웹/Figma Make용으로 복사한 사본이다.
- `quiz-data/mobile-decks`가 앱이 읽는 실제 덱 데이터다. 핵심 JSON 형태는 `title`, `questions[]`, `options[]`, `isCorrect` 중심이다.
- 빌드 산출물, `.vs`, `node_modules`, `dist`, `src-tauri/target*`, `build-artifacts`, screenshots는 구현 계획 수립에는 보조 참고로만 둔다.

## 2. 재구성 범위

1차 범위는 플레이용 퀴즈 앱이다.

에디터는 당장 Vulkan으로 재작성하지 않는 편이 맞다. 현재 에디터의 핵심 가치는 contentEditable, Git 작업, JSON 변환, Codex 연동이고, 이것은 네이티브 Vulkan UI로 옮기는 비용이 플레이 앱보다 훨씬 크다. 대신 에디터가 내보내는 JSON 스키마를 새 앱의 도메인 모델과 맞춘다.

## 3. 전체 구조와 UI 서브시스템

사용자 제안 구조는 전체 앱 구조가 아니라 UI 서브시스템 구조로 고정한다.

전체 앱은 다음처럼 나눈다.

```text
Platform/App Shell
  -> Domain Services
      - quiz data loading / normalization
      - learning state
      - quiz session state machine
      - persistence
      - remote/local asset loading
  -> UI Subsystem
      - modifier interface
      - scene layout data modifier
      - layout placer
      - UI renderer
      - Vulkan renderer
```

UI 서브시스템 내부 구조:

```text
main
  -> modifier_interface
      -> scene_layout_data_modifier impl
          -> writes scene_layout_patch / scene_layout_edit_data
              -> scene_layout_data
                  <- layout_placer
                      <- ui_renderer
                          <- vulkan_renderer
```

역할 분리:

- `vulkan_renderer`: device, swapchain, render pass, pipeline, descriptor, texture/font atlas, GPU buffer, draw submission만 담당한다. 퀴즈 상태를 모른다.
- `ui_renderer`: `scene_layout_data`를 읽어 quad/text/image/clip/hit draw command로 변환한다. 위젯의 의미는 몰라도 된다.
- `layout_placer`: scene node의 logical constraint를 실제 rect, z-order, clip, hit area로 배치한다. 텍스트 측정은 renderer service를 인터페이스로만 호출한다.
- `scene_layout_data`: 현재 프레임의 retained UI tree다. screen, nodes, style tokens, text runs, image refs, input regions, animations, focus를 가진다.
- `scene_layout_edit_data`: `scene_layout_data`의 부분집합이며 modifier가 쓸 수 있는 patch/command만 담는다. 즉 전체 scene을 직접 만지지 않고 `append_node`, `set_text`, `set_style`, `set_route`, `start_transition`, `bind_action` 같은 제한된 변경만 허용한다.
- `scene_layout_data_modifier`: domain state snapshot과 입력 이벤트를 받아 scene patch를 만든다. `home`, `deck_view`, `day_intro`, `quiz_active`, `quiz_results` modifier로 쪼갠다.
- `modifier_interface`: `main` 또는 App Shell이 호출하는 얇은 ABI/API다. platform input, timer, route, domain event, async load result를 modifier에 전달한다.
- `main`: platform loop, input pump, frame pacing을 담당한다. persistence, network/file loading, quiz session rule은 UI 밖 Domain Services에 둔다.

핵심 원칙:

- renderer는 scene data를 그릴 뿐 도메인 상태를 모른다.
- layout은 CPU에서 결정하고 Vulkan에는 이미 배치된 draw command만 보낸다.
- modifier는 전체 scene을 소유하지 않고 patch만 쓴다.
- quiz domain은 scene보다 아래에 둔다. `deck/day/question/learning/session`은 UI 없이 테스트 가능해야 한다.
- UI modifier는 domain을 직접 변경하지 않고 `app_action`을 발생시킨다. App Shell이 action을 domain service에 적용하고, 다음 frame에서 새 `app_snapshot`을 UI에 넘긴다.

## 4. 권장 디렉터리

```text
C:/aa/apps/quiz/quiz-vulkan/
  CMakeLists.txt
  CMakePresets.json
  vcpkg.json
  AGENTS.md
  README.md
  agent/command_brief.json
  docs/architecture.md
  docs/scene-schema.md
  src/
    app/main.cpp
    app/app_shell.hpp
    core/domain/
      quiz_model.hpp
      quiz_json_loader.hpp
      quiz_normalize.hpp
      learning_state.hpp
      quiz_session.hpp
      app_action.hpp
      app_snapshot.hpp
    core/scene/
      scene_id.hpp
      scene_layout_data.hpp
      scene_layout_patch.hpp
      scene_modifier.hpp
      scene_transaction.hpp
    core/layout/
      layout_placer.hpp
      constraints.hpp
      text_measure.hpp
    core/ui/
      ui_renderer.hpp
      widgets.hpp
      quiz_screens.hpp
    render/vulkan/
      vk_instance.hpp
      vk_device.hpp
      vk_swapchain.hpp
      vk_pipeline.hpp
      vk_batcher.hpp
      vk_texture_atlas.hpp
      vk_font_atlas.hpp
    platform/win32/
    platform/android/
  tests/
    domain/
    scene/
    layout/
```

## 5. 구현 순서

1. 스키마 고정
   - 기존 `data.ts`, `remoteDecks.ts`, `learning.ts`, `quizRuntime.ts`에서 도메인 계약을 추출한다.
   - `question_type`: `short`, `long`, `answer`, `blank`, `multi-answer`, `multi-blank`, `multiselect`.
   - 학습 상태: `learning`, `known`, `unknown`, `correct_streak`, `wrong_count`, `known_threshold`, `wrong_release`.

2. C++ 프로젝트 스캐폴드
   - `cpp-new` 계열 구조를 쓰되 `apps/quiz/quiz-vulkan` 전용으로 만든다.
   - Windows 데스크톱 clear-screen Vulkan MVP를 먼저 만든다. Android는 platform abstraction을 열어두고 2차로 붙인다.

3. Domain 먼저 포팅
   - JSON load/normalize, peer answer option rebuild, typed answer check, seeded shuffle, learning transition을 UI 없이 구현한다.
   - 기존 `quiz-data` 샘플 JSON으로 단위 테스트를 만든다.

4. `scene_layout_data`와 patch 계약 구현
   - node id, node kind, style token, text/image payload, interaction id, layout constraint를 정의한다.
   - patch는 append/update/remove/`bind_action`/route/animation 정도로 제한한다.

5. `layout_placer` CPU 테스트
   - 모바일 기준 360x800, 390x844, 412x915, 태블릿/데스크톱 기준으로 golden rect 테스트를 둔다.
   - 텍스트 overflow와 safe-area를 먼저 잡는다.

6. `ui_renderer` + `vulkan_renderer`
   - 처음에는 colored quad + scissor + text placeholder.
   - 이후 FreeType/HarfBuzz 기반 glyph atlas 또는 MSDF atlas를 붙인다.
   - 이미지 decode/upload는 async job으로 빼고 렌더 스레드에서 blocking하지 않는다.

7. Quiz screen modifier 포팅
   - `home` -> `deck_view` -> `day_intro` -> `quiz_active` -> `quiz_results` 순서.
   - 입력은 pointer/touch/keyboard/IME event로 정규화한 뒤 modifier에 전달한다.
   - swipe, long press, answer selection, multiselect slot, manual continue, auto advance의 UI gesture 해석은 modifier에 둔다.
   - 정답 처리, 학습 상태 전이, 결과 누적은 domain/session state machine에 둔다.

8. 데이터 로딩
   - v1은 local `quiz-data` JSON 폴더 로딩.
   - v2는 GitHub/jsDelivr/http directory loader를 C++ service로 이전한다.
   - remote 실패 메시지와 source URL normalize 규칙은 기존 앱 동작을 보존한다.

9. Android 패키징
   - Windows에서 MVP가 안정화된 뒤 Android NativeActivity/NDK 또는 얇은 platform shell로 Vulkan surface를 붙인다.
   - Tauri WebView 방식은 새 구조와 맞지 않으므로 플레이 앱은 별도 native target으로 본다.

10. 성능 마감
   - per-frame heap allocation 금지.
   - scene double buffering.
   - dirty layout subtree만 재배치.
   - draw command batching, persistent mapped ring buffer, texture/glyph atlas.
   - deck parse/network/image decode는 worker queue.

## 6. 에이전트 분배

각 worker는 서로 다른 write set을 가진다.

- Agent A: scaffold/platform owner
  - write: `CMake*`, `vcpkg*`, `src/app`, `src/platform`
  - output: clear-screen Vulkan window, build command

- Agent B: domain owner
  - write: `src/core/domain`, `tests/domain`
  - output: JSON normalize, learning/session, action/snapshot, existing quiz-data fixture tests

- Agent C: scene/layout owner
  - write: `src/core/scene`, `src/core/layout`, `tests/scene`, `tests/layout`
  - output: scene data, patch API, layout placer tests

- Agent D: renderer owner
  - write: `src/render/vulkan`, `src/core/ui/ui_renderer*`
  - output: quad/text/image command path, frame stats

- Agent E: quiz screens owner
  - write: `src/core/ui/quiz_screens*`
  - output: `home`/`deck_view`/`day_intro`/`quiz_active`/`quiz_results` UI modifiers, domain action emission

- Agent F: verification owner
  - write: `tests`, `tools`, `docs/test-plan.md`
  - output: build/test scripts, screenshot/perf baseline, regression checklist

주의: 같은 파일을 여러 agent가 동시에 만지지 않도록 owner를 엄격히 유지한다. 공용 타입 변경은 main agent가 먼저 승인/병합한다.

## 7. 토큰 운영안

기본 정책:

- 큰 소스 전체를 매번 읽지 않는다.
- `agent/command_brief.json`에 현재 목표, 디렉터리 owner, 핵심 스키마, 금지 영역을 1-2k tokens로 유지한다.
- worker는 `command_brief.json` + 자기 owner 파일 + 필요한 헤더만 읽는다.
- React 원본은 worker에게 직접 읽히지 말고, main agent가 뽑은 도메인/UX 요약을 제공한다.
- 빌드 로그는 실패한 마지막 100-200줄만 공유한다.
- 한 agent 작업은 입력 15k tokens 이하, 출력/patch 5k tokens 이하를 기본 상한으로 둔다. 넘으면 작업을 둘로 쪼갠다.

예상 토큰:

- Phase 0 설계/스키마: 30k-50k
- Phase 1 scaffold + clear screen: 50k-80k
- Phase 2 domain/tests: 60k-100k
- Phase 3 scene/layout/tests: 80k-130k
- Phase 4 Vulkan renderer MVP: 120k-180k
- Phase 5 quiz screens: 100k-160k
- Phase 6 Android/package/perf: 80k-140k

총량은 대략 520k-840k agent tokens 범위로 본다. 핵심 절감 지점은 React 원본 재독해를 막고, schema/brief/golden tests 중심으로 넘기는 것이다.

## 8. 바로 다음 액션

1. `apps/quiz/quiz-vulkan` 프로젝트 루트를 기준 위치로 사용한다.
2. `apps/quiz/quiz-vulkan/docs/domain-contract.md`에 `app_snapshot`, `app_action`, `quiz_session` 계약을 먼저 쓴다.
3. `apps/quiz/quiz-vulkan/docs/scene-schema.md`에 `scene_layout_data`, `scene_layout_patch`, `scene_layout_edit_data` 최소 스키마를 쓴다.
4. `apps/quiz/quiz-vulkan/agent/command_brief.json`을 만들고 위 에이전트 owner를 고정한다.
5. Agent A/B/C를 병렬로 시작한다.
6. A/B/C 결과가 합쳐지면 D/E/F를 시작한다.
