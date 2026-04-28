import { Outlet, useNavigate, useLocation, matchPath } from "react-router";
import { ArrowLeft } from "lucide-react";
import { useAppContext } from "../context";
import { getDayFromList, getDeckFromList } from "../data";

function HeaderTitle() {
  const location = useLocation();
  const { decks } = useAppContext();

  const deckMatch = matchPath("/deck/:deckId", location.pathname);
  const dayMatch = matchPath("/deck/:deckId/day/:dayId", location.pathname);

  if (dayMatch) {
    const { deckId = "", dayId = "" } = dayMatch.params as { deckId?: string; dayId?: string };
    const deck = getDeckFromList(decks, deckId);
    const day = getDayFromList(decks, deckId, dayId);
    return (
      <div className="flex flex-col leading-tight min-w-0">
        <span className="text-[11px] font-medium text-slate-400 uppercase tracking-widest truncate">
          {deck?.title || "덱"}
        </span>
        <span className="text-base font-bold text-slate-800 truncate">
          {day?.title || "Day"}
        </span>
      </div>
    );
  }

  if (deckMatch) {
    const { deckId = "" } = deckMatch.params as { deckId?: string };
    const deck = getDeckFromList(decks, deckId);
    return (
      <span className="text-base font-bold text-slate-800 truncate">
        {deck?.title || "덱"}
      </span>
    );
  }

  return <span className="text-base font-bold text-slate-800 truncate">뒤로가기</span>;
}

export function Layout() {
  const navigate = useNavigate();
  const location = useLocation();

  const isHome = location.pathname === "/";
  // The quiz active page requires full screen immersive feel.
  const isQuizActive = location.pathname.includes("/quiz/");
  const isResults = location.pathname === "/results";

  return (
    <div className="min-h-[100dvh] bg-slate-50 flex justify-center w-full">
      <div className={`w-full max-w-md bg-white shadow-2xl relative flex flex-col min-h-[100dvh] overflow-hidden ${isQuizActive || isResults ? '' : 'sm:border-x sm:border-slate-200'}`}>
        {!isHome && !isQuizActive && !isResults && (
          <header className="h-14 flex items-center px-4 border-b border-slate-100 shrink-0 gap-2">
            <button
              onClick={() => navigate(-1)}
              className="p-2 -ml-2 text-slate-600 hover:bg-slate-100 rounded-full transition-colors shrink-0"
              aria-label="뒤로가기"
            >
              <ArrowLeft className="w-5 h-5" />
            </button>
            <div className="flex-1 min-w-0 pr-7 text-left">
              <HeaderTitle />
            </div>
          </header>
        )}
        <div className="flex-1 overflow-y-auto">
          <Outlet />
        </div>
      </div>
    </div>
  );
}
