# Native Quiz Learning System 요구사항 정의서

- 과제: 과제3 - 요구사항 정의서 작성
- 제출자: 박준용
- 학번: STUDENTNO
- 작성일: 2026년 5월 18일
- GitHub Public Repository: <https://github.com/pjuny0301/cpp-env>
- 대상 구현/참조 프로젝트: `apps/quiz/quiz-vulkan`
- 문서 기준: Native C++23/Vulkan 기반 퀴즈 학습 시스템 재작성 요구사항 정의

본 요구사항 정의서는 공개 GitHub 저장소에서 관리되는 Native Quiz Learning System의 구현 기준을 정리한다. 구현 및 참조 프로젝트는 저장소 안에서 관리하며, 소스 프로젝트는 `apps/quiz/quiz-vulkan`에 둔다. 빌드 산출물은 `build/out`, 외부 의존성 및 재사용 도구는 `build/external` 아래에 분리하여 소스 코드와 생성물을 혼합하지 않는다.

## 1. 서론

### 1.1 문서 목적 및 범위

본 문서의 목적은 기존 Android 퀴즈 앱과 퀴즈 에디터의 기준 동작을 흡수하여 Native C++23/Vulkan 기반 학습 앱으로 재작성하는 데 필요한 요구사항을 명확히 정의하는 것이다. 요구사항은 기능, 품질, 인터페이스, 운영 정책, 제약사항, 향후 확장 항목으로 구분하며, 각 요구사항은 식별자와 검증 기준을 가진다.

문서 범위는 다음과 같다.

- 퀴즈 덱, Day, 문항, 학습 세션, 학습 상태를 다루는 공통 도메인 요구사항
- 퀴즈 풀이, 스와이프, 이미 앎 그룹, 오답노트, 빈칸 입력, 본문 학습 화면 요구사항
- retained scene/layout 계약, UI renderer, Vulkan renderer, text/image/asset/input 엔진 요구사항
- PDF/AI/OCR/증명 채점 기능을 앱 런타임 밖의 산출물 또는 외부 worker로 연동하기 위한 요구사항
- 빌드 산출물, 외부 의존성, 검증 증거, Git 기반 산출물 관리 정책

다음 항목은 본 문서에서 상세 구현 설계가 아니라 요구사항 수준으로만 다룬다.

- 실제 AI 모델 선택, API 과금 정책, 서버 배포 방식
- OCR, 수학 증명 채점, 코드 샌드박스의 내부 알고리즘
- Android surface와 동기화 서버의 최종 배포 구성

### 1.2 대상 시스템 개요

대상 시스템인 Native Quiz Learning System은 PDF 및 에디터 산출물에서 생성된 퀴즈 데이터를 불러와 학습자가 빠르게 반복 학습할 수 있게 하는 네이티브 학습 앱이다. 최종 수렴 지점은 `apps/quiz/quiz-vulkan`이며, 기존 `android-quiz-app`은 학습 UX 기준, `quiz-editor`는 저작 및 AI 산출물 기준으로 참조한다.

시스템의 핵심 구조는 다음 흐름을 따른다.

`main -> modifier_interface -> scene_layout_data_modifier -> scene_layout_edit_data -> scene_layout_patch -> scene_layout_data -> layout_placer -> ui_renderer -> vulkan_renderer`

도메인 상태는 renderer와 layout 코드 밖에 있어야 하며, app 계층이 도메인 snapshot을 scene edit로 변환한다. `layout_placer`는 scene/layout 데이터를 읽어 배치 결과를 만들고, `ui_renderer`는 배치된 UI 데이터를 draw list로 변환하며, `vulkan_renderer`는 renderer 소유 command/resource 데이터만 소비한다.

2026년 5월 18일 기준 프로젝트 저장소에는 도메인, 입력/IME, scene/layout, text/image/asset, Vulkan 경계에 대한 계약, 테스트, 진단용 fake path, 일부 prototype과 부분 구현 증거가 존재한다. 따라서 본 문서는 완성 기능을 선언하는 문서가 아니라, 구현과 검증이 따라야 할 요구사항 기준 문서이다.

### 1.3 용어 정의

