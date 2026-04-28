import { AnimatePresence, motion } from "motion/react";
import type { RuntimeQuestion } from "../quizRuntime";
import { ImageWithFallback } from "./figma/ImageWithFallback";

type QuizQuestionStageProps = {
  question: RuntimeQuestion;
  isShortType: boolean;
  isMultiselectType: boolean;
  selectedIndices: number[];
  multiselectCorrectCount: number;
  longTextFontSize: number;
  onLongPressStart: () => void;
  onLongPressEnd: () => void;
};

export function QuizQuestionStage({
  question,
  isShortType,
  isMultiselectType,
  selectedIndices,
  multiselectCorrectCount,
  longTextFontSize,
  onLongPressStart,
  onLongPressEnd,
}: QuizQuestionStageProps) {
  const pressHandlers = {
    onPointerDown: onLongPressStart,
    onPointerUp: onLongPressEnd,
    onPointerCancel: onLongPressEnd,
    onPointerLeave: onLongPressEnd,
  };

  return (
    <AnimatePresence mode="wait">
      <motion.div
        key={question.id}
        initial={{ opacity: 0, x: 20 }}
        animate={{ opacity: 1, x: 0 }}
        exit={{ opacity: 0, x: -20 }}
        transition={{ duration: 0.2 }}
        className="min-h-0 flex-1 overflow-y-auto overflow-x-hidden flex flex-col pt-6 pb-6 px-6 no-scrollbar"
      >
        {isShortType ? (
          <div
            className="flex-1 flex items-center justify-center min-h-[24vh] py-4"
            {...pressHandlers}
          >
            <h2 className="text-4xl sm:text-5xl font-extrabold text-center drop-shadow-sm tracking-tight leading-tight">
              {question.questionText}
            </h2>
          </div>
        ) : isMultiselectType ? (
          <div
            className="min-h-0 flex-1 flex flex-col gap-4"
            {...pressHandlers}
          >
            <h2 className="text-2xl sm:text-3xl font-bold text-slate-800 leading-snug whitespace-pre-line">
              {question.questionText}
            </h2>
            <div className="flex flex-wrap gap-2">
              {Array.from({ length: multiselectCorrectCount }).map((_, slotIdx) => {
                const optionIndex = selectedIndices[slotIdx];
                const filled = optionIndex !== undefined;
                const text = filled ? question.options[optionIndex]?.text ?? "" : "";
                return (
                  <div
                    key={slotIdx}
                    className={`min-w-[6rem] px-3 py-2 rounded-xl border-2 text-sm font-semibold ${
                      filled ? "border-blue-400 bg-blue-50 text-blue-700" : "border-dashed border-slate-300 bg-slate-50 text-slate-400"
                    }`}
                  >
                    <span className="mr-1 opacity-60 text-xs">{slotIdx + 1}.</span>
                    {filled ? text : "빈칸"}
                  </div>
                );
              })}
            </div>
          </div>
        ) : (
          <div
            className="min-h-0 flex-1 flex flex-col gap-6"
            {...pressHandlers}
          >
            {question.imageUrl && (
              <div className="w-full aspect-video rounded-2xl overflow-hidden shadow-sm bg-slate-100 border border-slate-200">
                <ImageWithFallback
                  src={question.imageUrl}
                  alt="Question image"
                  className="w-full h-full object-cover"
                />
              </div>
            )}
            {question.longText && (
              <div className="prose prose-slate max-w-none">
                <p
                  className="text-slate-700 font-light leading-relaxed whitespace-pre-line sm:leading-loose"
                  style={{ fontSize: `${longTextFontSize}px` }}
                >
                  {question.longText}
                </p>
              </div>
            )}
          </div>
        )}
      </motion.div>
    </AnimatePresence>
  );
}
