export interface Option {
  text: string;
  isCorrect: boolean;
}

export type QuestionType = 'short' | 'long' | 'answer' | 'blank' | 'multi-answer' | 'multi-blank' | 'multiselect';

export const QUIZ_DECK_SCHEMA_VERSION = "quiz-deck-v1";

export interface ContentRef {
  sourceId?: string;
  sourceUri?: string;
  pageId?: string;
  paragraphId?: string;
  paragraphIndex?: number;
}

export interface ConceptRef {
  id: string;
  title: string;
  prerequisiteIds?: string[];
}

export interface AnswerExplanation {
  text: string;
  conceptIds?: string[];
}

export interface Question {
  id: string;
  type: QuestionType;
  questionText?: string;
  longText?: string;
  imageUrl?: string;
  options: Option[];
  acceptedAnswers?: string[];
  source?: ContentRef;
  conceptIds?: string[];
  explanation?: AnswerExplanation;
}

export interface Day {
  id: string;
  title: string;
  questions: Question[];
}

export interface Deck {
  id: string;
  title: string;
  schemaVersion?: string;
  sourceUri?: string;
  concepts?: ConceptRef[];
  days: Day[];
}

export const mockDecks: Deck[] = [
  {
    id: "deck-1",
    title: "필수 영단어",
    days: [
      {
        id: "day-1",
        title: "Day 1",
        questions: [
          {
            id: "q-1",
            type: "short",
            questionText: "mom",
            options: [
              { text: "n. 엄마", isCorrect: true },
              { text: "adj. 어리둥절해하는, 이해하지 못하고 있는", isCorrect: false },
              { text: "adv. ~도 ~조차", isCorrect: false }
            ]
          },
          {
            id: "q-2",
            type: "short",
            questionText: "baffled",
            options: [
              { text: "n. 엄마", isCorrect: false },
              { text: "adj. 어리둥절해하는, 이해하지 못하고 있는", isCorrect: true },
              { text: "adv. ~도 ~조차", isCorrect: false }
            ]
          },
          {
            id: "q-3",
            type: "short",
            questionText: "even",
            options: [
              { text: "n. 엄마", isCorrect: false },
              { text: "adj. 어리둥절해하는, 이해하지 못하고 있는", isCorrect: false },
              { text: "adv. ~도 ~조차", isCorrect: true }
            ]
          },
          {
            id: "q-4",
            type: "long",
            longText: "Climate change is one of the most pressing issues of our time. Its effects are far-reaching and impact every aspect of our lives. From rising sea levels to more frequent and severe weather events, the consequences are undeniable. Immediate action is required to mitigate these effects and ensure a sustainable future for generations to come. Many scientists warn that if we do not act now, the damage may become irreversible within the next few decades.",
            imageUrl: "https://images.unsplash.com/photo-1621998951482-d096a4c7dee4?crop=entropy&cs=tinysrgb&fit=max&fm=jpg&ixid=M3w3Nzg4Nzd8MHwxfHNlYXJjaHwxfHxkb2N1bWVudCUyMHJlYWRpbmd8ZW58MXx8fHwxNzc2NDE2NDMxfDA&ixlib=rb-4.1.0&q=80&w=1080",
            options: [
              { text: "기후 변화의 긍정적인 측면", isCorrect: false },
              { text: "기후 변화로 인한 심각한 결과와 즉각적인 조치의 필요성", isCorrect: true },
              { text: "기후 변화를 무시해도 되는 이유", isCorrect: false },
              { text: "해수면 상승의 역사적 배경", isCorrect: false }
            ]
          }
        ]
      },
      {
        id: "day-2",
        title: "Day 2",
        questions: [
          {
            id: "q-5",
            type: "short",
            questionText: "apple",
            options: [
              { text: "n. 사과", isCorrect: true },
              { text: "n. 바나나", isCorrect: false }
            ]
          }
        ]
      }
    ]
  },
  {
    id: "deck-2",
    title: "고급 독해",
    days: [
      {
        id: "day-1",
        title: "Day 1",
        questions: []
      }
    ]
  }
];

export const getDeck = (id: string) => mockDecks.find(d => d.id === id);
export const getDay = (deckId: string, dayId: string) => {
  const deck = getDeck(deckId);
  return deck?.days.find(d => d.id === dayId);
};

export const getDeckFromList = (decks: Deck[], id: string) => decks.find(d => d.id === id);

export const getDayFromList = (decks: Deck[], deckId: string, dayId: string) => {
  const deck = getDeckFromList(decks, deckId);
  return deck?.days.find(d => d.id === dayId);
};

export const isRecord = (value: unknown): value is Record<string, any> =>
  Boolean(value) && typeof value === "object" && !Array.isArray(value);

export const asText = (value: unknown, fallback = "") =>
  typeof value === "string" && value.trim() ? value.trim() : fallback;

