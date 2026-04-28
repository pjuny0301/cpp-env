import { useEffect, useRef, useState } from "react";
import { Link } from "react-router";
import { BookOpen, RefreshCw, Settings2, Check, AlertCircle } from "lucide-react";
import { useAppContext } from "../context";

export function Home() {
  const { decks, sourceUrl, setSourceUrl, reloadRemoteDecks, remoteStatus } = useAppContext();
  const [showSource, setShowSource] = useState(false);
  const [statusVisible, setStatusVisible] = useState(false);
  const dismissTimerRef = useRef<number | null>(null);

  useEffect(() => {
    if (!remoteStatus.message) {
      setStatusVisible(false);
      return;
    }
    setStatusVisible(true);
    if (dismissTimerRef.current !== null) {
      window.clearTimeout(dismissTimerRef.current);
      dismissTimerRef.current = null;
    }
    if (remoteStatus.state === "success") {
      dismissTimerRef.current = window.setTimeout(() => {
        setStatusVisible(false);
        dismissTimerRef.current = null;
      }, 2500);
    }
    return () => {
      if (dismissTimerRef.current !== null) {
        window.clearTimeout(dismissTimerRef.current);
        dismissTimerRef.current = null;
      }
    };
  }, [remoteStatus]);

  const isLoading = remoteStatus.state === "loading";
  const isError = remoteStatus.state === "error";
  const showStatus = statusVisible && remoteStatus.message;

  return (
    <div className="p-6 bg-slate-50 min-h-full">
      <div className="mb-6 pt-8">
        <div className="flex items-start justify-between gap-3">
          <div className="min-w-0">
            <h1 className="text-3xl font-extrabold text-blue-900 mb-2 tracking-tight">QUIZ APP</h1>
            <p className="text-slate-500 font-medium">원하는 덱을 선택하세요</p>
          </div>
          <button
            onClick={() => setShowSource(prev => !prev)}
            aria-label="데이터 소스 설정"
            aria-expanded={showSource}
            className={`shrink-0 p-2 rounded-full transition-colors ${showSource ? 'bg-blue-100 text-blue-700' : 'text-slate-400 hover:bg-slate-100'}`}
          >
            <Settings2 className="w-5 h-5" />
          </button>
        </div>

        {showSource && (
          <div className="mt-4 rounded-2xl border border-slate-200 bg-white p-3 shadow-sm">
            <label className="block text-xs font-bold uppercase tracking-wider text-slate-400 mb-2">
              Git 데이터 소스
            </label>
            <div className="flex gap-2">
              <input
                type="url"
                value={sourceUrl}
                onChange={(event) => setSourceUrl(event.target.value)}
                placeholder="https://github.com/..."
                className="min-w-0 flex-1 rounded-xl border border-slate-200 bg-white px-3 py-2 text-sm text-slate-700 outline-none focus:border-blue-400 focus:ring-2 focus:ring-blue-100"
              />
              <button
                onClick={() => reloadRemoteDecks()}
                disabled={isLoading}
                className="shrink-0 px-3 py-2 bg-slate-800 text-white rounded-xl text-sm font-bold hover:bg-slate-900 disabled:opacity-60 transition-all flex items-center gap-2"
              >
                <RefreshCw className={`w-4 h-4 ${isLoading ? "animate-spin" : ""}`} />
                새로고침
              </button>
            </div>
          </div>
        )}

        {showStatus && (
          <div
            className={`mt-4 rounded-xl px-4 py-3 text-sm font-medium flex items-start gap-2 transition-opacity ${
              isError
                ? 'bg-red-50 border border-red-100 text-red-700'
                : isLoading
                  ? 'bg-slate-100 border border-slate-200 text-slate-600'
                  : 'bg-blue-50 border border-blue-100 text-blue-800'
            }`}
          >
            <span className="mt-0.5 shrink-0">
              {isError ? <AlertCircle className="w-4 h-4" /> : isLoading ? <RefreshCw className="w-4 h-4 animate-spin" /> : <Check className="w-4 h-4" />}
            </span>
            <span className="min-w-0 flex-1 leading-snug break-words">{remoteStatus.message}</span>
            {!isLoading && (
              <button
                onClick={() => setStatusVisible(false)}
                className="shrink-0 -my-1 -mr-1 p-1 text-current opacity-50 hover:opacity-100"
                aria-label="닫기"
              >
                ✕
              </button>
            )}
          </div>
        )}
      </div>

      {decks.length === 0 ? (
        <div className="rounded-2xl border-2 border-dashed border-slate-200 bg-white px-6 py-12 text-center">
          <p className="text-slate-700 font-bold mb-2">덱을 불러오지 못했습니다</p>
          <p className="text-sm text-slate-500 mb-4">Git 소스 URL을 확인하고 새로고침하세요.</p>
          <button
            onClick={() => setShowSource(true)}
            className="inline-flex items-center gap-2 px-4 py-2 rounded-xl bg-blue-600 text-white text-sm font-bold hover:bg-blue-700 transition-colors"
          >
            <Settings2 className="w-4 h-4" />
            데이터 소스 열기
          </button>
        </div>
      ) : (
        <div className="space-y-4">
          {decks.map(deck => (
            <Link
              key={deck.id}
              to={`/deck/${deck.id}`}
              className="block"
            >
              <div className="bg-white p-6 rounded-2xl shadow-sm border border-slate-100 hover:shadow-md hover:border-blue-200 transition-all group flex items-center justify-between">
                <div className="flex items-center gap-4">
                  <div className="w-12 h-12 bg-blue-100 rounded-full flex items-center justify-center text-blue-600 group-hover:bg-blue-600 group-hover:text-white transition-colors">
                    <BookOpen className="w-6 h-6" />
                  </div>
                  <div>
                    <h2 className="text-xl font-bold text-slate-800">{deck.title}</h2>
                    <p className="text-sm text-slate-500 mt-1">{deck.days.length} Days</p>
                  </div>
                </div>
              </div>
            </Link>
          ))}
        </div>
      )}
    </div>
  );
}
