# Native Quiz Learning System 요구사항 분석서

- 과제: 과제4 - 요구사항 분석서 작성
- 제출자: 박준용
- 학번: 2023125076
- 작성일: 2026년 5월 18일
- GitHub Public Repository: <https://github.com/pjuny0301/cpp-env>
- 대상 시스템: Native Quiz Learning System

## 1. 서론

### 1.1 문서 목적 및 범위

본 문서는 과제3 요구사항 정의서를 바탕으로 Native Quiz Learning System의 구조, 행위, 인터페이스, 제약사항, 요구사항 추적 관계를 분석한다. 분석 범위는 학습 앱 실행 경로, 퀴즈 데이터 처리, 입력/렌더링 엔진, 외부 산출물 연동이다.

### 1.2 용어 정의

| 용어 | 정의 |
| --- | --- |
| Actor | 시스템과 상호작용하는 사용자 또는 외부 구성요소 |
| Context Diagram | 대상 시스템과 외부 시스템의 관계를 나타내는 그림 |
| CRC Card | 클래스의 책임과 협력 대상을 정리한 분석 카드 |
| Sequence | 시간 순서에 따른 객체 간 메시지 흐름 |
| Requirement Traceability | 요구사항과 설계/구현/검증 항목의 연결 |

### 1.3 참조 문서

| 구분 | 내용 |
| --- | --- |
| 공개 저장소 | <https://github.com/pjuny0301/cpp-env> |
| 과제 지시문 | 과제4 - 요구사항 분석서 작성 |
| 요구사항 정의서 | `codex/quiz/assignments/hw3_requirements_definition.md` |
| 구현 대상 | `apps/quiz/quiz-vulkan` |

## 2. 시스템 개요

### 2.1 소프트웨어 문맥도

```text
학습자
  -> Native Quiz Learning System
       -> Local Deck/Artifact Files
       -> Local Learning State Storage
       -> External Processing Artifacts
       -> GitHub Repository
```

학습자는 Deck/Day를 선택하고 문제를 푼다. 시스템은 로컬 Deck과 artifact를 읽고, 학습 상태를 저장한다. AI, OCR, PDF 처리 결과는 외부 처리 모듈이 만든 artifact로 전달된다. GitHub 저장소는 요구사항 문서, 구현 문서, 코드, 테스트 증거를 관리한다.

### 2.2 주요 기능 분류

| 분류 | 주요 기능 |
| --- | --- |
| 데이터 관리 | Deck/Day/Question 로드, 학습 상태 저장, artifact 참조 |
| 학습 세션 | 문항 표시, 답안 입력, 채점 상태, 이전/다음 이동 |
| 복습 관리 | 이미 앎 그룹, 오답노트, SRS 복습 후보 |
| 화면 구성 | scene 생성, layout 배치, draw list 생성 |
| 렌더링 | rectangle, text, image, texture, Vulkan command 처리 |
| 입력 처리 | pointer, keyboard, gesture, IME 조합 |
| 외부 산출물 | PDF/OCR/AI/증명/코드 처리 결과 표시 |

## 3. 요구사항 분석 및 명세

### 3.1 정적 분석

정적 구조는 도메인, 앱, scene/layout, UI renderer, Vulkan renderer, engine, 외부 처리 artifact 계층으로 나눈다. 각 계층은 상위 의미를 직접 역참조하지 않고 필요한 데이터만 받는다.

```text
Domain Model
  Deck, Day, Question, Session, LearningState

App Layer
  AppState, Action, Presenter, Modifier Interface

Scene/Layout
  SceneEditData, ScenePatch, SceneLayoutData, LayoutPlacer

Render Layer
  UiRenderer, DrawList, VulkanRenderer

Engine Layer
  TextEngine, ImageEngine, AssetResolver, InputRouter

External Processing Artifact
  OCR result, AI explanation, PDF extraction, proof grading
```

### 3.2 동적 분석

#### 3.2.1 Deck 실행 흐름

```text
사용자 -> Deck 선택
앱 -> Deck artifact 로드
도메인 -> Session 생성
Presenter -> Scene edit 생성
Layout -> 화면 배치
UI renderer -> Draw list 생성
Vulkan renderer -> 화면 출력
```

#### 3.2.2 답안 제출 흐름

```text
입력 이벤트 -> InputRouter
InputRouter -> App Action
App Action -> Session 상태 갱신
Presenter -> Scene patch 생성
Layout/UI renderer -> 갱신 화면 생성
```

#### 3.2.3 외부 처리 산출물 흐름

```text
PDF/OCR/AI 처리 모듈 -> Artifact Manifest
앱 -> Manifest 검증
앱 -> Reviewed Artifact 표시
사용자 -> 학습 또는 검토
```

### 3.3 CRC 카드

