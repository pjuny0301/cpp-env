import {
  asText,
  type Day,
  type Deck,
  isRecord,
  normalizeDay,
  normalizeDeck,
  normalizeImportedDecks,
  normalizeQuestion,
  type Question,
  rebuildOptionsFromPeerAnswers,
} from "./data";

const normalizeGitInput = (url: string) => {
  const trimmed = url.trim().replace(/\\/g, "/");
  const shorthand = trimmed.match(/^([A-Za-z0-9_.-]+)\/([A-Za-z0-9_.-]+)(?:\/(.+))?$/);
  if (shorthand && !/^https?:\/\//i.test(trimmed)) {
    const repo = shorthand[2].replace(/\.git$/i, "");
    const path = shorthand[3] || (shorthand[1] === "pjuny0301" && repo === "quiz-data" ? "mobile-decks" : "");
    return `https://github.com/${shorthand[1]}/${repo}${path ? `/tree/main/${path}` : ""}`;
  }

  const repoRoot = trimmed.match(/^https:\/\/github\.com\/([^/]+)\/([^/.?#]+)(?:\.git)?\/?$/i);
  if (repoRoot) {
    const repo = repoRoot[2].replace(/\.git$/i, "");
    const path = repoRoot[1] === "pjuny0301" && repo === "quiz-data" ? "/tree/main/mobile-decks" : "";
    return `https://github.com/${repoRoot[1]}/${repo}${path}`;
  }

  const sshRoot = trimmed.match(/^git@github\.com:([^/]+)\/([^/]+?)(?:\.git)?$/i);
  if (sshRoot) {
    const repo = sshRoot[2].replace(/\.git$/i, "");
    const path = sshRoot[1] === "pjuny0301" && repo === "quiz-data" ? "/tree/main/mobile-decks" : "";
    return `https://github.com/${sshRoot[1]}/${repo}${path}`;
  }

  return trimmed;
};

export const normalizeGitJsonUrl = (url: string) => {
  const trimmed = normalizeGitInput(url);
  const githubBlob = trimmed.match(/^https:\/\/github\.com\/([^/]+)\/([^/]+)\/blob\/([^/]+)\/(.+)$/);

  if (githubBlob) {
    return `https://raw.githubusercontent.com/${githubBlob[1]}/${githubBlob[2]}/${githubBlob[3]}/${githubBlob[4]}`;
  }

  return trimmed;
};

const normalizeGitFolderUrl = (url: string) => normalizeGitInput(url).replace(/\/+$/, "");

const titleFromFileName = (name: string) =>
  name.replace(/\.[^.]+$/, "").replace(/[-_]+/g, " ").replace(/\s+/g, " ").trim();

const slug = (value: string) =>
  value
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9가-힣]+/g, "-")
    .replace(/^-+|-+$/g, "") || "untitled";

type RemoteEntry = {
  name: string;
  url: string;
  type: "file" | "directory";
  downloadUrl?: string;
};

type GitHubLocation = {
  owner: string;
  repo: string;
  ref: string;
  path: string;
};

const trimSlashes = (value: string) => value.replace(/^\/+|\/+$/g, "");

const parseGitHubLocation = (url: string): GitHubLocation | null => {
  const trimmed = normalizeGitFolderUrl(url);
  const tree = trimmed.match(/^https:\/\/github\.com\/([^/]+)\/([^/]+)\/tree\/([^/]+)(?:\/(.+))?$/);
  if (tree) {
    return {
      owner: tree[1],
      repo: tree[2].replace(/\.git$/i, ""),
      ref: tree[3],
      path: trimSlashes(tree[4] || ""),
    };
  }

  const repoRoot = trimmed.match(/^https:\/\/github\.com\/([^/]+)\/([^/]+)$/);
  if (repoRoot) {
    return {
      owner: repoRoot[1],
      repo: repoRoot[2].replace(/\.git$/i, ""),
      ref: "main",
      path: "",
    };
  }

  const apiContents = trimmed.match(/^https:\/\/api\.github\.com\/repos\/([^/]+)\/([^/]+)\/contents(?:\/([^?#]+))?(?:\?(.+))?$/);
  if (apiContents) {
    const params = new URLSearchParams(apiContents[4] || "");
    return {
      owner: apiContents[1],
      repo: apiContents[2].replace(/\.git$/i, ""),
      ref: params.get("ref") || "main",
      path: trimSlashes(apiContents[3] ? decodeURIComponent(apiContents[3]) : ""),
    };
  }

  return null;
};

const githubContentsApiUrl = (url: string) => {
  const trimmed = normalizeGitFolderUrl(url);
  const tree = trimmed.match(/^https:\/\/github\.com\/([^/]+)\/([^/]+)\/tree\/([^/]+)(?:\/(.+))?$/);
  if (tree) {
    const path = tree[4] || "";
    return `https://api.github.com/repos/${tree[1]}/${tree[2]}/contents/${path}?ref=${tree[3]}`;
  }

  const repoRoot = trimmed.match(/^https:\/\/github\.com\/([^/]+)\/([^/]+)$/);
  if (repoRoot) return `https://api.github.com/repos/${repoRoot[1]}/${repoRoot[2]}/contents?ref=main`;

  if (/^https:\/\/api\.github\.com\/repos\/[^/]+\/[^/]+\/contents(?:\/|$)/.test(trimmed)) return trimmed;
  return "";
};

const listGithubDirectory = async (url: string): Promise<RemoteEntry[]> => {
  const apiUrl = githubContentsApiUrl(url);
  if (!apiUrl) return [];

  const response = await fetch(apiUrl, { cache: "no-store" });
  if (!response.ok) throw new Error(`GitHub folder request failed: ${response.status}`);

  const payload = await response.json();
  if (!Array.isArray(payload)) return [];

  return payload
    .filter(isRecord)
    .map(item => ({
      name: asText(item.name),
      url: asText(item.url || item.html_url),
      type: item.type === "dir" ? "directory" : "file",
      downloadUrl: asText(item.download_url),
    }))
    .filter(entry => entry.name && entry.url);
};

const listJsDelivrDirectory = async (url: string): Promise<RemoteEntry[]> => {
  const location = parseGitHubLocation(url);
  if (!location) return [];

  const flatUrl = `https://data.jsdelivr.com/v1/package/gh/${location.owner}/${location.repo}@${location.ref}/flat`;
  const response = await fetch(flatUrl, { cache: "no-store" });
  if (!response.ok) throw new Error(`GitHub CDN folder request failed: ${response.status}`);

  const payload = await response.json();
  const files = Array.isArray(payload?.files) ? payload.files : [];
  const basePath = trimSlashes(location.path);
  const entries = new Map<string, RemoteEntry>();

  files
    .filter(isRecord)
    .map(file => asText(file.name).replace(/^\/+/, ""))
    .filter(Boolean)
    .forEach(filePath => {
      if (basePath && !filePath.startsWith(`${basePath}/`)) return;

      const rest = basePath ? filePath.slice(basePath.length + 1) : filePath;
      if (!rest) return;

      const [name, ...nestedParts] = rest.split("/");
      if (!name) return;

      const entryKey = nestedParts.length > 0 ? `directory:${name}` : `file:${name}`;
      if (entries.has(entryKey)) return;

      if (nestedParts.length > 0) {
        const childPath = [basePath, name].filter(Boolean).join("/");
        entries.set(entryKey, {
          name,
          type: "directory",
          url: `https://github.com/${location.owner}/${location.repo}/tree/${location.ref}/${childPath}`,
        });
        return;
      }

      const downloadUrl = `https://cdn.jsdelivr.net/gh/${location.owner}/${location.repo}@${location.ref}/${filePath}`;
      entries.set(entryKey, {
        name,
        type: "file",
        url: downloadUrl,
        downloadUrl,
      });
    });

  return Array.from(entries.values());
};

const listHttpDirectory = async (url: string): Promise<RemoteEntry[]> => {
  const baseUrl = `${normalizeGitFolderUrl(url)}/`;
  const response = await fetch(baseUrl, { cache: "no-store" });
  if (!response.ok) throw new Error(`Git folder request failed: ${response.status}`);

  const html = await response.text();
  const doc = new DOMParser().parseFromString(html, "text/html");

  return Array.from(doc.querySelectorAll("a"))
    .map(anchor => {
      const href = anchor.getAttribute("href") || "";
      const label = anchor.textContent?.trim() || "";
      if (!href || href === "../" || href === "./../" || label === "../" || label === "./../" || href.startsWith("?")) {
        return null;
      }

      const absoluteUrl = new URL(href, baseUrl);
      if (absoluteUrl.href === baseUrl) return null;

      const cleanName = decodeURIComponent(absoluteUrl.pathname.split("/").filter(Boolean).pop() || "");
      const isDirectory = href.endsWith("/") || anchor.textContent?.trim().endsWith("/");
      return {
        name: cleanName,
        url: absoluteUrl.href.replace(/\/+$/, ""),
        type: isDirectory ? "directory" as const : "file" as const,
        downloadUrl: absoluteUrl.href,
      };
    })
    .filter(Boolean) as RemoteEntry[];
};

const listRemoteDirectory = async (url: string) => {
  try {
    const githubEntries = await listGithubDirectory(url);
    if (githubEntries.length > 0) return githubEntries;
  } catch {
    // Unauthenticated GitHub API requests are easy to rate-limit on Android.
  }

  try {
    const cdnEntries = await listJsDelivrDirectory(url);
    if (cdnEntries.length > 0) return cdnEntries;
  } catch {
    // Fall through to generic HTTP directory parsing.
  }

  return listHttpDirectory(url);
};

const normalizeDayPayload = (payload: unknown, fallbackTitle: string): Day => {
  if (isRecord(payload)) {
    if (Array.isArray(payload.questions)) {
      return {
        id: `day-${slug(asText(payload.id, fallbackTitle))}`,
        title: asText(payload.title, fallbackTitle),
        questions: payload.questions.map(normalizeQuestion).filter(Boolean) as Question[],
      };
    }

    if (Array.isArray(payload.quizzes)) {
      return {
        id: `day-${slug(asText(payload.dayId, fallbackTitle))}`,
        title: asText(payload.dayTitle, fallbackTitle),
        questions: payload.quizzes.map(normalizeQuestion).filter(Boolean) as Question[],
      };
    }

    if (Array.isArray(payload.days)) {
      const day = normalizeDay(payload.days[0], 0);
      if (day) return { ...day, title: asText(day.title, fallbackTitle) };
    }

    if (Array.isArray(payload.decks)) {
      const deck = normalizeDeck(payload.decks[0], 0);
      const day = deck?.days[0];
      if (day) return { ...day, title: asText(day.title, fallbackTitle) };
    }
  }

  return {
    id: `day-${slug(fallbackTitle)}`,
    title: fallbackTitle,
    questions: [],
  };
};

const resolveImageUrl = (imageUrl: string | undefined, sourceUrl: string) => {
  if (!imageUrl || /^(?:https?:|data:|blob:)/i.test(imageUrl)) return imageUrl;
  return new URL(imageUrl, sourceUrl).toString();
};

const resolveDeckImageUrls = (decks: Deck[], sourceUrl: string) =>
  decks.map(deck => ({
    ...deck,
    days: deck.days.map(day => ({
      ...day,
      questions: day.questions.map(question => ({
        ...question,
        imageUrl: resolveImageUrl(question.imageUrl, sourceUrl),
      })),
    })),
  }));

const resolveDayImageUrls = (day: Day, sourceUrl: string) => ({
  ...day,
  questions: day.questions.map(question => ({
    ...question,
    imageUrl: resolveImageUrl(question.imageUrl, sourceUrl),
  })),
});

const fetchJson = async (url: string) => {
  const response = await fetch(url, { cache: "no-store" });
  if (!response.ok) throw new Error(`Git JSON request failed: ${response.status}`);
  return response.json();
};

const fetchDecksFromGitFolderUrl = async (url: string, depth = 0): Promise<Deck[]> => {
  const sourceUrl = normalizeGitFolderUrl(url);
  const sourceEntries = await listRemoteDirectory(sourceUrl);
  const deckFolders = sourceEntries
    .filter(entry => entry.type === "directory")
    .sort((a, b) => a.name.localeCompare(b.name));
  const directDayFiles = sourceEntries
    .filter(entry => entry.type === "file" && /\.json$/i.test(entry.name))
    .sort((a, b) => a.name.localeCompare(b.name));

  if (deckFolders.length === 0 && directDayFiles.length > 0) {
    const days = await Promise.all(directDayFiles.map(async dayFile => {
      const dayUrl = dayFile.downloadUrl || dayFile.url;
      const day = normalizeDayPayload(await fetchJson(dayUrl), titleFromFileName(dayFile.name));
      return resolveDayImageUrls(day, dayUrl);
    }));
    const folderName = decodeURIComponent(sourceUrl.split("/").filter(Boolean).pop() || "Git Deck");

    return rebuildOptionsFromPeerAnswers([{
      id: `deck-${slug(folderName)}`,
      title: titleFromFileName(folderName) || folderName,
      days,
    }]);
  }

  const decks = await Promise.all(deckFolders.map(async deckFolder => {
    const dayFiles = (await listRemoteDirectory(deckFolder.url))
      .filter(entry => entry.type === "file" && /\.json$/i.test(entry.name))
      .sort((a, b) => a.name.localeCompare(b.name));

    const days = await Promise.all(dayFiles.map(async dayFile => {
      const dayUrl = dayFile.downloadUrl || dayFile.url;
      const day = normalizeDayPayload(await fetchJson(dayUrl), titleFromFileName(dayFile.name));
      return resolveDayImageUrls(day, dayUrl);
    }));

    return {
      id: `deck-${slug(deckFolder.name)}`,
      title: titleFromFileName(deckFolder.name) || deckFolder.name,
      days,
    };
  }));

  const parsedDecks = decks.filter(deck => deck.days.length > 0);
  if (parsedDecks.length > 0) return rebuildOptionsFromPeerAnswers(parsedDecks);

  if (depth < 2) {
    const nestedDecks = await Promise.all(
      deckFolders.map(deckFolder => fetchDecksFromGitFolderUrl(deckFolder.url, depth + 1))
    );
    return nestedDecks.flat();
  }

  return [];
};

export const fetchDecksFromGitUrl = async (url: string) => {
  const trimmed = url.trim();
  const sourceUrl = /\.json(?:$|[?#])/i.test(trimmed) ? normalizeGitJsonUrl(trimmed) : normalizeGitFolderUrl(trimmed);

  if (!/\.json(?:$|[?#])/i.test(sourceUrl)) {
    return {
      sourceUrl,
      decks: await fetchDecksFromGitFolderUrl(sourceUrl),
    };
  }

  const decks = normalizeImportedDecks(await fetchJson(sourceUrl));
  return {
    sourceUrl,
    decks: resolveDeckImageUrls(decks, sourceUrl),
  };
};