const asOptionalText = (value: unknown) =>
  typeof value === "string" && value.trim() ? value.trim() : undefined;

const asOptionalNumber = (value: unknown) =>
  typeof value === "number" && Number.isFinite(value) ? value : undefined;

const normalizeStringList = (value: unknown): string[] | undefined => {
  const items = Array.isArray(value)
    ? value
    : typeof value === "string"
      ? value.split(/[\n,;]/)
      : [];
  const normalized = items
    .map(item => asText(item))
    .filter(Boolean)
    .filter((item, index, list) => list.indexOf(item) === index);

  return normalized.length > 0 ? normalized : undefined;
};

const normalizeContentRef = (value: unknown): ContentRef | undefined => {
  if (!isRecord(value)) return undefined;

  const ref: ContentRef = {
    sourceId: asOptionalText(value.sourceId || value.source_id),
    sourceUri: asOptionalText(value.sourceUri || value.source_uri),
    pageId: asOptionalText(value.pageId || value.page_id),
    paragraphId: asOptionalText(value.paragraphId || value.paragraph_id),
    paragraphIndex: asOptionalNumber(value.paragraphIndex ?? value.paragraph_index),
  };

  return Object.values(ref).some(item => item !== undefined) ? ref : undefined;
};

const normalizeConcepts = (value: unknown): ConceptRef[] | undefined => {
  if (!Array.isArray(value)) return undefined;

  const concepts = value
    .filter(isRecord)
    .map(item => ({
      id: asText(item.id),
      title: asText(item.title || item.name),
      prerequisiteIds: normalizeStringList(item.prerequisiteIds || item.prerequisite_ids),
    }))
    .filter(item => item.id && item.title);

  return concepts.length > 0 ? concepts : undefined;
};

const normalizeAnswerExplanation = (value: unknown): AnswerExplanation | undefined => {
  const text = isRecord(value)
    ? asText(value.text || value.explanation)
    : asText(value);
  if (!text) return undefined;

  return {
    text,
    conceptIds: isRecord(value) ? normalizeStringList(value.conceptIds || value.concept_ids) : undefined,
  };
};

const splitAnswerItems = (value: unknown): string[] => {
  if (Array.isArray(value)) {
    return value.map(item => asText(item)).filter(Boolean);
  }

  if (typeof value === "string") {
    return value
      .split(/[\n,;]/)
      .map(item => item.trim())
      .filter(Boolean);
  }

  return [];
};

const normalizeOptions = (value: unknown, answer?: unknown, distractors?: unknown, type?: QuestionType): Option[] => {
  if (Array.isArray(value)) {
    const options = value
      .filter(isRecord)
      .map(option => ({
        text: asText(option.text, "선택지"),
        isCorrect: Boolean(option.isCorrect),
      }))
      .filter(option => option.text);

    if (options.some(option => option.isCorrect)) return options;
  }

  const correctTexts = type === "multi-answer" || type === "multi-blank" || type === "multiselect"
    ? splitAnswerItems(answer)
    : [asText(answer, "정답 미입력")];
  const uniqueCorrectTexts = correctTexts
    .map(text => text.trim())
    .filter(Boolean)
    .filter((text, index, list) => list.indexOf(text) === index);
  const correctOptions = (uniqueCorrectTexts.length > 0 ? uniqueCorrectTexts : ["정답 미입력"])
    .map(text => ({ text, isCorrect: true }));
  const wrongTexts = Array.isArray(distractors)
    ? distractors.map(item => asText(item)).filter(Boolean)
    : [];

  return [
    ...correctOptions,
    ...wrongTexts
      .filter(text => !correctOptions.some(option => option.text === text))
      .map(text => ({ text, isCorrect: false })),
  ];
};

const correctAnswerFromOptions = (options: Option[]) =>
  options.find(option => option.isCorrect)?.text || "정답 미입력";

const correctAnswersFromOptions = (options: Option[]) =>
  options.filter(option => option.isCorrect).map(option => option.text).filter(Boolean);

const buildPeerOptions = (answer: string, peerAnswers: string[], quizId: number): Option[] => {
  const correctAnswer = answer.trim() || "정답 미입력";
  const wrongOptions = peerAnswers
    .map(item => item.trim())
    .filter(item => item && item !== correctAnswer)
    .filter((item, index, list) => list.indexOf(item) === index)
    .slice(0, 3)
    .map(text => ({ text, isCorrect: false }));
  const options = [{ text: correctAnswer, isCorrect: true }, ...wrongOptions];
  const correctOption = options[0];
  const restOptions = options.slice(1);
  const correctIndex = restOptions.length > 0 ? (quizId - 1) % (restOptions.length + 1) : 0;

  restOptions.splice(correctIndex, 0, correctOption);
  return restOptions;
};

