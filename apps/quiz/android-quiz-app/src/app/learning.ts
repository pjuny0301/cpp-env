export type LearningState = "learning" | "known" | "unknown";

export interface QuestionLearning {
  correctStreak: number;
  wrongCount: number;
  reviewCount?: number;
  state: LearningState;
  knownAt?: number;
  dueAt?: number;
  updatedAt: number;
}

export type LearningMap = Record<string, QuestionLearning>;

export const questionLearningKey = (deckId: string, dayId: string, questionId: string) =>
  `${deckId}::${dayId}::${questionId}`;

const DEFAULT_KNOWN_REVIEW_INTERVAL_MS = 24 * 60 * 60 * 1000;

const defaultQuestionLearning = (): QuestionLearning => ({
  correctStreak: 0,
  wrongCount: 0,
  reviewCount: 0,
  state: "learning",
  knownAt: 0,
  dueAt: 0,
  updatedAt: 0,
});

const nextReviewInterval = (reviewCount: number) => {
  const cappedCount = Math.max(0, Math.min(16, reviewCount));
  return DEFAULT_KNOWN_REVIEW_INTERVAL_MS * (2 ** cappedCount);
};

export const isQuestionDueForReview = (entry: QuestionLearning | undefined, now = Date.now()) =>
  entry?.state === "known" && Boolean(entry.dueAt) && now >= (entry.dueAt || 0);

export const recordQuestionLearningAnswer = (
  entry: QuestionLearning | undefined,
  isCorrect: boolean,
  knownThreshold: number,
  wrongRelease: number,
  updatedAt = Date.now(),
): QuestionLearning => {
  let {
    correctStreak,
    wrongCount,
    reviewCount = 0,
    state,
    knownAt = 0,
    dueAt = 0,
  } = entry || defaultQuestionLearning();

  if (isCorrect) {
    correctStreak += 1;
    wrongCount = 0;

    if (state === "known") {
      reviewCount += 1;
      dueAt = updatedAt + nextReviewInterval(reviewCount);
    } else if ((state === "learning" || state === "unknown") && correctStreak >= knownThreshold) {
      state = "known";
      reviewCount = 0;
      knownAt = updatedAt;
      dueAt = updatedAt + nextReviewInterval(reviewCount);
    }
  } else {
    wrongCount += 1;
    correctStreak = 0;
    if (state === "known" && wrongCount >= wrongRelease) {
      state = "learning";
      reviewCount = 0;
      knownAt = 0;
      dueAt = 0;
    }
  }

  return { correctStreak, wrongCount, reviewCount, state, knownAt, dueAt, updatedAt };
};

export const markQuestionLearningUnknown = (
  entry: QuestionLearning | undefined,
  updatedAt = Date.now(),
): QuestionLearning => ({
  ...(entry || defaultQuestionLearning()),
  state: "unknown",
  correctStreak: 0,
  wrongCount: 0,
  reviewCount: 0,
  knownAt: 0,
  dueAt: 0,
  updatedAt,
});
