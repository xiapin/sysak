use anyhow::bail;
use anyhow::Result;
use regex::Regex;

#[derive(Debug, Default)]
pub struct Status {
    active_connections: u32,
    accepts: u32,
    handled: u32,
    requests: u32,
    reading: u32,
    writing: u32,
    waiting: u32,

    url: String,
    last_accepts: usize,
    last_handled: usize,
    last_requests: usize,
}

impl Status {
    pub fn new(url: String) -> Self {
        let mut status = Status {
            url,
            ..Default::default()
        };

        let _ = status.refresh();
        status
    }

    pub fn metrics(&mut self) -> Option<String> {
        match self.refresh() {
            Ok(()) => Some(self.to_string()),
            Err(e) => {
                log::error!("{}", e);
                None
            }
        }
    }

    fn refresh(&mut self) -> Result<()> {
        let status_string = reqwest::blocking::get(&self.url)?.text()?;
        let numbers = parse_text(&status_string);
        if numbers.len() != 7 {
            bail!("wrong status format: {}", status_string);
        }

        self.active_connections = numbers[0] as u32;
        self.accepts = (numbers[1] - self.last_accepts) as u32;
        self.handled = (numbers[2] - self.last_handled) as u32;
        self.requests = (numbers[3] - self.last_requests) as u32;
        self.reading = numbers[4] as u32;
        self.writing = numbers[5] as u32;
        self.waiting = numbers[6] as u32;

        self.last_accepts = numbers[1];
        self.last_handled = numbers[2];
        self.last_requests = numbers[3];
        Ok(())
    }
}

impl ToString for Status {
    fn to_string(&self) -> String {
        format!("activeConnections={},accepts={},handled={},requests={},reading={},writing={},waiting={}", self.active_connections, self.accepts, self.handled, self.requests, self.reading, self.writing, self.waiting)
    }
}

fn parse_text(status_string: &str) -> Vec<usize> {
    let re = Regex::new(r"\d+").unwrap();
    let mut numbers = Vec::new();

    for capture in re.captures_iter(status_string) {
        if let Ok(number) = capture[0].parse::<usize>() {
            numbers.push(number);
        }
    }

    numbers
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn resolve_status_string() {
        let text = r#"Active connections: 1 
        server accepts handled requests
         136859 136859 137052 
        Reading: 0 Writing: 1 Waiting: 0 "#;
        let nums = parse_text(text);
        assert_eq!(nums.len(), 7);
        assert_eq!(nums[0], 1);
        assert_eq!(nums[1], 136859);
        assert_eq!(nums[2], 136859);
        assert_eq!(nums[3], 137052);
        assert_eq!(nums[4], 0);
        assert_eq!(nums[5], 1);
        assert_eq!(nums[6], 0);
    }
}
