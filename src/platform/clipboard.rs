//! Wayland asynchronous clipboard and primary selection data transfers via non-blocking UNIX pipes.

use std::os::fd::{AsRawFd, FromRawFd, OwnedFd, RawFd};

/// Standard UTF-8 MIME types in descending order of preference.
pub const UTF8_MIME_TYPES: &[&str] = &[
    "text/plain;charset=utf-8",
    "text/plain",
    "UTF8_STRING",
    "STRING",
    "TEXT",
];

/// Target selection buffer for paste or copy operations.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SelectionTarget {
    /// Standard system clipboard (`Ctrl+C` / `Ctrl+V`).
    Clipboard,
    /// Primary selection buffer (mouse highlight / middle-click).
    Primary,
}

/// Asynchronous non-blocking pipe reader for incoming selection data.
pub struct PipeReader {
    /// Reading end of the UNIX pipe.
    pub fd: OwnedFd,
    /// Intermediate accumulation buffer.
    pub buffer: Vec<u8>,
    /// Originating selection target.
    pub target: SelectionTarget,
}

impl PipeReader {
    /// Creates a new pipe reader for the given file descriptor and target buffer.
    pub fn new(fd: OwnedFd, target: SelectionTarget) -> Self {
        Self {
            fd,
            buffer: Vec::with_capacity(1024),
            target,
        }
    }
}

/// Creates a pair of non-blocking, close-on-exec UNIX file descriptors `(read_fd, write_fd)`.
pub fn create_nonblocking_pipe() -> std::io::Result<(OwnedFd, OwnedFd)> {
    let mut fds = [0 as RawFd; 2];
    // SAFETY: pipe2 with O_CLOEXEC and O_NONBLOCK creates a standard non-blocking pipe pair
    let ret = unsafe { libc::pipe2(fds.as_mut_ptr(), libc::O_CLOEXEC | libc::O_NONBLOCK) };
    if ret < 0 {
        return Err(std::io::Error::last_os_error());
    }

    // SAFETY: fds were created successfully by pipe2 and are distinct valid file descriptors
    let read_fd = unsafe { OwnedFd::from_raw_fd(fds[0]) };
    let write_fd = unsafe { OwnedFd::from_raw_fd(fds[1]) };

    Ok((read_fd, write_fd))
}

/// Matches the highest priority UTF-8 MIME type from a list of compositor offerings.
pub fn find_best_utf8_mime(offered: &[String]) -> Option<&'static str> {
    for &wanted in UTF8_MIME_TYPES {
        if offered.iter().any(|m| m.eq_ignore_ascii_case(wanted)) {
            return Some(wanted);
        }
    }
    None
}

/// Synchronously writes a UTF-8 text string to an outgoing data offer pipe.
pub fn write_text_to_pipe(fd: &OwnedFd, text: &str) -> std::io::Result<()> {
    let bytes = text.as_bytes();
    let mut written = 0;
    while written < bytes.len() {
        // SAFETY: write call with valid buffer slice and file descriptor
        let ret = unsafe {
            libc::write(
                fd.as_raw_fd(),
                bytes[written..].as_ptr() as *const libc::c_void,
                bytes.len() - written,
            )
        };
        if ret > 0 {
            written += ret as usize;
        } else if ret == 0 {
            break;
        } else {
            let err = std::io::Error::last_os_error();
            if err.kind() == std::io::ErrorKind::Interrupted {
                continue;
            }
            return Err(err);
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_mime_negotiation() {
        let offered = vec![
            "image/png".to_string(),
            "text/plain;charset=utf-8".to_string(),
            "STRING".to_string(),
        ];
        let chosen = find_best_utf8_mime(&offered);
        assert_eq!(chosen, Some("text/plain;charset=utf-8"));

        let fallback_offered = vec!["STRING".to_string()];
        let fallback_chosen = find_best_utf8_mime(&fallback_offered);
        assert_eq!(fallback_chosen, Some("STRING"));

        let none_offered = vec!["image/jpeg".to_string()];
        assert_eq!(find_best_utf8_mime(&none_offered), None);
    }

    #[test]
    fn test_nonblocking_pipe_write_read() {
        let (read_fd, write_fd) = create_nonblocking_pipe().expect("Failed to create pipe");
        write_text_to_pipe(&write_fd, "Hello Wayland Clipboard!").expect("Write failed");
        drop(write_fd);

        let mut buf = Vec::new();
        let mut chunk = [0u8; 64];
        loop {
            // SAFETY: safe read on pipe file descriptor
            let n = unsafe {
                libc::read(
                    read_fd.as_raw_fd(),
                    chunk.as_mut_ptr() as *mut libc::c_void,
                    chunk.len(),
                )
            };
            if n > 0 {
                buf.extend_from_slice(&chunk[..n as usize]);
            } else if n == 0 {
                break;
            } else {
                let err = std::io::Error::last_os_error();
                if err.kind() == std::io::ErrorKind::WouldBlock {
                    break;
                }
                panic!("Read error: {}", err);
            }
        }

        assert_eq!(String::from_utf8(buf).unwrap(), "Hello Wayland Clipboard!");
    }
}
