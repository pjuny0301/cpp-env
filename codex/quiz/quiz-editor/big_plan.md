# Quiz Editor Big Plan

이 문서는 현재 quiz editor의 제작 흐름을 native C++/Vulkan 리메이크와 외부 worker 파이프라인이 흡수해 대체하기 위한 폭포수 계획이다.

요구사항 번호는 추적 ID이며, 이 프로젝트의 실제 작업은 아래 의존관계 기반 단계 순서를 따른다.

## 목표

- 사람이 검수 가능한 방식으로 PDF-derived quiz, blank, distractor, answer explanation, AI patch를 만든다.
- AI가 생성한 내용은 즉시 덮어쓰지 않고 source paragraph, concept, answer traceability를 포함한 patch로 저장한다.
- Git 기반 `quiz-data` 저장소를 편집/검수/commit/push하는 제작 흐름을 표준화해 리메이크 쪽에서 재사용 가능하게 만든다.

## 선행조건

- 기본 data repo는 `C:/aa/build/external/quiz/quiz-data`다.
- 앱 source는 `apps/quiz/quiz-editor`만 canonical로 본다.
- 생성 결과는 앱 런타임에 바로 의존시키지 말고 파일 artifact와 schema-compatible export로 남긴다.

## 의존관계 기반 구현 순서

| 단계 | 기능 | 관련 요구사항 | 완료 조건 |
| --- | --- | --- | --- |
| 1 | Git data workspace | 3, 63, 68 | quiz-data repo 선택, tree 표시, save/commit/push 흐름이 안정적임 |
| 2 | Patch-based editor | 3, 13, 14, 27 | AI/사용자 수정이 reviewable patch로 표시되고 선택 적용됨 |
| 3 | PDF ingest bridge | 1, 4, 12, 15, 70, 77 | PDF index/generation worker 결과를 검수 큐로 불러옴 |
| 4 | Traceability fields | 26, 28, 29, 45 | paragraph, concept, answer, distractor source가 quiz JSON에 남음 |
| 5 | Study content authoring | 46, 67, 71, 72, 73a, 73b | original/translation/script artifact를 생성하고 재사용함 |
| 6 | Advanced grading inputs | 11, 17, 18, 34-44, 65 | OCR/proof/summary grading 요청을 외부 worker artifact로 연결 |
| 7 | QA and export | 33, 69, 79 | generated quiz QA, token cache, schema export 검증이 가능함 |

## 구현 문서 인덱스

이 프로젝트가 담당하거나 기준으로 참조해야 하는 요구사항별 구현계획이다. 이 표는 실행 순서가 아니라 추적 인덱스이며, 각 문서는 요구사항 원문, 구현 방향, 핵심 산출물, 구현 순서, 대안, 장단점, 구현 기준을 포함한다.

| 구현 | 구현 | 구현 | 구현 | 구현 | 구현 |
| --- | --- | --- | --- | --- | --- |
| [01](구현/01.md) | [03](구현/03.md) | [04](구현/04.md) | [11](구현/11.md) | [12](구현/12.md) | [13](구현/13.md) |
| [14](구현/14.md) | [15](구현/15.md) | [17](구현/17.md) | [18](구현/18.md) | [19](구현/19.md) | [21](구현/21.md) |
| [22](구현/22.md) | [23](구현/23.md) | [26](구현/26.md) | [27](구현/27.md) | [28](구현/28.md) | [29](구현/29.md) |
| [33](구현/33.md) | [34](구현/34.md) | [38](구현/38.md) | [39](구현/39.md) | [40](구현/40.md) | [41](구현/41.md) |
| [42](구현/42.md) | [43](구현/43.md) | [44](구현/44.md) | [45](구현/45.md) | [65](구현/65.md) | [68](구현/68.md) |
| [69](구현/69.md) | [70](구현/70.md) | [73b](구현/73b.md) | [77](구현/77.md) |  |  |

## 검증 기준

- dev API와 native filesystem service가 같은 quiz-data path를 바라본다.
- AI patch는 적용 전후 diff를 확인할 수 있어야 한다.
- PDF-derived quiz import는 fixture JSON으로 반복 실행해도 같은 cache key를 사용한다.
- Git 작업은 dry-run 또는 test repo에서 commit/push 전 상태를 표시한다.

## 관련 날짜 요구사항

- `일자별요구사항/2026-04-27/requirements.md`
- `구현/03.md`
- `구현/12.md`
- `구현/77.md`
