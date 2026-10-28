use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

fn main() {
    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR not set"));
    let shader_dir = Path::new("shaders");

    println!("cargo:rerun-if-changed=shaders");

    let entries = match fs::read_dir(shader_dir) {
        Ok(entries) => entries,
        Err(_) => return,
    };

    for entry in entries.flatten() {
        let path = entry.path();
        if path.is_file() {
            if let Some(ext) = path.extension().and_then(|s| s.to_str()) {
                if ext == "vert" || ext == "frag" || ext == "comp" {
                    println!("cargo:rerun-if-changed={}", path.display());
                    let file_name = path.file_name().unwrap().to_str().unwrap();
                    let out_path = out_dir.join(format!("{}.spv", file_name));

                    let status = Command::new("glslangValidator")
                        .args(["-V", path.to_str().unwrap(), "-o", out_path.to_str().unwrap()])
                        .status()
                        .expect("Failed to execute glslangValidator. Ensure glslangValidator is installed.");

                    if !status.success() {
                        panic!("Failed to compile shader {}", path.display());
                    }
                }
            }
        }
    }
}
