# Android Quiz App Big Plan

이 문서는 현재 Android-first quiz app의 사용자 경험을 native C++/Vulkan 리메이크가 흡수해 대체하기 위한 폭포수 계획이다.

요구사항 번호는 추적 ID이며, 이 프로젝트의 실제 작업은 아래 의존관계 기반 단계 순서를 따른다.

## 목표

- 현재 React 기반 학습 UX를 기준선으로 고정하고, C++/Vulkan rewrite가 따라야 할 동작 계약으로 추출한다.
- 퀴즈 풀이, 그룹 이동, 오답노트, 하단 navigation, keyboard-safe layout, 모바일 즉시 수정 플로우를 검증한다.
- AI/PDF/OCR 같은 무거운 작업은 앱 안에서 직접 처리하지 않고 외부 artifact와 정규화된 quiz data를 소비한다.

## 선행조건

- `codex/quiz/app-context/CODEMAP.md`를 먼저 읽고 필요한 React source만 연다.
- `build/external/quiz/quiz-data/mobile-decks`를 기본 데이터 fixture로 사용한다.
- UX 변경이 장기 규칙이면 `codex/quiz/구현/NN.md`의 상위 연결과 이 프로젝트의 `구현/NN.md`를 함께 확인한다.

## 의존관계 기반 구현 순서

| 단계 | 기능 | 관련 요구사항 | 완료 조건 |
| --- | --- | --- | --- |
| 1 | Git/deck loading 기준 고정 | 26, 28, 63, 64 | quiz-data Git URL과 local fixture가 같은 deck/day 구조로 로딩됨 |
| 2 | Quiz session UX 정리 | 6, 30, 57 | swipe skip, previous swipe, swipe-only known 이동이 충돌 없이 동작 |
| 3 | Feedback policy 정리 | 7, 8, 9, 47, 60, 61 | 유형별 feedback이 빨간 정답 박스와 큰 OX 없이 표시됨 |
| 4 | Learning buckets | 20, 32, 58, 59 | already-known, wrong note, due review가 분리된 상태로 이동 |
| 5 | Keyboard/mobile editing | 27, 31, 68 | 빈칸 입력 시 layout이 안전 영역에 맞고, 폰에서 즉시 수정 patch 생성 |
| 6 | Reader/study mode 준비 | 4, 46, 67, 70 | 원문/번역 split view와 content_ref 진입점이 정의됨 |
| 7 | Calendar/accessibility hooks | 35, 62, 66 | 옵션 구조와 calendar export hook이 앱 설정에 연결됨 |

## 구현 문서 인덱스

이 프로젝트가 담당하거나 기준으로 참조해야 하는 요구사항별 구현계획이다. 이 표는 실행 순서가 아니라 추적 인덱스이며, 각 문서는 요구사항 원문, 구현 방향, 핵심 산출물, 구현 순서, 대안, 장단점, 구현 기준을 포함한다.

| 구현 | 구현 | 구현 | 구현 | 구현 | 구현 |
| --- | --- | --- | --- | --- | --- |
| [04](구현/04.md) | [06](구현/06.md) | [07](구현/07.md) | [08](구현/08.md) | [09](구현/09.md) | [20](구현/20.md) |
| [26](구현/26.md) | [27](구현/27.md) | [28](구현/28.md) | [29](구현/29.md) | [30](구현/30.md) | [31](구현/31.md) |
| [32](구현/32.md) | [35](구현/35.md) | [46](구현/46.md) | [47](구현/47.md) | [49](구현/49.md) | [57](구현/57.md) |
| [58](구현/58.md) | [59](구현/59.md) | [60](구현/60.md) | [61](구현/61.md) | [62](구현/62.md) | [63](구현/63.md) |
| [64](구현/64.md) | [66](구현/66.md) | [67](구현/67.md) | [68](구현/68.md) | [70](구현/70.md) |  |

## 검증 기준

- 핵심 flow는 브라우저 smoke와 native C++/Vulkan shell에서 smoke test한다.
- keyboard-safe layout은 모바일 viewport 기준으로 확인한다.
- Git loading은 `pjuny0301/quiz-data/tree/main/mobile-decks`와 local `build/external/quiz/quiz-data/mobile-decks`를 모두 확인한다.
- 장기 유지해야 하는 동작은 기존 앱에 묶어두지 않고 `quiz-vulkan` domain/FSM test로 옮길 수 있게 문서화한다.

## 관련 날짜 요구사항

- `일자별요구사항/2026-04-27/requirements.md`
- `구현/06.md`
- `구현/31.md`
- `구현/64.md`
