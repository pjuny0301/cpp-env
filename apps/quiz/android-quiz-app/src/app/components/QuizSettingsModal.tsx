import { X } from "lucide-react";

type QuizSettingsModalProps = {
  delay: number;
  setDelay: (value: number) => void;
  longTextFontSize: number;
  setLongTextFontSize: (value: number) => void;
  manualContinue: boolean;
  setManualContinue: (value: boolean) => void;
  knownThreshold: number;
  setKnownThreshold: (value: number) => void;
  wrongRelease: number;
  setWrongRelease: (value: number) => void;
  onClose: () => void;
};

export function QuizSettingsModal({
  delay,
  setDelay,
  longTextFontSize,
  setLongTextFontSize,
  manualContinue,
  setManualContinue,
  knownThreshold,
  setKnownThreshold,
  wrongRelease,
  setWrongRelease,
  onClose,
}: QuizSettingsModalProps) {
  return (
    <div className="absolute inset-0 bg-black/60 z-50 flex items-center justify-center p-4">
      <div className="bg-white rounded-2xl w-full max-w-sm p-6 shadow-2xl relative text-slate-800">
        <div className="flex justify-between items-center mb-6">
          <h3 className="text-xl font-bold">퀴즈 설정</h3>
          <button onClick={onClose} className="p-2 -mr-2 text-slate-400 hover:text-slate-600">
            <X className="w-5 h-5" />
          </button>
        </div>
        <div className="mb-6">
          <div className="flex items-center justify-between mb-2">
            <label className="text-sm font-medium text-slate-600">탭해서 다음으로</label>
            <button
              type="button"
              role="switch"
              aria-checked={manualContinue}
              onClick={() => setManualContinue(!manualContinue)}
              className={`relative inline-flex h-7 w-12 items-center rounded-full transition-colors ${manualContinue ? "bg-blue-600" : "bg-slate-300"}`}
            >
              <span className={`inline-block h-5 w-5 transform rounded-full bg-white shadow transition-transform ${manualContinue ? "translate-x-6" : "translate-x-1"}`} />
            </button>
          </div>
          <p className="text-xs text-slate-400 leading-snug">
            {manualContinue ? "정답 확인 후 '다음 →' 버튼을 눌러 진행합니다." : "정답 확인 후 자동으로 다음 문제로 넘어갑니다."}
          </p>
        </div>
        {!manualContinue && (
          <div className="mb-6">
            <label className="block text-sm font-medium text-slate-600 mb-2">자동 넘김 지연</label>
            <input
              type="range"
              min="500"
              max="5000"
              step="100"
              value={delay}
              onChange={(event) => setDelay(Number(event.target.value))}
              className="w-full h-2 bg-slate-200 rounded-lg appearance-none cursor-pointer accent-blue-600"
            />
            <div className="text-right text-sm font-bold text-blue-600 mt-2">{(delay / 1000).toFixed(1)}초</div>
          </div>
        )}
        <div className="mb-6">
          <label className="block text-sm font-medium text-slate-600 mb-2">장문 글꼴 크기</label>
          <input
            type="range"
            min="14"
            max="32"
            step="1"
            value={longTextFontSize}
            onChange={(event) => setLongTextFontSize(Number(event.target.value))}
            className="w-full h-2 bg-slate-200 rounded-lg appearance-none cursor-pointer accent-blue-600"
          />
          <div className="text-right text-sm font-bold text-blue-600 mt-2">{longTextFontSize}px</div>
        </div>
        <div className="mb-6">
          <label className="block text-sm font-medium text-slate-600 mb-2">
            '이미 앎' 승급 연속 정답 수
          </label>
          <input
            type="range"
            min="1"
            max="10"
            step="1"
            value={knownThreshold}
            onChange={(event) => setKnownThreshold(Number(event.target.value))}
            className="w-full h-2 bg-slate-200 rounded-lg appearance-none cursor-pointer accent-blue-600"
          />
          <div className="text-right text-sm font-bold text-blue-600 mt-2">{knownThreshold}회</div>
        </div>
        <div className="mb-6">
          <label className="block text-sm font-medium text-slate-600 mb-2">
            '이미 앎' 해제 오답 수
          </label>
          <input
            type="range"
            min="1"
            max="10"
            step="1"
            value={wrongRelease}
            onChange={(event) => setWrongRelease(Number(event.target.value))}
            className="w-full h-2 bg-slate-200 rounded-lg appearance-none cursor-pointer accent-blue-600"
          />
          <div className="text-right text-sm font-bold text-blue-600 mt-2">{wrongRelease}회</div>
        </div>
        <button
          onClick={onClose}
          className="w-full py-3 bg-slate-800 text-white rounded-xl font-bold hover:bg-slate-900 transition-colors"
        >
          닫기
        </button>
      </div>
    </div>
  );
}
