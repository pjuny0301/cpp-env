# Quiz Vulkan Big Plan

이 문서는 기존 Android quiz app과 quiz editor의 기능을 흡수해 native C++/Vulkan 기반 최종 앱으로 대체하기 위한 폭포수 계획이다.

요구사항 번호는 추적 ID이며, 이 프로젝트의 실제 작업은 아래 의존관계 기반 단계 순서를 따른다.

## 목표

- 순수 C++ domain과 retained scene/layout 계약을 먼저 안정화하고, 그 위에 Vulkan renderer를 얹는다.
- UI와 renderer는 domain state를 직접 소유하지 않는다.
- 장기적으로 desktop MVP, Android surface, editor/authoring surface, cartoon/theme renderer, Codexbot study scene까지 확장한다.

## 선행조건

- 기존 재작성 계획은 `일자별요구사항/2026-04-27/archive/`에 보존한다.
- Android app과 quiz editor의 기준 동작은 `../android-quiz-app/big_plan.md`, `../quiz-editor/big_plan.md`, app-context status를 따른다.
- renderer 구현 전 domain/FSM, scene schema, layout constraint가 testable해야 한다.

## 의존관계 기반 구현 순서

| 단계 | 기능 | 관련 요구사항 | 완료 조건 |
| --- | --- | --- | --- |
| 1 | Domain model and schema | 5, 10, 16, 24, 26, 28, 79 | deck/day/question/session/learning state가 UI 없이 테스트됨 |
| 2 | Quiz FSM | 6, 20, 30, 32, 57-61 | swipe, known, wrong note, due review transition test 통과 |
| 3 | Scene and layout contract | 2, 31, 46, 47, 49, 64, 66 | keyboard inset, split reader, option feedback, deck grid layout snapshot 생성 |
| 4 | Renderer MVP | 78, 82 | desktop window/swapchain/quad/text/image draw path가 실제 화면에 표시됨 |
| 5 | Theme system | 50-56, 74-76, 80 | motion/style/layout token으로 option shape, feedback, cartoon style 제어 |
| 6 | Study companion scene | 67, 70-73b | saved chapter script를 interpreter가 읽어 Codexbot/말풍선/highlight 표시 |
| 7 | External workers integration | 1, 4, 11, 12, 15, 17-19, 21-23, 34-45, 65, 69, 77 | PDF/OCR/proof/code artifacts를 app runtime 밖에서 받아 표시 |

## 구현 문서 인덱스

이 프로젝트가 담당하거나 기준으로 참조해야 하는 요구사항별 구현계획이다. 이 표는 실행 순서가 아니라 추적 인덱스이며, 각 문서는 요구사항 원문, 구현 방향, 핵심 산출물, 구현 순서, 대안, 장단점, 구현 기준을 포함한다.

| 구현 | 구현 | 구현 | 구현 | 구현 | 구현 |
| --- | --- | --- | --- | --- | --- |
| [02](구현/02.md) | [05](구현/05.md) | [06](구현/06.md) | [07](구현/07.md) | [08](구현/08.md) | [09](구현/09.md) |
| [10](구현/10.md) | [16](구현/16.md) | [20](구현/20.md) | [24](구현/24.md) | [30](구현/30.md) | [31](구현/31.md) |
| [32](구현/32.md) | [46](구현/46.md) | [47](구현/47.md) | [49](구현/49.md) | [50](구현/50.md) | [51](구현/51.md) |
| [52](구현/52.md) | [53](구현/53.md) | [54](구현/54.md) | [55](구현/55.md) | [56](구현/56.md) | [57](구현/57.md) |
| [58](구현/58.md) | [59](구현/59.md) | [60](구현/60.md) | [61](구현/61.md) | [64](구현/64.md) | [66](구현/66.md) |
| [67](구현/67.md) | [71](구현/71.md) | [72](구현/72.md) | [73a](구현/73a.md) | [73b](구현/73b.md) | [74](구현/74.md) |
| [75](구현/75.md) | [76](구현/76.md) | [78](구현/78.md) | [79](구현/79.md) | [80](구현/80.md) | [81](구현/81.md) |
| [82](구현/82.md) |  |  |  |  |  |

## 검증 기준

- domain/FSM unit test가 renderer 없이 통과한다.
- scene/layout snapshot은 deterministic해야 한다.
- Vulkan smoke test는 desktop에서 nonblank frame을 확인한다.
- Android surface 작업은 desktop MVP 이후에만 시작한다.
- AI/PDF/OCR/proof 기능은 직접 AI 호출이 아니라 cached artifact fixture로 먼저 검증한다.

## 관련 날짜 요구사항

- `일자별요구사항/2026-04-27/requirements.md`
- `구현/24.md`
- `구현/76.md`
- `구현/82.md`
