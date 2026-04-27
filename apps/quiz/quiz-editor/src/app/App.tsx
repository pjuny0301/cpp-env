import React, { useRef, useState, useEffect } from 'react';
import {
  LayoutGrid,
  FolderOpen,
  FileDown,
  ListChecks,
  Type,
  File,
  Folder,
  FileText,
  X,
  GripVertical,
  PanelRight,
  PanelBottom,
  RefreshCw,
  Pencil,
  Trash2,
  FilePlus,
  FolderPlus,
  Upload,
  GitBranch,
  CheckCircle2,
  Plus,
} from 'lucide-react';
import { BulkEditor } from './components/BulkEditor';
import type { BulkEditorRef } from './components/BulkEditor';
import { CodexPromptPanel } from './components/CodexPromptPanel';
import { CommitModal } from './components/CommitModal';
import { PreviewPane, extractTextWithNewlines, splitQuizHtml } from './components/PreviewPane';
import type { AnswerFocusRequest } from './components/PreviewPane';
import { RepositoryModal } from './components/RepositoryModal';
import {
  buildOptions,
  createDay,
  fallbackGitTreeEntries,
  fileNameFromPath,
  initialDay,
  initialDeck,
  isRecord,
  makeId,
  managerDecksFromJson,
  sanitizeRelativePath,
  splitDistractors,
  textToEditorHtml,
} from './editorTransforms';
import type {
  CodexChatMessage,
  CodexRunResponse,
  DockPreview,
  GitDraft,
  GitIdentity,
  GitTreeContextMenu,
  GitTreeEntry,
  ManagerDay,
  ManagerDeck,
  QuestionContractMetadata,
  QuizDataRepository,
  QuizDock,
  ResizeTarget,
  TreeDock,
} from './editorTypes';

declare global {
  interface Window {
    __TAURI__?: {
      core?: {
        invoke: <T>(command: string, args?: Record<string, unknown>) => Promise<T>;
      };
    };
  }
}

