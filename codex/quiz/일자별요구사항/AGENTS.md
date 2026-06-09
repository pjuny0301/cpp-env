# Daily Requirements Rules

- 날짜 폴더는 `YYYY-MM-DD` 형식으로 만든다.
- 날짜 폴더의 `requirements.md`는 번호별 요구사항, 구현 파일 링크, 상태를 포함한다.
- 날짜 폴더는 과거 요구사항 인덱스와 notes를 보존한다. 상세 구현 문서는 루트 `codex/quiz/구현/NN.md`와 담당 프로젝트 `구현/NN.md`가 권위다.
- 날짜 폴더 안에 과거 `구현/*.md` 사본이 있으면 새로 수정하지 말고 canonical 문서로 라우팅한다.
- 과거/중복 계획은 날짜 폴더의 `archive/`에 보존한다.
- 렌더링 산출물과 plan-doc 번들은 manifest/hash/요약만 Git에 남기고, 재현 가능한 raw 출력은 `build/out/quiz` 또는 외부 artifact 위치에 둔다.
- 날짜 폴더 안에서는 `requirements.md`, `notes.md`, `archive/`, durable evidence manifest 순서로 확인한다.
- 요구사항이 수정되면 `codex/quiz/big_plan.md`, 루트 `codex/quiz/구현/NN.md`, 관련 프로젝트 `big_plan.md`와 `구현/NN.md`를 함께 확인한다.
