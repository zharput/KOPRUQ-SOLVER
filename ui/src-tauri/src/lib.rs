//! KOPRUQ-SOLVER desktop shell. It does no engineering: it starts the existing C++ CLI as a child
//! process (no shell, fixed argument vector built from validated fields), forwards its stderr lines and
//! `--progress json` events to the UI, and can stop it. Results are the CLI's own JSON files.
use serde::{Deserialize, Serialize};
use std::io::{BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::{Arc, Mutex};
use std::time::{Instant, SystemTime, UNIX_EPOCH};
use tauri::{AppHandle, Emitter, Manager, State};

#[derive(Default)]
struct Runner {
    child: Arc<Mutex<Option<Child>>>,
    cancelled: Arc<Mutex<bool>>,
    next_id: Mutex<u64>,
}

#[derive(Serialize)]
struct SolverInfo { path: String, version: String }

#[derive(Deserialize)]
struct RunRequest {
    command: String,          // inspect | solve | modal | spectrum
    model_path: String,
    format: String,           // native | midas-nx-json
    num_modes: Option<u32>,
    mass_type: Option<String>, // consistent | lumped
}

#[derive(Serialize, Clone)]
struct LogEvent { run_id: u64, line: String }
#[derive(Serialize, Clone)]
struct ProgressEvent { run_id: u64, event: serde_json::Value }
#[derive(Serialize, Clone)]
struct FinishedEvent { run_id: u64, exit: i32, cancelled: bool, output_path: String, seconds: f64 }

/// Where the CLI is: KOPRUQ_SOLVER_CLI, else next to this executable (packaged app), else the
/// repository build folders (development).
fn resolve_cli() -> Result<PathBuf, String> {
    let name = if cfg!(windows) { "kopruq_solver_cli.exe" } else { "kopruq_solver_cli" };
    let mut tried = Vec::new();
    if let Ok(p) = std::env::var("KOPRUQ_SOLVER_CLI") {
        let p = PathBuf::from(p);
        if p.is_file() { return Ok(p); }
        tried.push(p);
    }
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            let p = dir.join(name);
            if p.is_file() { return Ok(p); }
            tried.push(p);
        }
    }
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("..").join("..").join("build");
    for cfg in ["release", "gui-release", "debug"] {
        let p = root.join(cfg).join(name);
        if p.is_file() { return Ok(p); }
        tried.push(p);
    }
    Err(format!("kopruq_solver_cli not found (set KOPRUQ_SOLVER_CLI or place it next to the app). Looked in: {}",
        tried.iter().map(|p| p.display().to_string()).collect::<Vec<_>>().join("; ")))
}

fn hidden(cmd: &mut Command) -> &mut Command {
    #[cfg(windows)]
    { use std::os::windows::process::CommandExt; cmd.creation_flags(0x0800_0000); } // CREATE_NO_WINDOW
    cmd
}

#[tauri::command]
fn solver_info() -> Result<SolverInfo, String> {
    let cli = resolve_cli()?;
    let out = hidden(Command::new(&cli).arg("--version")).output().map_err(|e| format!("cannot run {}: {e}", cli.display()))?;
    if !out.status.success() { return Err(format!("{} --version failed", cli.display())); }
    Ok(SolverInfo { path: cli.display().to_string(), version: String::from_utf8_lossy(&out.stdout).trim().to_string() })
}

/// Reads a UTF-8 JSON file chosen by the user or written by the CLI (extension and size are checked).
#[tauri::command]
fn read_text_file(path: String) -> Result<String, String> {
    let p = Path::new(&path);
    if p.extension().and_then(|e| e.to_str()).map(|e| e.eq_ignore_ascii_case("json")) != Some(true) { return Err("only .json files can be read".into()); }
    let meta = std::fs::metadata(p).map_err(|e| format!("{path}: {e}"))?;
    if meta.len() > 512 * 1024 * 1024 { return Err(format!("{path}: file is larger than 512 MB")); }
    std::fs::read_to_string(p).map_err(|e| format!("{path}: {e}"))
}

