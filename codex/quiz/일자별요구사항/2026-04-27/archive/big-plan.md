# Big Plan Implementation Plan

작성일: 2026-04-27

이 문서는 `C:/aa/codex/quiz/big_plan.md`의 1-82번 확장 구상을 실제 구현 순서와 모듈로 풀어쓴 실행 계획이다. 기준으로 읽은 파일은 `apps/quiz/README.md`, `apps/quiz/CODEMAP.md`, `apps/quiz/requirements-status.txt`, `apps/quiz/editor-requests-status.txt`, `apps/quiz/quiz-vulkan/AGENTS.md`, `apps/quiz/quiz-vulkan/docs/*`, `build/external/tools/bootstrap/AGENTS.md`다.

## 운영 원칙

1. `apps/quiz/quiz-vulkan`을 새 플레이 앱의 기준으로 둔다. React/Tauri 앱은 기존 기능 확인과 에디터/데이터 제작 도구로 유지하고, 플레이 런타임은 순수 C++/Vulkan으로 옮긴다.
2. 도메인 상태는 UI와 렌더러 밖에 둔다. `app_snapshot -> scene modifier -> scene_layout_data -> layout_placer -> ui_renderer -> vulkan_renderer` 흐름을 깨지 않는다.
3. AI 기능은 먼저 파일 산출물과 캐시 키를 정의한다. PDF 텍스트, 번역, 퀴즈, 해설, 대화 스크립트, 이미지 생성 결과는 content hash 기반으로 Git 또는 artifact store에 저장한다.
4. 모든 새 기능은 schema, domain rule, UI state machine, renderer/style token, persistence, test 순서로 쪼갠다.
5. 큰 폴더는 `AGENTS.md`, `CODEMAP.md`, status 파일, manifest를 먼저 읽는다. 전체 소스 스캔은 마지막 수단이다.

## 목표 아키텍처

```text
C:/aa
├─ codex/                        # reusable Codex docs, plans, agent handoff
├─ build/                        # external reusable build material and ignored output
└─ apps/
   └─ quiz/
      ├─ android-quiz-app/       # Android-first React/Tauri quiz app
      ├─ quiz-editor/            # Tauri/React quiz editor
      ├─ quiz-vulkan/            # C++/Vulkan 플레이 앱
      │  ├─ src/core/domain/     # deck, question, learning, SRS, session FSM
      │  ├─ src/core/scene/      # retained UI tree and patch surface
      │  ├─ src/core/layout/     # CPU layout, keyboard/safe-area handling
      │  ├─ src/core/ui/         # quiz screens, theme interpreter
      │  └─ src/render/vulkan/   # GPU resources and draw submission
      └─ legacy/                 # old copies and leftover local artifacts
```

공통 표준은 `apps/quiz/quiz-vulkan/docs/domain-contract.md`와 별도 schema 문서에서 관리한다. 최소 표준은 `deck`, `day`, `question`, `answer`, `explanation`, `learning_state`, `content_ref`, `asset_ref`, `ai_artifact_ref`, `theme_ref`, `concept_ref`다.

## 빌드/부트스트랩 운영

- `C:/aa/build/external/tools/bootstrap`은 새 머신에서 Codex/build 환경을 복구하는 기준으로 둔다. durable tool이나 공통 명령을 추가할 때는 bootstrap mirror와 `C:/codex-local` 설치 경로를 같이 생각한다.
- `apps/quiz/quiz-vulkan` 자체 빌드는 현재 `CMakePresets.json`, `vcpkg.json`, `tools/run_windows_mingw_ascii.ps1` 흐름을 유지한다. bootstrap은 toolchain과 명령 환경을 준비하고, 앱 프로젝트는 CMake preset으로 빌드한다.
- 새 서비스(`pdf-ingest`, `math-ocr`, `proof-grader`, `code-sandbox`)는 처음에는 독립 CLI로 시작하고, 반복 사용이 확정되면 bootstrap의 repo-managed tool mirror로 승격한다.
- 빌드 로그는 실패한 마지막 100-200줄만 계획/에이전트에 전달한다. 전체 `build/`, `target/`, package output은 읽지 않는다.

