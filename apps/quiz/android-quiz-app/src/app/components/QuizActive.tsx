import { useEffect, useMemo, useRef, useState } from "react";
import { useParams, useNavigate, useLocation } from "react-router";
import { getDayFromList, getDeckFromList } from "../data";
import { questionLearningKey, useAppContext } from "../context";
import { isQuestionDueForReview } from "../learning";
import { playSuccessSound, playErrorSound } from "../audio";
import {
  isTypedAnswerCorrect as matchesTypedAnswer,
  LONG_PRESS_MS,
  pickVariant,
  type QuizAnswer,
  type RuntimeQuestion,
  shuffleWithSeed,
  SWIPE_DX_MIN,
  SWIPE_DY_MAX,
  SWIPE_MS_MAX,
} from "../quizRuntime";
import { QuizAnswerDock } from "./QuizAnswerDock";
import { QuizProgressHeader } from "./QuizProgressHeader";
import { QuizQuestionStage } from "./QuizQuestionStage";
import { QuizSettingsModal } from "./QuizSettingsModal";

export function QuizActive() {
  const { deckId, dayId } = useParams();
  const navigate = useNavigate();
  const location = useLocation();
  const {
    decks,
    delay,
    setDelay,
    longTextFontSize,
    setLongTextFontSize,
    manualContinue,
    setManualContinue,
    quizResults,
    setQuizResults,
    recordDayProgress,
    recordQuestionAnswer,
    markQuestionUnknown,
    knownThreshold,
    setKnownThreshold,
    wrongRelease,
    setWrongRelease,
    learning,
  } = useAppContext();
  const day = getDayFromList(decks, deckId || "", dayId || "");
  const deck = getDeckFromList(decks, deckId || "");
  const searchParams = new URLSearchParams(location.search);
  const randomMode = searchParams.get("random");
  const isRandomDeck = randomMode === "deck";
  const isRandomDay = randomMode === "day";
  const modeParam = searchParams.get("mode");
  const isWrongOnly = modeParam === "wrong";
  const isKnownOnly = modeParam === "known";

  const [currentIndex, setCurrentIndex] = useState(0);
  const [answers, setAnswers] = useState<QuizAnswer[]>([]);
  const [feedbackState, setFeedbackState] = useState<'idle' | 'showing'>('idle');
  const [selectedOption, setSelectedOption] = useState<number | null>(null);
  const [typedAnswer, setTypedAnswer] = useState("");
  const [showSettings, setShowSettings] = useState(false);
  const [selectedIndices, setSelectedIndices] = useState<number[]>([]);
  const pendingAnswersRef = useRef<typeof answers | null>(null);
  const autoAdvanceTimerRef = useRef<number | null>(null);
  const swipeStartRef = useRef<{ x: number; y: number; t: number } | null>(null);
  const longPressTimerRef = useRef<number | null>(null);
  const longPressTriggeredRef = useRef(false);

  // Auto-scroll long text if needed
  const containerRef = useRef<HTMLDivElement>(null);
  const optionButtonRefs = useRef<(HTMLButtonElement | null)[]>([]);
  const randomSeedRef = useRef(`${Date.now()}:${Math.random()}`);

  const baseQuestions = useMemo<RuntimeQuestion[]>(() => {
    const sourceQuestions = isRandomDeck
      ? (deck?.days || []).flatMap(dayItem => dayItem.questions)
      : day?.questions || [];
    const orderedQuestions = isRandomDeck || isRandomDay
      ? shuffleWithSeed(
          sourceQuestions,
          randomSeedRef.current,
          question => question.id,
        )
      : sourceQuestions;

    return orderedQuestions.map(question => ({
      ...question,
      options: shuffleWithSeed(
        question.options.map((option, index) => ({ ...option, originalIndex: index })),
        randomSeedRef.current,
        option => `${question.id}:${option.originalIndex}:${option.text}`,
      ),
    }));
  }, [deck, day, isRandomDeck, isRandomDay]);
  const wrongQuestionIds = isWrongOnly
    ? new Set(
        (quizResults?.deckId === deckId && quizResults?.dayId === (isRandomDeck ? "__deck_random" : dayId)
          ? quizResults.answers
          : []
        )
          .filter(answer => answer.isCorrect === false || (answer.isCorrect === undefined && answer.selectedOptionIndex === null))
          .map(answer => answer.questionId),
      )
    : null;
  const resolveLearningEntry = (questionId: string) => {
    if (!deckId) return undefined;
    if (isRandomDeck) {
      for (const d of deck?.days || []) {
        const entry = learning[questionLearningKey(deckId, d.id, questionId)];
        if (entry) return entry;
      }
      return undefined;
    }
    const key = questionLearningKey(deckId, dayId || "", questionId);
    return learning[key];
  };
  const filteredByLearning = (items: RuntimeQuestion[]) => {
    if (isWrongOnly) return items;
    if (isKnownOnly) return items.filter(q => resolveLearningEntry(q.id)?.state === 'known');
    return items.filter(q => {
      const entry = resolveLearningEntry(q.id);
      return entry?.state !== 'known' || isQuestionDueForReview(entry);
    });
  };
  const questions = wrongQuestionIds
    ? baseQuestions.filter(question => wrongQuestionIds.has(question.id))
    : filteredByLearning(baseQuestions);
  const currentQuestion = questions[currentIndex];
  const isAnswerType = currentQuestion
    ? currentQuestion.type === "answer" || currentQuestion.type === "blank" || currentQuestion.type === "multi-answer" || currentQuestion.type === "multi-blank"
    : false;
  const isMultiselectType = currentQuestion?.type === "multiselect";

  useEffect(() => () => {
    if (autoAdvanceTimerRef.current !== null) {
      window.clearTimeout(autoAdvanceTimerRef.current);
    }
    if (longPressTimerRef.current !== null) {
      window.clearTimeout(longPressTimerRef.current);
    }
  }, []);

  useEffect(() => {
    if (!currentQuestion || isAnswerType || feedbackState !== "showing") return;

    const correctIndex = currentQuestion.options.findIndex(option => option.isCorrect);
    const targetButton = selectedOption !== null
      ? optionButtonRefs.current[selectedOption]
      : correctIndex >= 0
        ? optionButtonRefs.current[correctIndex]
        : null;

    window.requestAnimationFrame(() => {
      targetButton?.scrollIntoView({ block: "nearest", inline: "nearest", behavior: "smooth" });
    });
  }, [currentQuestion, feedbackState, isAnswerType, selectedOption]);

  if (!day && !isRandomDeck) return <div className="p-8 text-center text-slate-500">Day not found.</div>;
  if (isRandomDeck && !deck) return <div className="p-8 text-center text-slate-500">Deck not found.</div>;

  const advanceTo = (newAnswers: typeof answers) => {
    setAnswers(newAnswers);
    if (currentIndex < questions.length - 1) {
      setCurrentIndex(prev => prev + 1);
      setSelectedOption(null);
      setTypedAnswer("");
      setSelectedIndices([]);
      setFeedbackState('idle');
    } else {
      const correctCount = newAnswers.reduce((acc, a) => acc + (a.isCorrect ? 1 : 0), 0);
      if (!isRandomDeck && !isRandomDay && deckId && dayId) {
        recordDayProgress(deckId, dayId, newAnswers.length, correctCount);
      }
      setQuizResults({
        deckId: deckId!,
        dayId: isRandomDeck ? "__deck_random" : dayId!,
        randomMode: isRandomDeck ? "deck" : isRandomDay ? "day" : undefined,
        answers: newAnswers
      });
      navigate("/results", { replace: true });
    }
  };

  const handleSkipToNext = () => {
    if (!currentQuestion) return;
    if (autoAdvanceTimerRef.current !== null) {
      window.clearTimeout(autoAdvanceTimerRef.current);
      autoAdvanceTimerRef.current = null;
    }
    const pending = pendingAnswersRef.current;
    pendingAnswersRef.current = null;
    if (pending) {
      advanceTo(pending);
      return;
    }
    advanceTo([
      ...answers,
      {
        questionId: currentQuestion.id,
        selectedOptionIndex: null,
        isCorrect: undefined,
      },
    ]);
  };

  const handleSwipePointerDown = (event: React.PointerEvent) => {
    const target = event.target as HTMLElement | null;
    if (target?.closest('input, textarea')) {
      swipeStartRef.current = null;
      return;
    }
    swipeStartRef.current = { x: event.clientX, y: event.clientY, t: Date.now() };
  };

  const handleSwipePointerUp = (event: React.PointerEvent) => {
    const start = swipeStartRef.current;
    swipeStartRef.current = null;
    if (longPressTriggeredRef.current) {
      longPressTriggeredRef.current = false;
      return;
    }
    if (!start) return;
    const dx = event.clientX - start.x;
    const dy = event.clientY - start.y;
    const dt = Date.now() - start.t;
    if (Math.abs(dx) >= SWIPE_DX_MIN && Math.abs(dy) <= SWIPE_DY_MAX && dt <= SWIPE_MS_MAX) {
      handleSkipToNext();
    }
  };

  const moveNextOrFinish = (newAnswers: typeof answers) => {
    pendingAnswersRef.current = newAnswers;
    if (manualContinue) return;
    if (autoAdvanceTimerRef.current !== null) {
      window.clearTimeout(autoAdvanceTimerRef.current);
    }
    autoAdvanceTimerRef.current = window.setTimeout(() => {
      autoAdvanceTimerRef.current = null;
      const pending = pendingAnswersRef.current;
      pendingAnswersRef.current = null;
      if (pending) advanceTo(pending);
    }, delay);
  };

  const handleManualContinue = () => {
    if (autoAdvanceTimerRef.current !== null) {
      window.clearTimeout(autoAdvanceTimerRef.current);
      autoAdvanceTimerRef.current = null;
    }
    const pending = pendingAnswersRef.current;
    pendingAnswersRef.current = null;
    if (pending) advanceTo(pending);
  };

  const handleOptionSelect = (index: number | null) => {
    if (feedbackState === 'showing') return;

    setSelectedOption(index);
    setFeedbackState('showing');

    const selected = index !== null ? currentQuestion.options[index] : null;
    const isCorrect = Boolean(selected?.isCorrect);

    if (isCorrect) {
      playSuccessSound();
    } else {
      playErrorSound();
    }

    if (!isRandomDeck && !isRandomDay && deckId && dayId && index !== null) {
      recordQuestionAnswer(deckId, dayId, currentQuestion.id, isCorrect);
    }

    // Save answer
    moveNextOrFinish([
      ...answers,
      {
        questionId: currentQuestion.id,
        selectedOptionIndex: selected?.originalIndex ?? null,
        isCorrect,
      },
    ]);
  };

  const handleMultiselectTap = (index: number) => {
    if (feedbackState === 'showing') return;
    setSelectedIndices(prev => {
      if (prev.includes(index)) return prev.filter(item => item !== index);
      return [...prev, index];
    });
  };

  const handleMultiselectSubmit = () => {
    if (feedbackState === 'showing' || !currentQuestion) return;
    const correctIndices = currentQuestion.options
      .map((option, idx) => ({ option, idx }))
      .filter(entry => entry.option.isCorrect)
      .map(entry => entry.idx);
    const selectedSet = new Set(selectedIndices);
    const isCorrect =
      correctIndices.length > 0
      && correctIndices.length === selectedSet.size
      && correctIndices.every(idx => selectedSet.has(idx));

    setFeedbackState('showing');
    if (isCorrect) playSuccessSound(); else playErrorSound();

    if (!isRandomDeck && !isRandomDay && deckId && dayId) {
      recordQuestionAnswer(deckId, dayId, currentQuestion.id, isCorrect);
    }

    const firstSelected = selectedIndices[0];
    const selectedOriginalIndex = firstSelected !== undefined
      ? currentQuestion.options[firstSelected]?.originalIndex ?? null
      : null;

    moveNextOrFinish([
      ...answers,
      {
        questionId: currentQuestion.id,
        selectedOptionIndex: selectedOriginalIndex,
        typedAnswer: selectedIndices.map(idx => currentQuestion.options[idx]?.text || "").join(", "),
        isCorrect,
      },
    ]);
  };

  const clearLongPress = () => {
    if (longPressTimerRef.current !== null) {
      window.clearTimeout(longPressTimerRef.current);
      longPressTimerRef.current = null;
    }
  };
  const handleLongPressStart = () => {
    if (feedbackState === 'showing' || !currentQuestion) return;
    if (isRandomDeck || isRandomDay || !deckId || !dayId) return;
    longPressTriggeredRef.current = false;
    clearLongPress();
    longPressTimerRef.current = window.setTimeout(() => {
      longPressTimerRef.current = null;
      longPressTriggeredRef.current = true;
      markQuestionUnknown(deckId, dayId, currentQuestion.id);
      handleSkipToNext();
    }, LONG_PRESS_MS);
  };
  const handleLongPressEnd = () => {
    clearLongPress();
  };

  const handleTypedAnswerSubmit = () => {
    if (feedbackState === 'showing' || !currentQuestion) return;

    const isCorrect = matchesTypedAnswer(currentQuestion, typedAnswer);
    setFeedbackState('showing');
    setSelectedOption(isCorrect ? 0 : null);
    const correctOriginalIndex = currentQuestion.options.find(option => option.isCorrect)?.originalIndex ?? 0;
    if (isCorrect) {
      playSuccessSound();
    } else {
      playErrorSound();
    }
    if (!isRandomDeck && !isRandomDay && deckId && dayId) {
      recordQuestionAnswer(deckId, dayId, currentQuestion.id, isCorrect);
    }
    moveNextOrFinish([
      ...answers,
      {
        questionId: currentQuestion.id,
        selectedOptionIndex: isCorrect ? correctOriginalIndex : null,
        typedAnswer,
        isCorrect,
      },
    ]);
  };

  if (!currentQuestion) {
    return (
      <div className="min-h-[100dvh] bg-slate-50 flex flex-col items-center justify-center p-8 text-center">
        <h1 className="text-xl font-bold text-slate-800 mb-2">문제가 없습니다</h1>
        <p className="text-slate-500 mb-6">이 Day에는 아직 풀 문제가 등록되지 않았습니다.</p>
        <button
          onClick={() => navigate(isRandomDeck ? `/deck/${deckId}` : `/deck/${deckId}/day/${dayId}`, { replace: true })}
          className="px-5 py-3 bg-blue-600 text-white rounded-xl font-bold hover:bg-blue-700 transition-colors"
        >
          돌아가기
        </button>
      </div>
    );
  }

  const isMultiInputType = currentQuestion.type === "multi-answer" || currentQuestion.type === "multi-blank";
  const isShortType = currentQuestion.type === 'short' || isAnswerType;
  const hasManyOptions = !isAnswerType && !isMultiselectType && currentQuestion.options.length > 4;
  const optionViewportClass = hasManyOptions ? "max-h-[52vh] overflow-y-auto pr-1 no-scrollbar" : "overflow-visible";
  const variant = pickVariant(currentQuestion.id);
  const bgClass = isShortType ? variant.shortBg : variant.longBg;
  const multiselectCorrectCount = isMultiselectType
    ? currentQuestion.options.filter(option => option.isCorrect).length
    : 0;
  const multiselectComplete = isMultiselectType && selectedIndices.length === multiselectCorrectCount && multiselectCorrectCount > 0;

  return (
    <div
      className={`flex flex-col h-full w-full min-h-[100dvh] transition-colors duration-300 ${bgClass} relative overflow-hidden`}
      ref={containerRef}
      onPointerDown={handleSwipePointerDown}
      onPointerUp={handleSwipePointerUp}
      onPointerCancel={() => { swipeStartRef.current = null; }}
    >
      
      {showSettings && (
        <QuizSettingsModal
          delay={delay}
          setDelay={setDelay}
          longTextFontSize={longTextFontSize}
          setLongTextFontSize={setLongTextFontSize}
          manualContinue={manualContinue}
          setManualContinue={setManualContinue}
          knownThreshold={knownThreshold}
          setKnownThreshold={setKnownThreshold}
          wrongRelease={wrongRelease}
          setWrongRelease={setWrongRelease}
          onClose={() => setShowSettings(false)}
        />
      )}

      <QuizProgressHeader
        title={isRandomDeck ? "전체 랜덤" : isRandomDay ? `${day?.title} 랜덤` : day?.title}
        isShortType={isShortType}
        variant={variant}
        currentIndex={currentIndex}
        totalCount={questions.length}
        isShowingFeedback={feedbackState === "showing"}
        onOpenSettings={() => setShowSettings(true)}
      />

      <QuizQuestionStage
        question={currentQuestion}
        isShortType={isShortType}
        isMultiselectType={isMultiselectType}
        selectedIndices={selectedIndices}
        multiselectCorrectCount={multiselectCorrectCount}
        longTextFontSize={longTextFontSize}
        onLongPressStart={handleLongPressStart}
        onLongPressEnd={handleLongPressEnd}
      />

      <QuizAnswerDock
        question={currentQuestion}
        feedbackState={feedbackState}
        isAnswerType={isAnswerType}
        isMultiselectType={isMultiselectType}
        isMultiInputType={isMultiInputType}
        isShortType={isShortType}
        manualContinue={manualContinue}
        optionViewportClass={optionViewportClass}
        selectedOption={selectedOption}
        selectedIndices={selectedIndices}
        typedAnswer={typedAnswer}
        setTypedAnswer={setTypedAnswer}
        multiselectComplete={multiselectComplete}
        multiselectCorrectCount={multiselectCorrectCount}
        optionButtonRefs={optionButtonRefs}
        onManualContinue={handleManualContinue}
        onOptionSelect={handleOptionSelect}
        onMultiselectTap={handleMultiselectTap}
        onMultiselectSubmit={handleMultiselectSubmit}
        onTypedAnswerSubmit={handleTypedAnswerSubmit}
      />
    </div>
  );
}
