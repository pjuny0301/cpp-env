import React, { useRef, useImperativeHandle, forwardRef, useState } from 'react';

export interface BulkEditorRef {
  insertDivider: (quizType?: DividerQuizType) => void;
  getContent: () => string;
  getActiveQuizNumber: () => number;
  focus: () => void;
  clear: () => void;
  setContent: (html: string) => void;
  saveSelection: () => void;
  restoreSelection: () => void;
}

interface BulkEditorProps {
  onChange: (html: string) => void;
}

export type DividerQuizType = 'select' | 'blank' | 'multi-answer' | 'multi-blank';

const dividerTypeMeta: Record<DividerQuizType, { label: string; className: string }> = {
  select: {
    label: '#Select',
    className: 'border-sky-100 bg-sky-50 text-sky-600',
  },
  blank: {
    label: '#Blank',
    className: 'border-emerald-100 bg-emerald-50 text-emerald-600',
  },
  'multi-answer': {
    label: '#MultiAnswer',
    className: 'border-violet-100 bg-violet-50 text-violet-600',
  },
  'multi-blank': {
    label: '#MultiBlank',
    className: 'border-fuchsia-100 bg-fuchsia-50 text-fuchsia-600',
  },
};

export const BulkEditor = forwardRef<BulkEditorRef, BulkEditorProps>(({ onChange }, ref) => {
  const editorRef = useRef<HTMLDivElement>(null);
  const isComposingRef = useRef(false);
  const selectionRef = useRef<Range | null>(null);
  const lastCtrlEnterRef = useRef(0);
  const lastDividerRef = useRef<HTMLElement | null>(null);
  const activeDividerRef = useRef<HTMLElement | null>(null);
  const [isEmpty, setIsEmpty] = useState(true);
  const [typeMenu, setTypeMenu] = useState<{ x: number; y: number; divider: HTMLElement } | null>(null);

  const getNextQuizNumber = () => {
    const dividerCount = editorRef.current?.querySelectorAll('.quiz-divider').length || 0;
    return dividerCount + 1;
  };

  const createDividerHtml = () => {
    const quizNumber = getNextQuizNumber();

    return `<div data-quiz-divider="true" contenteditable="false" class="quiz-divider-block my-1 flex items-center gap-2 select-none"><span data-quiz-divider-label="true" class="quiz-divider-label shrink-0 rounded-full border border-slate-200 bg-white px-2.5 py-0.5 text-[11px] font-semibold text-slate-500 tracking-wide">#${quizNumber}</span><hr class="quiz-divider flex-1 border-t border-dashed border-slate-300" /></div>`;
  };

  const renumberDividerLabels = () => {
    editorRef.current
      ?.querySelectorAll<HTMLElement>('[data-quiz-divider="true"]')
      .forEach((divider, index) => {
        const label = divider.querySelector<HTMLElement>('[data-quiz-divider-label="true"]');
        if (label) label.textContent = `#${index + 1}`;
      });
  };

  const getLastDivider = () => {
    const dividers = editorRef.current?.querySelectorAll<HTMLElement>('[data-quiz-divider="true"]');
    return dividers?.[dividers.length - 1] || null;
  };

  const getSelectedDivider = () => {
    const selection = window.getSelection();
    if (!selection || selection.rangeCount === 0) return null;

    const node = selection.getRangeAt(0).commonAncestorContainer;
    const element = node instanceof Element ? node : node.parentElement;
    return element?.closest<HTMLElement>('[data-quiz-divider="true"]') || null;
  };

  const setDividerType = (divider: HTMLElement | null, quizType: DividerQuizType) => {
    if (!divider) return;

    divider.querySelectorAll('[data-quiz-type-badge="true"]').forEach(badge => badge.remove());
    divider.dataset.quizType = quizType;

    const meta = dividerTypeMeta[quizType];
    const typeBadge = document.createElement('span');
    typeBadge.dataset.quizTypeBadge = 'true';
    if (quizType === 'select') typeBadge.dataset.quizSelect = 'true';
    typeBadge.className = `quiz-type-label shrink-0 rounded-full border px-2.5 py-1 text-xs font-bold ${meta.className}`;
    typeBadge.textContent = meta.label;
    divider.insertBefore(typeBadge, divider.querySelector('hr'));
  };

  const markDividerAsSelect = (divider: HTMLElement | null) => {
    setDividerType(divider, 'select');
  };

  const placeCaretAtStart = (element: HTMLElement) => {
    const selection = window.getSelection();
    if (!selection) return;

    const range = document.createRange();
    range.selectNodeContents(element);
    range.collapse(true);
    selection.removeAllRanges();
    selection.addRange(range);
    saveSelection();
  };

  const insertDividerAtSelection = (timestamp = Date.now(), quizType?: DividerQuizType) => {
    const editor = editorRef.current;
    if (!editor) return;

    editor.focus();
    const cursorId = `quiz-cursor-${timestamp}-${Math.random().toString(36).slice(2)}`;
    document.execCommand('insertHTML', false, `${createDividerHtml()}<div data-quiz-cursor="${cursorId}"><br /></div>`);
    renumberDividerLabels();
    const inserted = getLastDivider();
    if (inserted && quizType) setDividerType(inserted, quizType);
    lastDividerRef.current = inserted;
    lastCtrlEnterRef.current = timestamp;

    const cursorLine = editor.querySelector<HTMLElement>(`[data-quiz-cursor="${cursorId}"]`);
    if (cursorLine) {
      cursorLine.removeAttribute('data-quiz-cursor');
      placeCaretAtStart(cursorLine);
    }

    handleInput();
  };

  const getActiveQuizNumber = () => {
    const editor = editorRef.current;
    const selection = window.getSelection();
    if (!editor || !selection || selection.rangeCount === 0) return 1;

    const range = selection.getRangeAt(0);
    if (!editor.contains(range.commonAncestorContainer)) return 1;

    const selectedDivider = getSelectedDivider();
    const dividers = Array.from(editor.querySelectorAll<HTMLElement>('[data-quiz-divider="true"]'));
    if (selectedDivider) {
      const dividerIndex = dividers.indexOf(selectedDivider);
      return dividerIndex >= 0 ? dividerIndex + 1 : 1;
    }

    let quizNumber = 0;
    dividers.forEach((divider) => {
      const dividerRange = document.createRange();
      dividerRange.selectNode(divider);
      if (range.compareBoundaryPoints(Range.START_TO_END, dividerRange) >= 0) {
        quizNumber += 1;
      }
    });

    return Math.max(1, quizNumber);
  };

  const saveSelection = () => {
    const editor = editorRef.current;
    const selection = window.getSelection();
    if (!editor || !selection || selection.rangeCount === 0) return;

    const range = selection.getRangeAt(0);
    if (editor.contains(range.commonAncestorContainer)) {
      selectionRef.current = range.cloneRange();
    }
  };

  const restoreSelection = () => {
    const editor = editorRef.current;
    const selection = window.getSelection();
    if (!editor || !selection) return;

    editor.focus();
    selection.removeAllRanges();

    const savedRange = selectionRef.current;
    if (savedRange && editor.contains(savedRange.commonAncestorContainer)) {
      selection.addRange(savedRange);
      return;
    }

    const range = document.createRange();
    range.selectNodeContents(editor);
    range.collapse(false);
    selection.addRange(range);
  };

  // Expose methods to parent
  useImperativeHandle(ref, () => ({
    insertDivider: (quizType?: DividerQuizType) => {
      insertDividerAtSelection(Date.now(), quizType);
    },
    getContent: () => editorRef.current?.innerHTML || '',
    getActiveQuizNumber,
    focus: () => editorRef.current?.focus(),
    clear: () => {
      if (editorRef.current) {
        editorRef.current.innerHTML = '';
        handleInput();
      }
    },
    setContent: (html: string) => {
      if (editorRef.current) {
        editorRef.current.innerHTML = html;
        handleInput();
        setIsEmpty(!html.trim());
      }
    },
    saveSelection,
    restoreSelection,
  }));

  const handleInput = () => {
    if (editorRef.current) {
      setIsEmpty(editorRef.current.innerText.trim() === '' && editorRef.current.querySelectorAll('img, hr').length === 0);
    }

    if (isComposingRef.current) return;

    if (editorRef.current) {
      renumberDividerLabels();
      const html = editorRef.current.innerHTML;
      onChange(html);
      saveSelection();
    }
  };

  const handlePaste = (e: React.ClipboardEvent<HTMLDivElement>) => {
    // Intercept image pastes
    const items = e.clipboardData.items;
    let hasImage = false;

    for (let i = 0; i < items.length; i++) {
      if (items[i].type.indexOf('image') !== -1) {
        hasImage = true;
        e.preventDefault(); // Stop default paste
        
        const blob = items[i].getAsFile();
        if (!blob) continue;
        
        const reader = new FileReader();
        reader.onload = (event) => {
          if (event.target?.result) {
            // Insert image at cursor
            editorRef.current?.focus();
            const imgHtml = `<img src="${event.target.result}" class="max-w-md max-h-64 object-contain my-2 rounded-md border border-gray-200 inline-block shadow-sm" alt="Pasted Image" />&nbsp;`;
            document.execCommand('insertHTML', false, imgHtml);
            handleInput();
          }
        };
        reader.readAsDataURL(blob);
      }
    }

    // Let text paste happen naturally, but trigger handleInput shortly after
    if (!hasImage) {
      setTimeout(handleInput, 0);
    }
  };

  const typedDividerShortcuts: Record<string, DividerQuizType> = {
    '1': 'select',
    '2': 'blank',
    '3': 'multi-answer',
    '4': 'multi-blank',
  };

  const handleKeyDown = (e: React.KeyboardEvent<HTMLDivElement>) => {
    if (isComposingRef.current) return;

    // Ctrl + Shift + 1~4 inserts a typed divider directly
    if (e.ctrlKey && e.shiftKey && typedDividerShortcuts[e.key]) {
      e.preventDefault();
      const quizType = typedDividerShortcuts[e.key];
      const target = getSelectedDivider() || activeDividerRef.current;
      if (target) {
        setDividerType(target, quizType);
        handleInput();
      } else {
        insertDividerAtSelection(Date.now(), quizType);
      }
      lastCtrlEnterRef.current = 0;
      return;
    }

    // Ctrl + Enter inserts a divider
    if (e.ctrlKey && e.key === 'Enter') {
      e.preventDefault();
      const now = Date.now();
      const selectedDivider = getSelectedDivider() || activeDividerRef.current;

      if (selectedDivider) {
        markDividerAsSelect(selectedDivider);
        lastCtrlEnterRef.current = 0;
        handleInput();
        return;
      }

      if (lastDividerRef.current && now - lastCtrlEnterRef.current <= 750) {
        markDividerAsSelect(lastDividerRef.current);
        lastCtrlEnterRef.current = 0;
        handleInput();
        return;
      }

      insertDividerAtSelection(now);
    }
  };

  const handleContextMenu = (e: React.MouseEvent<HTMLDivElement>) => {
    const divider = (e.target as Element).closest<HTMLElement>('[data-quiz-divider="true"]');
    if (!divider) return;

    e.preventDefault();
    setTypeMenu({ x: e.clientX, y: e.clientY, divider });
  };

  const handleMouseDown = (e: React.MouseEvent<HTMLDivElement>) => {
    const divider = (e.target as Element).closest<HTMLElement>('[data-quiz-divider="true"]');
    activeDividerRef.current = divider;

    if (divider) {
      lastDividerRef.current = divider;
      editorRef.current?.focus();
    }
  };

  const applyDividerTypeFromMenu = (quizType: DividerQuizType) => {
    setDividerType(typeMenu?.divider || null, quizType);
    setTypeMenu(null);
    handleInput();
  };

  const deleteDividerType = () => {
    typeMenu?.divider.querySelectorAll('[data-quiz-type-badge="true"]').forEach(badge => badge.remove());
    if (typeMenu?.divider) delete typeMenu.divider.dataset.quizType;
    setTypeMenu(null);
    handleInput();
  };

  const handleCompositionStart = () => {
    isComposingRef.current = true;
    setIsEmpty(false);
  };

  const handleCompositionEnd = () => {
    isComposingRef.current = false;
    window.requestAnimationFrame(handleInput);
  };

  return (
    <div className="relative w-full h-full flex flex-col bg-white">
      {isEmpty && (
        <div className="absolute top-6 left-8 pointer-events-none text-gray-400 text-sm font-medium flex flex-col gap-2">
          <p>내용을 입력하세요</p>
        </div>
      )}
      <div
        ref={editorRef}
        className="w-full h-full p-8 outline-none overflow-y-auto text-sm leading-relaxed text-gray-800 font-sans whitespace-pre-wrap break-words"
        contentEditable={true}
        onInput={handleInput}
        onPaste={handlePaste}
        onKeyDown={handleKeyDown}
        onKeyUp={saveSelection}
        onMouseDown={handleMouseDown}
        onMouseUp={saveSelection}
        onBlur={saveSelection}
        onContextMenu={handleContextMenu}
        onCompositionStart={handleCompositionStart}
        onCompositionEnd={handleCompositionEnd}
        spellCheck="false"
        style={{ minHeight: '100%' }}
      />
      {typeMenu && (
        <div
          className="fixed z-50 rounded-md border border-gray-200 bg-white py-1 text-xs shadow-lg"
          style={{ left: typeMenu.x, top: typeMenu.y }}
        >
          {Object.entries(dividerTypeMeta).map(([quizType, meta]) => (
            <button
              key={quizType}
              onMouseDown={(event) => event.preventDefault()}
              onClick={() => applyDividerTypeFromMenu(quizType as DividerQuizType)}
              className="block w-full px-3 py-1.5 text-left text-gray-700 hover:bg-gray-50"
            >
              {meta.label}
            </button>
          ))}
          <button
            onMouseDown={(event) => event.preventDefault()}
            onClick={deleteDividerType}
            className="px-3 py-1.5 text-left text-gray-700 hover:bg-red-50 hover:text-red-600"
          >
            Delete
          </button>
        </div>
      )}
    </div>
  );
});

BulkEditor.displayName = 'BulkEditor';
