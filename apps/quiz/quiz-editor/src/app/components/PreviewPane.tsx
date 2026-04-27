import React, { useEffect, useMemo, useRef, useState } from 'react';
import { FileText, Image as ImageIcon, CheckCircle, Plus, X, Check } from 'lucide-react';

export interface AnswerFocusRequest {
  id: number;
  tick: number;
}

interface PreviewPaneProps {
  html: string;
  answers: Record<number, string>;
  distractors?: Record<number, string>;
  onAnswerChange: (id: number, answer: string) => void;
  onDistractorsChange?: (id: number, value: string) => void;
  answerFocusRequest?: AnswerFocusRequest | null;
  onReturnToEditor?: () => void;
  onInsertDividerAndReturn?: () => void;
  layout?: 'vertical' | 'horizontal';
}

export type QuizKind = 'basic' | 'select' | 'blank' | 'multi-answer' | 'multi-blank';

interface QuizData {
  id: number;
  textContent: string;
  imageCount: number;
  preview: string;
  quizType: QuizKind;
}

export const extractTextWithNewlines = (htmlString: string) => {
  const div = document.createElement('div');
  div.innerHTML = htmlString;
  
  const brs = div.querySelectorAll('br');
  brs.forEach(br => br.replaceWith('\n'));
  
  const blocks = div.querySelectorAll('div, p, li');
  blocks.forEach(block => {
    block.prepend('\n');
  });
  
  return (div.textContent || '').trim().replace(/\n{3,}/g, '\n\n');
};

