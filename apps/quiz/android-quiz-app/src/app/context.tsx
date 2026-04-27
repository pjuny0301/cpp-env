import { createContext, useContext, useEffect, useState, ReactNode } from "react";
import { Deck, mockDecks } from "./data";
import { fetchDecksFromGitUrl } from "./remoteDecks";
import {
  type LearningMap,
  markQuestionLearningUnknown,
  questionLearningKey,
  recordQuestionLearningAnswer,
} from "./learning";

export { questionLearningKey } from "./learning";

const QUIZ_SOURCE_URL_KEY = "quizGitSourceUrl";
const LONG_TEXT_FONT_SIZE_KEY = "quizLongTextFontSize";
const MANUAL_CONTINUE_KEY = "quizManualContinue";
const DELAY_KEY = "quizDelayMs";
const DAY_PROGRESS_KEY = "quizDayProgressV1";
const LEARNING_KEY = "quizLearningV1";
const KNOWN_THRESHOLD_KEY = "quizKnownThreshold";
const WRONG_RELEASE_KEY = "quizWrongRelease";
const builtInSourceUrl = "https://github.com/pjuny0301/quiz-data/tree/main/mobile-decks";
const defaultSourceUrl = String(import.meta.env.VITE_QUIZ_DATA_URL || builtInSourceUrl);

export interface DayProgress {
  accuracy: number;
  total: number;
  correct: number;
  attemptedAt: number;
}
export type DayProgressMap = Record<string, DayProgress>;
const dayProgressKey = (deckId: string, dayId: string) => `${deckId}::${dayId}`;

type RemoteStatus =
  | { state: "idle"; message: string }
  | { state: "loading"; message: string }
  | { state: "success"; message: string }
  | { state: "error"; message: string };

interface AppState {
  decks: Deck[];
  setDecks: (decks: Deck[]) => void;
  sourceUrl: string;
  setSourceUrl: (url: string) => void;
  reloadRemoteDecks: (url?: string) => Promise<void>;
  remoteStatus: RemoteStatus;
  delay: number;
  setDelay: (val: number) => void;
  longTextFontSize: number;
  setLongTextFontSize: (val: number) => void;
  manualContinue: boolean;
  setManualContinue: (val: boolean) => void;
  dayProgress: DayProgressMap;
  recordDayProgress: (deckId: string, dayId: string, total: number, correct: number) => void;
  learning: LearningMap;
  knownThreshold: number;
  setKnownThreshold: (val: number) => void;
  wrongRelease: number;
  setWrongRelease: (val: number) => void;
  recordQuestionAnswer: (deckId: string, dayId: string, questionId: string, isCorrect: boolean) => void;
  markQuestionUnknown: (deckId: string, dayId: string, questionId: string) => void;
  resetQuestionLearning: (deckId: string, dayId: string, questionId: string) => void;
  quizResults: {
    deckId: string;
    dayId: string;
    randomMode?: "deck" | "day";
    answers: {
      questionId: string;
      selectedOptionIndex: number | null;
      typedAnswer?: string;
      isCorrect?: boolean;
    }[];
  } | null;
  setQuizResults: (results: any) => void;
}

const AppContext = createContext<AppState | undefined>(undefined);

