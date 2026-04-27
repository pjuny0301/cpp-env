import type { MouseEvent } from "react";
import { GripVertical, X } from "lucide-react";
import type { CodexChatMessage } from "../editorTypes";

type CodexPromptPanelProps = {
  position: { x: number; y: number };
  messages: CodexChatMessage[];
  prompt: string;
  status: string;
  onPromptChange: (prompt: string) => void;
  onSubmit: () => void;
  onClose: () => void;
  onDragStart: (event: MouseEvent<HTMLDivElement>) => void;
};

export function CodexPromptPanel({
  position,
  messages,
  prompt,
  status,
  onPromptChange,
  onSubmit,
  onClose,
  onDragStart,
}: CodexPromptPanelProps) {
  return (
    <div
      className="fixed z-[65] w-[min(560px,calc(100vw-24px))] rounded-lg border border-sky-200 bg-white shadow-2xl overflow-hidden"
      style={{ left: position.x, top: position.y }}
    >
      <div
        onMouseDown={onDragStart}
        className="h-9 cursor-move select-none border-b border-sky-100 bg-sky-50 px-3 flex items-center justify-between"
      >
        <div className="flex items-center gap-2">
          <GripVertical className="w-4 h-4 text-sky-400" />
          <span className="text-xs font-semibold text-sky-700">Codex</span>
        </div>
        <button
          onMouseDown={(event) => event.stopPropagation()}
          onClick={onClose}
          className="w-7 h-7 rounded hover:bg-white text-sky-600 flex items-center justify-center"
          title="닫기"
        >
          <X className="w-4 h-4" />
        </button>
      </div>
      <div className="p-3">
        <div className="mb-3 h-72 overflow-y-auto rounded-md border border-sky-100 bg-slate-50/70 p-3">
          {messages.length === 0 ? (
            <div className="py-6 text-center text-xs text-slate-400">Codex</div>
          ) : (
            <div className="flex flex-col gap-2">
              {messages.map(message => (
                <div
                  key={message.id}
                  className={`max-w-[92%] rounded-md px-2.5 py-2 text-xs leading-5 ${
                    message.role === "user"
                      ? "ml-auto bg-sky-600 text-white"
                      : "mr-auto border border-sky-100 bg-white text-slate-700"
                  }`}
                >
                  <div className={`mb-1 text-[10px] font-semibold ${message.role === "user" ? "text-sky-100" : "text-sky-600"}`}>
                    {message.role === "user" ? "You" : "Codex"}
                  </div>
                  <div className="whitespace-pre-wrap break-words">{message.text}</div>
                </div>
              ))}
            </div>
          )}
        </div>
        <textarea
          autoFocus
          value={prompt}
          onChange={(event) => onPromptChange(event.target.value)}
          placeholder="Enter"
          className="h-28 w-full resize-none rounded border border-sky-100 bg-sky-50/40 px-3 py-2 text-sm text-slate-800 outline-none focus:border-sky-300 focus:ring-1 focus:ring-sky-200"
        />
        {status && <div className="mt-2 text-[11px] text-sky-700">{status}</div>}
        <div className="mt-3 flex justify-end gap-2">
          <button
            onClick={onClose}
            className="rounded border border-gray-200 px-3 py-1.5 text-xs font-medium text-gray-600 hover:bg-gray-50"
          >
            닫기
          </button>
          <button
            onClick={onSubmit}
            className="rounded bg-sky-600 px-3 py-1.5 text-xs font-semibold text-white hover:bg-sky-700"
          >
            실행
          </button>
        </div>
      </div>
    </div>
  );
}