| 클래스 | 책임 | 협력 대상 |
| --- | --- | --- |
| Deck | Day와 문항 목록 보관 | Day, Question |
| Question | 문제 유형, 정답, 해설 보관 | Session |
| Session | 현재 문항, 답안, 진행 상태 관리 | LearningState |
| LearningState | 이미 앎, 오답노트, 복습 상태 관리 | Persistence |
| AppPresenter | 도메인 snapshot을 scene edit로 변환 | SceneModifier |
| SceneModifier | 허용된 scene 수정 데이터만 변경 | ScenePatch |
| LayoutPlacer | scene을 viewport에 배치 | SceneLayoutData |
| UiRenderer | 배치 결과를 draw list로 변환 | VulkanRenderer |
| VulkanRenderer | draw list와 resource packet을 GPU 명령으로 변환 | Text/Image/Asset Engine |
| InputRouter | platform input을 app action으로 정규화 | AppState |

### 3.4 객체/행위 분석

| 분석 대상 | 주요 상태 | 주요 행위 |
| --- | --- | --- |
| 학습자 | 선택한 Deck, 현재 문항, 답안 | Deck 선택, 답안 입력, 스와이프, 복습 선택 |
| 문항 | 유형, 정답, 해설, 그룹 | 표시, 채점, 그룹 이동 |
| 세션 | 현재 index, 제출 상태, 결과 | 시작, 이동, 제출, 종료 |
| Renderer | swapchain, resource packet, draw list | resource 준비, command 기록, frame 출력 |
| 외부 처리 Artifact | source, cache key, review status | 로드, 검증, 표시 |

## 4. 인터페이스 분석

### 4.1 사용자 인터페이스

| 화면 | 입력 | 출력 |
| --- | --- | --- |
| Deck 목록 | Deck 선택 | Day 목록 또는 학습 세션 |
| Day 목록 | Day 선택 | 문항 세션 |
| 퀴즈 화면 | 선택, 텍스트, 스와이프 | 문항, 답안 상태, 피드백 |
| 이미 앎/오답노트 | 그룹 선택, 복귀 action | 복습 대상 목록 |
| 본문 학습 | 문단 선택, 이동 | 원문/번역 split view |

### 4.2 시스템 간 연계

| 연계 대상 | 입력 | 출력 |
| --- | --- | --- |
| quiz-editor | 편집된 quiz data | Deck artifact |
| PDF/OCR 처리 모듈 | PDF, 이미지 | text/OCR artifact |
| AI 처리 모듈 | 문항, 본문, 프롬프트 | 해설/토론 artifact |
| 증명/코드 처리 모듈 | 답안, 코드 | 채점 결과 artifact |
| GitHub | 문서, 코드, 테스트 | 변경 이력, 제출 증거 |

### 4.3 입력/출력 인터페이스

| 인터페이스 | 입력 형식 | 출력 형식 |
| --- | --- | --- |
| Deck Loader | file path, artifact manifest | domain Deck |
| InputRouter | Win32/platform input | normalized action |
| TextEngine | UTF-8 text, font request | glyph run, atlas packet |
| ImageEngine | image URI | texture packet |
| UiRenderer | placed scene | draw list |
| VulkanRenderer | draw list, resource packet | rendered frame |

## 5. 제약사항 및 요구사항 추적

### 5.1 제약사항

| ID | 제약사항 |
| --- | --- |
| C-01 | Renderer는 domain/app/input 상태를 직접 참조하지 않는다. |
| C-02 | LayoutPlacer는 scene data를 직접 수정하지 않는다. |
| C-03 | UI renderer는 Vulkan API를 직접 호출하지 않는다. |
| C-04 | AI/OCR/PDF 처리는 앱 런타임이 아니라 외부 처리 artifact로 연결한다. |
| C-05 | 소스, 빌드 산출물, 외부 의존성은 폴더를 분리한다. |

### 5.2 요구사항 추적표

| 요구사항 | 분석 항목 | 검증 항목 |
| --- | --- | --- |
| FR-01, FR-02 | Domain Model, Deck Loader | domain/schema fixture |
| FR-03 ~ FR-06 | Question, Session, InputRouter | 문항 유형별 입력 test |
| FR-07 ~ FR-09 | Gesture, LearningState | group 이동 및 오답노트 test |
| FR-10 ~ FR-12 | Scene/Layout, Artifact | layout snapshot, artifact load test |
| NFR-01, NFR-02 | 계층 의존성 | compile boundary test |
| NFR-03 ~ NFR-07 | layout, text, asset, 외부 처리 | snapshot, resource packet, diagnostics test |
| IR-01 ~ IR-09 | 외부/내부 interface | interface contract compile test |

## 6. 참고문헌 및 부록

### 6.1 참고문헌

1. GitHub Public Repository, <https://github.com/pjuny0301/cpp-env>
2. 과제4 지시문, 요구사항 분석서 작성
3. `codex/quiz/assignments/hw3_requirements_definition.md`
4. `codex/quiz/big_plan.md`
5. `apps/quiz/quiz-vulkan`

### 6.2 부록 A. 분석 요약

요구사항 분석 결과, 본 시스템의 핵심 위험은 렌더링과 도메인 규칙이 결합되는 것이다. 따라서 도메인 상태, scene 수정, layout 배치, draw list 생성, Vulkan rendering을 분리하고, 각 경계는 테스트 가능한 데이터 계약으로 유지한다.
