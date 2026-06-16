# cpp-env 구조 분석 및 UI 엔진 방향 요약

분석 대상: `https://github.com/pjuny0301/cpp-env`  
분석 커밋: `00ae55c9121546b2e5d9986380623df2137d8929`

상세 보고서: `reports/cpp-env-structure-ui-engine-report.md`

## 결론

현재 방향은 타당하다. 특히 `quiz-vulkan`의 구조가 이미 다음 경계를 갖고 있어서, UI를 스크립트 기반 엔진으로 확장하기 좋은 출발점이다.

```text
app/domain
  -> scene patch/data
      -> layout_placer
          -> ui_renderer
              -> vulkan_renderer
```

다만 바꿔야 할 대상은 `ui_renderer`가 아니다. 핵심은 현재의 `scene_action_binding(action_type, payload)`와 `app_action_router` 사이를 typed `Command(name, args)` 구조로 승격하는 것이다.

즉 계획한 구조는 이렇게 보는 것이 맞다.

```text
onEvent
  -> Command(name, args)
      -> command registry
          -> domain::app_action / app service invoke
```

## 큰 구조 변경 여부

- Vulkan 렌더러를 크게 갈아엎을 필요는 없다.
- `layout_placer`, `ui_renderer`, `vulkan_renderer` 경계는 유지하는 편이 좋다.
- 큰 변경은 스크립트 스키마, typed command, command registry, GUI 저작 도구 쪽에 필요하다.
- `app_quiz_screens.h`의 C++ 화면 생성 helper를 장기적으로 JSON/DSL 템플릿 컴파일러로 줄여야 한다.

## 주요 비효율

1. 요구사항/구현 문서가 너무 많이 복제되어 있다.
   - `codex/quiz/구현`
   - 날짜별 `구현`
   - 앱별 `구현`
   - 앱별 날짜 스냅샷
   - 권위 문서가 흐려진다.

2. React 기준 앱 두 개가 동일한 generated UI를 중복 보관한다.
   - `android-quiz-app/src/app/components/ui`
   - `quiz-editor/src/app/components/ui`
   - 두 폴더는 `diff -qr` 기준 동일했다.

3. `core/scene`에 quiz 전용 의미가 들어가 있다.
   - 일반 UI 엔진이 되려면 scene core는 generic node/style/layout/event 중심이어야 한다.
   - quiz semantic은 app/presentation/script extension 쪽으로 빼는 편이 낫다.

4. worker workflow가 tmux 화면 상태, 고정 경로, 고정 branch, 전역 build lock에 의존한다.
   - `/mnt/c/aa`, `/mnt/c/aa-workers`, `C:/qtmingw1310_ascii` 같은 로컬 경로가 많다.
   - worker ledger와 preflight가 필요하다.

5. CMake 파일 등록이 통합자 병목이 된다.
   - engine worker가 작은 private source split을 해도 top-level CMake 수정이 필요해질 수 있다.
   - 모듈별 CMake fragment나 source manifest를 검토해야 한다.

## UI 엔진화에 필요한 컴포넌트

| 컴포넌트 | 역할 |
| --- | --- |
| UI script schema | screen, node, style, event, command, binding 정의 |
| scene script compiler | script + app_snapshot을 scene_layout_patch로 변환 |
| typed command runtime | `action_type + string payload`를 `Command(args)`로 대체 |
| command registry | command를 검증하고 domain/app action으로 변환 |
| expression/binding engine | `{{ question.prompt }}` 같은 데이터 바인딩 처리 |
| GUI authoring editor | PowerPoint/Flash처럼 화면과 이벤트를 GUI로 작성 |
| preview/replay runner | script를 fixture snapshot으로 실행해 검증 |
| schema versioning | editor/native/Android 간 artifact 호환성 유지 |
| scenario/visual validation | 이벤트, layout, command, render 결과 검증 |
| shared React UI package | 두 React 앱의 동일 generated UI 중복 제거 |
| worker ledger/preflight | long-lived worker 상태와 환경 재현성 확보 |

## 추천 실행 순서

1. 기존 `scene_action_binding`은 호환 계층으로 유지한다.
2. `scene_command { name, args }`를 추가한다.
3. `start_quiz`, `submit_option`, `continue_after_feedback` 세 명령부터 typed command로 옮긴다.
4. 기존 string action과 새 typed command가 같은 `domain::app_action`을 만드는 테스트를 추가한다.
5. swipe/long press 같은 hard-coded gesture fallback을 scene event handler로 내린다.
6. `app_quiz_screens.h`의 화면 하나만 먼저 JSON/DSL script로 컴파일해 본다.
7. 그 뒤 `quiz-editor`에 node tree, property inspector, event panel, command picker를 추가한다.

## 최종 판단

`onEvent -> Command -> invokeSomething` 방향은 현재 코드 구조와 잘 맞는다. 이 방향으로 가면 퀴즈 앱뿐 아니라 GUI로 UI 스크립트를 만드는 엔진으로 확장할 수 있다.

단, 성공 조건은 명확하다.

- renderer에 script/domain 지식을 넣지 않는다.
- Command는 typed args를 가진 데이터 객체로 만든다.
- scene core는 일반화하고, quiz 전용 의미는 app/script extension으로 분리한다.
- GUI 저작 도구는 schema-first로 만든다.
