import type { MutableRefObject } from "react";
import {
  getCorrectAnswerLabel,
  isTypedAnswerCorrect as matchesTypedAnswer,
  type RuntimeQuestion,
} from "../quizRuntime";

type FeedbackState = "idle" | "showing";

type QuizAnswerDockProps = {
  question: RuntimeQuestion;
  feedbackState: FeedbackState;
  isAnswerType: boolean;
  isMultiselectType: boolean;
  isMultiInputType: boolean;
  isShortType: boolean;
  manualContinue: boolean;
  optionViewportClass: string;
  selectedOption: number | null;
  selectedIndices: number[];
  typedAnswer: string;
  setTypedAnswer: (value: string) => void;
  multiselectComplete: boolean;
  multiselectCorrectCount: number;
  optionButtonRefs: MutableRefObject<(HTMLButtonElement | null)[]>;
  onManualContinue: () => void;
  onOptionSelect: (index: number | null) => void;
  onMultiselectTap: (index: number) => void;
  onMultiselectSubmit: () => void;
  onTypedAnswerSubmit: () => void;
};

export function QuizAnswerDock({
  question,
  feedbackState,
  isAnswerType,
  isMultiselectType,
  isMultiInputType,
  isShortType,
  manualContinue,
  optionViewportClass,
  selectedOption,
  selectedIndices,
  typedAnswer,
  setTypedAnswer,
  multiselectComplete,
  multiselectCorrectCount,
  optionButtonRefs,
  onManualContinue,
  onOptionSelect,
  onMultiselectTap,
  onMultiselectSubmit,
  onTypedAnswerSubmit,
}: QuizAnswerDockProps) {
  const isShowingFeedback = feedbackState === "showing";

  return (
    <div className={`shrink-0 p-3.5 sm:p-5 pb-[calc(1rem+env(safe-area-inset-bottom))] z-20 transition-all ${isShortType ? "bg-blue-700/95 border-t border-white/10 shadow-[0_-12px_30px_rgba(30,64,175,0.28)]" : "bg-white border-t border-slate-100 shadow-[0_-12px_30px_rgba(15,23,42,0.08)]"}`}>
      <div className={`max-w-md mx-auto w-full relative ${optionViewportClass}`}>
        {!isAnswerType && !isShowingFeedback && (
          <div className="flex justify-center mb-2">
            <button
              onClick={() => onOptionSelect(null)}
              disabled={isShowingFeedback}
              className={`text-xs font-bold tracking-widest uppercase py-1 px-4 rounded-full transition-all active:scale-95 ${
                isShortType ? "text-blue-200 hover:text-white hover:bg-blue-500/30" : "text-slate-400 hover:text-slate-600 hover:bg-slate-100"
              }`}
            >
              PASS
            </button>
          </div>
        )}

        {!isAnswerType && manualContinue && isShowingFeedback && (
          <button
            onClick={onManualContinue}
            autoFocus
            className={`mb-2 w-full rounded-2xl py-3.5 text-base font-bold transition-all active:scale-[0.98] shadow-lg ${
              isShortType
                ? "bg-white text-blue-700 hover:bg-blue-50 shadow-blue-900/30"
                : "bg-blue-600 text-white hover:bg-blue-700 shadow-blue-600/30"
            }`}
          >
            다음 →
          </button>
        )}

        {isMultiselectType ? (
          <div className="flex flex-col gap-2">
            {!isShowingFeedback && (
              <button
                onClick={onMultiselectSubmit}
                disabled={!multiselectComplete}
                className="mb-1 w-full rounded-2xl py-3.5 text-base font-bold transition-colors disabled:bg-slate-200 disabled:text-slate-400 bg-blue-600 text-white hover:bg-blue-700 shadow-lg shadow-blue-600/30"
              >
                {multiselectComplete ? "제출" : `${selectedIndices.length} / ${multiselectCorrectCount}`}
              </button>
            )}
            {question.options.map((option, index) => {
              const isSelected = selectedIndices.includes(index);
              const isCorrect = option.isCorrect;
              let optionClass = "";
              if (!isShowingFeedback) {
                optionClass = isSelected
                  ? "bg-blue-50 text-blue-800 border-blue-400"
                  : "bg-white text-slate-700 border-slate-200 hover:bg-slate-50";
              } else if (isCorrect) {
                optionClass = "bg-yellow-400 text-yellow-950 font-bold border-transparent shadow-lg shadow-yellow-400/30";
              } else if (isSelected) {
                optionClass = "bg-white border-transparent text-red-600 opacity-90";
              } else {
                optionClass = "bg-slate-50 text-slate-400 border-slate-100 opacity-50";
              }
              const orderInSelection = isSelected ? selectedIndices.indexOf(index) + 1 : null;
              return (
                <button
                  key={index}
                  onClick={() => onMultiselectTap(index)}
                  disabled={isShowingFeedback}
                  className={`w-full text-left p-3.5 rounded-xl text-[15px] font-medium border-2 flex items-center min-h-[56px] ${optionClass} shadow-sm`}
                >
                  <span className={`w-7 h-7 shrink-0 rounded-full flex items-center justify-center text-xs font-bold mr-3 ${
                    isShowingFeedback && isCorrect ? "bg-yellow-500/30 text-yellow-900" :
                    isShowingFeedback && isSelected ? "bg-red-100 text-red-600" :
                    isSelected ? "bg-blue-500 text-white" : "bg-slate-100 text-slate-400"
                  }`}>
                    {orderInSelection ?? index + 1}
                  </span>
                  <span className="flex-1 leading-snug">{option.text}</span>
                </button>
              );
            })}
          </div>
        ) : isAnswerType ? (
          <div className="rounded-2xl bg-white p-3 shadow-lg">
            {isMultiInputType ? (
              <textarea
                autoFocus
                value={typedAnswer}
                disabled={isShowingFeedback}
                onChange={(event) => setTypedAnswer(event.target.value)}
                placeholder="정답 여러 개 입력"
                className="min-h-28 w-full resize-none rounded-xl border-2 border-blue-100 px-4 py-3 text-base font-bold text-slate-900 outline-none focus:border-blue-400"
              />
            ) : (
              <input
                autoFocus
                value={typedAnswer}
                disabled={isShowingFeedback}
                onChange={(event) => setTypedAnswer(event.target.value)}
                onKeyDown={(event) => {
                  if (event.key === "Enter" && !event.nativeEvent.isComposing) {
                    onTypedAnswerSubmit();
                  }
                }}
                placeholder={question.type === "blank" ? "빈칸 정답 입력" : "정답 입력"}
                className="w-full rounded-xl border-2 border-blue-100 px-4 py-4 text-lg font-bold text-slate-900 outline-none focus:border-blue-400"
              />
            )}
            {isShowingFeedback && (
              <div className={`mt-3 rounded-xl px-4 py-3 text-sm font-bold whitespace-pre-line break-words ${matchesTypedAnswer(question, typedAnswer) ? "bg-blue-50 text-blue-700" : "bg-red-50 text-red-700"}`}>
                정답: {getCorrectAnswerLabel(question)}
              </div>
            )}
            {manualContinue && isShowingFeedback ? (
              <button
                onClick={onManualContinue}
                autoFocus
                className="mt-3 w-full rounded-xl bg-blue-600 py-4 text-base font-bold text-white active:scale-[0.98] transition-all shadow-lg shadow-blue-600/30"
              >
                다음 →
              </button>
            ) : (
              <button
                onClick={onTypedAnswerSubmit}
                disabled={isShowingFeedback || !typedAnswer.trim()}
                className="mt-3 w-full rounded-xl bg-blue-600 py-4 text-base font-bold text-white disabled:bg-slate-300 active:scale-[0.98] transition-all"
              >
                제출
              </button>
            )}
          </div>
        ) : (
          <div className="flex flex-col gap-2">
            {question.options.map((option, index) => {
              const isSelected = selectedOption === index;
              const isCorrect = option.isCorrect;
              let optionClass = "";

              if (!isShowingFeedback) {
                if (isShortType) {
                  optionClass = "bg-white text-blue-900 border-transparent hover:bg-blue-50 active:scale-[0.98]";
                } else {
                  optionClass = "bg-white text-slate-700 border-slate-200 hover:bg-slate-50 hover:border-slate-300 active:scale-[0.98]";
                }
              } else if (isCorrect) {
                optionClass = "bg-yellow-400 text-yellow-950 font-bold border-transparent shadow-lg shadow-yellow-400/30 scale-[1.02] z-10";
              } else if (isSelected) {
                optionClass = "bg-white border-transparent text-red-600 opacity-90 scale-[0.98]";
              } else if (isShortType) {
                optionClass = "bg-white/50 text-blue-900/50 border-transparent opacity-50 grayscale";
              } else {
                optionClass = "bg-slate-50 text-slate-400 border-slate-100 opacity-50 grayscale";
              }

              return (
                <button
                  key={index}
                  ref={(node) => { optionButtonRefs.current[index] = node; }}
                  onClick={() => onOptionSelect(index)}
                  disabled={isShowingFeedback}
                  className={`w-full text-left p-3.5 rounded-xl text-[15px] font-medium transition-all duration-300 border-2 flex items-center min-h-[56px] ${optionClass} ${!isShortType && "shadow-sm"}`}
                >
                  <span className={`w-7 h-7 shrink-0 rounded-full flex items-center justify-center text-xs font-bold mr-3 transition-colors ${
                    isShowingFeedback && isCorrect ? "bg-yellow-500/30 text-yellow-900" :
                    isShowingFeedback && isSelected ? "bg-red-100 text-red-600" :
                    isShortType ? "bg-blue-50 text-blue-400" : "bg-slate-100 text-slate-400"
                  }`}>
                    {index + 1}
                  </span>
                  <span className="flex-1 leading-snug">{option.text}</span>
                </button>
              );
            })}
          </div>
        )}
      </div>
    </div>
  );
}