export function AppProvider({ children }: { children: ReactNode }) {
  const [decks, setDecks] = useState<Deck[]>(mockDecks);
  const [sourceUrl, setSourceUrlState] = useState(() => localStorage.getItem(QUIZ_SOURCE_URL_KEY) || defaultSourceUrl);
  const [remoteStatus, setRemoteStatus] = useState<RemoteStatus>({ state: "idle", message: "" });
  const [delay, setDelayState] = useState(() => {
    const saved = Number(localStorage.getItem(DELAY_KEY));
    return Number.isFinite(saved) && saved >= 500 && saved <= 5000 ? saved : 1500;
  });
  const [longTextFontSize, setLongTextFontSizeState] = useState(() => {
    const saved = Number(localStorage.getItem(LONG_TEXT_FONT_SIZE_KEY));
    return Number.isFinite(saved) && saved >= 14 && saved <= 32 ? saved : 18;
  });
  const [manualContinue, setManualContinueState] = useState(() => {
    const saved = localStorage.getItem(MANUAL_CONTINUE_KEY);
    return saved === null ? true : saved === "1";
  });
  const [dayProgress, setDayProgress] = useState<DayProgressMap>(() => {
    try {
      const raw = localStorage.getItem(DAY_PROGRESS_KEY);
      return raw ? JSON.parse(raw) as DayProgressMap : {};
    } catch {
      return {};
    }
  });
  const [learning, setLearning] = useState<LearningMap>(() => {
    try {
      const raw = localStorage.getItem(LEARNING_KEY);
      return raw ? JSON.parse(raw) as LearningMap : {};
    } catch {
      return {};
    }
  });
  const [knownThreshold, setKnownThresholdState] = useState(() => {
    const saved = Number(localStorage.getItem(KNOWN_THRESHOLD_KEY));
    return Number.isFinite(saved) && saved >= 1 && saved <= 10 ? saved : 3;
  });
  const [wrongRelease, setWrongReleaseState] = useState(() => {
    const saved = Number(localStorage.getItem(WRONG_RELEASE_KEY));
    return Number.isFinite(saved) && saved >= 1 && saved <= 10 ? saved : 1;
  });
  const [quizResults, setQuizResults] = useState<AppState['quizResults']>(null);

  const setKnownThreshold = (val: number) => {
    const next = Math.min(10, Math.max(1, Math.round(val)));
    setKnownThresholdState(next);
    localStorage.setItem(KNOWN_THRESHOLD_KEY, String(next));
  };
  const setWrongRelease = (val: number) => {
    const next = Math.min(10, Math.max(1, Math.round(val)));
    setWrongReleaseState(next);
    localStorage.setItem(WRONG_RELEASE_KEY, String(next));
  };
  const persistLearning = (next: LearningMap) => {
    try { localStorage.setItem(LEARNING_KEY, JSON.stringify(next)); } catch { /* ignore quota */ }
  };
  const recordQuestionAnswer = (deckId: string, dayId: string, questionId: string, isCorrect: boolean) => {
    if (!deckId || !dayId || !questionId) return;
    const key = questionLearningKey(deckId, dayId, questionId);
    setLearning(prev => {
      const next = {
        ...prev,
        [key]: recordQuestionLearningAnswer(prev[key], isCorrect, knownThreshold, wrongRelease),
      };
      persistLearning(next);
      return next;
    });
  };
  const markQuestionUnknown = (deckId: string, dayId: string, questionId: string) => {
    if (!deckId || !dayId || !questionId) return;
    const key = questionLearningKey(deckId, dayId, questionId);
    setLearning(prev => {
      const next = { ...prev, [key]: markQuestionLearningUnknown(prev[key]) };
      persistLearning(next);
      return next;
    });
  };
  const resetQuestionLearning = (deckId: string, dayId: string, questionId: string) => {
    if (!deckId || !dayId || !questionId) return;
    const key = questionLearningKey(deckId, dayId, questionId);
    setLearning(prev => {
      if (!prev[key]) return prev;
      const { [key]: _removed, ...rest } = prev;
      persistLearning(rest);
      return rest;
    });
  };

  const setDelay = (val: number) => {
    const next = Math.min(5000, Math.max(500, val));
    setDelayState(next);
    localStorage.setItem(DELAY_KEY, String(next));
  };
  const setManualContinue = (val: boolean) => {
    setManualContinueState(val);
    localStorage.setItem(MANUAL_CONTINUE_KEY, val ? "1" : "0");
  };
  const recordDayProgress = (deckId: string, dayId: string, total: number, correct: number) => {
    if (!deckId || !dayId || total <= 0) return;
    const accuracy = Math.round((correct / total) * 100);
    setDayProgress(prev => {
      const next = { ...prev, [dayProgressKey(deckId, dayId)]: { accuracy, total, correct, attemptedAt: Date.now() } };
      try { localStorage.setItem(DAY_PROGRESS_KEY, JSON.stringify(next)); } catch { /* ignore quota */ }
      return next;
    });
  };

  const setSourceUrl = (url: string) => {
    setSourceUrlState(url);
    if (url.trim()) {
      localStorage.setItem(QUIZ_SOURCE_URL_KEY, url.trim());
    } else {
      localStorage.removeItem(QUIZ_SOURCE_URL_KEY);
    }
  };

  const setLongTextFontSize = (value: number) => {
    const nextValue = Math.min(32, Math.max(14, value));
    setLongTextFontSizeState(nextValue);
    localStorage.setItem(LONG_TEXT_FONT_SIZE_KEY, String(nextValue));
  };

  const reloadRemoteDecks = async (url = sourceUrl) => {
    const targetUrl = url.trim();
    if (!targetUrl) {
      setRemoteStatus({ state: "idle", message: "" });
      return;
    }

    setRemoteStatus({ state: "loading", message: "Git 폴더를 불러오는 중입니다." });

    try {
      const result = await fetchDecksFromGitUrl(targetUrl);
      if (result.decks.length === 0) {
        setRemoteStatus({ state: "error", message: "불러올 수 있는 덱이 없습니다." });
        return;
      }

      setDecks(result.decks);
      setSourceUrl(result.sourceUrl);
      setRemoteStatus({ state: "success", message: `${result.decks.length}개 덱을 Git에서 불러왔습니다.` });
    } catch (error) {
      const detail = error instanceof Error && error.message ? `: ${error.message}` : "";
      setRemoteStatus({ state: "error", message: `Git 폴더를 불러오지 못했습니다${detail}` });
    }
  };

  useEffect(() => {
    if (sourceUrl) void reloadRemoteDecks(sourceUrl);
    // Load linked quiz data once at app start.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return (
    <AppContext.Provider
      value={{
        decks,
        setDecks,
        sourceUrl,
        setSourceUrl,
        reloadRemoteDecks,
        remoteStatus,
        delay,
        setDelay,
        longTextFontSize,
        setLongTextFontSize,
        manualContinue,
        setManualContinue,
        dayProgress,
        recordDayProgress,
        learning,
        knownThreshold,
        setKnownThreshold,
        wrongRelease,
        setWrongRelease,
        recordQuestionAnswer,
        markQuestionUnknown,
        resetQuestionLearning,
        quizResults,
        setQuizResults,
      }}
    >
      {children}
    </AppContext.Provider>
  );
}

export function useAppContext() {
  const context = useContext(AppContext);
  if (!context) throw new Error("useAppContext must be used within AppProvider");
  return context;
}
