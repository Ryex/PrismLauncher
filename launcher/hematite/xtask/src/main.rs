use std::{
    io::{BufRead, Seek, Write},
    process::{Command, ExitStatus},
};

use clap::{Parser, Subcommand};
use indoc::indoc;

/// Helper program to manage hematite crates
///
/// Can be invoked as `cargo xtask <command>`
#[derive(Debug, Parser)]
#[command(bin_name = "cargo xtask")]
struct Args {
    #[command(subcommand)]
    task: Task,
}

#[derive(Debug, Subcommand)]
enum Task {
    /// Add a new sub crate
    New {
        /// Name of the crate to add
        name: String,
    },
}

#[derive(thiserror::Error)]
enum Error {
    #[error("failed to run command `{0}`")]
    Command(String, #[source] std::io::Error),
}

impl std::fmt::Debug for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        use std::error::Error;
        use std::fmt::*;
        write!(f, "Error: {}", self)?;
        let mut err: &dyn Error = self;
        while let Some(cause) = err.source() {
            write!(f, "\nCaused by: ")?;
            Display::fmt(&cause, f)?;
            err = cause;
        }
        Ok(())
    }
}

// const VERSION: Option<&str> = option_env!("CARGO_PKG_VERSION");

fn main() -> color_eyre::Result<()> {
    color_eyre::install()?;
    let args = Args::parse();
    let workspace = {
        let out = Command::new("cargo")
            .arg("metadata")
            .arg("--no-deps")
            .arg("--format-version=1")
            .output()
            .map_err(|e| Error::Command("cargo metadata".to_string(), e))?;
        let s = std::str::from_utf8(&out.stdout).unwrap();
        let Some((_, s)) = s.split_once(r#"workspace_root":""#) else {
            panic!("couldn't find workspace root");
        };
        let Some((path, _)) = s.split_once("\",") else {
            panic!("couldn't find workspace root");
        };
        std::path::PathBuf::from(path)
    };
    match &args.task {
        Task::New { name } => {
            new_crate(&workspace, name)?;
        }
    }
    Ok(())
}