| 용어 | 정의 |
| --- | --- |
| Deck | 하나의 학습 묶음이다. 여러 Day와 문항, 본문 자료, 메타데이터를 포함한다. |
| Day | Deck 내부의 학습 단위이다. 사용자는 Day별로 문제를 풀고 복습 상태를 관리한다. |
| Question | 선택형, 다중선택형, 빈칸형, 단답형, 장문형 등을 포함하는 문항 단위이다. |
| Session | 사용자가 특정 Deck/Day/그룹에서 퀴즈를 푸는 실행 상태이다. |
| 이미 앎 그룹 | 사용자가 충분히 안다고 판단한 문항을 일반 학습 흐름에서 제외해 따로 복습하는 그룹이다. |
| 오답노트 | 설정된 오답 조건에 따라 이동한 문항을 별도 복습 대상으로 관리하는 그룹이다. |
| SRS | Spaced Repetition System의 약어로, 시간과 정답 이력에 따라 복습 시점을 조정하는 방식이다. |
| Scene | 도메인 snapshot을 화면 표현 가능한 UI 구조로 변환한 데이터이다. |
| Layout | Scene 노드의 위치, 크기, 키보드 inset, split view 등을 결정한 배치 결과이다. |
| Draw List | UI renderer가 Vulkan renderer로 넘기는 renderer 소유 그리기 명령 목록이다. |
| Artifact | PDF 처리, AI 생성, OCR, 에디터 수정 결과처럼 앱이 소비할 수 있게 저장된 산출물이다. |
| Worker | PDF, AI, OCR, proof grading, code sandbox 같은 무거운 처리를 앱 런타임 밖에서 수행하는 모듈이다. |
| IME | 한글 입력 등 조합형 텍스트 입력을 처리하는 Input Method Editor이다. |

### 1.4 참조 문서

| 구분 | 참조 |
| --- | --- |
| 공개 저장소 | <https://github.com/pjuny0301/cpp-env> |
| 과제 지시문 | `과제3 - 요구사항 정의서 작성.txt` |
| 현재 작업 요약 | `codex/quiz/current.md` |
| 퀴즈 전체 계획 | `codex/quiz/big_plan.md` |
| 요구사항 추적 | `codex/quiz/requirements_traceability_matrix.md` |
| quiz-vulkan 계획 | `codex/quiz/quiz-vulkan/big_plan.md` |
| quiz-vulkan 작업 규칙 | `codex/quiz/quiz-vulkan/AGENTS.md`, `apps/quiz/quiz-vulkan/AGENTS.md` |

## 2. 요구사항 정의

### 2.1 기능적 요구사항