## 토큰/AGENTS 운영

- 루트는 `AGENTS.md`, `codex/common/agent/shared_command.json`, `codex/common/agent/token_budget.json`을 먼저 읽는다.
- `codex/quiz/`은 `codex/quiz/AGENTS.md`가 번호 중복, 빈 항목, 다른 브랜치 항목을 설명한다.
- `apps/quiz/`는 소스 프로젝트 3개만 둔다. 읽기용 안내는 `codex/quiz/app-context/AGENTS.md`, `README.md`, `CODEMAP.md`, status 파일을 먼저 보고, 필요한 소스 파일만 제한적으로 연다.
- `apps/quiz/quiz-vulkan/`은 이미 `apps/quiz/quiz-vulkan/AGENTS.md`가 owner와 UI pipeline 규칙을 가진다.
- 큰 요구를 sub-agent에 넘길 때는 `codex/common/agent/shared_command.json`에 shared task와 write set을 먼저 적고, 각 agent가 자기 owner 파일만 읽게 한다.

## 실행 웨이브

- Wave 0: 문서/스키마/상태머신 골격. 항목 5, 10, 16, 24, 79, 81.
- Wave 1: 기존 퀴즈 UX 보존 및 C++ 도메인 포팅. 항목 6-9, 26-32, 47, 49, 57-61, 63-64.
- Wave 2: Vulkan 렌더러와 테마/카툰 UX. 항목 2, 46, 50-56, 66, 71-76, 78, 80, 82.
- Wave 3: PDF 본문/번역/퀴즈 생성 파이프라인. 항목 1, 4, 12, 15, 21, 45, 67-70, 77.
- Wave 4: AI 학습 피드백과 개인 지식 베이스. 항목 3, 13-14, 19-20, 62, 65.
- Wave 5: 수학/OCR/코드 샌드박스 고급 채점. 항목 11, 17-18, 22-23, 34-44.
- Wave 6: 서버/계정/보안/배포/품질 대시보드. `big plan`의 후반 설명에 나온 계정, API 서버, 보안, QA, CI/CD를 여기서 다룬다.

## 항목별 이행 계획

