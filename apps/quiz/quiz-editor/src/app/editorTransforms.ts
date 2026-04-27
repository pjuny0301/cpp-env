import type { DividerQuizType } from "./components/BulkEditor";
import type { GitTreeEntry, ManagerDay, ManagerDeck, QuestionContractMetadata } from "./editorTypes";

export const fallbackGitTreeEntries: GitTreeEntry[] = [
  { path: "README.md", kind: "file" },
  { path: "bulk.md", kind: "file" },
  { path: "quizzes.json", kind: "file" },
  { path: "assets", kind: "directory" },
];

export const sanitizeRelativePath = (value: string) =>
  value
    .replace(/\\/g, "/")
    .split("/")
    .map(part => part.trim())
    .filter(part => part && part !== "." && part !== "..")
    .join("/");

export const fileNameFromPath = (value: string) => value.split("/").filter(Boolean).pop() || value;

const escapeHtml = (value: string) =>
  value
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");

export const makeId = (prefix: string, value: string) => {
  const normalized = value
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9가-힣]+/g, "-")
    .replace(/^-+|-+$/g, "");

  return normalized ? `${prefix}-${normalized}` : `${prefix}-untitled`;
};

export const splitDistractors = (value: string) =>
  value
    .split(/[,\n]/)
    .map(item => item.trim())
    .filter(Boolean);

const splitAnswerItems = (value: string) =>
  value
    .split(/[\n,;]/)
    .map(item => item.trim())
    .filter(Boolean);

const dividerTypeMeta: Record<DividerQuizType, { label: string; className: string }> = {
  select: {
    label: "#Select",
    className: "border-blue-100 bg-blue-50 text-blue-700",
  },
  blank: {
    label: "#Blank",
    className: "border-blue-100 bg-blue-50 text-blue-700",
  },
  "multi-answer": {
    label: "#MultiAnswer",
    className: "border-slate-200 bg-slate-50 text-slate-700",
  },
  "multi-blank": {
    label: "#MultiBlank",
    className: "border-slate-200 bg-slate-50 text-slate-700",
  },
};

