# Daily Requirements Rules

- 날짜 폴더는 `YYYY-MM-DD` 형식으로 만든다.
- 날짜 폴더의 `requirements.md`는 번호별 요구사항, 구현 파일 링크, 상태를 포함한다.
- `구현/*.md` 파일 상단에는 반드시 원문 요구사항을 그대로 둔다.
- 과거/중복 계획은 날짜 폴더의 `archive/`에 보존한다.
- 렌더링 산출물과 plan-doc 번들은 날짜 폴더의 `generated/`에 보존한다.
- 날짜 폴더 안에서는 `requirements.md`, `구현/`, `archive/`, `generated/`, `screenshots/`, `verification-artifacts/` 순서로 확인한다.
- 요구사항이 수정되면 `codex/quiz/big_plan.md`, 루트 `codex/quiz/구현/NN.md`, 관련 프로젝트 `big_plan.md`와 `구현/NN.md`를 함께 확인한다.
