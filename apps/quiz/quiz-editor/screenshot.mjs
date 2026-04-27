import { chromium } from '@playwright/test';
import { mkdirSync } from 'node:fs';
import { resolve } from 'node:path';

const outDir = resolve('C:/aa/codex/quiz/일자별요구사항/2026-04-27/screenshots/editor-after-v3');
mkdirSync(outDir, { recursive: true });

const browser = await chromium.launch();
const ctx = await browser.newContext({ viewport: { width: 1440, height: 900 } });
const page = await ctx.newPage();

await page.goto('http://localhost:5174/', { waitUntil: 'networkidle', timeout: 15000 });
await page.evaluate(() => { localStorage.clear(); });
await page.reload({ waitUntil: 'networkidle' });
await page.waitForTimeout(800);
await page.screenshot({ path: `${outDir}/01-initial.png`, fullPage: false });

// Open Git tree
const gitBtn = await page.getByRole('button', { name: /Git\s*트리/ }).first();
if (await gitBtn.isVisible().catch(() => false)) {
  await gitBtn.click();
  await page.waitForTimeout(1500);
  await page.screenshot({ path: `${outDir}/03-git-tree.png`, fullPage: false });
  console.log('03-git-tree.png saved');
}

// Paste some sample content into the editor
const editor = await page.locator('[contenteditable="true"]').first();
if (await editor.isVisible().catch(() => false)) {
  await editor.click();

  // Scenario: test new numbering (#1, #2, #3 dividers) + inline blanks with English
  const html = `<div data-quiz-divider="true" data-quiz-type="select"><span data-quiz-divider-label="true">#1</span><hr class="quiz-divider" /></div>
<div>MVC 패턴은 어디에 해당하는가?</div>
<div data-quiz-divider="true" data-quiz-type="multi-blank"><span data-quiz-divider-label="true">#2</span><hr class="quiz-divider" /></div>
<div>MVC stands for ___, ___, and ___</div>
<div data-quiz-divider="true" data-quiz-type="multi-answer"><span data-quiz-divider-label="true">#3</span><hr class="quiz-divider" /></div>
<div>애자일 방법론의 대표 예를 모두 고르시오</div>`;
  await editor.evaluate((el, text) => {
    el.innerHTML = text;
    el.dispatchEvent(new Event('input', { bubbles: true }));
  }, html);
  await page.waitForTimeout(800);
  await page.screenshot({ path: `${outDir}/04-with-content.png`, fullPage: false });
  console.log('04-with-content.png saved');

  // Click into select option input on quiz 1
  const firstOption = page.locator('input[placeholder="보기 1"]').first();
  if (await firstOption.count()) {
    await firstOption.click();
    await firstOption.fill('모델-뷰-컨트롤러 분리');
    await firstOption.press('Enter');
    await page.keyboard.type('뷰만 분리');
    await firstOption.press('Enter');
    await page.keyboard.type('컨트롤러만 분리');
    await page.waitForTimeout(400);
  }
  await page.screenshot({ path: `${outDir}/05-select-options.png`, fullPage: false });
  console.log('05-select-options.png saved');

  // Chip input (multi-answer) — find the multi-answer card and type chips
  const chipInputs = page.locator('input[data-chip-input]');
  const chipCount = await chipInputs.count();
  if (chipCount > 0) {
    const first = chipInputs.first();
    await first.click();
    await first.type('스크럼');
    await first.press('Enter');
    await first.type('XP');
    await first.press(',');
    await first.type('칸반');
    await page.waitForTimeout(400);
    await page.screenshot({ path: `${outDir}/06-chips.png`, fullPage: false });
    console.log('06-chips.png saved');
  }

  // Inline blanks — fill English values to test width calc
  const blankInputs = page.locator('input[placeholder="①"]');
  if (await blankInputs.count()) {
    const blanks = await page.locator('.border-b-2.border-emerald-300').all();
    for (let idx = 0; idx < blanks.length; idx++) {
      await blanks[idx].click();
      await page.keyboard.type(['Model', 'View', 'Controller'][idx] || 'x');
    }
    await page.waitForTimeout(400);
  }
  await page.screenshot({ path: `${outDir}/07-inline-blanks-english.png`, fullPage: false });
  console.log('07-inline-blanks-english.png saved');

  // Test Ctrl+Enter from answer input → should insert new divider
  const answerInput = page.locator('input[placeholder="정답 입력"]').first();
  if (await answerInput.count()) {
    await answerInput.click();
    await answerInput.press('Control+Enter');
    await page.waitForTimeout(500);
    await page.screenshot({ path: `${outDir}/08-ctrl-enter-new-divider.png`, fullPage: false });
    console.log('08-ctrl-enter-new-divider.png saved');
  }

  // Test divider deletion renumbering
  await editor.click();
  await editor.evaluate((el) => {
    const dividers = el.querySelectorAll('[data-quiz-divider="true"]');
    if (dividers.length > 1) dividers[1].remove();
    el.dispatchEvent(new Event('input', { bubbles: true }));
  });
  await page.waitForTimeout(400);
  await page.screenshot({ path: `${outDir}/09-after-delete-divider.png`, fullPage: false });
  console.log('09-after-delete-divider.png saved');
}

await browser.close();
console.log('DONE');