const textDividerPattern = /^\s*(?:#Quiz\.\s*\d+(?:\s+#(?:Select|Blank|MultiAnswer|MultiBlank))*|-{3,})\s*$/i;

const quizTypeFromText = (value: string): DividerQuizType | undefined => {
  if (/#MultiBlank/i.test(value)) return "multi-blank";
  if (/#MultiAnswer/i.test(value)) return "multi-answer";
  if (/#Blank/i.test(value)) return "blank";
  if (/#Select/i.test(value)) return "select";
  return undefined;
};

const createEditorDividerHtml = (index: number, quizType?: DividerQuizType) => {
  const meta = quizType ? dividerTypeMeta[quizType] : null;
  const typeBadge = meta
    ? `<span data-quiz-type-badge="true" ${quizType === "select" ? 'data-quiz-select="true"' : ""} class="quiz-type-label shrink-0 rounded-full border px-2.5 py-1 text-xs font-bold ${meta.className}">${meta.label}</span>`
    : "";

  return `<div data-quiz-divider="true" ${quizType ? `data-quiz-type="${quizType}"` : ""} contenteditable="false" class="quiz-divider-block my-3 flex items-center gap-2 select-none"><span data-quiz-divider-label="true" class="quiz-divider-label shrink-0 rounded-full border border-slate-200 bg-white px-2.5 py-0.5 text-[11px] font-semibold text-slate-500 tracking-wide">#${index}</span>${typeBadge}<hr class="quiz-divider flex-1 border-t border-dashed border-slate-300" /></div>`;
};

const textLinesToHtml = (value: string) =>
  value
    .split(/\r?\n/)
    .map(line => line.trim() === "" ? "<div><br /></div>" : `<div>${escapeHtml(line)}</div>`)
    .join("");

export const textToEditorHtml = (value: string) => {
  let dividerIndex = 0;

  return value
    .split(/\r?\n/)
    .map(line => {
      if (textDividerPattern.test(line)) {
        dividerIndex += 1;
        const labelMatch = line.match(/#Quiz\.\s*(\d+)/i);
        return createEditorDividerHtml(labelMatch ? Number(labelMatch[1]) : dividerIndex + 1, quizTypeFromText(line));
      }

      return line.trim() === "" ? "<div><br /></div>" : `<div>${escapeHtml(line)}</div>`;
    })
    .join("");
};

export const buildOptions = (answer: string, peerAnswers: string[], quizId: number, multiCorrect = false) => {
  const correctAnswers = splitAnswerItems(answer);

  if (multiCorrect) {
    const uniqueCorrectAnswers = correctAnswers.filter((item, index, list) => list.indexOf(item) === index);
    return (uniqueCorrectAnswers.length > 0 ? uniqueCorrectAnswers : ["정답 미입력"]).map(text => ({ text, isCorrect: true }));
  }

  const correctAnswer = correctAnswers[0] || answer.trim() || "정답 미입력";
  const wrongOptions = peerAnswers
    .map(option => option.trim())
    .filter(option => option !== correctAnswer)
    .filter((option, index, list) => option && list.indexOf(option) === index)
    .slice(0, 3)
    .map(text => ({ text, isCorrect: false }));
  const options = [{ text: correctAnswer, isCorrect: true }, ...wrongOptions];
  const correctOption = options[0];
  const restOptions = options.slice(1);
  const correctIndex = restOptions.length > 0 ? (quizId - 1) % (restOptions.length + 1) : 0;

  restOptions.splice(correctIndex, 0, correctOption);
  return restOptions;
};

export const createDay = (index: number, title = `Day ${index}`): ManagerDay => ({
  id: makeId("day", `${title}-${Date.now()}`),
  title,
  content: "",
  answers: {},
  distractors: {},
});

export const initialDay = createDay(1, "Day 1");
export const initialDeck: ManagerDeck = {
  id: makeId("deck", "새 퀴즈 덱"),
  title: "새 퀴즈 덱",
  days: [initialDay],
};

export const isRecord = (value: unknown): value is Record<string, any> =>
  Boolean(value) && typeof value === "object" && !Array.isArray(value);

const textValue = (value: unknown, fallback = "") =>
  typeof value === "string" && value.trim() ? value.trim() : fallback;

const optionalText = (value: unknown) =>
  typeof value === "string" && value.trim() ? value.trim() : undefined;

const optionalNumber = (value: unknown) =>
  typeof value === "number" && Number.isFinite(value) ? value : undefined;

const stringList = (value: unknown): string[] | undefined => {
  const items = Array.isArray(value)
    ? value
    : typeof value === "string"
      ? value.split(/[\n,;]/)
      : [];
  const normalized = items
    .map(item => textValue(item))
    .filter(Boolean)
    .filter((item, index, list) => list.indexOf(item) === index);

  return normalized.length > 0 ? normalized : undefined;
};

const contentRefFromQuestion = (value: unknown) => {
  if (!isRecord(value)) return undefined;

  const ref = {
    sourceId: optionalText(value.sourceId || value.source_id),
    sourceUri: optionalText(value.sourceUri || value.source_uri),
    pageId: optionalText(value.pageId || value.page_id),
    paragraphId: optionalText(value.paragraphId || value.paragraph_id),
    paragraphIndex: optionalNumber(value.paragraphIndex ?? value.paragraph_index),
  };

  return Object.values(ref).some(item => item !== undefined) ? ref : undefined;
};

const explanationFromQuestion = (value: unknown) => {
  const text = isRecord(value)
    ? textValue(value.text || value.explanation)
    : textValue(value);
  if (!text) return undefined;

  return {
    text,
    conceptIds: isRecord(value) ? stringList(value.conceptIds || value.concept_ids) : undefined,
  };
};

const contractMetadataFromQuestion = (question: Record<string, any>): QuestionContractMetadata | undefined => {
  const metadata: QuestionContractMetadata = {
    acceptedAnswers: stringList(question.acceptedAnswers || question.accepted_answers),
    source: contentRefFromQuestion(question.source || question.sourceRef || question.contentRef || question.content_ref),
    conceptIds: stringList(question.conceptIds || question.concept_ids),
    explanation: explanationFromQuestion(question.explanation || question.answerExplanation || question.answer_explanation),
  };

  return Object.values(metadata).some(item => item !== undefined) ? metadata : undefined;
};

const answerFromQuestion = (question: Record<string, any>) => {
  const answerList = Array.isArray(question.answers)
    ? question.answers
    : Array.isArray(question.answer)
      ? question.answer
      : Array.isArray(question.blankAnswers)
        ? question.blankAnswers
        : null;

  if (answerList) {
    return answerList
      .map(answer => typeof answer === "string" ? answer.trim() : "")
      .filter(Boolean)
      .join("\n");
  }

  if (typeof question.answer === "string") return question.answer;

  if (Array.isArray(question.options)) {
    const correctOptions = question.options
      .filter((option: unknown) => isRecord(option) && option.isCorrect && typeof option.text === "string")
      .map((option: any) => option.text.trim())
      .filter(Boolean);
    if (correctOptions.length > 0) return correctOptions.join("\n");
  }

  return "";
};

const quizTypeFromQuestion = (question: Record<string, any>): DividerQuizType | undefined => {
  const type = textValue(question.type || question.quizType).toLowerCase();
  if (type === "select") return "select";
  if (type === "blank" || type === "fillblank" || type === "fill-in-the-blank") return "blank";
  if (type === "multi-answer" || type === "multianswer" || type === "multi-short-answer") return "multi-answer";
  if (type === "multi-blank" || type === "multiblank" || type === "multi-fill-blank") return "multi-blank";
  if (question.selectionMode === "select") return "select";
  return undefined;
};

const contentFromQuestion = (question: Record<string, any>) => {
  if (typeof question.html === "string" && question.html.trim()) return question.html.trim();

  const text = textValue(question.text || question.questionText || question.longText);
  const images = [
    typeof question.imageUrl === "string" ? question.imageUrl : "",
    ...(Array.isArray(question.images) ? question.images.filter((image: unknown): image is string => typeof image === "string") : []),
  ].filter(Boolean);
  const imageHtml = images
    .map(src => `<div><img src="${escapeHtml(src)}" class="max-w-md max-h-64 object-contain my-2 rounded-md border border-gray-200 inline-block shadow-sm" alt="Quiz image" /></div>`)
    .join("");

  return `${textLinesToHtml(text)}${imageHtml}`;
};

const managerDayFromQuestions = (title: string, questions: Record<string, any>[], dayIndex: number): ManagerDay => {
  const answers: Record<number, string> = {};
  const questionMetadata: Record<number, QuestionContractMetadata> = {};
  const content = questions
    .map((question, questionIndex) => {
      const quizNumber = questionIndex + 1;
      answers[quizNumber] = answerFromQuestion(question);
      const metadata = contractMetadataFromQuestion(question);
      if (metadata) questionMetadata[quizNumber] = metadata;
      const questionHtml = contentFromQuestion(question);
      return questionIndex === 0 ? questionHtml : `${createEditorDividerHtml(questionIndex + 1, quizTypeFromQuestion(question))}${questionHtml}`;
    })
    .join("");

  return {
    id: makeId("day", `${title}-${dayIndex + 1}`),
    title,
    content,
    answers,
    distractors: {},
    questionMetadata: Object.keys(questionMetadata).length > 0 ? questionMetadata : undefined,
  };
};

export const managerDecksFromJson = (payload: unknown): ManagerDeck[] => {
  if (Array.isArray(payload)) {
    return [{
      id: makeId("deck", "Imported Deck"),
      title: "Imported Deck",
      days: [managerDayFromQuestions("Day 1", payload.filter(isRecord), 0)],
    }];
  }

  if (!isRecord(payload)) return [];

  if (Array.isArray(payload.decks)) {
    return payload.decks
      .filter(isRecord)
      .map((deck, deckIndex) => {
        const days = Array.isArray(deck.days)
          ? deck.days
              .filter(isRecord)
              .map((day, dayIndex) => {
                const questions = Array.isArray(day.questions) ? day.questions.filter(isRecord) : [];
                return managerDayFromQuestions(textValue(day.title, `Day ${dayIndex + 1}`), questions, dayIndex);
              })
          : [];

        return {
          id: textValue(deck.id, makeId("deck", `${deck.title || `Deck ${deckIndex + 1}`}-${deckIndex}`)),
          title: textValue(deck.title, `Deck ${deckIndex + 1}`),
          schemaVersion: textValue(deck.schemaVersion || deck.schema_version, "quiz-deck-v1"),
          sourceUri: optionalText(deck.sourceUri || deck.source_uri),
          concepts: Array.isArray(deck.concepts) ? deck.concepts.filter(isRecord).map(concept => ({
            id: textValue(concept.id),
            title: textValue(concept.title || concept.name),
            prerequisiteIds: stringList(concept.prerequisiteIds || concept.prerequisite_ids),
          })).filter(concept => concept.id && concept.title) : undefined,
          days: days.length > 0 ? days : [createDay(1, "Day 1")],
        };
      });
  }

  if (Array.isArray(payload.days)) {
    return [{
      id: makeId("deck", textValue(payload.deckTitle || payload.title, "Imported Deck")),
      title: textValue(payload.deckTitle || payload.title, "Imported Deck"),
      schemaVersion: textValue(payload.schemaVersion || payload.schema_version, "quiz-deck-v1"),
      sourceUri: optionalText(payload.sourceUri || payload.source_uri),
      concepts: Array.isArray(payload.concepts) ? payload.concepts.filter(isRecord).map(concept => ({
        id: textValue(concept.id),
        title: textValue(concept.title || concept.name),
        prerequisiteIds: stringList(concept.prerequisiteIds || concept.prerequisite_ids),
      })).filter(concept => concept.id && concept.title) : undefined,
      days: payload.days
        .filter(isRecord)
        .map((day, dayIndex) => managerDayFromQuestions(
          textValue(day.title, `Day ${dayIndex + 1}`),
          Array.isArray(day.questions) ? day.questions.filter(isRecord) : [],
          dayIndex,
        )),
    }];
  }

  if (Array.isArray(payload.questions)) {
    return [{
      id: makeId("deck", textValue(payload.deckTitle, "Imported Deck")),
      title: textValue(payload.deckTitle, "Imported Deck"),
      days: [
        managerDayFromQuestions(
          textValue(payload.dayTitle || payload.title, "Day 1"),
          payload.questions.filter(isRecord),
          0,
        ),
      ],
    }];
  }

  if (Array.isArray(payload.quizzes)) {
    return [{
      id: makeId("deck", textValue(payload.deckTitle, "Imported Deck")),
      title: textValue(payload.deckTitle, "Imported Deck"),
      days: [
        managerDayFromQuestions(
          textValue(payload.dayTitle, "Day 1"),
          payload.quizzes.filter(isRecord),
          0,
        ),
      ],
    }];
  }

  return [];
};