export default function App() {
  const editorRef = useRef<BulkEditorRef>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const treePanelRef = useRef<HTMLDivElement>(null);
  const quizPanelRef = useRef<HTMLDivElement>(null);
  const treeDragRef = useRef<{ startX: number; startY: number; originX: number; originY: number } | null>(null);
  const quizDragRef = useRef<{ startX: number; startY: number; originX: number; originY: number } | null>(null);
  const commitDragRef = useRef<{ startX: number; startY: number; originX: number; originY: number } | null>(null);
  const codexDragRef = useRef<{ startX: number; startY: number; originX: number; originY: number } | null>(null);
  const lastCKeyRef = useRef(0);
  const gitFileLoadSeqRef = useRef(0);
  const isProgrammaticEditorChangeRef = useRef(false);
  const selectedGitPathRef = useRef('');
  const hasUnsavedGitChangesRef = useRef(false);
  const hasUncommittedGitChangesRef = useRef(false);
  const decksRef = useRef<ManagerDeck[]>([initialDeck]);
  const activeDeckIdRef = useRef(initialDeck.id);
  const activeDayIdRef = useRef(initialDay.id);
  const editorContentRef = useRef('');
  const currentFileNameRef = useRef<string | null>(null);
  const answersRef = useRef<Record<number, string>>({});
  const distractorsRef = useRef<Record<number, string>>({});
  const gitDraftsRef = useRef<Record<string, GitDraft>>({});
  const resizeRef = useRef<{
    target: ResizeTarget;
    startX: number;
    startY: number;
    width: number;
    height: number;
    originX: number;
    originY: number;
  } | null>(null);
  const [decks, setDecks] = useState<ManagerDeck[]>([initialDeck]);
  const [activeDeckId, setActiveDeckId] = useState(initialDeck.id);
  const [activeDayId, setActiveDayId] = useState(initialDay.id);
  const [editorContent, setEditorContent] = useState('');
  const [currentFileName, setCurrentFileName] = useState<string | null>(null);
  const [isFileMenuOpen, setIsFileMenuOpen] = useState(false);
  const [isRepoModalOpen, setIsRepoModalOpen] = useState(false);
  const [answers, setAnswers] = useState<Record<number, string>>({});
  const [distractors, setDistractors] = useState<Record<number, string>>({});
  const [isTreeOpen, setIsTreeOpen] = useState(false);
  const [treeDock, setTreeDock] = useState<TreeDock>('left');
  const [treePosition, setTreePosition] = useState({ x: 860, y: 64 });
  const [treeSize, setTreeSize] = useState({ width: 340, height: 360 });
  const [quizDock, setQuizDock] = useState<QuizDock>('right');
  const [quizPosition, setQuizPosition] = useState({ x: 900, y: 120 });
  const [quizFloatingSize, setQuizFloatingSize] = useState({ width: 390, height: 540 });
  const [quizRightWidth, setQuizRightWidth] = useState(360);
  const [quizBottomHeight, setQuizBottomHeight] = useState(260);
  const [dockPreview, setDockPreview] = useState<DockPreview>(null);
  const [answerFocusRequest, setAnswerFocusRequest] = useState<AnswerFocusRequest | null>(null);
  const [gitTreeEntries, setGitTreeEntries] = useState<GitTreeEntry[]>(fallbackGitTreeEntries);
  const [gitTreeStatus, setGitTreeStatus] = useState('');
  const [gitRepositories, setGitRepositories] = useState<QuizDataRepository[]>([]);
  const [selectedGitPath, setSelectedGitPath] = useState('');
  const [hasUnsavedGitChanges, setHasUnsavedGitChanges] = useState(false);
  const [hasUncommittedGitChanges, setHasUncommittedGitChanges] = useState(false);
  const [gitTreeBasePath, setGitTreeBasePath] = useState('');
  const [gitCommitMessage, setGitCommitMessage] = useState('Update quiz data');
  const [gitCommitStatus, setGitCommitStatus] = useState('');
  const [gitIdentity, setGitIdentity] = useState<GitIdentity>({ name: '', email: '' });
  const [gitIdentityStatus, setGitIdentityStatus] = useState('');
  const [isCommitModalOpen, setIsCommitModalOpen] = useState(false);
  const [gitContextMenu, setGitContextMenu] = useState<GitTreeContextMenu>(null);
  const [commitModalPosition, setCommitModalPosition] = useState({ x: 420, y: 64 });
  const [codexPromptOpen, setCodexPromptOpen] = useState(false);
  const [codexPosition, setCodexPosition] = useState({ x: 640, y: 80 });
  const [codexPrompt, setCodexPrompt] = useState('');
  const [codexStatus, setCodexStatus] = useState('');
  const [codexMessages, setCodexMessages] = useState<CodexChatMessage[]>([]);
  const [lastCodexResponse, setLastCodexResponse] = useState('');

  const activeDeck = decks.find(deck => deck.id === activeDeckId) || decks[0];
  const activeDay = activeDeck?.days.find(day => day.id === activeDayId) || activeDeck?.days[0];
  const setSelectedGitPathValue = (path: string) => {
    selectedGitPathRef.current = path;
    setSelectedGitPath(path);
    if (!path) {
      hasUnsavedGitChangesRef.current = false;
      setHasUnsavedGitChanges(false);
    }
  };
  const setGitDraftClean = () => {
    hasUnsavedGitChangesRef.current = false;
    setHasUnsavedGitChanges(false);
  };
  const markGitUncommitted = () => {
    if (!selectedGitPathRef.current) return;
    hasUncommittedGitChangesRef.current = true;
    setHasUncommittedGitChanges(true);
  };
  const setGitCommittedClean = () => {
    hasUncommittedGitChangesRef.current = false;
    setHasUncommittedGitChanges(false);
  };
  const isGitFilePath = (path: string) => {
    if (!path) return false;
    const entry = gitTreeEntries.find(item => item.path === path);
    return entry?.kind !== 'directory';
  };
  const markGitDraftDirty = () => {
    if (isProgrammaticEditorChangeRef.current || !selectedGitPathRef.current) return;
    if (!isGitFilePath(selectedGitPathRef.current)) return;

    hasUnsavedGitChangesRef.current = true;
    setHasUnsavedGitChanges(true);
  };
  const applyEditorContent = (html: string) => {
    isProgrammaticEditorChangeRef.current = true;
    editorContentRef.current = html;
    setEditorContent(html);
    window.requestAnimationFrame(() => {
      editorRef.current?.setContent(html);
      window.requestAnimationFrame(() => {
        isProgrammaticEditorChangeRef.current = false;
      });
    });
  };
  const clampCodexPosition = (position: { x: number; y: number }) => {
    const codexWidth = Math.min(560, window.innerWidth - 24);
    return {
      x: Math.min(Math.max(12, position.x), Math.max(12, window.innerWidth - codexWidth - 12)),
      y: Math.min(Math.max(56, position.y), Math.max(56, window.innerHeight - 440)),
    };
  };

  const saveActiveDay = (
    sourceDecks = decksRef.current,
    content = editorRef.current?.getContent() || editorContentRef.current,
    nextAnswers = answersRef.current,
    nextDistractors = distractorsRef.current,
    targetDeckId = activeDeckIdRef.current,
    targetDayId = activeDayIdRef.current,
  ) =>
    sourceDecks.map(deck => {
      if (deck.id !== targetDeckId) return deck;

      return {
        ...deck,
        days: deck.days.map(day =>
          day.id === targetDayId
            ? { ...day, content, answers: nextAnswers, distractors: nextDistractors }
            : day,
        ),
      };
    });

  const captureCurrentGitDraft = (targetPath = selectedGitPathRef.current): GitDraft => {
    const content = editorRef.current?.getContent() || editorContentRef.current;
    const savedDecks = saveActiveDay(
      decksRef.current,
      content,
      answersRef.current,
      distractorsRef.current,
      activeDeckIdRef.current,
      activeDayIdRef.current,
    );

    return {
      decks: savedDecks,
      activeDeckId: activeDeckIdRef.current,
      activeDayId: activeDayIdRef.current,
      editorContent: content,
      answers: { ...answersRef.current },
      distractors: { ...distractorsRef.current },
      fileName: currentFileNameRef.current || targetPath,
    };
  };

  const rememberCurrentGitDraft = () => {
    const targetPath = selectedGitPathRef.current;
    if (!isGitFilePath(targetPath)) return;
    gitDraftsRef.current[targetPath] = captureCurrentGitDraft(targetPath);
  };

  const applyGitDraft = (draft: GitDraft) => {
    decksRef.current = draft.decks;
    activeDeckIdRef.current = draft.activeDeckId;
    activeDayIdRef.current = draft.activeDayId;
    editorContentRef.current = draft.editorContent;
    currentFileNameRef.current = draft.fileName;
    answersRef.current = { ...draft.answers };
    distractorsRef.current = { ...draft.distractors };

    setDecks(draft.decks);
    setActiveDeckId(draft.activeDeckId);
    setActiveDayId(draft.activeDayId);
    setCurrentFileName(draft.fileName);
    applyEditorContent(draft.editorContent);
    setAnswers({ ...draft.answers });
    setDistractors({ ...draft.distractors });
    setGitDraftClean();
  };

  const loadDay = (day: ManagerDay) => {
    applyEditorContent(day.content);
    answersRef.current = day.answers;
    distractorsRef.current = day.distractors;
    setAnswers(day.answers);
    setDistractors(day.distractors);
  };

  const handleEditorChange = (html: string) => {
    editorContentRef.current = html;
    setEditorContent(html);
    markGitDraftDirty();
    rememberCurrentGitDraft();
  };

  const handleSelectDay = (deckId: string, dayId: string) => {
    const savedDecks = saveActiveDay();
    const targetDeck = savedDecks.find(deck => deck.id === deckId);
    const targetDay = targetDeck?.days.find(day => day.id === dayId);

    decksRef.current = savedDecks;
    activeDeckIdRef.current = deckId;
    activeDayIdRef.current = dayId;
    setDecks(savedDecks);
    setActiveDeckId(deckId);
    setActiveDayId(dayId);

    if (targetDay) loadDay(targetDay);
  };

  const handleAddDeck = () => {
    const title = window.prompt('Deck 이름', `Deck ${decks.length + 1}`);
    if (!title) return;

    const day = createDay(1, 'Day 1');
    const deck: ManagerDeck = {
      id: makeId('deck', `${title}-${Date.now()}`),
      title,
      days: [day],
    };

    setDecks([...saveActiveDay(), deck]);
    setActiveDeckId(deck.id);
    setActiveDayId(day.id);
    loadDay(day);
  };

  const handleAddDay = () => {
    if (!activeDeck) return;

    const title = window.prompt('Day 이름', `Day ${activeDeck.days.length + 1}`);
    if (!title) return;

    const day = createDay(activeDeck.days.length + 1, title);
    const savedDecks = saveActiveDay();
    const nextDecks = savedDecks.map(deck =>
      deck.id === activeDeck.id ? { ...deck, days: [...deck.days, day] } : deck,
    );

    setDecks(nextDecks);
    setActiveDeckId(activeDeck.id);
    setActiveDayId(day.id);
    loadDay(day);
  };

  const placeTreeAtToolbar = () => {
    if (isTreeOpen && treeDock === 'left') {
      setIsTreeOpen(false);
      return;
    }
    setTreeDock('left');
    setIsTreeOpen(true);
  };

  const invokeTauri = async <T,>(command: string, args?: Record<string, unknown>) => {
    const invoke = window.__TAURI__?.core?.invoke;
    if (!invoke) {
      const response = await fetch('/__quiz-data/invoke', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ command, args: args || {} }),
      });
      const payload = await response.json();
      if (!response.ok) throw new Error(payload?.error || 'Dev Git API failed.');
      return payload.result as T;
    }
    return invoke<T>(command, args);
  };

  const loadGitTree = async () => {
    try {
      const entries = await invokeTauri<GitTreeEntry[]>('list_quiz_data_tree');
      setGitTreeEntries(entries);
      setGitTreeStatus('');
    } catch {
      setGitTreeEntries(fallbackGitTreeEntries);
      setGitTreeStatus('Tauri에서 실행하면 실제 Git 트리를 표시합니다.');
    }
  };

  const loadGitRepositories = async () => {
    try {
      const repositories = await invokeTauri<QuizDataRepository[]>('list_quiz_data_repositories');
      setGitRepositories(repositories);
    } catch {
      setGitRepositories([{
        name: 'quiz-data',
        path: 'C:\\aa\\build\\external\\quiz\\quiz-data',
        remote: 'https://github.com/pjuny0301/quiz-data.git',
        active: true,
      }]);
    }
  };

  const loadGitIdentity = async () => {
    try {
      const identity = await invokeTauri<GitIdentity>('get_git_identity');
      setGitIdentity(identity);
      setGitIdentityStatus('');
    } catch {
      setGitIdentityStatus('Tauri에서 실행하면 Git 계정을 관리할 수 있습니다.');
    }
  };

  const openRepositoryModal = async () => {
    setIsFileMenuOpen(false);
    setIsRepoModalOpen(true);
    await Promise.all([loadGitRepositories(), loadGitIdentity()]);
  };

  const saveGitIdentity = async () => {
    try {
      const message = await invokeTauri<string>('set_git_identity', {
        name: gitIdentity.name,
        email: gitIdentity.email,
      });
      setGitIdentityStatus(message);
    } catch {
      setGitIdentityStatus('Git 계정 저장 실패');
    }
  };

  const selectRepository = async (repository: QuizDataRepository) => {
    try {
      const message = await invokeTauri<string>('select_quiz_data_repository', { path: repository.path });
      setGitCommitStatus(message);
      setSelectedGitPathValue('');
      setGitTreeBasePath('');
      setIsRepoModalOpen(false);
      await loadGitRepositories();
      await loadGitTree();
    } catch {
      setGitCommitStatus('Git 레포 연결 실패');
    }
  };

  const loadJsonIntoManager = (content: string, fileName: string) => {
    const importedDecks = managerDecksFromJson(JSON.parse(content));
    const firstDeck = importedDecks[0];
    const firstDay = firstDeck?.days[0];

    if (!firstDeck || !firstDay) {
      alert('불러올 수 있는 퀴즈 JSON이 아닙니다.');
      return;
    }

    decksRef.current = importedDecks;
    activeDeckIdRef.current = firstDeck.id;
    activeDayIdRef.current = firstDay.id;
    currentFileNameRef.current = fileName;
    setDecks(importedDecks);
    setActiveDeckId(firstDeck.id);
    setActiveDayId(firstDay.id);
    setCurrentFileName(fileName);
    loadDay(firstDay);
    setGitDraftClean();
  };

  const loadTextIntoActiveDay = (html: string, fileName: string) => {
    applyEditorContent(html);
    currentFileNameRef.current = fileName;
    answersRef.current = {};
    distractorsRef.current = {};
    setCurrentFileName(fileName);
    setAnswers({});
    setDistractors({});
    const nextDecks = saveActiveDay(decksRef.current, html, {}, {});
    decksRef.current = nextDecks;
    setDecks(nextDecks);
    setGitDraftClean();
  };

  const activeDayFromDecks = (
    sourceDecks: ManagerDeck[],
    targetDeckId = activeDeckIdRef.current,
    targetDayId = activeDayIdRef.current,
  ) => {
    const targetDeck = sourceDecks.find(deck => deck.id === targetDeckId) || sourceDecks[0];
    return targetDeck?.days.find(day => day.id === targetDayId) || targetDeck?.days[0];
  };

  const buildDayJsonContent = (targetPath: string, sourceDecks: ManagerDeck[]) => {
    const day = activeDayFromDecks(sourceDecks);
    const fileTitle = fileNameFromPath(targetPath).replace(/\.json$/i, '') || 'Day 1';
    const parsed = day ? parseQuizzes(day.content, day.answers, day.distractors, day.questionMetadata) : [];

    return JSON.stringify({
      schemaVersion: 'quiz-deck-v1',
      source: 'bulk-quiz-manager',
      exportedAt: new Date().toISOString(),
      title: day?.title || fileTitle,
      questions: parsed.map(item => item.question),
      quizzes: parsed.map(item => item.raw),
    }, null, 2);
  };

  const createBlankGitContent = (relativePath: string) => {
    if (!relativePath.toLowerCase().endsWith('.json')) return '';

    return JSON.stringify({
      schemaVersion: 'quiz-deck-v1',
      source: 'bulk-quiz-manager',
      exportedAt: new Date().toISOString(),
      title: fileNameFromPath(relativePath).replace(/\.json$/i, '') || 'Day 1',
      questions: [],
      quizzes: [],
    }, null, 2);
  };

  const openBlankGitFileInEditor = (relativePath: string) => {
    const day = createDay(1, 'Day 1');
    const deckTitle = fileNameFromPath(relativePath).replace(/\.[^/.]+$/, '') || '새 퀴즈 덱';
    const deck: ManagerDeck = {
      id: makeId('deck', `${deckTitle}-${Date.now()}`),
      title: deckTitle,
      days: [day],
    };

    gitFileLoadSeqRef.current += 1;
    decksRef.current = [deck];
    activeDeckIdRef.current = deck.id;
    activeDayIdRef.current = day.id;
    editorContentRef.current = '';
    currentFileNameRef.current = relativePath;
    answersRef.current = {};
    distractorsRef.current = {};
    setDecks([deck]);
    setActiveDeckId(deck.id);
    setActiveDayId(day.id);
    setCurrentFileName(relativePath);
    setSelectedGitPathValue(relativePath);
    applyEditorContent('');
    setAnswers({});
    setDistractors({});
    gitDraftsRef.current[relativePath] = {
      decks: [deck],
      activeDeckId: deck.id,
      activeDayId: day.id,
      editorContent: '',
      answers: {},
      distractors: {},
      fileName: relativePath,
    };
    setGitDraftClean();
  };

  const saveCurrentGitDraftIfNeeded = async () => {
    const targetPath = selectedGitPathRef.current;
    if (isGitFilePath(targetPath)) rememberCurrentGitDraft();
    if (!targetPath || !hasUnsavedGitChangesRef.current) return true;
    return saveSelectedGitFile(targetPath, { silent: true });
  };

  const selectGitDirectory = async (path: string) => {
    const saved = await saveCurrentGitDraftIfNeeded();
    if (saved) setSelectedGitPathValue(path);
  };

  const loadGitFile = async (entry: GitTreeEntry) => {
    if (entry.kind !== 'file') return;

    const loadSeq = gitFileLoadSeqRef.current + 1;
    gitFileLoadSeqRef.current = loadSeq;
    const saved = await saveCurrentGitDraftIfNeeded();
    if (!saved || loadSeq !== gitFileLoadSeqRef.current) return;

    setSelectedGitPathValue(entry.path);
    const cachedDraft = gitDraftsRef.current[entry.path];
    if (cachedDraft) {
      applyGitDraft(cachedDraft);
      return;
    }

    try {
      const content = await invokeTauri<string>('read_quiz_data_file', { relativePath: entry.path });
      if (loadSeq !== gitFileLoadSeqRef.current) return;

      const lowerPath = entry.path.toLowerCase();

      if (lowerPath.endsWith('.json')) {
        loadJsonIntoManager(content, entry.path);
      } else if (lowerPath.endsWith('.html') || lowerPath.endsWith('.htm')) {
        const parser = new DOMParser();
        const doc = parser.parseFromString(content, 'text/html');
        loadTextIntoActiveDay(doc.body.innerHTML, entry.path);
      } else if (lowerPath.endsWith('.txt') || lowerPath.endsWith('.md')) {
        loadTextIntoActiveDay(textToEditorHtml(content), entry.path);
      }
    } catch {
      setGitTreeStatus('Tauri에서 실행하면 Git 파일을 열 수 있습니다.');
    }
  };

  const commitQuizData = async () => {
    try {
      const selectedEntry = gitTreeEntries.find(entry => entry.path === selectedGitPath);
      if (selectedEntry?.kind === 'file') {
        setGitCommitStatus('저장 후 커밋 중...');
        const saved = await saveSelectedGitFile(selectedGitPath);
        if (!saved) return;
      } else {
        setGitCommitStatus('커밋 중...');
      }
      const message = await invokeTauri<string>('commit_quiz_data', { message: gitCommitMessage });
      setGitCommitStatus(message);
      if (message.includes('커밋 완료') || message.includes('변경 사항이 없습니다')) {
        setGitDraftClean();
        setGitCommittedClean();
      }
      setIsCommitModalOpen(false);
      await loadGitTree();
    } catch {
      setGitCommitStatus('Tauri에서 실행하면 GUI 커밋을 사용할 수 있습니다.');
    }
  };

  const runGitAction = async (command: 'pull_quiz_data' | 'push_quiz_data') => {
    try {
      setGitCommitStatus(command === 'pull_quiz_data' ? 'Pull 중...' : 'Push 중...');
      const message = await invokeTauri<string>(command);
      setGitCommitStatus(message);
      await loadGitTree();
    } catch {
      setGitCommitStatus(command === 'pull_quiz_data' ? 'Pull 실패' : 'Push 실패');
    }
  };

  const pathUnderTreeBase = (value: string) => {
    const nextPath = sanitizeRelativePath(value);
    if (!nextPath) return '';
    if (!normalizedGitTreeBasePath) return nextPath;
    if (nextPath === normalizedGitTreeBasePath || nextPath.startsWith(`${normalizedGitTreeBasePath}/`)) return nextPath;
    return `${normalizedGitTreeBasePath}/${nextPath}`;
  };

  const createGitFile = async () => {
    const input = window.prompt('새 파일 상대경로', normalizedGitTreeBasePath ? 'day-1.md' : 'decks/new-deck/day-1.md');
    const relativePath = pathUnderTreeBase(input || '');
    if (!relativePath) return;

    try {
      const saved = await saveCurrentGitDraftIfNeeded();
      if (!saved) return;

      const message = await invokeTauri<string>('create_quiz_data_file', { relativePath });
      const blankContent = createBlankGitContent(relativePath);
      if (blankContent) {
        await invokeTauri<string>('write_quiz_data_file', { relativePath, content: blankContent });
      }
      setGitCommitStatus(message);
      openBlankGitFileInEditor(relativePath);
      await loadGitTree();
    } catch {
      setGitCommitStatus('파일 생성 실패');
    }
  };

  const createGitFolder = async () => {
    const input = window.prompt('새 폴더 상대경로', normalizedGitTreeBasePath ? 'new-day' : 'decks/new-deck');
    const relativePath = pathUnderTreeBase(input || '');
    if (!relativePath) return;

    try {
      const message = await invokeTauri<string>('create_quiz_data_folder', { relativePath });
      setGitCommitStatus(message);
      await loadGitTree();
    } catch {
      setGitCommitStatus('폴더 생성 실패');
    }
  };

  const editorHtmlToPlainText = (html: string) => {
    const div = document.createElement('div');
    div.innerHTML = html;
    return extractTextWithNewlines(div.innerHTML);
  };

  const saveSelectedGitFile = async (targetPath = selectedGitPath, options: { silent?: boolean } = {}) => {
    if (!targetPath) {
      setGitCommitStatus('저장할 Git 파일을 선택하세요.');
      return false;
    }

    if (gitTreeEntries.find(entry => entry.path === targetPath)?.kind === 'directory') {
      setGitCommitStatus('파일을 선택해야 저장할 수 있습니다.');
      return false;
    }

    const lowerPath = targetPath.toLowerCase();
    const savedDecks = saveActiveDay();
    const content = lowerPath.endsWith('.json')
      ? buildDayJsonContent(targetPath, savedDecks)
      : lowerPath.endsWith('.html') || lowerPath.endsWith('.htm')
        ? editorRef.current?.getContent() || editorContentRef.current
        : editorHtmlToPlainText(editorRef.current?.getContent() || editorContentRef.current);

    try {
      const message = await invokeTauri<string>('write_quiz_data_file', {
        relativePath: targetPath,
        content,
      });
      decksRef.current = savedDecks;
      setDecks(savedDecks);
      gitDraftsRef.current[targetPath] = captureCurrentGitDraft(targetPath);
      setGitDraftClean();
      markGitUncommitted();
      if (!options.silent) {
        setGitCommitStatus(message);
        await loadGitTree();
      }
      return true;
    } catch {
      setGitCommitStatus('저장 실패');
      return false;
    }
  };

  const deleteSelectedGitPath = async (targetPath = selectedGitPath) => {
    if (!targetPath) {
      setGitCommitStatus('삭제할 Git 항목을 선택하세요.');
      return;
    }

    if (!window.confirm(`${fileNameFromPath(targetPath)} 삭제`)) return;

    try {
      const message = await invokeTauri<string>('delete_quiz_data_path', { relativePath: targetPath });
      if (selectedGitPath === targetPath) setSelectedGitPathValue('');
      setGitCommitStatus(message);
      await loadGitTree();
    } catch {
      setGitCommitStatus('삭제 실패');
    }
  };

  const renameSelectedGitPath = async (targetPath = selectedGitPath) => {
    if (!targetPath) {
      setGitCommitStatus('이름을 바꿀 Git 항목을 선택하세요.');
      return;
    }

    const currentName = fileNameFromPath(targetPath);
    const input = window.prompt('새 이름 또는 상대경로', currentName);
    const sanitizedInput = sanitizeRelativePath(input || '');
    if (!sanitizedInput) return;

    const parentPath = targetPath.split('/').slice(0, -1).join('/');
    const nextPath = sanitizedInput.includes('/')
      ? pathUnderTreeBase(sanitizedInput)
      : [parentPath, sanitizedInput].filter(Boolean).join('/');

    if (!nextPath || nextPath === targetPath) return;

    try {
      const message = await invokeTauri<string>('rename_quiz_data_path', {
        fromRelativePath: targetPath,
        toRelativePath: nextPath,
      });
      if (selectedGitPath === targetPath) setSelectedGitPathValue(nextPath);
      setGitCommitStatus(message);
      await loadGitTree();
    } catch {
      setGitCommitStatus('이름 바꾸기 실패');
    }
  };

  const handleGitTreeContextMenu = (event: React.MouseEvent, entry?: GitTreeEntry) => {
    event.preventDefault();
    event.stopPropagation();

    setGitContextMenu({
      x: event.clientX,
      y: event.clientY,
      path: entry?.path || '',
      kind: entry?.kind || 'blank',
    });
  };

  const closeGitContextMenu = () => setGitContextMenu(null);

  const handleCommitDragStart = (event: React.MouseEvent<HTMLDivElement>) => {
    commitDragRef.current = {
      startX: event.clientX,
      startY: event.clientY,
      originX: commitModalPosition.x,
      originY: commitModalPosition.y,
    };
    event.preventDefault();
  };

  const handleCodexDragStart = (event: React.MouseEvent<HTMLDivElement>) => {
    codexDragRef.current = {
      startX: event.clientX,
      startY: event.clientY,
      originX: codexPosition.x,
      originY: codexPosition.y,
    };
    event.preventDefault();
  };

  const appendCodexMessage = (role: CodexChatMessage['role'], text: string) => {
    setCodexMessages(messages => [
      ...messages,
      { id: `${role}-${Date.now()}-${messages.length}`, role, text },
    ]);
  };

  const readCodexResponse = async () => {
    try {
      const content = await invokeTauri<string>('read_quiz_data_file', {
        relativePath: 'codex-requests/latest-response.json',
      });
      if (!content.trim() || content === lastCodexResponse) return;

      setLastCodexResponse(content);
      try {
        const payload = JSON.parse(content);
        const message = typeof payload.message === 'string'
          ? payload.message
          : typeof payload.reply === 'string'
            ? payload.reply
            : typeof payload.text === 'string'
              ? payload.text
              : content;
        handleCodexReply(message);
      } catch {
        handleCodexReply(content);
      }
    } catch {
      // The response file is optional; it appears only when an external Codex worker writes back.
    }
  };

  const stringRecordFromUnknown = (value: unknown) => {
    if (!isRecord(value)) return null;

    return Object.fromEntries(
      Object.entries(value)
        .map(([key, item]) => [Number(key), typeof item === 'string' ? item : String(item ?? '')] as const)
        .filter(([key]) => Number.isFinite(key)),
    ) as Record<number, string>;
  };

  const extractCodexEditorPayload = (message: string) => {
    const block = message.match(/```quiz-editor-json\s*([\s\S]*?)```/i);
    const jsonText = block?.[1]?.trim() || (message.trim().startsWith('{') ? message.trim() : '');
    if (!jsonText) return null;

    try {
      const payload = JSON.parse(jsonText);
      return isRecord(payload) ? payload : null;
    } catch {
      return null;
    }
  };

  const handleCodexReply = (message: string) => {
    const payload = extractCodexEditorPayload(message);
    if (!payload) {
      appendCodexMessage('codex', message);
      return;
    }

    const nextHtml = typeof payload.html === 'string' ? payload.html : editorRef.current?.getContent() || editorContentRef.current;
    const nextAnswers = stringRecordFromUnknown(payload.answers) || answersRef.current;
    const nextDistractors = stringRecordFromUnknown(payload.distractors) || distractorsRef.current;
    const nextDecks = saveActiveDay(decksRef.current, nextHtml, nextAnswers, nextDistractors);

    editorRef.current?.setContent(nextHtml);
    editorContentRef.current = nextHtml;
    answersRef.current = nextAnswers;
    distractorsRef.current = nextDistractors;
    decksRef.current = nextDecks;
    setEditorContent(nextHtml);
    setAnswers(nextAnswers);
    setDistractors(nextDistractors);
    setDecks(nextDecks);
    markGitDraftDirty();
    rememberCurrentGitDraft();

    const visibleMessage = typeof payload.message === 'string' && payload.message.trim()
      ? payload.message.trim()
      : '편집기에 반영했습니다.';
    appendCodexMessage('codex', visibleMessage);
  };

  const submitCodexPrompt = async () => {
    const prompt = codexPrompt.trim();
    if (!prompt) return;

    appendCodexMessage('user', prompt);
    setCodexPrompt('');
    setCodexStatus('Codex 응답 대기...');

    const currentHtml = editorRef.current?.getContent() || editorContentRef.current;
    const payload = JSON.stringify({
      requestedAt: new Date().toISOString(),
      deckId: activeDeckIdRef.current,
      dayId: activeDayIdRef.current,
      selectedGitPath,
      prompt,
      html: currentHtml,
      answers: answersRef.current,
    }, null, 2);

    try {
      const response = await invokeTauri<CodexRunResponse>('ask_codex', {
        prompt,
        context: payload,
      });
      setLastCodexResponse(response.responseJson);
      handleCodexReply(response.message);
      setCodexStatus('Codex 응답 완료');
      return;
    } catch {
      setCodexStatus('Codex 직접 호출 실패. 요청 파일로 전환합니다.');
    }

    try {
      await invokeTauri<string>('create_quiz_data_folder', { relativePath: 'codex-requests' });
      const message = await invokeTauri<string>('write_quiz_data_file', {
        relativePath: 'codex-requests/latest-request.json',
        content: payload,
      });
      setCodexStatus(message);
      appendCodexMessage('codex', '전달됨. 응답 대기.');
    } catch {
      const blob = new Blob([payload], { type: 'application/json;charset=utf-8' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'codex-quiz-request.json';
      document.body.appendChild(a);
      a.click();
      a.remove();
      window.setTimeout(() => URL.revokeObjectURL(url), 0);
      setCodexStatus('Codex 요청 파일을 내보냈습니다.');
      appendCodexMessage('codex', '요청 파일을 내보냈습니다.');
    }
  };

  const handleResizeStart = (target: ResizeTarget, event: React.MouseEvent<HTMLDivElement>) => {
    const rect = target.startsWith('tree') ? treePanelRef.current?.getBoundingClientRect() : quizPanelRef.current?.getBoundingClientRect();
    if (!rect) return;

    resizeRef.current = {
      target,
      startX: event.clientX,
      startY: event.clientY,
      width: rect.width,
      height: rect.height,
      originX: rect.left,
      originY: rect.top,
    };
    event.stopPropagation();
    event.preventDefault();
  };

  const handleTreeDragStart = (event: React.MouseEvent<HTMLDivElement>) => {
    const rect = treePanelRef.current?.getBoundingClientRect();
    if (!rect) return;

    treeDragRef.current = {
      startX: event.clientX,
      startY: event.clientY,
      originX: rect.left,
      originY: rect.top,
    };
    setTreeDock('floating');
    setTreePosition({ x: rect.left, y: rect.top });
    setDockPreview(null);
    event.preventDefault();
  };

  const handleQuizDragStart = (event: React.MouseEvent<HTMLDivElement>) => {
    const rect = quizPanelRef.current?.getBoundingClientRect();
    if (!rect) return;

    quizDragRef.current = {
      startX: event.clientX,
      startY: event.clientY,
      originX: rect.left,
      originY: rect.top,
    };
    setQuizDock('floating');
    setQuizPosition({ x: rect.left, y: rect.top });
    setDockPreview(null);
    event.preventDefault();
  };

  const handleOpenFile = () => {
    fileInputRef.current?.click();
    setIsFileMenuOpen(false);
  };

  const handleFileSelected = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;

    const pendingGitSave = saveCurrentGitDraftIfNeeded();
    const reader = new FileReader();
    reader.onload = async (event) => {
      const saved = await pendingGitSave;
      if (!saved) return;

      setSelectedGitPathValue('');
      const content = event.target?.result as string;
      const fileName = file.name.toLowerCase();
      let html = '';

      if (fileName.endsWith('.json')) {
        try {
          loadJsonIntoManager(content, file.name);
          return;
        } catch {
          alert('JSON 파일을 읽지 못했습니다.');
          return;
        }
      }

      if (fileName.endsWith('.html') || fileName.endsWith('.htm')) {
        const parser = new DOMParser();
        const doc = parser.parseFromString(content, 'text/html');
        html = doc.body.innerHTML;
      } else {
        html = textToEditorHtml(content);
      }

      applyEditorContent(html);
      currentFileNameRef.current = file.name;
      answersRef.current = {};
      distractorsRef.current = {};
      setCurrentFileName(file.name);
      setAnswers({});
      setDistractors({});
      const nextDecks = saveActiveDay(decksRef.current, html, {}, {});
      decksRef.current = nextDecks;
      setDecks(nextDecks);
    };

    reader.readAsText(file, 'UTF-8');
    e.target.value = '';
  };

  const handleAnswerChange = (id: number, answer: string) => {
    setAnswers(prev => {
      const next = { ...prev, [id]: answer };
      answersRef.current = next;
      markGitDraftDirty();
      rememberCurrentGitDraft();
      return next;
    });
  };

  const handleDistractorsChange = (id: number, value: string) => {
    setDistractors(prev => {
      const next = { ...prev, [id]: value };
      distractorsRef.current = next;
      markGitDraftDirty();
      rememberCurrentGitDraft();
      return next;
    });
  };

  const parseQuizzes = (
    html: string,
    dayAnswers: Record<number, string>,
    dayDistractors: Record<number, string>,
    questionMetadata: Record<number, QuestionContractMetadata> = {},
  ) => {
    const chunks = splitQuizHtml(html);

    const parsed = chunks
      .map((quizChunk, index) => {
        const chunk = quizChunk.html;
        const parser = new DOMParser();
        const doc = parser.parseFromString(chunk, 'text/html');
        const images = Array.from(doc.querySelectorAll('img')).map(img => img.getAttribute('src') || '');
        const textContent = extractTextWithNewlines(chunk);

        if (!textContent && images.length === 0) return null;

        const quizId = index + 1;
        const isLong = images.length > 0 || textContent.length > 80;
        const contractMetadata = questionMetadata[quizId] || {};

        return {
          raw: {
            id: quizId,
            text: textContent,
            images,
            answer: dayAnswers[quizId] || '',
            distractors: splitDistractors(dayDistractors[quizId] || ''),
            quizType: quizChunk.quizType,
            type: quizChunk.quizType === 'basic' || quizChunk.quizType === 'select'
              ? undefined
              : quizChunk.quizType,
            html: chunk.trim(),
            ...contractMetadata,
          },
          questionBase: {
            id: `q-${quizId}`,
            type: quizChunk.quizType === 'blank' || quizChunk.quizType === 'multi-answer' || quizChunk.quizType === 'multi-blank'
              ? quizChunk.quizType
              : isLong ? 'long' : 'short',
            questionText: isLong && quizChunk.quizType === 'basic' ? undefined : textContent,
            longText: isLong && quizChunk.quizType === 'basic' ? textContent : undefined,
            imageUrl: images[0] || undefined,
            selectionMode: quizChunk.quizType === 'select' ? 'select' : undefined,
            ...contractMetadata,
          },
        };
      })
      .filter(Boolean) as Array<{ raw: any; questionBase: any }>;

    const peerAnswers = parsed.map(item => item.raw.answer).filter(Boolean);

    return parsed.map(item => ({
      raw: item.raw,
      question: {
        ...item.questionBase,
        options: buildOptions(
          item.raw.answer,
          [...item.raw.distractors, ...peerAnswers],
          item.raw.id,
          item.raw.quizType === 'multi-answer' || item.raw.quizType === 'multi-blank',
        ),
      },
    }));
  };

  const focusAnswerInput = (mode: 'current' | 'firstMissing' = 'current') => {
    const currentHtml = editorRef.current?.getContent() || editorContentRef.current;
    const activeDay = activeDayFromDecks(decksRef.current);
    const quizzes = parseQuizzes(currentHtml, answersRef.current, distractorsRef.current, activeDay?.questionMetadata);
    const currentQuizNumber = editorRef.current?.getActiveQuizNumber() || 1;
    const targetQuiz = mode === 'current'
      ? quizzes.find(item => item.raw.id === currentQuizNumber) || quizzes[0]
      : quizzes.find(item => !answersRef.current[item.raw.id]?.trim()) || quizzes[0];

    if (!targetQuiz) return;

    editorRef.current?.saveSelection();
    setAnswerFocusRequest(prev => ({
      id: targetQuiz.raw.id,
      tick: (prev?.tick || 0) + 1,
    }));
  };

  const returnToEditor = () => {
    editorRef.current?.restoreSelection();
  };

  const insertDividerAndReturn = () => {
    editorRef.current?.focus();
    editorRef.current?.insertDivider();
  };

  const handleExportJson = () => {
    setIsFileMenuOpen(false);

    const savedDecks = saveActiveDay();
    decksRef.current = savedDecks;
    setDecks(savedDecks);

    const exportedDecks = savedDecks.map(deck => ({
      id: deck.id,
      title: deck.title,
      schemaVersion: deck.schemaVersion || 'quiz-deck-v1',
      sourceUri: deck.sourceUri,
      concepts: deck.concepts,
      days: deck.days.map(day => ({
        id: day.id,
        title: day.title,
        questions: parseQuizzes(day.content, day.answers, day.distractors, day.questionMetadata).map(item => item.question),
      })),
    }));
    const quizzes = savedDecks.flatMap(deck =>
      deck.days.flatMap(day => parseQuizzes(day.content, day.answers, day.distractors, day.questionMetadata).map(item => item.raw)),
    );

    if (quizzes.length === 0) {
      alert('내보낼 내용이 없습니다.');
      return;
    }

    const json = JSON.stringify({
      schemaVersion: 'quiz-deck-v1',
      source: 'bulk-quiz-manager',
      exportedAt: new Date().toISOString(),
      total: quizzes.length,
      decks: exportedDecks,
      quizzes,
    }, null, 2);
    const blob = new Blob([json], { type: 'application/json;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${currentFileName ? currentFileName.replace(/\.[^/.]+$/, '') : 'quiz-export'}.json`;
    document.body.appendChild(a);
    a.click();
    a.remove();
    window.setTimeout(() => URL.revokeObjectURL(url), 0);
  };

  useEffect(() => {
    decksRef.current = decks;
  }, [decks]);

  useEffect(() => {
    activeDeckIdRef.current = activeDeckId;
  }, [activeDeckId]);

  useEffect(() => {
    activeDayIdRef.current = activeDayId;
  }, [activeDayId]);

  useEffect(() => {
    editorContentRef.current = editorContent;
  }, [editorContent]);

  useEffect(() => {
    currentFileNameRef.current = currentFileName;
  }, [currentFileName]);

  useEffect(() => {
    answersRef.current = answers;
  }, [answers]);

  useEffect(() => {
    distractorsRef.current = distractors;
  }, [distractors]);

  useEffect(() => {
    selectedGitPathRef.current = selectedGitPath;
  }, [selectedGitPath]);

  useEffect(() => {
    hasUnsavedGitChangesRef.current = hasUnsavedGitChanges;
  }, [hasUnsavedGitChanges]);

  useEffect(() => {
    hasUncommittedGitChangesRef.current = hasUncommittedGitChanges;
  }, [hasUncommittedGitChanges]);

  useEffect(() => {
    const handleBeforeUnload = (event: BeforeUnloadEvent) => {
      if (!hasUnsavedGitChangesRef.current && !hasUncommittedGitChangesRef.current) return;

      event.preventDefault();
      event.returnValue = '변경사항을 Commit하시겠습니까?';
      return event.returnValue;
    };

    window.addEventListener('beforeunload', handleBeforeUnload);
    return () => window.removeEventListener('beforeunload', handleBeforeUnload);
  }, []);

  useEffect(() => {
    const handleClickOutside = (e: MouseEvent) => {
      if (!(e.target as Element).closest('.file-menu-container')) {
        setIsFileMenuOpen(false);
      }
      if (!(e.target as Element).closest('.git-context-menu')) {
        setGitContextMenu(null);
      }
    };
    document.addEventListener('click', handleClickOutside);
    return () => document.removeEventListener('click', handleClickOutside);
  }, []);

  useEffect(() => {
    if (!codexPromptOpen) return;

    void readCodexResponse();
    const timer = window.setInterval(() => {
      void readCodexResponse();
    }, 2500);

    return () => window.clearInterval(timer);
  }, [codexPromptOpen, lastCodexResponse]);

  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.ctrlKey && event.altKey && event.key.toLowerCase() === 'l') {
        event.preventDefault();
        setIsTreeOpen(open => !open);
      }

      if (event.ctrlKey && event.altKey && event.key.toLowerCase() === 'a') {
        event.preventDefault();
        focusAnswerInput('firstMissing');
      }

      if (event.ctrlKey && (event.key === "'" || event.code === 'Quote')) {
        event.preventDefault();
        focusAnswerInput('current');
      }

      const target = event.target as HTMLElement | null;
      const isTypingTarget =
        target?.isContentEditable ||
        target?.tagName === 'INPUT' ||
        target?.tagName === 'TEXTAREA';

      if (!isTypingTarget && !event.ctrlKey && !event.altKey && !event.metaKey && event.key.toLowerCase() === 'c') {
        const now = Date.now();
        if (now - lastCKeyRef.current <= 750) {
          event.preventDefault();
          setCodexPosition(position => clampCodexPosition(position));
          setCodexPromptOpen(true);
          setCodexStatus('');
        }
        lastCKeyRef.current = now;
      }

      if (event.key === 'Escape') {
        setIsTreeOpen(false);
        setGitContextMenu(null);
        setCodexPromptOpen(false);
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [focusAnswerInput]);

  useEffect(() => {
    const handleMouseMove = (event: MouseEvent) => {
      const commitDrag = commitDragRef.current;
      if (commitDrag) {
        const nextX = Math.min(Math.max(12, commitDrag.originX + event.clientX - commitDrag.startX), window.innerWidth - 340);
        const nextY = Math.min(Math.max(56, commitDrag.originY + event.clientY - commitDrag.startY), window.innerHeight - 180);
        setCommitModalPosition({ x: nextX, y: nextY });
        return;
      }

      const codexDrag = codexDragRef.current;
      if (codexDrag) {
        setCodexPosition(clampCodexPosition({
          x: codexDrag.originX + event.clientX - codexDrag.startX,
          y: codexDrag.originY + event.clientY - codexDrag.startY,
        }));
        return;
      }

      const resize = resizeRef.current;
      if (resize) {
        const deltaX = event.clientX - resize.startX;
        const deltaY = event.clientY - resize.startY;

        if (resize.target.startsWith('tree')) {
          const maxWidth = Math.min(720, window.innerWidth - 40);
          const maxHeight = Math.min(720, window.innerHeight - 80);
          let nextWidth = resize.width;
          let nextHeight = resize.height;
          let nextX = resize.originX;
          let nextY = resize.originY;

          if (resize.target === 'tree-right' || resize.target === 'tree-corner') {
            nextWidth = Math.min(Math.max(280, resize.width + deltaX), maxWidth);
          }
          if (resize.target === 'tree-bottom' || resize.target === 'tree-corner') {
            nextHeight = Math.min(Math.max(240, resize.height + deltaY), maxHeight);
          }
          if (resize.target === 'tree-left') {
            nextWidth = Math.min(Math.max(280, resize.width - deltaX), maxWidth);
            nextX = resize.originX + (resize.width - nextWidth);
          }
          if (resize.target === 'tree-top') {
            nextHeight = Math.min(Math.max(240, resize.height - deltaY), maxHeight);
            nextY = resize.originY + (resize.height - nextHeight);
          }

          setTreeDock('floating');
          setTreeSize({ width: nextWidth, height: nextHeight });
          setTreePosition({
            x: Math.min(Math.max(12, nextX), window.innerWidth - nextWidth - 12),
            y: Math.min(Math.max(58, nextY), window.innerHeight - nextHeight - 12),
          });
        } else if (resize.target === 'quiz-floating') {
          setQuizFloatingSize({
            width: Math.min(Math.max(320, resize.width + deltaX), Math.min(760, window.innerWidth - 40)),
            height: Math.min(Math.max(260, resize.height + deltaY), Math.min(720, window.innerHeight - 80)),
          });
        } else if (resize.target === 'quiz-right') {
          setQuizRightWidth(Math.min(Math.max(300, resize.width - deltaX), Math.min(720, window.innerWidth - 420)));
        } else {
          setQuizBottomHeight(Math.min(Math.max(220, resize.height - deltaY), Math.min(520, window.innerHeight - 220)));
        }

        return;
      }

      const treeDrag = treeDragRef.current;
      if (treeDrag) {
        const panelWidth = treePanelRef.current?.offsetWidth || treeSize.width;
        const panelHeight = treePanelRef.current?.offsetHeight || treeSize.height;
        const nextX = Math.min(Math.max(12, treeDrag.originX + event.clientX - treeDrag.startX), window.innerWidth - panelWidth - 12);
        const nextY = Math.min(Math.max(58, treeDrag.originY + event.clientY - treeDrag.startY), window.innerHeight - panelHeight - 12);
        const nearRight = nextX > window.innerWidth - panelWidth - 140;
        const nearBottom = nextY > window.innerHeight - panelHeight - 140;
        setTreePosition({ x: nextX, y: nextY });
        setDockPreview(nearBottom ? { target: 'tree', placement: 'bottom' } : nearRight ? { target: 'tree', placement: 'right' } : null);
        return;
      }

      const quizDrag = quizDragRef.current;
      if (quizDrag) {
        const panelWidth = quizPanelRef.current?.offsetWidth || quizFloatingSize.width;
        const panelHeight = quizPanelRef.current?.offsetHeight || quizFloatingSize.height;
        const nextX = Math.min(Math.max(12, quizDrag.originX + event.clientX - quizDrag.startX), window.innerWidth - panelWidth - 12);
        const nextY = Math.min(Math.max(58, quizDrag.originY + event.clientY - quizDrag.startY), window.innerHeight - panelHeight - 12);
        const nearRight = nextX > window.innerWidth - panelWidth - 140;
        const nearBottom = nextY > window.innerHeight - panelHeight - 140;
        setQuizPosition({ x: nextX, y: nextY });
        setDockPreview(nearBottom ? { target: 'quiz', placement: 'bottom' } : nearRight ? { target: 'quiz', placement: 'right' } : null);
      }
    };

    const handleMouseUp = () => {
      const treeDrag = treeDragRef.current;
      const quizDrag = quizDragRef.current;
      const resize = resizeRef.current;
      const commitDrag = commitDragRef.current;
      const codexDrag = codexDragRef.current;

      if (commitDrag) {
        commitDragRef.current = null;
        return;
      }

      if (codexDrag) {
        codexDragRef.current = null;
        return;
      }

      if (resize) {
        resizeRef.current = null;
        setDockPreview(null);
        return;
      }

      if (treeDrag) {
        treeDragRef.current = null;
        setTreePosition(position => {
          const panelWidth = treePanelRef.current?.offsetWidth || treeSize.width;
          const panelHeight = treePanelRef.current?.offsetHeight || treeSize.height;
          const nearRight = position.x > window.innerWidth - panelWidth - 140;
          const nearBottom = position.y > window.innerHeight - panelHeight - 140;

          if (nearBottom) {
            setTreeDock('bottom');
            setTreeSize(size => ({
              width: Math.min(Math.max(size.width, 560), Math.max(560, window.innerWidth - 40)),
              height: Math.min(Math.max(size.height, 240), 420),
            }));
            return { x: Math.max(20, Math.min(position.x, window.innerWidth - 580)), y: Math.max(58, window.innerHeight - panelHeight - 20) };
          }

          if (nearRight) {
            setTreeDock('right');
            return { x: Math.max(16, window.innerWidth - panelWidth - 20), y: 64 };
          }

          setTreeDock('floating');
          return {
            x: position.x,
            y: position.y,
          };
        });
        setDockPreview(null);
      }

      if (quizDrag) {
        quizDragRef.current = null;
        setQuizPosition(position => {
          const panelWidth = quizPanelRef.current?.offsetWidth || quizFloatingSize.width;
          const panelHeight = quizPanelRef.current?.offsetHeight || quizFloatingSize.height;
          const shouldDockBottom = position.y > window.innerHeight - panelHeight - 140;
          const shouldDockRight = position.x > window.innerWidth - panelWidth - 140;

          if (shouldDockBottom) {
            setQuizDock('bottom');
          } else if (shouldDockRight) {
            setQuizDock('right');
          }

          return position;
        });
        setDockPreview(null);
      }
    };

    window.addEventListener('mousemove', handleMouseMove);
    window.addEventListener('mouseup', handleMouseUp);
    return () => {
      window.removeEventListener('mousemove', handleMouseMove);
      window.removeEventListener('mouseup', handleMouseUp);
    };
  }, []);

  useEffect(() => {
    if (isTreeOpen) void loadGitTree();
  }, [isTreeOpen]);

  const normalizedGitTreeBasePath = sanitizeRelativePath(gitTreeBasePath);

  const visibleGitTreeEntries = gitTreeEntries.filter(entry => {
    const relativePath = normalizedGitTreeBasePath
      ? entry.path === normalizedGitTreeBasePath
        ? ''
        : entry.path.startsWith(`${normalizedGitTreeBasePath}/`)
          ? entry.path.slice(normalizedGitTreeBasePath.length + 1)
          : ''
      : entry.path;

    return Boolean(relativePath) && !relativePath.includes('/');
  });

  const getGitTreeDisplayPath = (path: string) => {
    if (!normalizedGitTreeBasePath) return path;
    if (path === normalizedGitTreeBasePath) return '.';
    return path.slice(normalizedGitTreeBasePath.length + 1);
  };

  const getDockPreviewStyle = (): React.CSSProperties => {
    if (!dockPreview) return {};

    if (dockPreview.target === 'quiz') {
      return dockPreview.placement === 'right'
        ? { top: 56, right: 0, bottom: 0, width: quizRightWidth }
        : { left: 16, right: 16, bottom: 16, height: quizBottomHeight };
    }

    if (dockPreview.placement === 'right') {
      const right = quizDock === 'right' ? quizRightWidth + 28 : 20;
      return { top: 64, right, bottom: 20, width: treeSize.width };
    }

    const previewWidth = Math.min(Math.max(treeSize.width, 560), window.innerWidth - 40);
    const left = Math.min(Math.max(20, treePosition.x), Math.max(20, window.innerWidth - previewWidth - 20));
    const bottom = quizDock === 'bottom' ? quizBottomHeight + 32 : 20;
    return { left, bottom, width: previewWidth, height: treeSize.height };
  };

  const getTreePanelStyle = (): React.CSSProperties => {
    if (treeDock === 'left') {
      return { left: 0, top: 48, bottom: 0, width: 300 };
    }

    if (treeDock === 'right') {
      const right = quizDock === 'right' ? quizRightWidth + 28 : 20;
      return { right, top: 64, bottom: 20, width: treeSize.width };
    }

    if (treeDock === 'bottom') {
      const width = Math.min(Math.max(treeSize.width, 560), window.innerWidth - 40);
      const left = Math.min(Math.max(20, treePosition.x), Math.max(20, window.innerWidth - width - 20));
      const bottom = quizDock === 'bottom' ? quizBottomHeight + 32 : 20;
      return { left, bottom, width, height: treeSize.height };
    }

    return { left: treePosition.x, top: treePosition.y, width: treeSize.width, height: treeSize.height };
  };

  const renderQuizPanel = (mode: QuizDock) => {
    const isFloating = mode === 'floating';
    const panelClass = isFloating
      ? 'fixed z-40 rounded-xl border border-slate-200 bg-white shadow-2xl overflow-hidden flex flex-col'
      : mode === 'right'
        ? 'relative min-w-[320px] flex flex-col bg-white'
        : 'relative min-h-[240px] flex flex-col bg-white border-t border-slate-200';
    const panelStyle = isFloating
      ? { left: quizPosition.x, top: quizPosition.y, width: quizFloatingSize.width, height: quizFloatingSize.height }
      : mode === 'right'
        ? { width: quizRightWidth }
        : { height: quizBottomHeight };

    return (
      <div
        ref={quizPanelRef}
        className={panelClass}
        style={panelStyle}
      >
        {mode === 'right' && (
          <div
            onMouseDown={(event) => handleResizeStart('quiz-right', event)}
            className="absolute left-0 top-0 z-10 h-full w-1 cursor-ew-resize hover:bg-blue-300/60"
            title="추출 결과 너비 조절"
          />
        )}
        {mode === 'bottom' && (
          <div
            onMouseDown={(event) => handleResizeStart('quiz-bottom', event)}
            className="absolute left-0 top-0 z-10 h-1 w-full cursor-ns-resize hover:bg-blue-300/60"
            title="추출 결과 높이 조절"
          />
        )}
        <div
          onMouseDown={handleQuizDragStart}
          className="h-9 bg-white border-b border-slate-100 flex items-center justify-between px-3 flex-shrink-0 cursor-move select-none"
        >
          <div className="flex items-center gap-2 text-slate-500">
            <GripVertical className="w-3.5 h-3.5 text-slate-300" />
            <ListChecks className="w-3.5 h-3.5" />
            <span className="text-xs font-medium tracking-tight">프리뷰</span>
          </div>
          <div className="flex items-center gap-0.5">
            <button
              onMouseDown={(event) => event.stopPropagation()}
              onClick={() => setQuizDock('right')}
              className={`w-7 h-7 rounded-md flex items-center justify-center transition-colors ${
                mode === 'right' ? 'bg-blue-50 text-blue-600' : 'text-slate-400 hover:bg-slate-100 hover:text-slate-700'
              }`}
              title="오른쪽에 고정"
            >
              <PanelRight className="w-3.5 h-3.5" />
            </button>
            <button
              onMouseDown={(event) => event.stopPropagation()}
              onClick={() => setQuizDock('bottom')}
              className={`w-7 h-7 rounded-md flex items-center justify-center transition-colors ${
                mode === 'bottom' ? 'bg-blue-50 text-blue-600' : 'text-slate-400 hover:bg-slate-100 hover:text-slate-700'
              }`}
              title="하단에 고정"
            >
              <PanelBottom className="w-3.5 h-3.5" />
            </button>
          </div>
        </div>

        <div className="flex-1 overflow-hidden">
          <PreviewPane
            html={editorContent}
            answers={answers}
            distractors={distractors}
            onAnswerChange={handleAnswerChange}
            onDistractorsChange={handleDistractorsChange}
            answerFocusRequest={answerFocusRequest}
            onReturnToEditor={returnToEditor}
            onInsertDividerAndReturn={insertDividerAndReturn}
            layout={mode === 'bottom' ? 'horizontal' : 'vertical'}
          />
        </div>
        {isFloating && (
          <div
            onMouseDown={(event) => handleResizeStart('quiz-floating', event)}
            className="absolute bottom-0 right-0 h-4 w-4 cursor-nwse-resize bg-transparent"
            title="추출 결과 크기 조절"
          />
        )}
      </div>
    );
  };

  return (
    <div className="flex flex-col h-screen bg-slate-50 font-sans overflow-hidden">
      <input
        ref={fileInputRef}
        type="file"
        accept=".html,.htm,.txt,.json,application/json"
        className="hidden"
        onChange={handleFileSelected}
      />

      <header className="h-12 bg-white border-b border-slate-200 flex items-center justify-between px-4 text-slate-800 z-20 flex-shrink-0">
        <div className="flex items-center gap-3 min-w-0">
          <div className="flex items-center gap-2 shrink-0">
            <div className="w-6 h-6 rounded-md bg-gradient-to-br from-blue-500 to-indigo-600 flex items-center justify-center text-white">
              <LayoutGrid className="w-3.5 h-3.5" />
            </div>
            <h1 className="text-sm font-semibold tracking-tight text-slate-900">Quiz Editor</h1>
          </div>
          {currentFileName && (
            <div className="flex items-center gap-1.5 min-w-0 pl-3 ml-1 border-l border-slate-200">
              <span className="text-xs font-medium text-slate-600 truncate max-w-[280px]" title={currentFileName}>
                {currentFileName}
              </span>
              {hasUnsavedGitChanges && (
                <span
                  className="w-1.5 h-1.5 rounded-full bg-amber-500 shrink-0"
                  title="저장되지 않은 변경 사항"
                  aria-label="저장되지 않은 변경 사항"
                />
              )}
            </div>
          )}
        </div>

        <div className="flex items-center gap-1">
          <button
            onClick={placeTreeAtToolbar}
            className={`flex items-center gap-1.5 px-2.5 py-1.5 rounded-md text-xs font-medium transition-colors ${
              isTreeOpen && treeDock === 'left'
                ? 'bg-blue-50 text-blue-700 hover:bg-blue-100'
                : 'text-slate-600 hover:bg-slate-100 hover:text-slate-900'
            }`}
            title="Git 트리 토글 (Ctrl+Alt+L)"
          >
            <Folder className="w-3.5 h-3.5" />
            Git 트리
          </button>

          <div className="relative file-menu-container">
            <button
              onClick={() => setIsFileMenuOpen(!isFileMenuOpen)}
              className="flex items-center justify-center w-8 h-8 rounded-md text-slate-500 hover:bg-slate-100 hover:text-slate-900 transition-colors"
              title="파일 메뉴"
            >
              <File className="w-4 h-4" />
            </button>

            {isFileMenuOpen && (
              <div className="absolute right-0 mt-1.5 w-48 bg-white rounded-lg shadow-lg ring-1 ring-slate-900/5 border border-slate-100 py-1 z-50 text-slate-700 font-medium">
                <button
                  onClick={handleOpenFile}
                  className="w-full text-left px-3 py-2 text-sm hover:bg-slate-50 flex items-center gap-2.5 transition-colors"
                >
                  <FolderOpen className="w-4 h-4 text-slate-400" />
                  파일 열기
                </button>
                <button
                  onClick={() => void openRepositoryModal()}
                  className="w-full text-left px-3 py-2 text-sm hover:bg-slate-50 flex items-center gap-2.5 transition-colors"
                >
                  <GitBranch className="w-4 h-4 text-slate-400" />
                  Git 레포 선택
                </button>
                <button
                  onClick={() => void openRepositoryModal()}
                  className="w-full text-left px-3 py-2 text-sm hover:bg-slate-50 flex items-center gap-2.5 transition-colors"
                >
                  <CheckCircle2 className="w-4 h-4 text-slate-400" />
                  Git 계정 관리
                </button>
                <div className="my-1 border-t border-slate-100" />
                <button
                  onClick={handleExportJson}
                  className="w-full text-left px-3 py-2 text-sm hover:bg-slate-50 flex items-center gap-2.5 transition-colors"
                >
                  <FileDown className="w-4 h-4 text-slate-400" />
                  JSON 내보내기
                </button>
              </div>
            )}
          </div>
        </div>
      </header>

      <main
        className={`flex-1 overflow-hidden ${quizDock === 'bottom' ? 'flex flex-col' : 'flex'} transition-[padding] duration-150`}
        style={isTreeOpen && treeDock === 'left' ? { paddingLeft: 300 } : undefined}
      >
        <div
          className={`flex-1 flex flex-col relative bg-white overflow-hidden min-w-0 ${quizDock === 'right' ? 'border-r border-slate-200' : quizDock === 'bottom' ? 'border-b border-slate-200' : ''}`}
        >
          <div className="h-9 bg-white border-b border-slate-100 flex items-center px-4 flex-shrink-0 justify-between gap-3">
            <div className="flex items-center gap-2 text-slate-500 shrink-0">
              <Type className="w-3.5 h-3.5" />
              <span className="text-xs font-medium tracking-tight">에디터</span>
            </div>
            <div className="flex items-center gap-1 overflow-x-auto">
              <button
                type="button"
                onClick={() => editorRef.current?.insertDivider()}
                className="flex items-center gap-1 px-2 py-0.5 rounded text-[11px] font-medium text-slate-600 hover:bg-slate-100 hover:text-slate-900 border border-slate-200 transition-colors"
                title="구분선 (Ctrl+Enter)"
              >
                <Plus className="w-3 h-3" />구분선
              </button>
              {([
                { type: 'select', label: 'Select', shortcut: '1', tone: 'text-blue-700 hover:bg-blue-50 border-blue-100' },
                { type: 'blank', label: 'Blank', shortcut: '2', tone: 'text-emerald-700 hover:bg-emerald-50 border-emerald-100' },
                { type: 'multi-answer', label: 'MultiAnswer', shortcut: '3', tone: 'text-violet-700 hover:bg-violet-50 border-violet-100' },
                { type: 'multi-blank', label: 'MultiBlank', shortcut: '4', tone: 'text-amber-700 hover:bg-amber-50 border-amber-100' },
              ] as const).map(({ type, label, shortcut, tone }) => (
                <button
                  key={type}
                  type="button"
                  onClick={() => editorRef.current?.insertDivider(type)}
                  className={`flex items-center gap-1 px-2 py-0.5 rounded text-[11px] font-medium border transition-colors ${tone}`}
                  title={`#${label} 삽입 (Ctrl+Shift+${shortcut})`}
                >
                  #{label}
                </button>
              ))}
            </div>
            <div className="hidden xl:flex items-center gap-1.5 text-[10px] text-slate-400 shrink-0">
              <kbd className="px-1.5 py-0.5 rounded bg-slate-100 text-slate-500 font-mono text-[10px] border border-slate-200">Ctrl</kbd>
              <span>+</span>
              <kbd className="px-1.5 py-0.5 rounded bg-slate-100 text-slate-500 font-mono text-[10px] border border-slate-200">V</kbd>
              <span className="ml-1">이미지</span>
            </div>
          </div>
          <div className="flex-1 overflow-hidden relative">
            <BulkEditor ref={editorRef} onChange={handleEditorChange} />
          </div>
        </div>

        {quizDock === 'right' && renderQuizPanel('right')}
        {quizDock === 'bottom' && renderQuizPanel('bottom')}
      </main>

      {quizDock === 'floating' && renderQuizPanel('floating')}

      {dockPreview && (
        <div
          className="fixed z-[45] pointer-events-none rounded-lg border-2 border-emerald-300/60 bg-emerald-200/10 shadow-[0_0_0_1px_rgba(16,185,129,0.08)]"
          style={getDockPreviewStyle()}
        />
      )}

      {isTreeOpen && (
        <div
          ref={treePanelRef}
          className={`fixed z-50 rounded-lg border border-slate-200 bg-white overflow-hidden ${
            treeDock === 'floating' ? 'shadow-2xl' : ''
          }`}
          style={getTreePanelStyle()}
          onContextMenu={(event) => handleGitTreeContextMenu(event)}
        >
          <div
            onMouseDown={handleTreeDragStart}
            className="h-9 bg-gray-50 border-b border-gray-200 flex items-center justify-between px-2 cursor-move select-none"
          >
            <div className="flex items-center gap-2 text-xs font-semibold text-gray-700">
              <GripVertical className="w-4 h-4 text-gray-400" />
              <Folder className="w-4 h-4 text-blue-500" />
              Git 트리
            </div>
            <div className="flex items-center gap-1">
              <button
                onMouseDown={(event) => event.stopPropagation()}
                onClick={loadGitTree}
                className="w-6 h-6 rounded hover:bg-blue-50 text-gray-500 hover:text-blue-600 flex items-center justify-center"
                title="새로고침"
              >
                <RefreshCw className="w-3.5 h-3.5" />
              </button>
              <button
                onMouseDown={(event) => event.stopPropagation()}
                onClick={() => setIsTreeOpen(false)}
                className="w-6 h-6 rounded hover:bg-red-50 text-gray-500 hover:text-red-600 flex items-center justify-center"
                title="닫기"
              >
                <X className="w-3.5 h-3.5" />
              </button>
            </div>
          </div>
          <div className="h-[calc(100%-2.25rem)] overflow-hidden p-2 text-xs bg-white flex flex-col gap-2">
            <input
              value={gitTreeBasePath}
              onChange={(event) => setGitTreeBasePath(sanitizeRelativePath(event.target.value))}
              placeholder="상대경로 기준"
              className="w-full rounded border border-gray-200 px-2 py-1.5 text-xs text-gray-700 outline-none focus:border-blue-300 focus:ring-1 focus:ring-blue-200"
            />
            <div className="grid grid-cols-3 gap-1">
              <button
                onClick={loadGitTree}
                className="rounded border border-gray-200 px-2 py-1.5 text-xs font-semibold text-gray-600 hover:bg-gray-50 flex items-center justify-center gap-1"
              >
                <RefreshCw className="w-3.5 h-3.5" />
                새로고침
              </button>
              <button
                onClick={() => setIsCommitModalOpen(true)}
                className="shrink-0 rounded bg-blue-600 px-2.5 py-1.5 text-xs font-semibold text-white hover:bg-blue-700"
              >
                Commit
              </button>
              <button
                onClick={() => runGitAction('push_quiz_data')}
                className="rounded border border-gray-200 px-2 py-1.5 text-xs font-semibold text-gray-600 hover:bg-gray-50 flex items-center justify-center gap-1"
              >
                <Upload className="w-3.5 h-3.5" />
                Push
              </button>
            </div>
            {gitCommitStatus && (
              <div className="rounded bg-blue-50 px-2 py-1.5 text-[11px] text-blue-700">
                {gitCommitStatus}
              </div>
            )}
            {gitTreeStatus && (
              <div className="rounded bg-amber-50 px-2 py-1.5 text-[11px] text-amber-700">
                {gitTreeStatus}
              </div>
            )}
            <div className="min-h-0 flex-1 overflow-y-auto">
              {visibleGitTreeEntries.length === 0 ? (
                <div className="px-2 py-4 text-center text-gray-400">표시할 파일이 없습니다.</div>
              ) : (
                visibleGitTreeEntries.map(entry => {
                  const displayPath = getGitTreeDisplayPath(entry.path);

                  return (
                    <button
                      key={`${entry.kind}:${entry.path}`}
                      onContextMenu={(event) => handleGitTreeContextMenu(event, entry)}
                      onClick={() => {
                        if (entry.kind === 'file') {
                          void loadGitFile(entry);
                        } else {
                          void selectGitDirectory(entry.path);
                        }
                      }}
                      onDoubleClick={() => {
                        if (entry.kind === 'directory') {
                          setGitTreeBasePath(entry.path);
                          void selectGitDirectory(entry.path);
                        }
                      }}
                      className={`w-full flex items-center gap-2 px-2 py-1.5 rounded text-left ${
                        selectedGitPath === entry.path
                          ? 'bg-blue-600 text-white'
                        : entry.kind === 'directory'
                            ? 'text-gray-600 hover:bg-gray-100'
                            : 'text-gray-500 hover:bg-gray-100'
                      }`}
                      title={displayPath}
                    >
                      {entry.kind === 'directory' ? (
                        <Folder className="w-3.5 h-3.5 shrink-0" />
                      ) : (
                        <FileText className="w-3.5 h-3.5 shrink-0" />
                      )}
                      <span className="truncate">{displayPath}</span>
                    </button>
                  );
                })
              )}
            </div>
          </div>
          <div
            onMouseDown={(event) => handleResizeStart('tree-top', event)}
            className="absolute left-0 top-0 h-1 w-full cursor-ns-resize hover:bg-blue-200"
            title="Git 트리 위쪽 크기 조절"
          />
          <div
            onMouseDown={(event) => handleResizeStart('tree-bottom', event)}
            className="absolute bottom-0 left-0 h-1 w-full cursor-ns-resize hover:bg-blue-200"
            title="Git 트리 아래쪽 크기 조절"
          />
          <div
            onMouseDown={(event) => handleResizeStart('tree-left', event)}
            className="absolute left-0 top-0 h-full w-1 cursor-ew-resize hover:bg-blue-200"
            title="Git 트리 왼쪽 크기 조절"
          />
          <div
            onMouseDown={(event) => handleResizeStart('tree-right', event)}
            className="absolute right-0 top-0 h-full w-1 cursor-ew-resize hover:bg-blue-200"
            title="Git 트리 오른쪽 크기 조절"
          />
          <div
            onMouseDown={(event) => handleResizeStart('tree-corner', event)}
            className="absolute bottom-0 right-0 h-4 w-4 cursor-nwse-resize bg-transparent"
            title="Git 트리 크기 조절"
          />
        </div>
      )}

      {gitContextMenu && (
        <div
          className="git-context-menu fixed z-[70] min-w-40 rounded-md border border-gray-200 bg-white py-1 text-xs text-gray-700 shadow-xl"
          style={{ left: gitContextMenu.x, top: gitContextMenu.y }}
        >
          {gitContextMenu.kind === 'blank' ? (
            <>
              <button
                onClick={() => {
                  closeGitContextMenu();
                  void createGitFile();
                }}
                className="flex w-full items-center gap-2 px-3 py-2 text-left hover:bg-gray-50"
              >
                <FilePlus className="w-3.5 h-3.5 text-blue-500" />
                새 파일
              </button>
              <button
                onClick={() => {
                  closeGitContextMenu();
                  void createGitFolder();
                }}
                className="flex w-full items-center gap-2 px-3 py-2 text-left hover:bg-gray-50"
              >
                <FolderPlus className="w-3.5 h-3.5 text-blue-500" />
                새 폴더
              </button>
            </>
          ) : (
            <>
              <button
                onClick={() => {
                  closeGitContextMenu();
                  void renameSelectedGitPath(gitContextMenu.path);
                }}
                className="flex w-full items-center gap-2 px-3 py-2 text-left hover:bg-gray-50"
              >
                <Pencil className="w-3.5 h-3.5 text-slate-500" />
                이름 바꾸기
              </button>
              <button
                onClick={() => {
                  closeGitContextMenu();
                  void deleteSelectedGitPath(gitContextMenu.path);
                }}
                className="flex w-full items-center gap-2 px-3 py-2 text-left text-red-600 hover:bg-red-50"
              >
                <Trash2 className="w-3.5 h-3.5" />
                삭제
              </button>
            </>
          )}
        </div>
      )}

      {isRepoModalOpen && (
        <RepositoryModal
          repositories={gitRepositories}
          gitIdentity={gitIdentity}
          gitIdentityStatus={gitIdentityStatus}
          onClose={() => setIsRepoModalOpen(false)}
          onSelectRepository={(repository) => void selectRepository(repository)}
          onGitIdentityChange={setGitIdentity}
          onSaveGitIdentity={() => void saveGitIdentity()}
        />
      )}

      {isCommitModalOpen && (
        <CommitModal
          position={commitModalPosition}
          message={gitCommitMessage}
          onMessageChange={setGitCommitMessage}
          onCommit={() => void commitQuizData()}
          onClose={() => setIsCommitModalOpen(false)}
          onDragStart={handleCommitDragStart}
        />
      )}

      {codexPromptOpen && (
        <CodexPromptPanel
          position={codexPosition}
          messages={codexMessages}
          prompt={codexPrompt}
          status={codexStatus}
          onPromptChange={setCodexPrompt}
          onSubmit={() => void submitCodexPrompt()}
          onClose={() => setCodexPromptOpen(false)}
          onDragStart={handleCodexDragStart}
        />
      )}
    </div>
  );
}