| ID | 요구사항 | 수용 기준 및 검증 방법 |
| --- | --- | --- |
| FR-001 | 시스템은 Deck, Day, Question, Session, Learning State를 표준 데이터 모델로 관리해야 한다. | schema/domain test에서 Deck/Day/Question/Session을 앱 실행 없이 생성, 검증, 직렬화할 수 있어야 한다. |
| FR-002 | 시스템은 외부 worker 또는 에디터가 만든 퀴즈 artifact를 입력으로 받아 학습 가능한 Deck으로 로드해야 한다. | `--deck` 또는 동등한 입력 경로로 fixture artifact를 로드했을 때 문항 수, Day, 개념, 정답, 해설 메타데이터가 손실되지 않아야 한다. |
| FR-003 | 시스템은 select, multiselect, multiblank, blank, short answer, long question 유형을 구분하여 표시해야 한다. | 각 문항 유형별 scene snapshot 또는 app screen test에서 유형별 입력 영역과 피드백 영역이 생성되어야 한다. |
| FR-004 | select 유형은 선택 직후 별도 빨간 정답 박스나 OX 표시를 띄우지 않아야 한다. | select 정답/오답 fixture에서 UI snapshot에 빨간 정답 박스와 OX 표시가 없어야 한다. |
| FR-005 | multiselect 유형은 여러 정답을 순차 선택할 수 있고, 선택된 답을 빈칸 또는 선택 목록에 차례대로 반영해야 한다. | 다중 정답 fixture에서 선택 순서와 답안 배열이 일치하고, 불필요한 선지 누름 애니메이션 상태가 생성되지 않아야 한다. |
| FR-006 | blank, multiblank, short answer 유형은 텍스트 입력과 제출을 지원해야 하며 한글 IME 조합 중에는 미완성 문자열을 도메인 답안으로 확정하지 않아야 한다. | IME preedit/commit test에서 조합 중 raw text와 shortcut route가 domain action을 발생시키지 않고, commit 이후에만 답안이 반영되어야 한다. |
| FR-007 | 사용자는 퀴즈 카드를 좌우로 스와이프하여 이전/다음 문항으로 이동할 수 있어야 한다. | gesture recognizer test에서 오른쪽 또는 다음 방향 스와이프는 다음 문항, 왼쪽 또는 이전 방향 스와이프는 이전 문항 action으로 분류되어야 한다. |
| FR-008 | 사용자는 설정된 횟수 이상 문항을 넘긴 뒤 해당 문항을 이미 앎 그룹으로 이동할 수 있어야 한다. | learning state test에서 threshold 이상 스와이프된 문항만 이미 앎 후보가 되고, 확정 시 일반 그룹에서 제외되어야 한다. |
| FR-009 | 이미 앎 그룹의 문항은 별도 탭 또는 하단 navigation에서 접근할 수 있어야 하며, 그룹 안에서 별도 퀴즈 세션을 실행할 수 있어야 한다. | app state test에서 이미 앎 navigation action이 known group session을 생성하고 일반 session과 분리된 진행 상태를 가져야 한다. |
| FR-010 | 사용자는 이미 앎 그룹의 문항을 모름으로 지정하여 일반 그룹으로 즉시 되돌릴 수 있어야 한다. | long press 또는 동등한 action fixture에서 설정된 오답 횟수와 무관하게 문항 group이 일반 학습 group으로 복귀해야 한다. |
| FR-011 | 오답노트 옵션이 켜져 있으면 설정된 오답 횟수 조건에 따라 문항이 오답노트로 이동해야 한다. | wrong note option test에서 오답 횟수 threshold 도달 시 wrong note group으로 이동하고, 설정된 정답 횟수 달성 시 원래 group으로 복귀해야 한다. |
| FR-012 | 시스템은 SRS 정책을 통해 이미 앎 그룹의 문항을 시간 경과에 따라 다시 복습 대상으로 전환할 수 있어야 한다. | 기준 날짜와 마지막 복습 기록 fixture를 주입했을 때 due 상태가 결정적으로 계산되어야 한다. |
| FR-013 | 빈칸 입력 중 OS 키보드 또는 IME UI가 올라오면 문제와 입력창이 화면 밖으로 밀려 사라지지 않도록 keyboard inset을 반영해야 한다. | layout snapshot test에서 keyboard inset 적용 후 focused input과 제출 버튼이 viewport 안에 있어야 한다. |
| FR-014 | 시스템은 PDF 본문과 번역본을 나란히 보는 split reader 화면을 제공해야 한다. | 본문 fixture에서 원문 영역과 번역 영역이 같은 문단 단위로 매핑되고, 번역 영역은 별도 테두리 없이 읽기 가능한 배치로 표현되어야 한다. |
| FR-015 | 정답 해설, 정답 사유 작성, AI 토론 결과는 앱 런타임의 직접 AI 호출이 아니라 검수 가능한 artifact로 표시할 수 있어야 한다. | cached explanation/discussion fixture를 로드하면 UI에 출처, 생성 시점, 검수 상태가 표시되고 네트워크 없이 재현 가능해야 한다. |
| FR-016 | 시스템은 Deck 목록과 Day 목록을 3 x n 카드형 grid 또는 동등한 반복 학습용 선택 화면으로 표시해야 한다. | deck/day fixture에서 화면 폭에 맞는 grid 배치 snapshot이 생성되고 카드 선택 action이 해당 Deck/Day session으로 이어져야 한다. |
| FR-017 | 시스템은 테마 token을 통해 선지 형태, 피드백 표시, 색상, 폰트, 카툰형 학습 companion 표현을 확장할 수 있어야 한다. | theme fixture 변경만으로 UI renderer snapshot의 style token이 바뀌고 domain state가 변경되지 않아야 한다. |
| FR-018 | 시스템은 모든 주요 사용자 action을 추적 가능한 app action 또는 diagnostics로 기록해야 한다. | 입력, 스와이프, 텍스트 제출, 그룹 이동, artifact 로드 실패가 테스트에서 안정적인 action/diagnostic 문자열로 확인되어야 한다. |

