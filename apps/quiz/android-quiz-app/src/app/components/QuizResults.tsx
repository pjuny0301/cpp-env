import { useNavigate } from "react-router";
import { getDayFromList, getDeckFromList, Question } from "../data";
import { useAppContext } from "../context";
import { RotateCcw, Target, ArrowRight } from "lucide-react";

export function QuizResults() {
  const navigate = useNavigate();
  const { decks, quizResults } = useAppContext();

  if (!quizResults) {
    return (
      <div className="p-8 text-center text-slate-500 min-h-[100dvh] flex flex-col justify-center bg-slate-50">
        <p className="mb-4">결과가 없습니다.</p>
        <button 
          onClick={() => navigate("/")}
          className="text-blue-600 font-bold hover:underline"
        >
          홈으로 돌아가기
        </button>
      </div>
    );
  }

  const { deckId, dayId, answers, randomMode } = quizResults;
  const isDeckRandom = dayId === "__deck_random" || randomMode === "deck";
  const isDayRandom = randomMode === "day";
  const deck = getDeckFromList(decks, deckId);
  const day = isDeckRandom ? null : getDayFromList(decks, deckId, dayId);

  if (isDeckRandom && !deck) return <div className="p-8 text-center text-slate-500">Deck not found.</div>;
  if (!isDeckRandom && !day) return <div className="p-8 text-center text-slate-500">Day not found.</div>;

  const questions = isDeckRandom
    ? deck!.days.flatMap(dayItem => dayItem.questions)
    : day!.questions;
  const resultTitle = isDeckRandom ? `${deck!.title} 전체랜덤` : isDayRandom ? `${day!.title} 랜덤` : day!.title;
  const currentDayIndex = !isDeckRandom && deck ? deck.days.findIndex(dayItem => dayItem.id === dayId) : -1;
  const nextDay = currentDayIndex >= 0 ? deck?.days[currentDayIndex + 1] : null;
  const retryUrl = isDeckRandom
    ? `/quiz/${deckId}/__deck_random?random=deck`
    : isDayRandom
      ? `/quiz/${deckId}/${dayId}?random=day`
      : `/quiz/${deckId}/${dayId}`;
  const wrongOnlyUrl = `${retryUrl}${retryUrl.includes("?") ? "&" : "?"}mode=wrong`;
  let correctCount = 0;

  const resultDetails = answers.map((ans) => {
    const q = questions.find(q => q.id === ans.questionId);
    if (!q) return null;

    const isCorrect = typeof ans.isCorrect === "boolean"
      ? ans.isCorrect
      : ans.selectedOptionIndex !== null && q.options[ans.selectedOptionIndex]?.isCorrect;
    if (isCorrect) correctCount++;

    return {
      question: q,
      selectedOptionIndex: ans.selectedOptionIndex,
      typedAnswer: ans.typedAnswer,
      isCorrect,
    };
  }).filter(Boolean) as { question: Question, selectedOptionIndex: number | null, typedAnswer?: string, isCorrect: boolean }[];

  const total = questions.length;
  const accuracy = total > 0 ? (correctCount / total) * 100 : 0;

  // Determine Background Color
  let bgClass = "bg-blue-600"; // High / 100%
  let headerTextClass = "text-white";
  if (accuracy < 50) {
    bgClass = "bg-red-500"; // Low
  } else if (accuracy < 90) {
    // Medium - Aurora
    bgClass = "bg-gradient-to-br from-indigo-500 via-purple-500 to-pink-500"; 
  }

  return (
    <div className={`min-h-full flex flex-col transition-colors duration-500 ${bgClass}`}>
      {/* Header/Score Section */}
      <div className={`pt-16 pb-12 px-6 flex flex-col items-center justify-center text-center ${headerTextClass}`}>
        <h1 className="text-xl font-medium opacity-90 mb-2">{resultTitle} 완료</h1>
        <div className="text-[5rem] font-extrabold leading-none tracking-tighter mb-4 drop-shadow-md">
          {Math.round(accuracy)}<span className="text-4xl opacity-70">%</span>
        </div>
        <p className="text-lg font-medium opacity-90">
          총 {total}문제 중 {correctCount}문제 정답
        </p>
      </div>

      {/* Review Section */}
      <div className="flex-1 bg-slate-50 rounded-t-3xl p-6 sm:p-8 shadow-[0_-8px_30px_rgb(0,0,0,0.12)] pb-32">
        <h2 className="text-xl font-bold text-slate-800 mb-6 flex items-center gap-2">
          오답 노트
        </h2>

        <div className="space-y-4">
          {resultDetails.map((detail, i) => {
            const { question, isCorrect, selectedOptionIndex } = detail;
            const isPromptType = question.type === 'short'
              || question.type === 'answer'
              || question.type === 'blank'
              || question.type === 'multi-answer'
              || question.type === 'multi-blank';
            const sourceText = isPromptType ? question.questionText : question.longText;
            const titleText = sourceText || "이미지 문제";
            const correctOption = question.options.find(o => o.isCorrect)?.text;
            const selectedOptionText = detail.typedAnswer || (selectedOptionIndex !== null ? question.options[selectedOptionIndex]?.text : "PASS (선택안함)");

            return (
              <div 
                key={i} 
                className={`p-5 rounded-2xl border-2 transition-all ${
                  isCorrect 
                    ? 'bg-white border-slate-100 hover:border-slate-200' 
                    : 'bg-red-50 border-red-100'
                }`}
              >
                <div className="flex items-start gap-4">
                  <div className="shrink-0 mt-0.5">
                    {isCorrect ? (
                      <div className="w-8 h-8 rounded-full bg-blue-100 text-blue-600 flex items-center justify-center font-bold text-lg">
                        O
                      </div>
                    ) : (
                      <div className="w-8 h-8 rounded-full bg-red-100 text-red-600 flex items-center justify-center font-bold text-lg">
                        X
                      </div>
                    )}
                  </div>
                  
                  <div className="flex-1 min-w-0">
                    <p className={`text-base font-bold mb-3 whitespace-pre-line break-words ${isCorrect ? 'text-slate-700' : 'text-red-600'}`}>
                      {titleText}
                    </p>

                    {!isCorrect && (
                      <div className="flex flex-col gap-2 text-[15px]">
                        <div className="flex items-start gap-2 bg-white/60 p-3 rounded-xl">
                          <span className="font-bold text-slate-400 shrink-0 w-12 text-xs uppercase tracking-wider mt-0.5">정답</span>
                          <span className="font-medium text-slate-800 leading-snug whitespace-pre-line break-words">{correctOption}</span>
                        </div>
                        <div className="flex items-start gap-2 bg-red-100/50 p-3 rounded-xl">
                          <span className="font-bold text-red-400 shrink-0 w-12 text-xs uppercase tracking-wider mt-0.5">내 선택</span>
                          <span className="font-medium text-red-700 leading-snug whitespace-pre-line break-words">{selectedOptionText}</span>
                        </div>
                      </div>
                    )}

                    {isCorrect && (
                      <p className="text-[15px] font-medium text-slate-500 mt-1 whitespace-pre-line break-words">
                        {correctOption}
                      </p>
                    )}
                  </div>
                </div>
              </div>
            );
          })}
        </div>
      </div>

      {/* Floating Action Buttons */}
      <div className="fixed bottom-0 left-0 right-0 p-4 sm:p-6 bg-gradient-to-t from-slate-50 via-slate-50 to-transparent z-10">
        <div className="max-w-md mx-auto grid grid-cols-3 gap-2">
          <button 
            onClick={() => navigate(retryUrl, { replace: true })}
            className="py-3 bg-white border-2 border-slate-200 text-slate-700 rounded-2xl font-bold text-sm hover:bg-slate-50 hover:border-slate-300 active:scale-[0.98] transition-all flex flex-col items-center justify-center gap-1 shadow-sm"
          >
            <RotateCcw className="w-5 h-5" />
            전체다시풀기
          </button>
          <button 
            onClick={() => navigate(wrongOnlyUrl, { replace: true })}
            disabled={resultDetails.every(detail => detail.isCorrect)}
            className="py-3 bg-slate-800 text-white rounded-2xl font-bold text-sm hover:bg-slate-900 active:scale-[0.98] transition-all shadow-lg flex flex-col items-center justify-center gap-1 disabled:bg-slate-300 disabled:shadow-none"
          >
            <Target className="w-5 h-5" />
            오답만
          </button>
          <button
            onClick={() => nextDay ? navigate(`/deck/${deckId}/day/${nextDay.id}`, { replace: true }) : navigate(`/deck/${deckId}`, { replace: true })}
            className="py-3 bg-blue-600 text-white rounded-2xl font-bold text-sm hover:bg-blue-700 active:scale-[0.98] transition-all shadow-lg flex flex-col items-center justify-center gap-1"
          >
            <ArrowRight className="w-5 h-5" />
            다음 Day
          </button>
        </div>
      </div>
    </div>
  );
}
