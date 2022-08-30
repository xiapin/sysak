use crate::utils::macros::*;

ebpf_common_use!(eventdiagnosing);

#[derive(Debug, StructOpt)]
pub struct EventDiagnosingCommand {
    #[structopt(long, help = "Tracing sched event including sched in and sched out")]
    sched: bool,
    #[structopt(long, help = "Tracing wakeup event")]
    wakeup: bool,
    #[structopt(long, help = "Tracing vring interrupt event")]
    vring: bool,
    #[structopt(long, help = "Tracing kvmexit event")]
    kvmexit: bool,
    //
  
}

pub fn build_event_diagnosing(
    opts: &EventDiagnosingCommand,
    debug: bool,
    btf: &Option<String>,
) -> Result<()> {




    Ok(())
}