### 2.2 비기능적 요구사항

| ID | 요구사항 | 수용 기준 및 검증 방법 |
| --- | --- | --- |
| NFR-001 | 시스템은 C++23을 기준 언어로 사용하고, renderer MVP는 Vulkan backend를 목표로 해야 한다. | CMake target이 C++23로 빌드되고, Vulkan backend contract test가 scene/UI/domain header 의존 없이 통과해야 한다. |
| NFR-002 | domain, scene/layout, UI renderer, Vulkan renderer는 단방향 의존성을 유지해야 한다. | architecture boundary test에서 `ui_renderer`가 `layout_placer` 구현이나 domain/app header를 포함하면 실패해야 한다. |
| NFR-003 | 학습 UX는 빠른 반복 풀이에 적합해야 하며 입력 action 후 상태 전이가 지연 없이 관찰되어야 한다. | 기준 desktop 환경에서 주요 input-to-state 전이가 100ms 이내라는 목표를 성능 측정 항목으로 기록하고 release 전 profiling으로 확인해야 한다. |
| NFR-004 | scene/layout 결과는 deterministic해야 한다. | 동일 Deck, viewport, keyboard inset, theme token 입력에 대해 snapshot diff가 없어야 한다. |
| NFR-005 | renderer는 실제 GPU resource path가 완성되기 전에도 CPU fallback 또는 fake diagnostics로 실패 원인을 재현 가능하게 보고해야 한다. | texture, text atlas, descriptor, swapchain fixture에서 blocked/ready 상태와 blocker reason이 안정적으로 출력되어야 한다. |
| NFR-006 | 텍스트 엔진은 UTF-8과 한글 IME 입력을 보존해야 한다. | 한글, combining mark, mixed script fixture에서 caret, selection, glyph run, fallback diagnostics가 깨지지 않아야 한다. |
| NFR-007 | asset pipeline은 font, image, shader, deck payload의 source URI, cache key, materialized path, byte count, content hash를 추적해야 한다. | asset byte provider test에서 누락, 빈 파일, hash mismatch, 잘못된 scheme, 중복 id를 구분해야 한다. |
| NFR-008 | 앱 런타임은 AI/PDF/OCR/proof/code sandbox 같은 무거운 처리에 직접 결합되지 않아야 한다. | 앱 테스트는 네트워크 없이 cached artifact fixture만으로 통과해야 하며, AI worker 실패는 artifact 상태로 표시되어야 한다. |
| NFR-009 | 소스, 외부 의존성, 빌드 산출물은 물리적으로 분리되어야 한다. | `apps/quiz`에는 source project만 두고, generated output은 `build/out`, dependency snapshot은 `build/external`에 있어야 한다. |
| NFR-010 | 사용자의 학습 기록과 개인 메모 연동 데이터는 최소한의 명시적 입력과 로컬 저장 정책을 가져야 한다. | persistence/sync 설계 전까지 테스트 fixture에는 민감한 원문 데이터를 직접 포함하지 않고, 사용자 승인 없는 외부 전송 경로를 만들지 않아야 한다. |
| NFR-011 | 요구사항과 구현 증거는 추적 가능해야 한다. | 각 주요 요구사항은 구현 문서, 테스트, 현재 상태, 다음 확인 항목 중 하나 이상과 연결되어야 한다. |

### 2.3 인터페이스 요구사항

