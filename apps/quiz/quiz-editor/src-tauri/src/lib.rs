use serde::Serialize;
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::{Mutex, OnceLock};
use std::time::{SystemTime, UNIX_EPOCH};

#[derive(Serialize)]
struct GitTreeEntry {
    path: String,
    kind: String,
}

#[derive(Serialize, Clone)]
#[serde(rename_all = "camelCase")]
struct QuizDataRepository {
    name: String,
    path: String,
    remote: String,
    active: bool,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct GitIdentity {
    name: String,
    email: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct CodexChatResponse {
    message: String,
    response_json: String,
}

static SELECTED_REPO: OnceLock<Mutex<Option<PathBuf>>> = OnceLock::new();

fn selected_repo_store() -> &'static Mutex<Option<PathBuf>> {
    SELECTED_REPO.get_or_init(|| Mutex::new(None))
}

fn canonical_git_repo(path: &Path) -> Option<PathBuf> {
    let canonical = path.canonicalize().ok()?;
    if canonical.join(".git").is_dir() {
        Some(canonical)
    } else {
        None
    }
}

fn find_quiz_data_repo() -> Result<PathBuf, String> {
    if let Ok(guard) = selected_repo_store().lock() {
        if let Some(path) = guard.as_ref() {
            if path.join(".git").is_dir() {
                return path.canonicalize().map_err(|error| error.to_string());
            }
        }
    }

    let mut starts = Vec::new();

    if let Ok(current_dir) = std::env::current_dir() {
        starts.push(current_dir);
    }

    if let Ok(current_exe) = std::env::current_exe() {
        if let Some(parent) = current_exe.parent() {
            starts.push(parent.to_path_buf());
        }
    }

    for start in starts {
        for ancestor in start.ancestors() {
            let candidate = ancestor.join("quiz-data");
            if candidate.join(".git").is_dir() {
                return candidate.canonicalize().map_err(|error| error.to_string());
            }
        }
    }

    Err("quiz-data Git repository was not found.".to_string())
}

fn collect_candidate_repositories() -> Vec<PathBuf> {
    let mut candidates = Vec::new();

    if let Ok(current_dir) = std::env::current_dir() {
        candidates.push(current_dir);
    }

    if let Ok(current_exe) = std::env::current_exe() {
        if let Some(parent) = current_exe.parent() {
            candidates.push(parent.to_path_buf());
        }
    }

    candidates.push(PathBuf::from("C:/aa/build/external/quiz/quiz-data"));

    let mut repos = Vec::new();
    for start in candidates {
        for ancestor in start.ancestors() {
            if let Some(repo) = canonical_git_repo(ancestor) {
                repos.push(repo);
            }

            if let Some(repo) = canonical_git_repo(&ancestor.join("quiz-data")) {
                repos.push(repo);
            }
        }
    }

    repos.sort();
    repos.dedup();
    repos
}

fn relative_path(root: &Path, path: &Path) -> Result<String, String> {
    let relative = path.strip_prefix(root).map_err(|error| error.to_string())?;
    Ok(relative.to_string_lossy().replace('\\', "/"))
}

fn resolve_repo_path(root: &Path, relative_path: &str) -> Result<PathBuf, String> {
    let requested = PathBuf::from(relative_path);

    if relative_path.trim().is_empty()
        || requested.is_absolute()
        || requested
            .components()
            .any(|component| matches!(component, std::path::Component::ParentDir))
    {
        return Err("Only quiz-data relative paths are allowed.".to_string());
    }

    let target = root.join(requested);
    let parent = target
        .parent()
        .ok_or_else(|| "Invalid quiz-data path.".to_string())?;
    let canonical_parent = parent
        .canonicalize()
        .map_err(|_| "Parent folder was not found.".to_string())?;

    if !canonical_parent.starts_with(root) {
        return Err("Only quiz-data relative paths are allowed.".to_string());
    }

    Ok(target)
}

fn join_new_repo_path(root: &Path, relative_path: &str) -> Result<PathBuf, String> {
    let requested = PathBuf::from(relative_path);

    if relative_path.trim().is_empty()
        || requested.is_absolute()
        || requested
            .components()
            .any(|component| matches!(component, std::path::Component::ParentDir))
    {
        return Err("Only quiz-data relative paths are allowed.".to_string());
    }

    Ok(root.join(requested))
}

fn collect_tree(
    root: &Path,
    current: &Path,
    entries: &mut Vec<GitTreeEntry>,
) -> Result<(), String> {
    let mut children = fs::read_dir(current)
        .map_err(|error| error.to_string())?
        .collect::<Result<Vec<_>, _>>()
        .map_err(|error| error.to_string())?;

    children.sort_by_key(|entry| entry.path());

    for child in children {
        let path = child.path();
        let file_name = child.file_name();
        if file_name.to_string_lossy() == ".git" {
            continue;
        }

        let metadata = child.metadata().map_err(|error| error.to_string())?;
        let kind = if metadata.is_dir() {
            "directory"
        } else {
            "file"
        };
        entries.push(GitTreeEntry {
            path: relative_path(root, &path)?,
            kind: kind.to_string(),
        });

        if metadata.is_dir() {
            collect_tree(root, &path, entries)?;
        }
    }

    Ok(())
}

fn run_git(root: &Path, args: &[&str]) -> Result<String, String> {
    let output = Command::new("git")
        .args(args)
        .current_dir(root)
        .output()
        .map_err(|error| error.to_string())?;

    if !output.status.success() {
        return Err(String::from_utf8_lossy(&output.stderr).trim().to_string());
    }

    Ok(String::from_utf8_lossy(&output.stdout).trim().to_string())
}

fn ensure_git_identity(root: &Path) -> Result<(), String> {
    if run_git(root, &["config", "user.name"])
        .unwrap_or_default()
        .trim()
        .is_empty()
    {
        run_git(root, &["config", "user.name", "Quiz Sync Bot"])?;
    }

    if run_git(root, &["config", "user.email"])
        .unwrap_or_default()
        .trim()
        .is_empty()
    {
        run_git(root, &["config", "user.email", "quiz-sync@example.local"])?;
    }

    Ok(())
}

#[tauri::command]
fn list_quiz_data_repositories() -> Result<Vec<QuizDataRepository>, String> {
    let active_root = find_quiz_data_repo().ok();
    let repos = collect_candidate_repositories()
        .into_iter()
        .map(|repo| {
            let remote = run_git(&repo, &["config", "--get", "remote.origin.url"]).unwrap_or_default();
            let name = repo
                .file_name()
                .map(|value| value.to_string_lossy().to_string())
                .unwrap_or_else(|| "Git repository".to_string());
            let active = active_root.as_ref().map(|root| root == &repo).unwrap_or(false);

            QuizDataRepository {
                name,
                path: repo.to_string_lossy().to_string(),
                remote,
                active,
            }
        })
        .collect();

    Ok(repos)
}

#[tauri::command]
fn select_quiz_data_repository(path: String) -> Result<String, String> {
    let repo = canonical_git_repo(Path::new(&path))
        .ok_or_else(|| "Git repository was not found.".to_string())?;
    let mut guard = selected_repo_store()
        .lock()
        .map_err(|_| "Repository selection is locked.".to_string())?;
    *guard = Some(repo.clone());
    Ok(format!("Git 레포 연결: {}", repo.to_string_lossy()))
}

#[tauri::command]
fn get_git_identity() -> Result<GitIdentity, String> {
    let root = find_quiz_data_repo()?;
    Ok(GitIdentity {
        name: run_git(&root, &["config", "user.name"]).unwrap_or_default(),
        email: run_git(&root, &["config", "user.email"]).unwrap_or_default(),
    })
}

#[tauri::command]
fn set_git_identity(name: String, email: String) -> Result<String, String> {
    let root = find_quiz_data_repo()?;
    let trimmed_name = name.trim();
    let trimmed_email = email.trim();

    if trimmed_name.is_empty() || trimmed_email.is_empty() {
        return Err("Git user.name and user.email are required.".to_string());
    }

    run_git(&root, &["config", "user.name", trimmed_name])?;
    run_git(&root, &["config", "user.email", trimmed_email])?;
    Ok("Git 계정 저장 완료".to_string())
}

#[tauri::command]
fn list_quiz_data_tree() -> Result<Vec<GitTreeEntry>, String> {
    let root = find_quiz_data_repo()?;
    let mut entries = Vec::new();
    collect_tree(&root, &root, &mut entries)?;
    Ok(entries)
}

#[tauri::command]
fn read_quiz_data_file(relative_path: String) -> Result<String, String> {
    let root = find_quiz_data_repo()?;
    let target = resolve_repo_path(&root, &relative_path)?
        .canonicalize()
        .map_err(|_| "File was not found.".to_string())?;
    if !target.starts_with(&root) || !target.is_file() {
        return Err("Requested file is outside quiz-data or is not a file.".to_string());
    }

    fs::read_to_string(target).map_err(|_| "File could not be read.".to_string())
}

#[tauri::command]
fn write_quiz_data_file(relative_path: String, content: String) -> Result<String, String> {
    let root = find_quiz_data_repo()?;
    let target = resolve_repo_path(&root, &relative_path)?;
    fs::write(target, content).map_err(|_| "File could not be saved.".to_string())?;
    Ok("저장 완료".to_string())
}

#[tauri::command]
fn create_quiz_data_file(relative_path: String) -> Result<String, String> {
    let root = find_quiz_data_repo()?;
    let target = join_new_repo_path(&root, &relative_path)?;

    if target.exists() {
        return Err("File already exists.".to_string());
    }

    if let Some(parent) = target.parent() {
        fs::create_dir_all(parent)
            .map_err(|_| "Parent folder could not be created.".to_string())?;
    }
    fs::write(target, "").map_err(|_| "File could not be created.".to_string())?;
    Ok("파일 생성 완료".to_string())
}

#[tauri::command]
fn create_quiz_data_folder(relative_path: String) -> Result<String, String> {
    let root = find_quiz_data_repo()?;
    let target = join_new_repo_path(&root, &relative_path)?;
    fs::create_dir_all(&target).map_err(|_| "Folder could not be created.".to_string())?;
    Ok("폴더 생성 완료".to_string())
}

#[tauri::command]
fn delete_quiz_data_path(relative_path: String) -> Result<String, String> {
    let root = find_quiz_data_repo()?;
    let target = resolve_repo_path(&root, &relative_path)?
        .canonicalize()
        .map_err(|_| "Path was not found.".to_string())?;

    if target == root || !target.starts_with(&root) {
        return Err("Only quiz-data relative paths are allowed.".to_string());
    }

    if target.is_dir() {
        fs::remove_dir_all(target).map_err(|_| "Folder could not be deleted.".to_string())?;
    } else {
        fs::remove_file(target).map_err(|_| "File could not be deleted.".to_string())?;
    }

    Ok("삭제 완료".to_string())
}

#[tauri::command]
fn rename_quiz_data_path(
    from_relative_path: String,
    to_relative_path: String,
) -> Result<String, String> {
    let root = find_quiz_data_repo()?;
    let source = resolve_repo_path(&root, &from_relative_path)?
        .canonicalize()
        .map_err(|_| "Path was not found.".to_string())?;

    if source == root || !source.starts_with(&root) {
        return Err("Only quiz-data relative paths are allowed.".to_string());
    }

    let target = join_new_repo_path(&root, &to_relative_path)?;
    if target.exists() {
        return Err("Target already exists.".to_string());
    }

    let parent = target
        .parent()
        .ok_or_else(|| "Invalid quiz-data path.".to_string())?;
    let canonical_parent = parent
        .canonicalize()
        .map_err(|_| "Parent folder was not found.".to_string())?;

    if !canonical_parent.starts_with(&root) {
        return Err("Only quiz-data relative paths are allowed.".to_string());
    }

    fs::rename(source, target).map_err(|_| "Path could not be renamed.".to_string())?;
    Ok("이름 바꾸기 완료".to_string())
}

#[tauri::command]
async fn ask_codex(prompt: String, context: String) -> Result<CodexChatResponse, String> {
    tauri::async_runtime::spawn_blocking(move || {
        let root = find_quiz_data_repo()?;
        let request_dir = root.join("codex-requests");
        fs::create_dir_all(&request_dir)
            .map_err(|_| "Codex 요청 폴더를 만들 수 없습니다.".to_string())?;

        let output_path = request_dir.join("codex-last-message.txt");
        let instruction = format!(
            "너는 퀴즈관리앱 안에서 호출된 Codex다.\n\
             사용자의 요청을 한국어로 짧고 구체적으로 처리해라.\n\
             필요한 경우 현재 작업 폴더의 quiz-data 파일만 수정하고, 수정 내용은 답변에 요약해라.\n\
             현재 편집기를 수정해야 하면 반드시 아래 형식의 fenced block 하나를 답변에 포함해라.\n\
             ```quiz-editor-json\n\
             {{\"message\":\"사용자에게 보일 짧은 설명\",\"html\":\"편집기 전체 HTML\",\"answers\":{{\"1\":\"정답\"}},\"distractors\":{{}}}}\n\
             ```\n\
             html은 일부가 아니라 편집기 본문 전체여야 한다.\n\
             사용자 요청:\n{prompt}\n\n현재 퀴즈 컨텍스트:\n{context}\n"
        );

        let mut child = Command::new("codex")
            .args(["exec", "--full-auto", "--skip-git-repo-check", "-C"])
            .arg(&root)
            .arg("-o")
            .arg(&output_path)
            .arg("-")
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
            .map_err(|_| "Codex CLI를 실행할 수 없습니다.".to_string())?;

        let mut stdin = child
            .stdin
            .take()
            .ok_or_else(|| "Codex 입력 스트림을 열 수 없습니다.".to_string())?;
        stdin
            .write_all(instruction.as_bytes())
            .map_err(|_| "Codex에 요청을 전달할 수 없습니다.".to_string())?;
        drop(stdin);

        let output = child
            .wait_with_output()
            .map_err(|_| "Codex 응답을 기다릴 수 없습니다.".to_string())?;

        if !output.status.success() {
            let stderr = String::from_utf8_lossy(&output.stderr).trim().to_string();
            let stdout = String::from_utf8_lossy(&output.stdout).trim().to_string();
            let detail = if !stderr.is_empty() { stderr } else { stdout };
            return Err(if detail.is_empty() {
                "Codex 실행 실패".to_string()
            } else {
                format!("Codex 실행 실패: {detail}")
            });
        }

        let message = fs::read_to_string(&output_path)
            .unwrap_or_else(|_| String::from_utf8_lossy(&output.stdout).trim().to_string())
            .trim()
            .to_string();
        let message = if message.is_empty() {
            "Codex 응답이 비어 있습니다.".to_string()
        } else {
            message
        };
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|duration| duration.as_secs())
            .unwrap_or_default();
        let response_json = serde_json::to_string_pretty(&serde_json::json!({
            "message": message,
            "updatedAtEpoch": timestamp
        }))
        .map_err(|error| error.to_string())?;

        fs::write(request_dir.join("latest-response.json"), &response_json)
            .map_err(|_| "Codex 응답 파일을 저장할 수 없습니다.".to_string())?;

        Ok(CodexChatResponse {
            message,
            response_json,
        })
    })
    .await
    .map_err(|error| error.to_string())?
}