export const QUIZ_DIVIDER_SPLIT_REGEX =
  /<div[^>]*data-quiz-divider="true"[^>]*>[\s\S]*?<\/div>|<hr[^>]*class="[^"]*quiz-divider[^"]*"[^>]*>/i;

const readQuizTypeFromDivider = (dividerHtml: string): QuizKind => {
  const typeMatch = dividerHtml.match(/data-quiz-type="([^"]+)"/i);
  const type = typeMatch?.[1]?.toLowerCase();
  if (type === 'select' || type === 'blank' || type === 'multi-answer' || type === 'multi-blank') return type;
  if (/#Select/i.test(dividerHtml)) return 'select';
  if (/#Blank/i.test(dividerHtml)) return 'blank';
  if (/#MultiAnswer/i.test(dividerHtml)) return 'multi-answer';
  if (/#MultiBlank/i.test(dividerHtml)) return 'multi-blank';
  return 'basic';
};

const quizTypeLabel: Record<Exclude<QuizKind, 'basic'>, string> = {
  select: '#Select',
  blank: '#Blank',
  'multi-answer': '#MultiAnswer',
  'multi-blank': '#MultiBlank',
};

const quizTypeBadgeClass: Record<Exclude<QuizKind, 'basic'>, string> = {
  select: 'text-blue-700 bg-blue-50',
  blank: 'text-emerald-700 bg-emerald-50',
  'multi-answer': 'text-violet-700 bg-violet-50',
  'multi-blank': 'text-amber-700 bg-amber-50',
};

export const splitQuizHtml = (htmlString: string) => {
  const dividerRegex = /(<div[^>]*data-quiz-divider="true"[^>]*>[\s\S]*?<\/div>|<hr[^>]*class="[^"]*quiz-divider[^"]*"[^>]*>)/gi;
  const chunks: Array<{ html: string; quizType: QuizKind }> = [];
  let cursor = 0;
  let pendingType: QuizKind | null = null;
  let match: RegExpExecArray | null;

  while ((match = dividerRegex.exec(htmlString)) !== null) {
    if (pendingType !== null) {
      chunks.push({ html: htmlString.slice(cursor, match.index), quizType: pendingType });
    }
    pendingType = readQuizTypeFromDivider(match[0]);
    cursor = match.index + match[0].length;
  }

  if (pendingType !== null) {
    chunks.push({ html: htmlString.slice(cursor), quizType: pendingType });
  } else {
    const leftover = htmlString.slice(cursor);
    if (leftover.trim()) chunks.push({ html: leftover, quizType: 'basic' });
  }

  return chunks;
};

export const PreviewPane: React.FC<PreviewPaneProps> = ({
  html,
  answers,
  distractors = {},
  onAnswerChange,
  onDistractorsChange,
  answerFocusRequest,
  onReturnToEditor,
  onInsertDividerAndReturn,
  layout = 'vertical',
}) => {
  const answerRefs = useRef<Record<number, HTMLInputElement | null>>({});
  const lastAnswerEnterRef = useRef<Record<number, number>>({});
  const answerReturnTimersRef = useRef<Record<number, number>>({});
  const ctrlTabRef = useRef(0);
  const lastSelectionEnterRef = useRef(0);
  const [activeQuizId, setActiveQuizId] = useState<number | null>(null);
  const [selectionEditorId, setSelectionEditorId] = useState<number | null>(null);
  const [selectionDraft, setSelectionDraft] = useState('');
  const [chipDrafts, setChipDrafts] = useState<Record<number, string>>({});

  const quizzes = useMemo(() => {
    if (!html.trim()) return [];

    const chunks = splitQuizHtml(html);
    
    const parsedQuizzes: QuizData[] = [];
    
    chunks.forEach((quizChunk, index) => {
      const chunk = quizChunk.html;
      const parser = new DOMParser();
      const doc = parser.parseFromString(chunk, 'text/html');
      
      const images = doc.querySelectorAll('img');
      const textContent = extractTextWithNewlines(chunk);
      
      if (textContent || images.length > 0) {
        parsedQuizzes.push({
          id: index + 1,
          textContent: textContent,
          imageCount: images.length,
          preview: textContent.slice(0, 100),
          quizType: quizChunk.quizType,
        });
      }
    });

    return parsedQuizzes;
  }, [html]);

  const totalImages = quizzes.reduce((sum, q) => sum + q.imageCount, 0);

  useEffect(() => {
    if (!answerFocusRequest) return;

    window.requestAnimationFrame(() => {
      const input = answerRefs.current[answerFocusRequest.id];
      input?.scrollIntoView({ block: 'center', inline: 'nearest', behavior: 'smooth' });
      input?.focus();
      input?.select();
    });
  }, [answerFocusRequest]);

  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.ctrlKey && event.key.toLowerCase() === 'i' && activeQuizId) {
        const quiz = quizzes.find(item => item.id === activeQuizId);
        if (quiz?.quizType !== 'select') return;

        event.preventDefault();
        setSelectionDraft(formatSelectionDraft(distractors[activeQuizId] || ''));
        setSelectionEditorId(activeQuizId);
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [activeQuizId, distractors, quizzes]);

  const formatSelectionDraft = (value: string) => {
    const lines = value
      .split(/\r?\n|,/)
      .map(line => line.replace(/^#\d+\s*/, '').trim())
      .filter(Boolean);

    return lines.length > 0 ? lines.map((line, index) => `#${index + 1} ${line}`).join('\n') : '#1 ';
  };

  const parseSelectionDraft = (value: string) =>
    value
      .split(/\r?\n/)
      .map(line => line.replace(/^#\d+\s*/, '').trim())
      .filter(Boolean)
      .join('\n');

  const openSelectionEditor = (quizId: number) => {
    setActiveQuizId(quizId);
    setSelectionDraft(formatSelectionDraft(distractors[quizId] || ''));
    setSelectionEditorId(quizId);
  };

  const commitSelectionDraft = () => {
    if (!selectionEditorId) return;

    onDistractorsChange?.(selectionEditorId, parseSelectionDraft(selectionDraft));
    setSelectionEditorId(null);
    lastSelectionEnterRef.current = 0;
  };

  const focusAnswerByOffset = (quizId: number, offset: number) => {
    const currentIndex = quizzes.findIndex(quiz => quiz.id === quizId);
    if (currentIndex < 0) return;

    const nextQuiz = quizzes[currentIndex + offset];
    if (!nextQuiz) return;

    setActiveQuizId(nextQuiz.id);
    answerRefs.current[nextQuiz.id]?.focus();
    answerRefs.current[nextQuiz.id]?.select();
  };

  const selectOptionRefs = useRef<Record<string, HTMLInputElement | null>>({});
  const pendingFocusRef = useRef<string | null>(null);

  useEffect(() => {
    if (!pendingFocusRef.current) return;
    const node = selectOptionRefs.current[pendingFocusRef.current];
    pendingFocusRef.current = null;
    node?.focus();
    node?.select();
  });

  const parseAnswerList = (raw: string) =>
    raw.split(/\r?\n|,/).map(s => s.trim()).filter(Boolean);

  const renderChipsInput = (quiz: QuizData) => {
    const raw = answers[quiz.id] || '';
    const chips = parseAnswerList(raw);
    const isMultiBlank = quiz.quizType === 'multi-blank';
    const draftKey = `chip-draft:${quiz.id}`;
    const draft = chipDrafts[quiz.id] ?? '';

    const commit = (newChips: string[]) => {
      onAnswerChange(quiz.id, newChips.join('\n'));
    };

    const flushDraft = () => {
      const trimmed = draft.trim();
      if (!trimmed) return false;
      commit([...chips, trimmed]);
      setChipDrafts(prev => ({ ...prev, [quiz.id]: '' }));
      return true;
    };

    return (
      <div className="flex items-start gap-2 w-full">
        <span className="font-semibold text-slate-600 whitespace-nowrap pt-1.5">정답</span>
        <div
          className="flex-1 min-w-0 flex flex-wrap items-center gap-1 px-2 py-1 border border-slate-200 rounded-md bg-white min-h-[34px] focus-within:border-blue-400 focus-within:ring-1 focus-within:ring-blue-200 cursor-text"
          onClick={(e) => {
            if ((e.target as HTMLElement).closest('[data-chip]')) return;
            const input = (e.currentTarget.querySelector('input[data-chip-input]') as HTMLInputElement | null);
            input?.focus();
          }}
        >
          {chips.map((chip, i) => (
            <span
              key={`${i}-${chip}`}
              data-chip
              className="inline-flex items-center gap-1 px-2 py-0.5 rounded-full bg-emerald-50 text-emerald-700 text-xs font-medium border border-emerald-200"
            >
              {isMultiBlank && <span className="text-emerald-500 font-bold">{i + 1}.</span>}
              <span className="truncate max-w-[160px]">{chip}</span>
              <button
                type="button"
                onClick={() => commit(chips.filter((_, j) => j !== i))}
                className="text-emerald-400 hover:text-emerald-700"
                title="삭제"
              >
                <X className="w-3 h-3" />
              </button>
            </span>
          ))}
          <input
            ref={(node) => { answerRefs.current[quiz.id] = node; }}
            type="text"
            data-chip-input
            value={draft}
            placeholder={chips.length === 0 ? (isMultiBlank ? '빈칸 순서대로 입력 → Enter' : '정답 입력 → Enter') : ''}
            onFocus={() => setActiveQuizId(quiz.id)}
            onChange={(e) => setChipDrafts(prev => ({ ...prev, [quiz.id]: e.target.value }))}
            onKeyDown={(e) => {
              if (e.ctrlKey && (e.key === "'" || e.code === 'Quote')) {
                e.preventDefault();
                flushDraft();
                focusAnswerByOffset(quiz.id, 1);
                return;
              }
              if (e.key === 'Enter' && !e.nativeEvent.isComposing) {
                e.preventDefault();
                if (e.ctrlKey) {
                  flushDraft();
                  onInsertDividerAndReturn?.();
                  return;
                }
                if (!flushDraft()) {
                  onReturnToEditor?.();
                }
                return;
              }
              if (e.key === ',' && !e.nativeEvent.isComposing) {
                e.preventDefault();
                flushDraft();
                return;
              }
              if (e.key === 'Tab' && !e.shiftKey) {
                e.preventDefault();
                flushDraft();
                focusAnswerByOffset(quiz.id, 1);
                return;
              }
              if (e.key === 'Backspace' && draft === '' && chips.length > 0) {
                e.preventDefault();
                commit(chips.slice(0, -1));
              }
            }}
            onBlur={() => { void draftKey; flushDraft(); }}
            className="flex-1 min-w-[80px] outline-none text-sm text-slate-800 bg-transparent"
          />
        </div>
      </div>
    );
  };

  const blankPattern = /_{3,}/g;

  const renderInlineBlanks = (quiz: QuizData) => {
    const text = quiz.textContent;
    const segments: Array<{ type: 'text'; value: string } | { type: 'blank'; index: number }> = [];
    let lastIndex = 0;
    let blankIndex = 0;
    let match: RegExpExecArray | null;
    blankPattern.lastIndex = 0;
    while ((match = blankPattern.exec(text)) !== null) {
      if (match.index > lastIndex) {
        segments.push({ type: 'text', value: text.slice(lastIndex, match.index) });
      }
      segments.push({ type: 'blank', index: blankIndex });
      blankIndex += 1;
      lastIndex = match.index + match[0].length;
    }
    if (lastIndex < text.length) {
      segments.push({ type: 'text', value: text.slice(lastIndex) });
    }

    const fills = parseAnswerList(answers[quiz.id] || '');
    const blankCount = blankIndex;

    const commitFill = (i: number, value: string) => {
      const next = Array.from({ length: Math.max(blankCount, fills.length) }, (_, k) => fills[k] || '');
      next[i] = value;
      // Trim trailing empties
      while (next.length > blankCount && !next[next.length - 1]) next.pop();
      onAnswerChange(quiz.id, next.join('\n'));
    };

    return (
      <div className="flex flex-col gap-1.5">
        <p className="text-sm text-slate-700 leading-relaxed whitespace-pre-line">
          {segments.map((seg, i) =>
            seg.type === 'text' ? (
              <span key={i}>{seg.value}</span>
            ) : (
              <input
                key={i}
                ref={(node) => { if (seg.index === 0) answerRefs.current[quiz.id] = node; }}
                type="text"
                value={fills[seg.index] || ''}
                placeholder={`①②③④⑤⑥⑦⑧⑨`[seg.index] || `${seg.index + 1}`}
                onFocus={() => setActiveQuizId(quiz.id)}
                onChange={(e) => commitFill(seg.index, e.target.value)}
                onKeyDown={(e) => {
                  if (e.ctrlKey && (e.key === "'" || e.code === 'Quote')) {
                    e.preventDefault();
                    focusAnswerByOffset(quiz.id, 1);
                    return;
                  }
                  if (e.key === 'Tab' && !e.shiftKey) {
                    e.preventDefault();
                    const inputs = (e.currentTarget.parentElement?.querySelectorAll('input') || []) as NodeListOf<HTMLInputElement>;
                    const idx = Array.from(inputs).indexOf(e.currentTarget);
                    if (idx >= 0 && idx + 1 < inputs.length) {
                      inputs[idx + 1].focus();
                    } else {
                      focusAnswerByOffset(quiz.id, 1);
                    }
                    return;
                  }
                  if (e.ctrlKey && e.key === 'Enter' && !e.nativeEvent.isComposing) {
                    e.preventDefault();
                    onInsertDividerAndReturn?.();
                    return;
                  }
                  if (e.key === 'Enter' && !e.nativeEvent.isComposing) {
                    e.preventDefault();
                    onReturnToEditor?.();
                  }
                }}
                className="inline-block min-w-[60px] max-w-[180px] mx-0.5 px-1.5 py-0 align-baseline border-b-2 border-emerald-300 bg-emerald-50/40 text-sm text-emerald-800 font-semibold outline-none focus:border-emerald-500 focus:bg-emerald-50"
                style={{ width: `${Math.max(
                  60,
                  Array.from(fills[seg.index] || '').reduce((sum, ch) => sum + (/[\u3000-\u9fff\uac00-\ud7af]/.test(ch) ? 14 : 7), 0) + 20
                )}px` }}
              />
            )
          )}
        </p>
      </div>
    );
  };

  const renderSelectOptions = (quiz: QuizData) => {
    const optionsRaw = distractors[quiz.id] || '';
    const parsedOptions = optionsRaw
      .split(/\r?\n/)
      .map(s => s.replace(/^#\d+\s*/, '').trim())
      .filter(Boolean);
    const currentAnswer = answers[quiz.id] || '';
    const options = parsedOptions.length === 0
      ? (currentAnswer ? [currentAnswer] : [''])
      : parsedOptions;
    let correctIndex = options.findIndex(o => o === currentAnswer);
    if (correctIndex < 0) correctIndex = 0;

    const commit = (newOptions: string[], newCorrect: number) => {
      const safeCorrect = Math.max(0, Math.min(newCorrect, newOptions.length - 1));
      onDistractorsChange?.(quiz.id, newOptions.join('\n'));
      onAnswerChange(quiz.id, newOptions[safeCorrect] ?? '');
    };

    return (
      <div className="flex flex-col gap-1">
        {options.map((opt, i) => {
          const refKey = `${quiz.id}:${i}`;
          const isCorrect = i === correctIndex;
          return (
            <div key={i} className="flex items-center gap-1.5">
              <button
                type="button"
                onClick={() => commit(options, i)}
                title={isCorrect ? '정답으로 지정됨' : '정답으로 지정'}
                className={`shrink-0 w-5 h-5 rounded-full border-2 flex items-center justify-center transition-colors ${
                  isCorrect
                    ? 'bg-emerald-500 border-emerald-500 text-white'
                    : 'border-slate-300 hover:border-emerald-400 text-transparent hover:text-emerald-300'
                }`}
              >
                <Check className="w-3 h-3" strokeWidth={3} />
              </button>
              <input
                ref={(node) => {
                  selectOptionRefs.current[refKey] = node;
                  if (isCorrect) answerRefs.current[quiz.id] = node;
                }}
                type="text"
                value={opt}
                placeholder={`보기 ${i + 1}`}
                onFocus={() => setActiveQuizId(quiz.id)}
                onChange={(e) => {
                  const next = [...options];
                  next[i] = e.target.value;
                  commit(next, correctIndex);
                }}
                onKeyDown={(e) => {
                  if (e.ctrlKey && (e.key === "'" || e.code === 'Quote')) {
                    e.preventDefault();
                    focusAnswerByOffset(quiz.id, 1);
                    return;
                  }
                  if (e.key === 'Enter' && !e.nativeEvent.isComposing) {
                    e.preventDefault();
                    if (e.ctrlKey) {
                      onInsertDividerAndReturn?.();
                      return;
                    }
                    const next = [...options];
                    next.splice(i + 1, 0, '');
                    const newCorrect = correctIndex >= i + 1 ? correctIndex + 1 : correctIndex;
                    pendingFocusRef.current = `${quiz.id}:${i + 1}`;
                    commit(next, newCorrect);
                    return;
                  }
                  if (e.key === 'Tab' && !e.shiftKey) {
                    e.preventDefault();
                    focusAnswerByOffset(quiz.id, 1);
                    return;
                  }
                  if (e.key === 'Backspace' && opt === '' && options.length > 1) {
                    e.preventDefault();
                    const next = options.filter((_, j) => j !== i);
                    const newCorrect = correctIndex > i ? correctIndex - 1 : correctIndex;
                    pendingFocusRef.current = `${quiz.id}:${Math.max(0, i - 1)}`;
                    commit(next, newCorrect);
                  }
                }}
                className={`flex-1 px-2 py-1 border rounded text-sm bg-white outline-none transition-colors ${
                  isCorrect
                    ? 'border-emerald-300 text-slate-900 font-medium focus:border-emerald-400 focus:ring-1 focus:ring-emerald-200'
                    : 'border-slate-200 text-slate-700 focus:border-blue-400 focus:ring-1 focus:ring-blue-200'
                }`}
              />
              {options.length > 1 && (
                <button
                  type="button"
                  onClick={() => {
                    const next = options.filter((_, j) => j !== i);
                    const newCorrect = correctIndex > i
                      ? correctIndex - 1
                      : correctIndex >= next.length
                        ? Math.max(0, next.length - 1)
                        : correctIndex;
                    commit(next, newCorrect);
                  }}
                  className="shrink-0 w-5 h-5 rounded text-slate-300 hover:text-red-500 hover:bg-red-50 flex items-center justify-center"
                  title="옵션 삭제"
                >
                  <X className="w-3.5 h-3.5" />
                </button>
              )}
            </div>
          );
        })}
        <button
          type="button"
          onClick={() => {
            pendingFocusRef.current = `${quiz.id}:${options.length}`;
            commit([...options, ''], correctIndex);
          }}
          className="self-start mt-0.5 ml-[26px] flex items-center gap-1 text-[11px] font-semibold text-blue-600 hover:text-blue-700"
        >
          <Plus className="w-3 h-3" /> 옵션 추가
        </button>
      </div>
    );
  };

  if (quizzes.length === 0) {
    return (
      <div className="w-full h-full flex flex-col items-center justify-center p-6 text-slate-500 bg-slate-50">
        <FileText className="w-12 h-12 mb-4 text-slate-300" />
        <p className="text-sm font-medium">분석된 퀴즈가 없습니다.</p>
        <p className="text-xs text-slate-400 mt-1">에디터에 내용을 입력하면 여기에 요약이 표시됩니다.</p>
      </div>
    );
  }

  return (
    <div className="w-full h-full flex flex-col bg-slate-50">
      <div className={`${layout === 'horizontal' ? 'px-4 py-2' : 'px-4 py-3'} border-b border-slate-200 bg-white z-10 sticky top-0 flex flex-col gap-1.5`}>
        <div className="flex items-center justify-between gap-3">
          <h3 className={`${layout === 'horizontal' ? 'text-xs' : 'text-sm'} font-semibold text-slate-800 flex items-center gap-2`}>
            <CheckCircle className="w-4 h-4 text-emerald-500" />
            퀴즈 {quizzes.length}개
          </h3>
          {totalImages > 0 && (
            <span className="text-xs font-medium text-slate-500 flex items-center gap-1">
              <ImageIcon className="w-3 h-3" /> {totalImages}
            </span>
          )}
        </div>
        <div className="flex items-center gap-2 text-[10px] text-slate-400 flex-wrap">
          <span className="flex items-center gap-1">
            <kbd className="px-1 py-0 rounded bg-slate-100 text-slate-500 font-mono text-[10px] border border-slate-200">Tab</kbd>
            <span className="text-slate-300">/</span>
            <kbd className="px-1 py-0 rounded bg-slate-100 text-slate-500 font-mono text-[10px] border border-slate-200">Ctrl</kbd>
            <span>+</span>
            <kbd className="px-1 py-0 rounded bg-slate-100 text-slate-500 font-mono text-[10px] border border-slate-200">'</kbd>
            <span>다음 정답</span>
          </span>
          <span className="text-slate-300">·</span>
          <span className="flex items-center gap-1">
            <kbd className="px-1 py-0 rounded bg-slate-100 text-slate-500 font-mono text-[10px] border border-slate-200">Ctrl</kbd>
            <span>+</span>
            <kbd className="px-1 py-0 rounded bg-slate-100 text-slate-500 font-mono text-[10px] border border-slate-200">↵</kbd>
            <span>새 문제</span>
          </span>
          <span className="text-slate-300">·</span>
          <span className="flex items-center gap-1">
            <kbd className="px-1 py-0 rounded bg-slate-100 text-slate-500 font-mono text-[10px] border border-slate-200">↵</kbd>
            <span>에디터로</span>
          </span>
        </div>
      </div>

      <div
        className={
          layout === 'horizontal'
            ? 'flex-1 overflow-x-auto overflow-y-auto p-4 flex items-start gap-3'
            : 'flex-1 overflow-y-auto p-4 flex flex-col gap-3'
        }
      >
        {quizzes.map((quiz, index) => {
          const hasInlineBlanks =
            (quiz.quizType === 'blank' || quiz.quizType === 'multi-blank') &&
            /_{3,}/.test(quiz.textContent);
          const useChips = !hasInlineBlanks && (quiz.quizType === 'multi-answer' || quiz.quizType === 'multi-blank');
          return (
          <div
            key={quiz.id}
            onClick={() => setActiveQuizId(quiz.id)}
            onDoubleClick={() => {
              if (quiz.quizType === 'select') openSelectionEditor(quiz.id);
            }}
            className={`bg-white rounded-lg p-4 shadow-sm border flex flex-col gap-2 transition-colors cursor-pointer ${
              activeQuizId === quiz.id
                ? 'border-blue-400 ring-1 ring-blue-200'
                : 'border-slate-200 hover:border-blue-300'
            } ${layout === 'horizontal' ? 'w-80 shrink-0' : ''}`}
          >
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-1.5">
                <span className="text-xs font-bold text-slate-600 bg-slate-100 px-2 py-0.5 rounded-full">
                  #{index + 1}
                </span>
                {quiz.quizType !== 'basic' && (
                  <span className={`text-xs font-bold px-2 py-0.5 rounded-full ${quizTypeBadgeClass[quiz.quizType]}`}>
                    {quizTypeLabel[quiz.quizType]}
                  </span>
                )}
              </div>
              {quiz.imageCount > 0 && (
                <span className="text-xs font-medium text-amber-600 bg-amber-50 px-2 py-0.5 rounded-full flex items-center gap-1">
                  <ImageIcon className="w-3 h-3" />
                  {quiz.imageCount}
                </span>
              )}
            </div>

            {!hasInlineBlanks && (
              <p className="text-sm text-slate-700 font-medium line-clamp-2 whitespace-pre-line leading-relaxed">
                {quiz.preview || '(내용 없음)'}
              </p>
            )}

            <div className={`flex flex-col gap-1.5 mt-1 pt-2 border-t border-slate-100 text-xs text-slate-400 ${layout === 'horizontal' ? 'min-h-0' : ''}`}>
              {quiz.quizType === 'select' ? (
                renderSelectOptions(quiz)
              ) : hasInlineBlanks ? (
                renderInlineBlanks(quiz)
              ) : useChips ? (
                renderChipsInput(quiz)
              ) : (
                <>
                  <div className="flex items-center gap-2 w-full">
                    <span className="font-semibold text-slate-600 whitespace-nowrap">정답</span>
                    <input
                      ref={(node) => {
                        answerRefs.current[quiz.id] = node;
                      }}
                      type="text"
                      placeholder={quiz.quizType === 'multi-answer' || quiz.quizType === 'multi-blank' ? '정답 여러 개: 쉼표/줄바꿈' : '정답 입력'}
                      value={answers[quiz.id] || ''}
                      onFocus={() => setActiveQuizId(quiz.id)}
                      onChange={(e) => onAnswerChange(quiz.id, e.target.value)}
                      onKeyDown={(e) => {
                        if (e.ctrlKey && e.key === 'Tab') {
                          e.preventDefault();
                          ctrlTabRef.current = Date.now();
                          return;
                        }

                        if (e.ctrlKey && e.key === 'Backspace' && Date.now() - ctrlTabRef.current <= 750) {
                          e.preventDefault();
                          focusAnswerByOffset(quiz.id, -1);
                          ctrlTabRef.current = 0;
                          return;
                        }

                        if (!e.ctrlKey && e.key === 'Tab') {
                          e.preventDefault();
                          focusAnswerByOffset(quiz.id, 1);
                          return;
                        }

                        if (e.ctrlKey && (e.key === "'" || e.code === 'Quote')) {
                          e.preventDefault();
                          focusAnswerByOffset(quiz.id, 1);
                          return;
                        }

                        if (e.ctrlKey && e.key === 'Enter' && !e.nativeEvent.isComposing) {
                          e.preventDefault();
                          onInsertDividerAndReturn?.();
                          return;
                        }

                        if (e.key === 'Enter' && !e.nativeEvent.isComposing) {
                          e.preventDefault();
                          onReturnToEditor?.();
                        }
                      }}
                      className="w-full px-2.5 py-1.5 border border-slate-200 rounded-md text-sm text-slate-800 bg-white focus:outline-none focus:border-blue-400 focus:ring-1 focus:ring-blue-200"
                    />
                  </div>
                </>
              )}
            </div>
          </div>
          );
        })}
      </div>
    </div>
  );
};