| ID | 인터페이스 | 요구사항 | 수용 기준 및 검증 방법 |
| --- | --- | --- | --- |
| IR-001 | Deck artifact 입력 | 앱은 quiz editor 또는 외부 worker가 생성한 Deck artifact를 파일 경로 또는 launch option으로 입력받아야 한다. | fixture path를 입력했을 때 Deck id, Day id, 문항, 해설, 원문 참조가 로드되어야 한다. |
| IR-002 | Domain snapshot | app 계층은 domain 상태를 renderer가 직접 알 수 없는 snapshot으로 노출해야 한다. | renderer target이 domain header 없이 빌드되어야 한다. |
| IR-003 | Scene edit/patch | app presenter는 domain snapshot을 `scene_layout_edit_data`와 patch 흐름으로 변환해야 한다. | scene patch test에서 동일 action은 동일 patch 결과를 만들어야 한다. |
| IR-004 | Layout output | `layout_placer`는 scene/layout data와 viewport/inset/theme 입력만 소비하고 scene data를 직접 변경하지 않아야 한다. | layout test에서 입력 scene은 불변이고 placed scene output만 생성되어야 한다. |
| IR-005 | UI renderer draw list | UI renderer는 placed scene을 draw list로 변환하되 Vulkan API를 직접 호출하지 않아야 한다. | UI renderer compile boundary와 draw-list snapshot test에서 Vulkan include가 없어야 한다. |
| IR-006 | Vulkan command/resource packet | Vulkan renderer는 draw list, texture packet, glyph packet, descriptor payload 등 renderer 소유 데이터만 소비해야 한다. | Vulkan tests에서 scene/UI/domain/app/input/audio header 의존이 없어야 하고 blocked resource reason이 기록되어야 한다. |
| IR-007 | Text engine | text engine은 font source, UTF-8 run, glyph layout, atlas/page/upload readiness를 renderer-facing packet으로 제공해야 한다. | glyph packet fixture에서 atlas page, UV bounds, fallback, missing font blocker가 검증되어야 한다. |
| IR-008 | Image/asset engine | image/asset engine은 image URI, alt text, aspect, sampler/cache identity, materialized bytes를 texture packet으로 제공해야 한다. | texture packet fixture에서 invalid URI, duplicate stable id, sampler mismatch, missing bytes가 구분되어야 한다. |
| IR-009 | Input/IME | platform input은 gesture, text edit, shortcut, pointer capture, IME preedit/commit route로 정규화되어 app action으로 전달되어야 한다. | active IME composition 중 shortcut/raw text suppression diagnostics가 남고 domain action은 발생하지 않아야 한다. |
| IR-010 | Persistence/sync export | 학습 상태는 local persistence와 향후 sync worker가 소비할 수 있는 stable record 형태로 내보낼 수 있어야 한다. | Deck id, question id, group, due date, answer history가 stable key로 직렬화되어야 한다. |
| IR-011 | External worker artifact | PDF/OCR/AI/proof/code worker는 앱 런타임에 직접 결합하지 않고 manifest, cache key, artifact status를 통해 결과를 전달해야 한다. | worker failure, pending, reviewed, accepted 상태가 앱 UI에서 동일 schema로 표시되어야 한다. |

## 3. 기타 요구사항

### 3.1 운영 정책

| ID | 운영 정책 | 검증 기준 |
| --- | --- | --- |
| OP-001 | 모든 요구사항 문서, 구현 문서, 테스트 증거는 GitHub 공개 저장소에서 관리한다. | 제출 PDF 상단에 공개 저장소 링크가 있어야 하고, 관련 Markdown 문서는 저장소에 commit되어야 한다. |
| OP-002 | `apps/quiz`는 source project만 포함하고 generated output, dependency snapshot, editor/user setting은 `build/out` 또는 `build/external`로 분리한다. | repository review에서 source tree 안에 build artifact가 추가되지 않아야 한다. |
| OP-003 | 구현 변경은 focused test, interface contract compile test, architecture boundary test, `git diff --check` 중 관련 항목을 통과해야 한다. | 각 변경 기록 또는 handoff에 실행한 검증 명령과 결과가 남아야 한다. |
| OP-004 | worker가 만든 PDF/AI/OCR 산출물은 사람이 검수할 수 있는 artifact와 diff 가능한 JSON 또는 Markdown으로 남겨야 한다. | 생성 결과가 cache key, source reference, review status를 포함해야 한다. |
| OP-005 | 매주 1회 이상 commit 기록을 남기고, LMS 제출물과 GitHub 반영을 병행한다. | 제출 전 요구사항 정의서 commit hash를 확인할 수 있어야 한다. |

