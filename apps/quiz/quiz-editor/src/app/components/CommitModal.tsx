import type { MouseEvent } from "react";
import { GripVertical, X } from "lucide-react";

type CommitModalProps = {
  position: { x: number; y: number };
  message: string;
  onMessageChange: (message: string) => void;
  onCommit: () => void;
  onClose: () => void;
  onDragStart: (event: MouseEvent<HTMLDivElement>) => void;
};

export function CommitModal({
  position,
  message,
  onMessageChange,
  onCommit,
  onClose,
  onDragStart,
}: CommitModalProps) {
  return (
    <div
      className="fixed z-[60] w-[360px] rounded-lg bg-white shadow-2xl border border-gray-200 overflow-hidden"
      style={{ left: position.x, top: position.y }}
    >
      <div
        onMouseDown={onDragStart}
        className="h-9 cursor-move select-none border-b border-gray-200 bg-gray-50 px-3 flex items-center justify-between"
      >
        <div className="flex items-center gap-2">
          <GripVertical className="w-4 h-4 text-gray-400" />
          <h2 className="text-sm font-semibold text-gray-800">Git Commit</h2>
        </div>
        <button
          onMouseDown={(event) => event.stopPropagation()}
          onClick={onClose}
          className="w-7 h-7 rounded hover:bg-gray-100 text-gray-500 flex items-center justify-center"
          title="닫기"
        >
          <X className="w-4 h-4" />
        </button>
      </div>
      <div className="p-4">
        <input
          autoFocus
          value={message}
          onChange={(event) => onMessageChange(event.target.value)}
          onKeyDown={(event) => {
            if (event.key === "Enter" && !event.nativeEvent.isComposing) {
              onCommit();
            }
          }}
          placeholder="Commit message"
          className="w-full rounded border border-gray-200 px-3 py-2 text-sm text-gray-800 outline-none focus:border-blue-400 focus:ring-1 focus:ring-blue-300"
        />
        <div className="mt-4 flex justify-end gap-2">
          <button
            onClick={onClose}
            className="rounded border border-gray-200 px-3 py-2 text-sm font-medium text-gray-600 hover:bg-gray-50"
          >
            취소
          </button>
          <button
            onClick={onCommit}
            className="rounded bg-blue-600 px-3 py-2 text-sm font-semibold text-white hover:bg-blue-700"
          >
            Commit
          </button>
        </div>
      </div>
    </div>
  );
}