fn validate(req: &RunRequest) -> Result<(), String> {
    if !["inspect", "solve", "modal", "spectrum"].contains(&req.command.as_str()) { return Err(format!("unsupported command '{}'", req.command)); }
    if !["native", "midas-nx-json"].contains(&req.format.as_str()) { return Err(format!("unsupported format '{}'", req.format)); }
    if let Some(m) = &req.mass_type { if !["consistent", "lumped"].contains(&m.as_str()) { return Err(format!("unsupported mass type '{m}'")); } }
    if let Some(n) = req.num_modes { if n == 0 || n > 10_000 { return Err("number of modes must be 1..10000".into()); } }
    let p = Path::new(&req.model_path);
    if p.extension().and_then(|e| e.to_str()).map(|e| e.eq_ignore_ascii_case("json")) != Some(true) { return Err("the model must be a .json file".into()); }
    if !p.is_file() { return Err(format!("model file not found: {}", req.model_path)); }
    Ok(())
}

#[tauri::command]
fn run_solver(app: AppHandle, state: State<Runner>, req: RunRequest) -> Result<u64, String> {
    validate(&req)?;
    {
        let guard = state.child.lock().unwrap();
        if guard.is_some() { return Err("an analysis is already running".into()); }
    }
    let cli = resolve_cli()?;
    let run_id = { let mut n = state.next_id.lock().unwrap(); *n += 1; *n };
    let dir = app.path().app_cache_dir().map_err(|e| e.to_string())?.join("runs");
    std::fs::create_dir_all(&dir).map_err(|e| format!("{}: {e}", dir.display()))?;
    let stamp = SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_millis()).unwrap_or(0);
    let output = dir.join(format!("{}-{}-{}.json", req.command, stamp, run_id));

    let mut args: Vec<String> = vec![req.command.clone(), req.model_path.clone(), "-o".into(), output.display().to_string(), "--format".into(), req.format.clone()];
    if req.command != "inspect" { args.push("--progress".into()); args.push("json".into()); }
    if let Some(n) = req.num_modes { if req.command == "modal" { args.push("-n".into()); args.push(n.to_string()); } }
    if let Some(m) = &req.mass_type { if req.command == "modal" || req.command == "spectrum" { args.push("--mass-type".into()); args.push(m.clone()); } }

    let mut child = hidden(Command::new(&cli).args(&args).stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::piped()))
        .spawn().map_err(|e| format!("cannot start {}: {e}", cli.display()))?;
    let stderr = child.stderr.take().ok_or("no stderr pipe")?;
    *state.cancelled.lock().unwrap() = false;
    *state.child.lock().unwrap() = Some(child);

    let slot = Arc::clone(&state.child);
    let cancelled = Arc::clone(&state.cancelled);
    let out_path = output.display().to_string();
    std::thread::spawn(move || {
        let started = Instant::now();
        for line in BufReader::new(stderr).lines().map_while(Result::ok) {
            if line.starts_with("{\"event\"") {
                if let Ok(v) = serde_json::from_str::<serde_json::Value>(&line) { let _ = app.emit("solver-progress", ProgressEvent { run_id, event: v }); continue; }
            }
            let _ = app.emit("solver-log", LogEvent { run_id, line });
        }
        let exit = match slot.lock().unwrap().take() {
            Some(mut c) => c.wait().ok().and_then(|s| s.code()).unwrap_or(-1),
            None => -1,
        };
        let was_cancelled = *cancelled.lock().unwrap();
        let _ = app.emit("solver-finished", FinishedEvent { run_id, exit, cancelled: was_cancelled, output_path: out_path, seconds: started.elapsed().as_secs_f64() });
    });
    Ok(run_id)
}

#[tauri::command]
fn cancel_solver(state: State<Runner>) -> bool {
    let mut guard = state.child.lock().unwrap();
    if let Some(c) = guard.as_mut() {
        *state.cancelled.lock().unwrap() = true;
        let _ = c.kill();
        return true;
    }
    false
}

pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(Runner::default())
        .invoke_handler(tauri::generate_handler![solver_info, read_text_file, run_solver, cancel_solver])
        .run(tauri::generate_context!())
        .expect("error while running the KOPRUQ-SOLVER UI");
}