### 3.2 제약사항

| ID | 제약사항 | 설명 |
| --- | --- | --- |
| C-001 | renderer는 domain/app/input/audio 상태를 직접 참조할 수 없다. | renderer는 draw list와 renderer-owned resource packet만 소비한다. |
| C-002 | 앱 런타임은 PDF 파싱, AI 호출, OCR, proof grading, code sandbox 실행을 직접 수행하지 않는다. | 해당 기능은 외부 worker artifact로 연동한다. |
| C-003 | 구현 상태가 계약, 테스트, prototype인 기능은 완료 기능으로 문서화하지 않는다. | 제출 문서와 GitHub 문서에서 "부분 구현", "계획", "검증 대상"을 구분한다. |
| C-004 | 요구사항 번호는 실행 순서가 아니라 추적 ID이다. | 실제 구현 우선순위는 dependency-based phase를 따른다. |
| C-005 | 항목 17과 18은 중복 요구이며, 항목 48은 비어 있고, 항목 53은 원문이 불완전하다. | 추적표에서는 중복/예약/불완전 상태를 유지하고 임의로 새 의미를 부여하지 않는다. |
| C-006 | Windows MinGW와 Vulkan native path는 플랫폼 의존성이 크므로 fake path와 diagnostics를 병행한다. | 실제 GPU/OS 기능이 없는 환경에서도 contract test가 원인을 설명하며 실패 또는 skip되어야 한다. |

### 3.3 향후 확장사항

| ID | 확장사항 | 기대 효과 |
| --- | --- | --- |
| EX-001 | OCR과 수학 증명 채점 worker를 연동한다. | 태블릿 필기 또는 카메라 입력을 LaTeX/텍스트로 변환하고 proof grading artifact로 표시할 수 있다. |
| EX-002 | 개념 의존성 그래프와 Zettelkasten/Obsidian 연동을 추가한다. | 막힌 문제에서 선행 개념, 과거 메모, 관련 본문을 추천할 수 있다. |
| EX-003 | 코드/로직 스니펫 샌드박스 채점 worker를 추가한다. | 알고리즘, 프로그래밍, 디버깅 퀴즈를 실행 결과와 AI 피드백으로 검증할 수 있다. |
| EX-004 | 카툰 렌더링 게임형 theme와 Codexbot 학습 companion scene을 추가한다. | 본문 학습, 말풍선 설명, script 기반 guided learning을 같은 renderer 계약 안에서 확장할 수 있다. |
| EX-005 | Android surface, calendar 연동, sync backend, 학습 통계 dashboard를 추가한다. | desktop MVP 이후 모바일 학습과 기기간 진행 상태 공유가 가능해진다. |

### 3.4 특이사항

| ID | 특이사항 | 처리 방침 |
| --- | --- | --- |
| S-001 | 현재 프로젝트는 완성 앱이 아니라 native remake 진행 중인 구현/검증 기준이다. | 문서에는 완료 기능과 목표 요구사항을 구분해서 작성한다. |
| S-002 | 학번은 저장소 context에 없으므로 `STUDENTNO` placeholder를 유지한다. | LMS 제출 전 사용자가 실제 학번으로 교체한다. |
| S-003 | 과제 제출물은 PDF이지만 저장소에는 Markdown 원본도 함께 관리한다. | Markdown은 GitHub에서 변경 이력을 추적하고, PDF는 LMS 업로드용으로 변환한다. |

## 4. 참고문헌 및 부록

### 4.1 참고문헌

1. 박준용, GitHub Public Repository, <https://github.com/pjuny0301/cpp-env>
2. 과제 지시문, `과제3 - 요구사항 정의서 작성.txt`, 2026년 5월 18일 제출 과제
3. `codex/quiz/current.md`, Quiz Current Handoff, 2026년 5월 17일 기준
4. `codex/quiz/big_plan.md`, Quiz Product Big Plan
5. `codex/quiz/requirements_traceability_matrix.md`, Quiz 요구사항 추적 매트릭스
6. `codex/quiz/quiz-vulkan/big_plan.md`, Quiz Vulkan Big Plan
7. `apps/quiz/quiz-vulkan/AGENTS.md`, quiz-vulkan Agent Rules
8. Khronos Group, Vulkan Specification
9. ISO/IEC, C++23 Programming Language Standard