export const rebuildOptionsFromPeerAnswers = (decks: Deck[]) =>
  decks.map(deck => ({
    ...deck,
    days: deck.days.map(day => {
      const peerAnswers = day.questions.flatMap(question => correctAnswersFromOptions(question.options));

      return {
        ...day,
        questions: day.questions.map((question, index) => {
          if (question.type === "answer" || question.type === "blank" || question.type === "multi-answer" || question.type === "multi-blank" || question.type === "multiselect") {
            return question;
          }

          return {
            ...question,
            options: buildPeerOptions(correctAnswerFromOptions(question.options), peerAnswers, index + 1),
          };
        }),
      };
    }),
  }));

export const normalizeQuestion = (value: unknown, index: number): Question | null => {
  if (!isRecord(value)) return null;

  const imageList = Array.isArray(value.images) ? value.images.filter((item: unknown) => typeof item === "string") : [];
  const imageUrl = asText(value.imageUrl, imageList[0] || "");
  const text = asText(value.text || value.questionText || value.longText);

  if (!text && !imageUrl) return null;

  const rawType = asText(value.type || value.quizType).toLowerCase();
  const importedType =
    rawType === "answer" || rawType === "shortanswer" || rawType === "short-answer" || rawType === "input"
      ? "answer"
      : rawType === "blank" || rawType === "fillblank" || rawType === "fill-in-the-blank"
        ? "blank"
        : rawType === "multi-answer" || rawType === "multi_answer" || rawType === "multianswer" || rawType === "multi-short-answer"
          ? "multi-answer"
          : rawType === "multi-blank" || rawType === "multi_blank" || rawType === "multiblank" || rawType === "multi-fill-blank"
            ? "multi-blank"
            : rawType === "multiselect" || rawType === "multi-select" || rawType === "multi-choice" || rawType === "multichoice"
              ? "multiselect"
              : value.type === "short" || value.type === "long"
                ? value.type
                : value.selectionMode === "select" || rawType === "select"
                  ? undefined
          : undefined;
  const type: Question["type"] = importedType || (imageUrl || text.length > 80 ? "long" : "short");
  const usesQuestionText = type === "short" || type === "answer" || type === "blank" || type === "multi-answer" || type === "multi-blank" || type === "multiselect";

  return {
    id: asText(value.id, `q-${index + 1}`),
    type,
    questionText: usesQuestionText ? text : undefined,
    longText: type === "long" ? text : undefined,
    imageUrl: imageUrl || undefined,
    options: normalizeOptions(value.options, value.answer || value.answers || value.blankAnswers, value.distractors, type),
    acceptedAnswers: normalizeStringList(value.acceptedAnswers || value.accepted_answers || value.answers || value.blankAnswers),
    source: normalizeContentRef(value.source || value.sourceRef || value.contentRef || value.content_ref),
    conceptIds: normalizeStringList(value.conceptIds || value.concept_ids),
    explanation: normalizeAnswerExplanation(value.explanation || value.answerExplanation || value.answer_explanation),
  };
};

export const normalizeDay = (value: unknown, index: number): Day | null => {
  if (!isRecord(value)) return null;

  const questions = Array.isArray(value.questions)
    ? value.questions.map(normalizeQuestion).filter(Boolean) as Question[]
    : [];

  return {
    id: asText(value.id, `day-${index + 1}`),
    title: asText(value.title, `Day ${index + 1}`),
    questions,
  };
};

export const normalizeDeck = (value: unknown, index: number): Deck | null => {
  if (!isRecord(value)) return null;

  const days = Array.isArray(value.days)
    ? value.days.map(normalizeDay).filter(Boolean) as Day[]
    : [];

  return {
    id: asText(value.id, `deck-${index + 1}`),
    title: asText(value.title, `Imported Deck ${index + 1}`),
    schemaVersion: asText(value.schemaVersion || value.schema_version, QUIZ_DECK_SCHEMA_VERSION),
    sourceUri: asOptionalText(value.sourceUri || value.source_uri),
    concepts: normalizeConcepts(value.concepts),
    days,
  };
};

export const normalizeImportedDecks = (payload: unknown): Deck[] => {
  if (!isRecord(payload)) return [];

  if (Array.isArray(payload.decks)) {
    return rebuildOptionsFromPeerAnswers(payload.decks.map(normalizeDeck).filter(Boolean) as Deck[]);
  }

  if (Array.isArray(payload.quizzes)) {
    const questions = payload.quizzes.map(normalizeQuestion).filter(Boolean) as Question[];

    return rebuildOptionsFromPeerAnswers([
      {
        id: "deck-imported",
        title: asText(payload.deckTitle, "Imported Deck"),
        days: [
          {
            id: "day-imported",
            title: asText(payload.dayTitle, "Day 1"),
            questions,
          },
        ],
      },
    ]);
  }

  return [];
};