fn new_crate(workspace: &std::path::Path, name: &str) -> color_eyre::Result<()> {
    let crate_name = format!("hematite_{}", name);
    run_command(
        "cargo",
        ["new", "--name", &crate_name, "--lib", name],
        workspace,
    )?;
    {
        let mut file = std::fs::File::options()
            .append(true)
            .open(workspace.join(name).join("Cargo.toml"))?;
        write!(
            &mut file,
            indoc!(
                r#"

                [lib]
                crate-type = ["staticlib"]

                "#
            )
        )?;
    }
    run_command(
        "cargo",
        ["add", "cxx", "cxx-qt", "cxx-qt-lib"],
        workspace.join(name),
    )?;
    run_command(
        "cargo",
        ["add", "--build", "cxx-qt-build"],
        workspace.join(name),
    )?;

    {
        let mut build_rs = std::fs::File::create(workspace.join(name).join("build.rs"))?;
        write!(
            &mut build_rs,
            indoc!(
                r#"
                //Generated build.rs, modify as needed
                
                use cxx_qt_build::{{CxxQtBuilder, QmlModule}};
                
                fn main() {{
                    CxxQtBuilder::new()
                        // Link Qt's Network library
                        // - Qt Core is always linked
                        // - Qt Gui is linked by enabling the qt_gui Cargo feature (default).
                        // - Qt Qml is linked by enabling the qt_qml Cargo feature (default).
                        // - Qt Qml requires linking Qt Network on macOS
                        // - use .qt_module("Network") qt link a Qt library e.g. Link Qt's Network library
                        // .qml_module(QmlModule {{
                        //     uri: "org.prismlauncher.hematite.{}",
                        //     rust_files: &["src/cxxqt_object.rs"],
                        //     qml_files: &["../qml/main.qml"],
                        //     ..Default::default()
                        // }})
                        .file("src/lib.rs")
                        .cc_builder(|cc| {{
                            cc.include("../../")
                        }})
                        .build();
                }}
                "#,
            ),
            crate_name
        )?;
    }
    {
        std::fs::create_dir_all(workspace.join(name).join("src"))?;
        let mut lib_rs = std::fs::File::create(workspace.join(name).join("src").join("lib.rs"))?;
        write!(
            &mut lib_rs,
            indoc!(
                r#"
                /// The bridge definition for our QObject
                #[cxx_qt::bridge]
                pub mod qobject {{

                    unsafe extern "C++" {{
                        include!("cxx-qt-lib/qstring.h");
                        /// An alias to the QString type
                        type QString = cxx_qt_lib::QString;
                    }}

                    unsafe extern "RustQt" {{
                        // The QObject definition
                        // We tell CXX-Qt that we want a QObject class with the name MyObject
                        // based on the Rust struct MyObjectRust.
                        #[qobject]
                        type MyObject = super::MyObjectRust;
                    }}

                    unsafe extern "RustQt" {{
                        // Declare the invocable methods we want to expose on the QObject
                    }}
                }}

                use core::pin::Pin;
                use cxx_qt_lib::QString;

                /// The Rust struct for the QObject
                #[derive(Default)]
                pub struct MyObjectRust {{
                }}

                impl qobject::MyObject {{
                }}

                "#
            )
        )?;
    }
    {
        let mut file = std::fs::File::options()
            .read(true)
            .write(true)
            .open(workspace.join("CMakeLists.txt"))?;
        let reader = std::io::BufReader::new(&file);
        let mut before: Vec<String> = Vec::new();
        let mut build_lines: Vec<String> = Vec::new();
        let mut after: Vec<String> = Vec::new();

        enum AppendMode {
            Before,
            Build,
            After,
        }

        let mut mode = AppendMode::Before;

        let begin_re = regex::Regex::new(r"^# BEGIN CRATES_TO_BUILD").unwrap();
        let end_re = regex::Regex::new(r"^# END CRATES_TO_BUILD").unwrap();
        let build_line_re = regex::Regex::new(r"^LinkCxxQtCrate\((.*)\)$").unwrap();

        for line in reader.lines() {
            let line = line?;
            match mode {
                AppendMode::Before => {
                    if begin_re.is_match(&line) {
                        mode = AppendMode::Build;
                    }
                    before.push(line);
                }
                AppendMode::Build => {
                    if end_re.is_match(&line) {
                        mode = AppendMode::After;
                        build_lines.push(line);
                    } else if build_line_re.is_match(&line) {
                        if !build_lines.contains(&line) {
                            build_lines.push(line);
                        }
                    } else {
                        after.push(line);
                    }
                }
                AppendMode::After => {
                    after.push(line);
                }
            }
        }

        build_lines.push(format!("LinkCxxQtCrate({name} {crate_name})"));

        build_lines.sort();

        file.seek(std::io::SeekFrom::Start(0))?;
        file.write_all((before.join("\n") + "\n").as_bytes())?;
        file.write_all((build_lines.join("\n") + "\n").as_bytes())?;
        file.write_all(after.join("\n").as_bytes())?;
    }
    Ok(())
}

fn build_command<S, A, I, P>(program: S, args: I, working_dir: P) -> Command
where
    S: AsRef<std::ffi::OsStr>,
    A: AsRef<std::ffi::OsStr>,
    I: IntoIterator<Item = A>,
    P: AsRef<std::path::Path>,
{
    let mut cmd = Command::new(program);
    cmd.current_dir(working_dir);
    cmd.args(args);
    cmd
}

fn run_command<S, A, I, P>(
    program: S,
    args: I,
    working_dir: P,
) -> Result<(ExitStatus, Command), Error>
where
    S: AsRef<std::ffi::OsStr>,
    A: AsRef<std::ffi::OsStr>,
    I: IntoIterator<Item = A>,
    P: AsRef<std::path::Path>,
{
    let mut cmd = build_command(program, args, working_dir);
    let status = cmd
        .status()
        .map_err(|e| Error::Command(format!("{}", cmd.get_program().to_string_lossy()), e))?;
    Ok((status, cmd))
}
