import type { Option, Question } from "./data";

export type RuntimeOption = Option & { originalIndex: number };
export type RuntimeQuestion = Question & { options: RuntimeOption[] };

export type QuizAnswer = {
  questionId: string;
  selectedOptionIndex: number | null;
  typedAnswer?: string;
  isCorrect?: boolean;
};

const hashToUnit = (value: string) => {
  let hash = 2166136261;
  for (let index = 0; index < value.length; index += 1) {
    hash ^= value.charCodeAt(index);
    hash = Math.imul(hash, 16777619);
  }
  return (hash >>> 0) / 4294967296;
};

export const shuffleWithSeed = <T,>(
  items: T[],
  seed: string,
  keyOf: (item: T, index: number) => string,
) =>
  items
    .map((item, index) => ({
      item,
      sort: hashToUnit(`${seed}:${keyOf(item, index)}:${index}`),
    }))
    .sort((a, b) => a.sort - b.sort)
    .map(entry => entry.item);

export const SWIPE_DX_MIN = 60;
export const SWIPE_DY_MAX = 40;
export const SWIPE_MS_MAX = 800;
export const LONG_PRESS_MS = 600;

export type Variant = {
  id: string;
  shortBg: string;
  shortHeaderPill: string;
  shortHeaderText: string;
  shortCounter: string;
  shortProgressTrack: string;
  shortProgressFill: string;
  shortDockBg: string;
  shortOption: string;
  shortOptionBadge: string;
  longBg: string;
  longAccent: string;
};

const VARIANTS: Variant[] = [
  {
    id: "cobalt",
    shortBg: "bg-blue-600 text-white",
    shortHeaderPill: "bg-blue-500 text-white",
    shortHeaderText: "text-blue-200",
    shortCounter: "text-blue-200",
    shortProgressTrack: "bg-blue-500/40",
    shortProgressFill: "bg-white",
    shortDockBg: "bg-blue-700/95 border-t border-white/10 shadow-[0_-12px_30px_rgba(30,64,175,0.28)]",
    shortOption: "bg-white text-blue-900 border-transparent hover:bg-blue-50 active:scale-[0.98]",
    shortOptionBadge: "bg-blue-50 text-blue-400",
    longBg: "bg-white text-slate-800",
    longAccent: "text-blue-600",
  },
  {
    id: "sunset",
    shortBg: "bg-rose-500 text-white",
    shortHeaderPill: "bg-rose-400 text-white",
    shortHeaderText: "text-rose-100",
    shortCounter: "text-rose-100",
    shortProgressTrack: "bg-rose-400/40",
    shortProgressFill: "bg-white",
    shortDockBg: "bg-rose-600/95 border-t border-white/10 shadow-[0_-12px_30px_rgba(190,24,93,0.28)]",
    shortOption: "bg-white text-rose-900 border-transparent hover:bg-rose-50 active:scale-[0.98]",
    shortOptionBadge: "bg-rose-50 text-rose-400",
    longBg: "bg-orange-50 text-slate-800",
    longAccent: "text-rose-600",
  },
  {
    id: "forest",
    shortBg: "bg-emerald-600 text-white",
    shortHeaderPill: "bg-emerald-500 text-white",
    shortHeaderText: "text-emerald-100",
    shortCounter: "text-emerald-100",
    shortProgressTrack: "bg-emerald-500/40",
    shortProgressFill: "bg-white",
    shortDockBg: "bg-emerald-700/95 border-t border-white/10 shadow-[0_-12px_30px_rgba(5,150,105,0.28)]",
    shortOption: "bg-white text-emerald-900 border-transparent hover:bg-emerald-50 active:scale-[0.98]",
    shortOptionBadge: "bg-emerald-50 text-emerald-500",
    longBg: "bg-slate-50 text-slate-800",
    longAccent: "text-emerald-600",
  },
  {
    id: "ink",
    shortBg: "bg-slate-800 text-white",
    shortHeaderPill: "bg-slate-700 text-white",
    shortHeaderText: "text-slate-300",
    shortCounter: "text-slate-300",
    shortProgressTrack: "bg-slate-600/50",
    shortProgressFill: "bg-amber-300",
    shortDockBg: "bg-slate-900/95 border-t border-white/10 shadow-[0_-12px_30px_rgba(15,23,42,0.4)]",
    shortOption: "bg-white text-slate-800 border-transparent hover:bg-slate-100 active:scale-[0.98]",
    shortOptionBadge: "bg-slate-100 text-slate-500",
    longBg: "bg-white text-slate-800",
    longAccent: "text-slate-800",
  },
];

export const pickVariant = (_seed: string): Variant => {
  return VARIANTS[0];
};

const normalizeText = (value: string) => value.trim().replace(/\s+/g, " ").toLowerCase();

export const splitAnswerItems = (value: string) =>
  value
    .split(/[\n,;]/)
    .map(item => item.trim())
    .filter(Boolean);

export const getCorrectAnswers = (question: RuntimeQuestion | undefined) =>
  question?.options.filter(option => option.isCorrect).map(option => option.text).filter(Boolean) || [];

export const getCorrectAnswerLabel = (question: RuntimeQuestion | undefined) =>
  getCorrectAnswers(question).join(", ") || "정답 미입력";

export const isTypedAnswerCorrect = (question: RuntimeQuestion | undefined, value: string) => {
  if (!question) return false;

  const expectedAnswers = getCorrectAnswers(question).map(normalizeText).filter(Boolean);
  if (question.type === "multi-answer" || question.type === "multi-blank") {
    const typedAnswers = splitAnswerItems(value).map(normalizeText).filter(Boolean);
    return expectedAnswers.length > 0
      && expectedAnswers.every(answer => typedAnswers.includes(answer))
      && typedAnswers.every(answer => expectedAnswers.includes(answer));
  }

  const normalizedValue = normalizeText(value);
  return expectedAnswers.some(answer => answer === normalizedValue);
};
