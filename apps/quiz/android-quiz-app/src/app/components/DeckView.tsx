import { useParams, Link, useNavigate } from "react-router";
import { getDeckFromList } from "../data";
import { Calendar, Check, Shuffle } from "lucide-react";
import { useAppContext } from "../context";

const accuracyTone = (accuracy: number) => {
  if (accuracy >= 90) return { ring: 'ring-blue-200', text: 'text-blue-700', bg: 'bg-blue-50', accent: 'bg-blue-600' };
  if (accuracy >= 50) return { ring: 'ring-purple-200', text: 'text-purple-700', bg: 'bg-purple-50', accent: 'bg-purple-500' };
  return { ring: 'ring-red-200', text: 'text-red-700', bg: 'bg-red-50', accent: 'bg-red-500' };
};

export function DeckView() {
  const { deckId } = useParams();
  const navigate = useNavigate();
  const { decks, dayProgress } = useAppContext();
  const deck = getDeckFromList(decks, deckId || "");

  if (!deck) return <div className="p-8 text-center text-slate-500">Deck not found.</div>;
  const totalQuestions = deck.days.reduce((sum, day) => sum + day.questions.length, 0);

  return (
    <div className="p-6 bg-slate-50 min-h-full">
      <div className="mb-8 pt-4 flex items-start justify-between gap-4">
        <div className="min-w-0">
          <h1 className="text-2xl font-extrabold text-blue-900 truncate">{deck.title}</h1>
          <p className="text-slate-500 font-medium mt-1">학습할 Day를 선택하세요</p>
        </div>
        <button
          onClick={() => navigate(`/quiz/${deck.id}/__deck_random?random=deck`)}
          disabled={totalQuestions === 0}
          className={`shrink-0 rounded-2xl px-4 py-3 font-bold transition-all shadow-lg flex items-center justify-center gap-2 ${
            totalQuestions > 0
              ? "bg-slate-800 text-white hover:bg-slate-900 active:scale-[0.98] shadow-slate-800/20"
              : "bg-slate-300 text-slate-500 cursor-not-allowed shadow-none"
          }`}
          title="Deck 전체랜덤"
        >
          <Shuffle className="w-5 h-5" />
          <span className="text-sm">전체랜덤</span>
        </button>
      </div>

      <div className="grid grid-cols-2 gap-4">
        {deck.days.map((day) => {
          const progress = dayProgress[`${deck.id}::${day.id}`];
          const tone = progress ? accuracyTone(progress.accuracy) : null;
          return (
            <Link
              key={day.id}
              to={`/deck/${deck.id}/day/${day.id}`}
              className="block h-full group"
            >
              <div className={`relative bg-white p-5 rounded-2xl shadow-sm border border-slate-100 hover:shadow-md hover:border-blue-200 hover:bg-blue-50 transition-all flex flex-col items-center justify-center text-center gap-2 h-full ${progress ? `ring-2 ${tone!.ring}` : ''}`}>
                {progress && (
                  <div className={`absolute top-2 right-2 flex items-center gap-1 rounded-full px-2 py-0.5 text-[10px] font-bold ${tone!.bg} ${tone!.text}`}>
                    <Check className="w-3 h-3" strokeWidth={3} />
                    {progress.accuracy}%
                  </div>
                )}
                <div className={`w-10 h-10 rounded-full flex items-center justify-center mb-1 transition-colors ${
                  progress
                    ? `${tone!.accent} text-white`
                    : 'bg-slate-100 text-slate-500 group-hover:bg-blue-100 group-hover:text-blue-600'
                }`}>
                  <Calendar className="w-5 h-5" />
                </div>
                <h2 className="text-lg font-bold text-slate-800 line-clamp-2">{day.title}</h2>
                <p className="text-xs text-slate-400 font-medium">
                  {day.questions.length} 문제
                  {progress && (
                    <span className="ml-1">· {progress.correct}/{progress.total}</span>
                  )}
                </p>
              </div>
            </Link>
          );
        })}
      </div>
    </div>
  );
}
