use chrono::Local;
use log::LevelFilter;
use log::Log;
use log::Metadata;
use log::Record;
use log::SetLoggerError;
use std::fs::File;
use std::fs::OpenOptions;
use std::io::Write;
use std::io::{self};
use std::path::Path;
use std::sync::Mutex;

struct FileLogger {
    file: Mutex<File>,
}

impl FileLogger {
    fn new(log_path: &str) -> io::Result<FileLogger> {
        let path = Path::new(log_path);
        if let Some(dir_path) = path.parent() {
            std::fs::create_dir_all(dir_path)?;
        }

        let file = OpenOptions::new()
            .create(true)
            .write(true)
            .append(true)
            .open(log_path)?;
        Ok(FileLogger {
            file: Mutex::new(file),
        })
    }
}

impl Log for FileLogger {
    fn enabled(&self, metadata: &Metadata) -> bool {
        metadata.level() <= log::Level::Info
    }

    fn log(&self, record: &Record) {
        if self.enabled(record.metadata()) {
            let now = Local::now();
            let log_entry = format!("{} - {} - {}\n", now, record.level(), record.args());
            if let Ok(mut file) = self.file.lock() {
                let _ = writeln!(file, "{}", log_entry);
            }
        }
    }

    fn flush(&self) {}
}

pub fn setup_file_logger(verbose: bool) -> Result<(), SetLoggerError> {
    let start_time = Local::now().format("%Y-%m-%dT%H:%M:%S").to_string();
    let log_file_path = format!("/var/log/rtrace/{}.log", start_time);

    let logger = FileLogger::new(&log_file_path).expect("Unable to create logger");

    log::set_boxed_logger(Box::new(logger)).map(|()| {
        log::set_max_level(if verbose {
            LevelFilter::Debug
        } else {
            LevelFilter::Info
        })
    })
}
