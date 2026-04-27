import { useParams, useNavigate } from "react-router";
import { getDayFromList } from "../data";
import { BookmarkCheck, ListOrdered, PlayCircle, Settings2, Shuffle } from "lucide-react";
import { questionLearningKey, useAppContext } from "../context";
import { useState } from "react";

export function DayIntro() {
  const { deckId, dayId } = useParams();
  const navigate = useNavigate();
  const { decks, delay, setDelay, longTextFontSize, setLongTextFontSize, manualContinue, setManualContinue, learning } = useAppContext();
  const day = getDayFromList(decks, deckId || "", dayId || "");
  const [showSettings, setShowSettings] = useState(false);

  if (!day) return <div className="p-8 text-center text-slate-500">Day not found.</div>;

  const hasQuestions = day.questions.length > 0;
  const knownCount = day.questions.reduce((acc, q) => {
    const entry = learning[questionLearningKey(deckId || "", dayId || "", q.id)];
    return entry?.state === 'known' ? acc + 1 : acc;
  }, 0);
  const learningCount = day.questions.length - knownCount;

  return (
    <div className="p-6 bg-slate-50 min-h-full flex flex-col items-center justify-center text-center relative overflow-hidden">
      {/* Settings Modal - Simple overlay */}
      {showSettings && (
        <div className="absolute inset-0 bg-black/40 z-50 flex items-center justify-center p-4">
          <div className="bg-white rounded-2xl w-full max-w-sm p-6 shadow-xl relative">
            <h3 className="text-xl font-bold mb-4 text-slate-800">퀴즈 설정</h3>
            <div className="mb-6 text-left">
              <div className="flex items-center justify-between mb-2">
                <label className="text-sm font-medium text-slate-600">탭해서 다음으로</label>
                <button
                  type="button"
                  role="switch"
                  aria-checked={manualContinue}
                  onClick={() => setManualContinue(!manualContinue)}
                  className={`relative inline-flex h-7 w-12 items-center rounded-full transition-colors ${manualContinue ? 'bg-blue-600' : 'bg-slate-300'}`}
                >
                  <span className={`inline-block h-5 w-5 transform rounded-full bg-white shadow transition-transform ${manualContinue ? 'translate-x-6' : 'translate-x-1'}`} />
                </button>
              </div>
              <p className="text-xs text-slate-400 leading-snug">
                {manualContinue ? "정답 확인 후 '다음 →' 버튼을 눌러 진행합니다." : "정답 확인 후 자동으로 다음 문제로 넘어갑니다."}
              </p>
            </div>
            {!manualContinue && (
              <div className="mb-6 text-left">
                <label className="block text-sm font-medium text-slate-600 mb-2">자동 넘김 지연</label>
                <input
                  type="range"
                  min="500"
                  max="5000"
                  step="100"
                  value={delay}
                  onChange={(e) => setDelay(Number(e.target.value))}
                  className="w-full h-2 bg-slate-200 rounded-lg appearance-none cursor-pointer accent-blue-600"
                />
                <div className="text-right text-sm font-bold text-blue-600 mt-2">{(delay / 1000).toFixed(1)}초</div>
              </div>
            )}
            <div className="mb-6 text-left">
              <label className="block text-sm font-medium text-slate-600 mb-2">장문 글꼴 크기</label>
              <input
                type="range"
                min="14"
                max="32"
                step="1"
                value={longTextFontSize}
                onChange={(e) => setLongTextFontSize(Number(e.target.value))}
                className="w-full h-2 bg-slate-200 rounded-lg appearance-none cursor-pointer accent-blue-600"
              />
              <div className="text-right text-sm font-bold text-blue-600 mt-2">{longTextFontSize}px</div>
            </div>
            <button 
              onClick={() => setShowSettings(false)}
              className="w-full py-3 bg-slate-800 text-white rounded-xl font-bold hover:bg-slate-900 transition-colors"
            >
              닫기
            </button>
          </div>
        </div>
      )}

      <div className="flex flex-col items-center justify-center w-full max-w-sm mb-12 flex-1">
        <div className="w-24 h-24 bg-blue-100 rounded-3xl flex items-center justify-center text-blue-600 mb-6 shadow-sm rotate-3">
          <PlayCircle className="w-12 h-12" />
        </div>
        <h1 className="text-3xl font-extrabold text-slate-900 mb-2">{day.title}</h1>
        <p className="text-slate-500 font-medium mb-1">총 {day.questions.length}문제</p>
        {knownCount > 0 && (
          <p className="text-xs text-blue-600 font-semibold mb-3">
            학습 대기 {learningCount}문제 · 이미 앎 {knownCount}문제
          </p>
        )}
        {!hasQuestions && (
          <p className="text-sm text-red-500 font-semibold">
            아직 등록된 문제가 없습니다.
          </p>
        )}
        {knownCount > 0 && (
          <button
            onClick={() => navigate(`/quiz/${deckId}/${dayId}?mode=known`)}
            className="mt-4 inline-flex items-center gap-2 rounded-full bg-blue-50 text-blue-700 px-4 py-2 text-xs font-bold hover:bg-blue-100 transition-colors"
          >
            <BookmarkCheck className="w-4 h-4" />
            이미 앎 {knownCount}문제 다시 보기
          </button>
        )}
      </div>
      
      <div className="w-full flex gap-3 mt-auto pb-8">
        <button 
          onClick={() => setShowSettings(true)}
          className="p-4 bg-slate-200 text-slate-600 rounded-2xl font-bold hover:bg-slate-300 transition-colors flex items-center justify-center"
        >
          <Settings2 className="w-6 h-6" />
        </button>
        <div className="flex-1 grid grid-cols-2 gap-3">
          <button
            onClick={() => navigate(`/quiz/${deckId}/${dayId}`)}
            disabled={!hasQuestions}
            className={`py-4 rounded-2xl font-bold text-sm transition-all shadow-lg flex items-center justify-center gap-2 ${
              hasQuestions
                ? "bg-blue-600 text-white hover:bg-blue-700 active:scale-[0.98] shadow-blue-600/30"
                : "bg-slate-300 text-slate-500 cursor-not-allowed shadow-none"
            }`}
          >
            <ListOrdered className="w-5 h-5" />
            순차퀴즈
          </button>
          <button
            onClick={() => navigate(`/quiz/${deckId}/${dayId}?random=day`)}
            disabled={!hasQuestions}
            className={`py-4 rounded-2xl font-bold text-sm transition-all shadow-lg flex items-center justify-center gap-2 ${
              hasQuestions
                ? "bg-slate-800 text-white hover:bg-slate-900 active:scale-[0.98] shadow-slate-800/20"
                : "bg-slate-300 text-slate-500 cursor-not-allowed shadow-none"
            }`}
          >
            <Shuffle className="w-5 h-5" />
            랜덤퀴즈
          </button>
        </div>
      </div>
    </div>
  );
}