### 4.2 부록 A. 요구사항 추적표

| 요구사항 ID | 관련 원 요구사항/프로젝트 항목 | 주요 프로젝트 영역 | 추적 및 검증 자료 |
| --- | --- | --- | --- |
| FR-001, FR-002 | 1, 5, 10, 12, 15, 16, 24, 26, 28, 79 | domain, persistence/sync | `src/core/domain/*`, deck artifact loader tests, schema fixture |
| FR-003, FR-004, FR-005 | 7, 8, 9, 47, 49, 50-56, 60, 61 | domain, scene/layout, ui renderer | app screen snapshot tests, quiz session tests, theme token fixture |
| FR-006, FR-013 | 27, 31, 34-44 | input/IME, scene/layout, text engine | IME route tests, text input model tests, keyboard inset layout snapshot |
| FR-007, FR-008, FR-009, FR-010 | 6, 20, 30, 57, 58 | domain, input/IME, persistence/sync | gesture recognizer tests, learning state tests, app state tests |
| FR-011, FR-012 | 20, 32, 59, 62 | domain, persistence/sync | wrong note tests, SRS due calculation fixture, sync export record |
| FR-014, FR-015 | 4, 13, 14, 46, 67, 70 | scene/layout, text engine, persistence/sync | split reader fixture, explanation/discussion artifact fixture |
| FR-016 | 64, 66 | scene/layout, ui renderer | deck/day grid layout snapshot |
| FR-017 | 71-76, 80, 81 | ui renderer, text engine, image/asset | theme fixture, Codexbot script artifact, asset catalog |
| FR-018 | 24, 33, 78, 79, 82 | domain, input/IME, vulkan renderer | diagnostics strings, interface contract compile tests, architecture boundary tests |
| NFR-001, NFR-002 | 10, 16, 24, 78, 82 | vulkan renderer, ui renderer, scene/layout | C++23 compile target, architecture boundary tests |
| NFR-003, NFR-004, NFR-005 | 31, 46, 64, 66, 78, 82 | scene/layout, ui renderer, vulkan renderer | deterministic snapshot diff, CPU fallback framebuffer smoke, Vulkan diagnostics |
| NFR-006, IR-009 | 31, 34-44 | text engine, input/IME | UTF-8 run tests, IME preedit/commit route tests |
| NFR-007, IR-007, IR-008 | 2, 46, 67, 74, 78, 82 | text engine, image/asset, vulkan renderer | glyph packet tests, texture packet tests, asset byte provider tests |
| NFR-008, IR-011 | 1, 3, 4, 11-23, 34-45, 65, 69, 77 | persistence/sync, image/asset, domain | worker artifact manifest, cache key fixture, review status |
| NFR-009, OP-002, C-002 | workspace policy | build/out, build/external, apps/quiz | repository layout review, `git diff --check` |
| IR-001, IR-002, IR-003, IR-004 | 5, 10, 16, 24, 79 | domain, scene/layout | app presenter tests, scene patch tests, layout placer tests |
| IR-005, IR-006 | 2, 78, 82 | ui renderer, vulkan renderer | draw-list snapshot, command/resource packet tests |
| IR-010 | 20, 32, 62, 69, 79 | persistence/sync | stable progress record fixture, export/import test |

### 4.3 부록 B. 제출 전 확인 목록

| 항목 | 확인 내용 |
| --- | --- |
| GitHub 링크 | PDF 상단에 <https://github.com/pjuny0301/cpp-env> 포함 |
| 학번 placeholder | `STUDENTNO`는 사용자가 실제 학번으로 교체해야 함 |
| 파일명 | LMS 제출 전 `hw3_STUDENTNO_박준용.pdf`에서 `STUDENTNO` 교체 필요 |
| 저장소 반영 | Markdown 원본은 `codex/quiz/assignments/hw3_requirements_definition.md`로 commit |
| 산출물 분리 | Desktop PDF/Markdown은 제출용이며 repo commit 대상이 아님 |
