# REVIEW QUEUE — 사람 검토 필요 (자동 수행 금지)

되돌릴 수 없거나 권한이 필요한 작업은 여기에 **제안만** 등록하고 진행하지 않는다.
사람이 승인(approved)한 항목만 별도 지시로 반영한다. append-only.

대상: main·통합 브랜치 머지 / git 히스토리 재작성 / force push /
매니페스트 없는 외부 다운로드 소비.
(트랙 브랜치 push·PR 생성, Track C 중복 삭제·stale 제거는 자동 허용이므로 여기 등록 불필요.)

## 항목
형식:
```text
### [RV-<순번>]
- 시각 UTC:
- 제안 세션:
- 종류: delete | merge | push | pr | external-download | history-rewrite | other
- 대상(경로/브랜치/URL):
- 변경 요약:
- 준비 상태(브랜치/스테이징 위치):
- 영향/위험:
- status: proposed        # proposed | approved | rejected | done
- 사람 결정:
```

### [RV-example-001]
- 시각 UTC: 2026-06-09T00:00Z
- 제안 세션: s-C
- 종류: delete
- 대상: apps/quiz/android-quiz-app/src/app/components/ui (48 dup files)
- 변경 요약: shared-ui 추출 후 중복 제거
- 준비 상태: branch codex/structure-shared-ui 에 추출 완료, 삭제는 미실행
- 영향/위험: 두 React 앱 빌드 영향, 되돌리기 가능하나 광범위
- status: proposed
- 사람 결정:
