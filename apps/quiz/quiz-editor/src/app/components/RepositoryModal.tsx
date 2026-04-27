import { CheckCircle2, GitBranch, X } from "lucide-react";
import type { GitIdentity, QuizDataRepository } from "../editorTypes";

type RepositoryModalProps = {
  repositories: QuizDataRepository[];
  gitIdentity: GitIdentity;
  gitIdentityStatus: string;
  onClose: () => void;
  onSelectRepository: (repository: QuizDataRepository) => void;
  onGitIdentityChange: (identity: GitIdentity) => void;
  onSaveGitIdentity: () => void;
};

export function RepositoryModal({
  repositories,
  gitIdentity,
  gitIdentityStatus,
  onClose,
  onSelectRepository,
  onGitIdentityChange,
  onSaveGitIdentity,
}: RepositoryModalProps) {
  return (
    <div className="fixed inset-0 z-[75] bg-slate-950/30 flex items-start justify-center px-4 pt-20">
      <div className="w-full max-w-xl overflow-hidden rounded-lg border border-gray-200 bg-white shadow-2xl">
        <div className="h-10 border-b border-gray-200 bg-gray-50 px-3 flex items-center justify-between">
          <div className="flex items-center gap-2 text-sm font-semibold text-gray-800">
            <GitBranch className="w-4 h-4 text-blue-500" />
            Git 레포 / 계정 관리
          </div>
          <button
            onClick={onClose}
            className="w-7 h-7 rounded hover:bg-gray-100 text-gray-500 flex items-center justify-center"
            title="닫기"
          >
            <X className="w-4 h-4" />
          </button>
        </div>
        <div className="max-h-[420px] overflow-y-auto p-3">
          {repositories.length === 0 ? (
            <div className="rounded border border-dashed border-gray-200 p-6 text-center text-sm text-gray-400">
              연결 가능한 Git 레포가 없습니다.
            </div>
          ) : (
            <div className="space-y-2">
              {repositories.map(repository => (
                <button
                  key={repository.path}
                  onClick={() => onSelectRepository(repository)}
                  className={`w-full rounded-md border px-3 py-3 text-left transition-colors ${
                    repository.active
                      ? "border-blue-200 bg-blue-50"
                      : "border-gray-200 bg-white hover:bg-gray-50"
                  }`}
                >
                  <div className="flex items-center justify-between gap-3">
                    <div className="min-w-0">
                      <div className="flex items-center gap-2 text-sm font-semibold text-gray-800">
                        <span className="truncate">{repository.name}</span>
                        {repository.active && <CheckCircle2 className="w-4 h-4 text-blue-600" />}
                      </div>
                      <div className="mt-1 truncate text-xs text-gray-500">{repository.remote || repository.path}</div>
                    </div>
                    <span className="shrink-0 rounded bg-gray-100 px-2 py-1 text-[11px] font-medium text-gray-500">
                      선택
                    </span>
                  </div>
                </button>
              ))}
            </div>
          )}
          <div className="mt-3 rounded bg-slate-50 px-3 py-2 text-xs text-slate-500">
            모바일 앱 Git URL: https://github.com/pjuny0301/quiz-data/tree/main/mobile-decks
          </div>
          <div className="mt-3 rounded-md border border-gray-200 bg-white p-3">
            <div className="mb-2 flex items-center gap-2 text-sm font-semibold text-gray-800">
              <CheckCircle2 className="w-4 h-4 text-blue-500" />
              Git 계정 관리
            </div>
            <div className="grid gap-2 sm:grid-cols-2">
              <input
                value={gitIdentity.name}
                onChange={(event) => onGitIdentityChange({ ...gitIdentity, name: event.target.value })}
                placeholder="user.name"
                className="rounded border border-gray-200 px-2 py-1.5 text-xs text-gray-700 outline-none focus:border-blue-300 focus:ring-1 focus:ring-blue-200"
              />
              <input
                value={gitIdentity.email}
                onChange={(event) => onGitIdentityChange({ ...gitIdentity, email: event.target.value })}
                placeholder="user.email"
                className="rounded border border-gray-200 px-2 py-1.5 text-xs text-gray-700 outline-none focus:border-blue-300 focus:ring-1 focus:ring-blue-200"
              />
            </div>
            <div className="mt-2 flex items-center justify-between gap-2">
              <div className="min-h-4 text-[11px] text-slate-500">{gitIdentityStatus}</div>
              <button
                onClick={onSaveGitIdentity}
                className="shrink-0 rounded bg-blue-600 px-2.5 py-1.5 text-xs font-semibold text-white hover:bg-blue-700"
              >
                저장
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
