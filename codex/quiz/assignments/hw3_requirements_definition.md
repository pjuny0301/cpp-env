# Native Quiz Learning System 요구사항 정의서

- 과제: 과제3 - 요구사항 정의서 작성
- 제출자: 박준용
- 학번: 2023125076
- 작성일: 2026년 5월 18일
- GitHub Public Repository: <https://github.com/pjuny0301/cpp-env>
- 대상 시스템: Native Quiz Learning System

## 1. 서론

### 1.1 문서 목적 및 범위

본 문서는 PDF, 에디터 산출물, 기존 퀴즈 앱의 학습 흐름을 바탕으로 C++23 기반 네이티브 퀴즈 학습 시스템을 개발하기 위한 요구사항을 정의한다. 요구사항은 기능, 품질, 인터페이스, 운영, 제약, 확장 항목으로 구분한다.

범위는 퀴즈 데이터 로드, 학습 세션, 정답 입력, 오답 관리, 이미 앎 그룹, 본문 학습, UI/렌더링, 입력, 자산 관리, 외부 작업 산출물 연동까지 포함한다. AI, OCR, PDF 처리, 증명 채점은 앱 내부에서 직접 실행하지 않고 산출물 형태로 연동하는 대상으로 정의한다.

### 1.2 대상 시스템 개요

대상 시스템은 학습자가 Deck과 Day를 선택하고, 여러 유형의 문항을 반복 풀이하며, 학습 상태를 저장하는 네이티브 데스크톱 학습 앱이다. 기존 Android 퀴즈 앱은 학습 UX 기준으로, quiz-editor는 데이터 제작 기준으로 참고한다.

핵심 구조는 다음과 같다.

```text
도메인 상태 -> Scene 수정 데이터 -> Scene/Layout 데이터 -> 배치 결과 -> Draw List -> Renderer
```

Renderer는 도메인이나 학습 규칙을 직접 알지 않고, 화면 그리기에 필요한 명령과 자산만 소비해야 한다.

### 1.3 용어 정의

| 용어 | 정의 |
| --- | --- |
| Deck | 여러 Day와 문항을 포함하는 학습 묶음 |
| Day | Deck 안의 학습 단위 |
| Question | 선택형, 빈칸형, 단답형, 장문형 등 문제 단위 |
| Session | 사용자가 특정 Deck/Day를 푸는 실행 상태 |
| 이미 앎 그룹 | 사용자가 충분히 안다고 표시한 문항 그룹 |
| 오답노트 | 반복 오답 문항을 별도로 복습하는 그룹 |
| Scene | 화면에 표시할 UI 구조 데이터 |
| Draw List | 렌더러가 소비하는 그리기 명령 목록 |
| Artifact | PDF, AI, OCR, 에디터 작업 결과로 생성된 입력 산출물 |

### 1.4 참조 문서

| 구분 | 내용 |
| --- | --- |
| 공개 저장소 | <https://github.com/pjuny0301/cpp-env> |
| 과제 지시문 | 과제3 - 요구사항 정의서 작성 |
| 프로젝트 계획 | `codex/quiz/big_plan.md` |
| 요구사항 추적 | `codex/quiz/requirements_traceability_matrix.md` |
| 구현 대상 | `apps/quiz/quiz-vulkan` |

## 2. 요구사항 정의

### 2.1 기능적 요구사항

| ID | 요구사항 | 검증 기준 |
| --- | --- | --- |
| FR-01 | 시스템은 Deck, Day, Question, Session, Learning State를 관리해야 한다. | fixture 데이터로 각 모델을 생성하고 직렬화할 수 있다. |
| FR-02 | 시스템은 외부에서 생성된 quiz artifact를 Deck으로 로드해야 한다. | Deck id, Day, 문항, 정답, 해설이 손실 없이 로드된다. |
| FR-03 | 시스템은 선택형, 다중선택형, 빈칸형, 단답형, 장문형 문항을 구분해야 한다. | 문항 유형별 입력 방식과 화면 상태가 다르게 생성된다. |
| FR-04 | 선택형 문항은 선택 직후 불필요한 OX 표시나 정답 강조를 하지 않아야 한다. | 선택 직후 화면 snapshot에 별도 OX 표시가 없다. |
| FR-05 | 다중선택형 문항은 여러 답을 순서대로 선택할 수 있어야 한다. | 선택 순서가 답안 배열에 반영된다. |
| FR-06 | 텍스트 입력 문항은 한글 IME 조합과 제출을 지원해야 한다. | 조합 중 문자열은 확정 답안으로 처리되지 않는다. |
| FR-07 | 사용자는 스와이프 또는 버튼으로 이전/다음 문항으로 이동할 수 있어야 한다. | 입력 이벤트가 문항 이동 action으로 변환된다. |
| FR-08 | 사용자는 문항을 이미 앎 그룹으로 옮기거나 다시 일반 그룹으로 되돌릴 수 있어야 한다. | 학습 상태에서 문항 그룹이 정확히 변경된다. |
| FR-09 | 오답노트 옵션이 켜져 있으면 반복 오답 문항을 별도 그룹으로 이동해야 한다. | 오답 횟수 조건 충족 시 오답노트 상태가 된다. |
| FR-10 | 본문 학습 화면은 원문과 번역을 함께 볼 수 있어야 한다. | 원문/번역 문단이 같은 화면에서 대응되어 표시된다. |
| FR-11 | AI 해설, OCR, PDF 처리 결과는 검수 가능한 산출물로 표시해야 한다. | 네트워크 없이 저장된 artifact를 읽어 표시할 수 있다. |
| FR-12 | Deck 목록과 Day 목록은 반복 학습에 적합한 카드형 선택 화면을 제공해야 한다. | Deck/Day 선택 후 해당 학습 세션으로 진입한다. |

### 2.2 비기능적 요구사항

| ID | 요구사항 | 검증 기준 |
| --- | --- | --- |
| NFR-01 | 시스템은 C++23 기준으로 개발한다. | CMake 빌드 설정과 컴파일 테스트가 C++23을 사용한다. |
| NFR-02 | 도메인, 레이아웃, UI renderer, Vulkan renderer는 단방향 의존성을 유지한다. | renderer가 domain/app header를 include하면 boundary test가 실패한다. |
| NFR-03 | 같은 입력에 대한 scene/layout 결과는 항상 같아야 한다. | 동일 fixture의 snapshot diff가 없다. |
| NFR-04 | 한글과 UTF-8 텍스트를 깨뜨리지 않아야 한다. | 한글 입력, caret, selection test가 통과한다. |
| NFR-05 | 빌드 산출물과 외부 의존성은 소스 폴더와 분리한다. | source tree에는 generated output이 남지 않는다. |
| NFR-06 | 렌더링 실패 원인은 진단 정보로 확인 가능해야 한다. | texture/font/swapchain 실패 시 reason이 기록된다. |
| NFR-07 | AI/OCR/PDF 처리는 앱 실행 경로에 직접 결합하지 않는다. | 앱은 cached artifact만으로 주요 test를 통과한다. |

### 2.3 인터페이스 요구사항

| ID | 인터페이스 | 요구사항 |
| --- | --- | --- |
| IR-01 | Deck 입력 | 파일 경로 또는 실행 옵션으로 Deck artifact를 입력받는다. |
| IR-02 | Scene 수정 | app 계층은 도메인 상태를 scene edit/patch로 변환한다. |
| IR-03 | Layout 출력 | layout 계층은 scene data를 직접 변경하지 않고 배치 결과를 만든다. |
| IR-04 | UI renderer | UI renderer는 placed scene을 draw list로 변환한다. |
| IR-05 | Vulkan renderer | Vulkan renderer는 draw list와 renderer resource packet만 소비한다. |
| IR-06 | Text engine | 글꼴, glyph, atlas, UTF-8 layout 정보를 제공한다. |
| IR-07 | Image/asset engine | image URI, decoded bytes, sampler, cache key를 제공한다. |
| IR-08 | Input/IME | platform input을 gesture, text edit, shortcut action으로 정규화한다. |
| IR-09 | 외부 처리 모듈 | AI/OCR/PDF/증명 채점 결과를 manifest와 artifact 상태로 전달한다. |

## 3. 기타 요구사항

### 3.1 운영 정책

| ID | 정책 |
| --- | --- |
| OP-01 | 요구사항 문서, 구현 문서, 테스트 증거는 GitHub 저장소에서 관리한다. |
| OP-02 | LMS 제출물은 PDF로 제출하고, 원본 Markdown은 저장소에 보관한다. |
| OP-03 | 변경 작업은 관련 테스트와 diff 검사를 거쳐 반영한다. |
| OP-04 | AI/OCR/PDF 산출물은 사람이 검토할 수 있는 형태로 저장한다. |

### 3.2 제약사항

| ID | 제약사항 |
| --- | --- |
| C-01 | renderer는 도메인 상태를 직접 참조할 수 없다. |
| C-02 | 앱 런타임은 AI, OCR, PDF 파싱, 코드 실행을 직접 수행하지 않는다. |
| C-03 | 기능 완료 여부는 구현 코드와 테스트 증거로 구분한다. |
| C-04 | 요구사항 번호는 추적 ID이며 실제 구현 순서는 의존관계에 따른다. |

### 3.3 향후 확장사항

| ID | 확장사항 |
| --- | --- |
| EX-01 | OCR 기반 손글씨 답안 인식 |
| EX-02 | 수학 증명 채점 외부 처리 모듈 |
| EX-03 | 코드 실행형 문제 채점 외부 처리 모듈 |
| EX-04 | 모바일 표면과 기기간 동기화 |
| EX-05 | 학습 통계와 캘린더 연동 |

## 4. 참고문헌 및 부록

### 4.1 참고문헌

1. GitHub Public Repository, <https://github.com/pjuny0301/cpp-env>
2. 과제3 지시문, 요구사항 정의서 작성
3. `codex/quiz/big_plan.md`
4. `codex/quiz/requirements_traceability_matrix.md`
5. `apps/quiz/quiz-vulkan`

### 4.2 부록 A. 요구사항 추적 요약

| 요구사항 | 관련 영역 |
| --- | --- |
| FR-01, FR-02 | domain, artifact loader |
| FR-03 ~ FR-09 | quiz session, input, learning state |
| FR-10 ~ FR-12 | scene/layout, editor artifact |
| NFR-01 ~ NFR-07 | build, boundary, renderer, asset |
| IR-01 ~ IR-09 | 앱, 엔진, 외부 처리 인터페이스 |
