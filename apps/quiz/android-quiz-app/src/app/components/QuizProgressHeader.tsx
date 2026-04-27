import { Settings2 } from "lucide-react";
import type { Variant } from "../quizRuntime";

type QuizProgressHeaderProps = {
  title: string | undefined;
  isShortType: boolean;
  variant: Variant;
  currentIndex: number;
  totalCount: number;
  isShowingFeedback: boolean;
  onOpenSettings: () => void;
};

export function QuizProgressHeader({
  title,
  isShortType,
  variant,
  currentIndex,
  totalCount,
  isShowingFeedback,
  onOpenSettings,
}: QuizProgressHeaderProps) {
  const progress = totalCount > 0
    ? ((currentIndex + (isShowingFeedback ? 1 : 0)) / totalCount) * 100
    : 0;

  return (
    <header className="shrink-0 z-10 border-b border-black/5">
      <div className="flex items-center justify-between p-4">
        <div className="flex items-center gap-3 min-w-0">
          <div className={`px-3 py-1 rounded-full text-xs font-bold truncate max-w-[60vw] ${isShortType ? variant.shortHeaderPill : "bg-slate-100 text-slate-600"}`}>
            {title}
          </div>
          <div className={`text-sm font-bold tracking-widest shrink-0 ${isShortType ? variant.shortCounter : "text-slate-400"}`}>
            {currentIndex + 1} <span className="opacity-50">/</span> {totalCount}
          </div>
        </div>
        <button
          onClick={onOpenSettings}
          className={`p-2 rounded-full transition-colors ${isShortType ? `${variant.shortHeaderText} hover:bg-white/10` : "text-slate-400 hover:bg-slate-100"}`}
        >
          <Settings2 className="w-5 h-5" />
        </button>
      </div>
      <div className={`h-1 w-full ${isShortType ? variant.shortProgressTrack : "bg-slate-100"}`}>
        <div
          className={`h-full transition-[width] duration-500 ease-out ${isShortType ? variant.shortProgressFill : "bg-blue-600"}`}
          style={{ width: `${progress}%` }}
        />
      </div>
    </header>
  );
}