| 번호 | 이행 계획 |
| --- | --- |
| 1 | PDF에서 문제만 추출하는 `pdf-ingest` 워커를 만든다. 단계는 PDF 페이지 추출, 챕터 감지, 문제 후보 분리, AI 검증, `deck/day/question` JSON 컴파일, 에디터 검수 큐 순서다. 모든 프롬프트 입력과 결과는 PDF hash, page range, prompt version으로 캐시한다. |
| 2 | 문제마다 `theme_variant`와 `presentation_profile`을 줄 수 있게 스키마를 확장한다. `apps/quiz/quiz-vulkan`에서는 modifier가 variant를 style token과 layout token으로 변환하고, renderer는 의미를 몰라도 quad/text/image만 그리게 한다. |
| 3 | 기존 Tauri 에디터 상단에 AI 스트리밍 패널을 붙인다. AI 응답은 바로 본문을 덮어쓰지 않고 `quiz-editor-patch` 형식으로 제안, 미리보기, 적용 단계를 거치게 한다. |
| 4 | 본문학습 모드를 `content_ref` 기반으로 만든다. PDF 원문 페이지 텍스트, Claude 번역본, 이미지 ref를 나란히 보고, 학습 종료 후 같은 chapter context로 AI 질답과 즉석 퀴즈 생성을 실행한다. 번역/질답 context는 Git 캐시를 우선 조회한다. |
| 5 | 요구사항 트리는 `plan/requirements-tree` 계열 문서로 유지한다. 각 task에 비용, 변경량, 의존성, 검증 방법을 붙이고 wave 종료 때 실측치로 갱신한다. |
| 6 | 스와이프/이미 앎/모름은 domain FSM으로 구현한다. `skip_question_without_feedback`, `offer_mark_known`, `mark_known_by_swipe`, `mark_unknown`, `known_mode`, `wrong_release` action을 분리하고, UI는 action만 발생시킨다. |
| 7 | select 유형의 정답 빨간 박스 제거는 이미 React 앱에 반영되어 있으므로 C++ 포팅 시 기본 정책으로 고정한다. feedback scene은 옵션 자체의 상태만 은은하게 바꾸고 별도 정답 박스를 만들지 않는다. |
| 8 | select 유형의 큰 OX 오버레이는 금지한다. blank, multi_blank, multiselect에는 유형별로 낮은 불투명도의 inline feedback을 허용하되, 화면 중앙 오버레이는 쓰지 않는다. |
| 9 | multiselect는 선택 순서대로 상단 슬롯을 채우는 `multiselect_session_state`를 둔다. 선지 눌림 scale animation은 theme가 별도로 켜지 않는 한 기본 off로 두고, 제출 시 domain의 correct option set과 비교한다. |
| 10 | 리팩토링은 React 코드 정리보다 C++ rewrite의 경계 확정이 우선이다. `domain`, `scene`, `layout`, `ui`, `render`, `platform`, `services` owner를 고정하고, 공통 동작은 domain test로 먼저 잠근다. |
| 11 | 수학 증명 채점은 `proof_submission` 모델을 만든 뒤 AI rubric grader로 붙인다. 입력은 텍스트/LaTeX/이미지 ref를 모두 받되, 채점 결과는 claim, step, error span, explanation, confidence로 구조화한다. |
| 12 | PDF 등록 파이프라인은 페이지별 텍스트와 이미지 인덱스를 먼저 만들고, main agent가 헷갈릴 개념/지문 후보를 뽑아 distractor agent에 넘긴다. 하위 agent는 "그럴듯하지만 절대 정답이 아닌" 선지를 생성하고 validator가 걸러낸다. |
| 13 | 정답 후 "왜 정답인지" 입력은 `answer_rationale` 단계로 추가한다. 사용자가 맞힌 뒤 짧은 사유를 쓰게 하고, AI는 정답 여부가 아니라 설명의 충분성, 누락 개념, 오개념을 평가한다. |
| 14 | 사유가 틀렸거나 오답이면 `discussion_session`을 연다. 토론은 원문 page/concept/question/rationale ref를 들고 시작하며, 전체 대화는 요약 캐시를 만들어 다음 호출 토큰을 줄인다. |
| 15 | `content-index` CLI를 만든다. 출력은 `pdf-index/<book>/<chapter>/pages.json`, `images.json`, `paragraphs.json`이고 각 텍스트 블록은 파일 경로, page, bbox, image refs를 가진다. |
| 16 | 기능은 독립 프로그램과 표준 계약으로 나눈다. 최소 프로그램은 player, editor, pdf-indexer, ai-generator, translation-cache, zettel-indexer, proof-grader, code-sandbox, sync-api다. 공통 schema version을 두고 breaking change는 migration으로 처리한다. |
| 17 | 수학 수식 필기 OCR은 18번과 같은 기능으로 묶는다. 태블릿 stroke 또는 이미지 입력을 `math_ocr_request`로 저장하고, OCR 결과는 원본 좌표와 LaTeX 후보를 함께 반환한다. |
| 18 | 17번과 중복이다. 별도 구현하지 않고 같은 `math_ocr` 모듈의 acceptance criteria에 "11번 proof grader와 연동"을 넣는다. |
| 19 | 포커스 모드는 Obsidian/Zettelkasten vault indexer가 필요하다. 로컬 markdown을 chunk, concept, embedding으로 인덱싱하고, 퀴즈에서 막힌 concept id를 기준으로 작은 popup scene을 띄운다. 손필기 기반 생성물도 같은 artifact ref로 캐시한다. |
| 20 | SRS aging은 `known` 상태에 `known_since`, `due_at`, `ease`, `review_interval`을 추가한다. due가 지나면 본 그룹으로 자연 복귀시키거나 "복습 예정" 탭에 노출하고, 정답/오답 결과로 interval을 갱신한다. |
| 21 | 개념 의존성 그래프는 `concept_graph.json` DAG로 저장한다. PDF 인덱싱 시 concept node와 prerequisite edge를 만들고, AI 토론 중 기초 개념 혼동이 감지되면 ancestor node를 역추적해 본문/덱을 추천한다. |
| 22 | 코드/로직 샌드박스는 proof grader와 별도 워커로 둔다. compile/run은 제한된 컨테이너나 Windows sandbox에서 실행하고, AI는 실행 결과, 테스트 실패, 코드 구조 요약만 받아 피드백한다. |
| 23 | 리버스 엔지니어링/디버깅 퀴즈는 `question_type=debugging` 또는 `scenario_kind=reverse_debug`로 추가한다. AI가 일부러 넣은 오류는 metadata에 숨겨 저장하고, 학생 답변은 오류 위치/이유/수정안을 기준으로 채점한다. |
| 24 | 오토마타 FSM은 `quiz_session`과 `ui_flow_state` 두 층으로 나눈다. events, guards, actions, transitions를 표로 문서화하고, swipe, debate, focus popup, keyboard, animation conflict를 상태 전이 테스트로 막는다. |
| 25 | 원본 목록에 25번이 없다. 새 요구가 생기기 전까지 reserved로 남긴다. |
| 26 | 책의 주요개념/관련 설명은 PDF paragraph index에서 `concept_ref -> paragraph_ref -> quiz question`으로 매핑한다. MultiSelect 문제는 concept id와 정답 concept list를 저장하고, 텍스트 파일 import도 같은 schema로 변환한다. |
| 27 | 유저가 빈칸을 직접 뚫는 기능은 에디터에서 먼저 만든다. 텍스트 선택 후 "blank" action을 적용하면 원문 span과 정답 텍스트가 저장되고, 모바일 즉시 수정은 68번에서 같은 patch API를 재사용한다. |
| 28 | "문단 / 문제 개념 / 정답"은 모든 생성형 문제의 최소 traceability 필드로 둔다. `paragraph_id`, `concept_ids`, `answer_key`, `source_quote_hash`를 저장해 검수와 재생성을 가능하게 한다. |
| 29 | 오답 선지는 정답 concept과 가까운 sibling/prerequisite/commonly-confused concept에서 만들되, validator가 원문 근거상 정답이 될 수 있는 선지를 제거한다. 생성 prompt에 "헷갈리지만 절대 아닌 것"을 규칙으로 고정한다. |
| 30 | 왼쪽 스와이프 이전 문제는 history stack과 current index rewind로 구현한다. 이미 제출된 feedback을 되살릴지, 단순 복습으로 볼지는 FSM 정책으로 분리하고 setting으로 선택 가능하게 한다. |
| 31 | 키보드가 올라올 때 문제 영역이 화면 밖으로 밀리는 문제는 layout placer에 `keyboard_inset`과 focused input anchoring을 넣어 해결한다. 입력창 포커스 시 question stage는 축소/스크롤 우선순위를 계산하고, 답변 dock은 안전 영역 위에 고정한다. |
| 32 | 오답노트는 `wrong_note` learning bucket으로 추가한다. 옵션이 켜져 있으면 첫 오답 또는 N회 오답 시 이동하고, 사용자가 정한 연속 정답 수를 만족하면 본래 그룹으로 복귀한다. |
| 33 | 배포/테스트/디자인 파이프라인 분석은 별도 audit 문서로 한다. 현재 build scripts, APK/desktop output, Figma export, test coverage, visual verification, release signing 경로를 표로 만들고 병목과 자동화 우선순위를 제안한다. |
| 34 | 수학 증명 전달은 camera mode와 tablet proof mode를 같은 `proof_capture` 인터페이스로 묶는다. 설정에 따라 카메라 촬영, 이미지 import, 태블릿 stroke canvas 중 하나를 기본 입력으로 연다. |
| 35 | 옵션에는 `basic`과 `advanced` 섹션을 둔다. 고급옵션에는 AI/OCR/proof/camera/tablet/cache/debug/performance처럼 일반 학습자가 매번 보지 않아도 되는 설정만 둔다. |
| 36 | 카메라/태블릿 선택창은 고급옵션의 proof input source selector로 구현한다. 선택값은 deck별 override와 global default를 모두 지원한다. |
| 37 | 태블릿에 아무것도 없으면 제출 시 카메라로 이동하는 옵션은 `empty_tablet_submission_policy`로 둔다. `ask`, `open_camera`, `block_submit` 중 하나를 선택하게 한다. |
| 38 | 이미지 분석에서 텍스트 변환은 OCR worker가 담당한다. 일반 텍스트 OCR과 수식 OCR을 분리하고, 결과는 bbox와 confidence를 포함해 UI가 틀린 영역을 표시할 수 있게 한다. |
| 39 | 텍스트에서 LaTeX 변환은 `latex_normalizer` 단계로 둔다. OCR raw text를 바로 채점하지 말고, 사용자 확인 가능한 LaTeX preview와 edit step을 거친다. |
| 40 | 자신만의 표기법은 `notation_profile`로 저장한다. 예: 벡터 표기, 약어, 함수명, 생략 규칙을 AI prompt와 LaTeX normalizer 모두에 주입하고 version을 붙인다. |
| 41 | LaTeX 채점 AI는 proof grader의 한 입력 모드로 구현한다. 정답만 채점할 때는 equivalent transform 위주, 과정 채점 때는 step graph와 justification을 본다. |
| 42 | AI 설명은 `notation_profile`을 거쳐 사용자 표기법으로 다시 렌더링한다. 내부 canonical LaTeX와 사용자-facing LaTeX를 둘 다 저장해 재채점과 표시를 분리한다. |
| 43 | 정답만 채점/과정도 채점은 `grading_mode` 옵션으로 둔다. 정답만 모드는 빠르고 싸게, 과정 모드는 느리지만 오류 위치와 reasoning feedback을 제공하게 비용을 명확히 표시한다. |
| 44 | 태블릿에서 어느 부분이 틀렸는지 가리키는 기능은 feedback anchor로 구현한다. AI가 bbox/stroke range를 반환하면 UI가 해당 위치로 스크롤하고 highlight/pointer overlay를 표시한다. |
| 45 | 오답 생성은 중앙 `distractor_generator`가 맡는다. PDF 문제 생성, 수동 퀴즈 보강, debugging quiz 모두 같은 generator와 validator를 써서 품질 기준을 통일한다. |
| 46 | PDF 텍스트 로딩 시 오른쪽 흐린 번역본 창은 reader scene의 split pane style로 만든다. 원문과 동일 font metrics를 쓰고, 번역 pane은 border 없는 연보라 배경과 약간 진한 보라 글씨 style token을 적용한다. |
| 47 | 단답형 OX는 화면과 같은 색 계열의 높은 투명도 feedback으로 제한한다. 짧은 정답은 문제 바로 아래 bold inline, 긴 정답은 별도 answer panel에 표시하며, 정답 표시가 문제를 위로 자연스럽게 밀도록 layout rule을 둔다. 기존 빨간 글씨 정답 표기는 제거한다. |
| 48 | 원본 목록에서 내용이 비어 있다. 다음 요구 입력 전까지 reserved로 남기고 구현하지 않는다. |
| 49 | 장문/짧은 문제 구분은 `question_length_class`를 계산하거나 명시 필드로 둔다. 이미 나뉘는 경우 threshold를 낮춰 장문 layout이 더 빨리 적용되게 하고, long text font size와 scroll policy를 별도 테스트한다. |
| 50 | 다른 브랜치 작업으로 분리한다. 문제 전환 animation은 theme token `transition.question.enabled=false`로 끌 수 있게 만들어 merge 충돌을 줄인다. |
| 51 | 다른 브랜치 작업으로 분리한다. 선택형 정답 성공 animation은 feedback policy에서 제거하고, domain 결과와 UI animation을 분리한다. |
| 52 | 다른 브랜치 작업으로 분리한다. 정답 시 나머지 선지를 흐리게 하는 처리는 style policy에서 off로 둔다. |
| 53 | 원문이 "정답 틀렸을 때 나머지"에서 끝나므로 세부 동작 확인이 필요하다. 임시로 다른 브랜치의 clarification item으로 두고 구현하지 않는다. |
| 54 | 다른 브랜치 작업으로 분리한다. 선지 shape token을 rectangle로 바꾸고 border radius를 낮춘다. |
| 55 | 다른 브랜치 작업으로 분리한다. 선지 번호는 renderer label policy에서 제거하고 accessibility label에는 필요 시 유지한다. |
| 56 | 다른 브랜치 작업으로 분리한다. PASS는 answer dock 오른쪽 끝에 배치하고, 오답 후에는 같은 위치의 label/action을 Next로 바꾼다. |
| 57 | "이미 앎" 이동은 정답 제출이 아니라 스와이프 기반 action으로만 허용한다. 정답 streak 자동 승급과 충돌하므로 설정에서 둘 중 하나를 고르게 하거나, 기본 정책을 swipe-only known으로 바꾼다. |
| 58 | "이미 앎" 탭은 하단 navigation bar의 독립 destination으로 만든다. DayIntro 버튼은 보조 진입점으로만 남기거나 제거한다. |
| 59 | "오답노트"도 하단 bar destination으로 추가하되, deck metadata나 user setting에서 wrong note가 켜진 deck에만 노출한다. |
| 60 | 퀴즈 위의 유형 표시 label은 기본 숨김 처리한다. debug mode나 editor preview에서만 question type을 볼 수 있게 한다. |
| 61 | 다중빈칸 카드 유형은 `multi_blank_card` presentation으로 추가한다. 위쪽 카드 tray에 선택지를 많이 띄우고, 배치 완료 후 slot별 correct/incorrect를 표시한다. |
| 62 | 달력앱 연동은 SRS due date 기반으로 한다. 1차는 ICS export, 2차는 Android Calendar Provider/desktop calendar API 연동, 3차는 양방향 완료 상태 sync로 나눈다. |
| 63 | Git repo는 앱 옵션으로 뺀다. source repo URL, branch, deck root, auth 필요 여부를 settings에 저장하고, 기존 Git URL 입력 로직은 source manager로 흡수한다. |
| 64 | Deck 화면은 3 x N 박스 grid, Day는 얇은 horizontal strip으로 디자인한다. `apps/quiz/quiz-vulkan`에서는 responsive grid/strip layout token으로 만들고, 모바일/데스크톱별 column count를 layout test로 고정한다. |
| 65 | 백지 요약 채점은 concept coverage grader로 구현한다. 해당 장의 concept graph와 paragraph summary를 기준으로 사용자의 요약이 진짜 요약인지, 빠뜨린 핵심이 있는지 AI가 구조화 평가한다. |
| 66 | UI 통일성은 디자인 토큰과 theme contract로 관리한다. 색, 글꼴, radius, spacing, motion, feedback policy를 한 곳에 두고 screen modifier는 token만 참조한다. |
| 67 | PDF 본문/번역본 따로 표시되는 화면은 46번 split pane의 독립 route로 만든다. quiz mode와 study mode 사이에 current page/concept/question context를 공유한다. |
| 68 | 폰에서 즉시 수정은 lightweight editor mode로 구현한다. 문제 화면에서 edit action을 열고, 변경은 local patch로 저장한 뒤 Git sync queue에 넣어 충돌 시 에디터에서 해결하게 한다. |
| 69 | AI 토큰 비용 관리는 모듈별 cache, prompt version, summary memory, chunking, model routing으로 한다. 생성 결과는 파일로 저장하고 같은 입력 hash에서는 재호출하지 않는다. |
| 70 | PDF가 본문+문제 형식이면 classifier가 본문 block과 exercise block을 분리한다. 본문은 reader content로, 문제는 quiz JSON으로 보내며, 설명용 이미지는 chapter artifact로 생성해서 캐시한다. |
| 71 | 데스크톱 학습의 Codexbot+말풍선은 `study_companion_scene`으로 만든다. 화면 가로를 크게 쓰되 세로 1/3-1/5 범위 layout constraint를 두고, 대화 텍스트는 content script artifact에서 읽는다. |
| 72 | 연필 캐릭터의 부가 설명은 anchor-based annotation으로 구현한다. 특정 키워드/문장/문단 앞뒤에 배치 가능한 bubble과 icon layer를 두고, 겹침 방지 constraint를 둔다. |
| 73a | NPC 대화창, 다음 대사 넘김, 형광펜 highlight는 scripted interaction interpreter로 만든다. script는 dialogue lines, speaker, target text range, highlight color, optional pencil note를 포함한다. |
| 73b | AI가 챕터 텍스트를 받아 71-73a script를 생성하되, 재생성하지 않도록 `chapter_study_script.json`으로 저장한다. renderer는 이 형식을 해석만 하고, AI 호출은 generation worker에서만 수행한다. |
| 74 | 말풍선/캐릭터 색과 글꼴은 cartoon theme pack의 기본 token으로 둔다. 파스텔 노랑 bubble, 파스텔 파랑 Codexbot, cartoon pencil, rounded font를 지정하되 접근성 대비 기준을 통과해야 한다. |
| 75 | 선지 animation은 theme implementer가 제어한다. core FSM은 animation을 몰라야 하며, theme는 press, correct, wrong, skip, disabled motion token을 선택적으로 제공한다. |
| 76 | 테마 기능은 `theme_manifest.json`과 style/motion/layout token 묶음으로 만든다. deck selection, main screen, option shape, feedback, companion scene을 theme가 바꿀 수 있게 하되 domain rule은 바꾸지 못하게 한다. |
| 77 | Deck에 PDF 업로드하면 서버 job으로 보내고, 완료 후 앱 접속 시 다운로드 prompt를 띄운다. upload/progress/analyze/done 상태는 job id로 poll 또는 push하며, 업로드/분석 animation은 theme asset으로 교체 가능하게 한다. |
| 78 | 앱은 가볍고 빠르게 유지한다. native C++에서는 per-frame allocation 최소화, dirty layout, draw batching, atlas, async IO를 적용하고, AI/PDF 처리는 앱 밖 워커와 캐시 파일로 분리한다. |
| 79 | 일관 실행을 위해 ADR, schema version, owner map, wave plan, test matrix, token budget을 고정한다. 새 요구는 반드시 schema/domain/UI/render/service 중 어디에 속하는지 분류한 뒤 구현한다. |
| 80 | 전체 앱을 카툰 렌더링 게임처럼 보이게 하려면 2D cartoon UI theme부터 만들고, 이후 Vulkan에서 cel-shaded companion/scene layer를 추가한다. 학습 진행은 NPC dialogue와 highlight script가 이끈다. |
| 81 | 토큰 관리는 `codex/common/agent/shared_command.json`, subtree `AGENTS.md`, hash cache, prompt summaries, artifact manifests로 한다. AI 호출 전에는 cache lookup, chunk selection, max token budget, expected output schema를 기록한다. |
| 82 | Vulkan renderer 구현은 `apps/quiz/quiz-vulkan`에서 순수 C++로 진행한다. 1차는 draw command counter가 아닌 실제 window/swapchain/quad/text/image, 2차는 retained scene UI, 3차는 Android surface, 4차는 cartoon/theme renderer 순서로 간다. |

## 당장 다음 작업

1. `apps/quiz/quiz-vulkan/docs/domain-contract.md`에 `content_ref`, `concept_ref`, `theme_ref`, `wrong_note`, `srs`, `proof_submission` 확장 초안을 추가한다.
2. `apps/quiz/quiz-vulkan/docs/scene-schema.md`에 keyboard inset, bottom nav, split reader, companion scene, theme token 요구를 추가한다.
3. `codex/quiz/requirements-tree.md`를 wave 종료 때마다 상태/비용/의존성 기준으로 갱신한다.
4. `codex/quiz/app-context/AGENTS.md`를 기준으로 기존 React 앱에서 이미 끝난 항목과 C++에 포팅할 항목을 분리한다.
5. `apps/quiz/quiz-vulkan`은 먼저 domain/FSM test를 늘리고, renderer는 real Vulkan MVP를 별도 branch/task로 진행한다.