#[tauri::command]
fn commit_quiz_data(message: String) -> Result<String, String> {
    let root = find_quiz_data_repo()?;
    ensure_git_identity(&root)?;

    run_git(&root, &["add", "."])?;
    let status = run_git(&root, &["status", "--porcelain"])?;
    if status.trim().is_empty() {
        return Ok("변경 사항이 없습니다.".to_string());
    }

    let commit_message = if message.trim().is_empty() {
        "Update quiz data".to_string()
    } else {
        message.trim().to_string()
    };

    run_git(&root, &["commit", "-m", &commit_message])?;
    Ok("커밋 완료".to_string())
}

#[tauri::command]
fn pull_quiz_data() -> Result<String, String> {
    let root = find_quiz_data_repo()?;
    run_git(&root, &["pull", "--rebase"]).map_err(|_| "Pull 실패".to_string())?;
    Ok("Pull 완료".to_string())
}

#[tauri::command]
fn push_quiz_data() -> Result<String, String> {
    let root = find_quiz_data_repo()?;
    run_git(&root, &["push"]).map_err(|_| "Push 실패".to_string())?;
    Ok("Push 완료".to_string())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            list_quiz_data_repositories,
            select_quiz_data_repository,
            get_git_identity,
            set_git_identity,
            list_quiz_data_tree,
            read_quiz_data_file,
            write_quiz_data_file,
            create_quiz_data_file,
            create_quiz_data_folder,
            delete_quiz_data_path,
            rename_quiz_data_path,
            ask_codex,
            commit_quiz_data,
            pull_quiz_data,
            push_quiz_data
        ])
        .setup(|app| {
            if cfg!(debug_assertions) {
                app.handle().plugin(
                    tauri_plugin_log::Builder::default()
                        .level(log::LevelFilter::Info)
                        .build(),
                )?;
            }
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
