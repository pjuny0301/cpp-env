export interface ManagerDay {
  id: string;
  title: string;
  content: string;
  answers: Record<number, string>;
  distractors: Record<number, string>;
  questionMetadata?: Record<number, QuestionContractMetadata>;
}

export interface ManagerDeck {
  id: string;
  title: string;
  schemaVersion?: string;
  sourceUri?: string;
  concepts?: ConceptContractRef[];
  days: ManagerDay[];
}

export interface ContentContractRef {
  sourceId?: string;
  sourceUri?: string;
  pageId?: string;
  paragraphId?: string;
  paragraphIndex?: number;
}

export interface ConceptContractRef {
  id: string;
  title: string;
  prerequisiteIds?: string[];
}

export interface AnswerExplanationContract {
  text: string;
  conceptIds?: string[];
}

export interface QuestionContractMetadata {
  acceptedAnswers?: string[];
  source?: ContentContractRef;
  conceptIds?: string[];
  explanation?: AnswerExplanationContract;
}

export type QuizDock = "right" | "bottom" | "floating";
export type TreeDock = "left" | "right" | "bottom" | "floating";
export type DockPreview = { target: "tree" | "quiz"; placement: "right" | "bottom" } | null;

export interface GitTreeEntry {
  path: string;
  kind: "file" | "directory";
}

export interface QuizDataRepository {
  name: string;
  path: string;
  remote: string;
  active: boolean;
}

export interface GitIdentity {
  name: string;
  email: string;
}

export type GitTreeContextMenu = {
  x: number;
  y: number;
  path: string;
  kind: GitTreeEntry["kind"] | "blank";
} | null;

export type CodexChatMessage = {
  id: string;
  role: "user" | "codex";
  text: string;
};

export interface CodexRunResponse {
  message: string;
  responseJson: string;
}

export interface GitDraft {
  decks: ManagerDeck[];
  activeDeckId: string;
  activeDayId: string;
  editorContent: string;
  answers: Record<number, string>;
  distractors: Record<number, string>;
  fileName: string;
}

export type ResizeTarget =
  | "tree-left"
  | "tree-right"
  | "tree-top"
  | "tree-bottom"
  | "tree-corner"
  | "quiz-floating"
  | "quiz-right"
  | "quiz-bottom";
