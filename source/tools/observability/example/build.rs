use coolbpf_builder::CoolBPFBuilder;

fn main() {
    CoolBPFBuilder::default()
        .source("src/bpf/example.bpf.c")
        .header("src/bpf/example.h")
        .build()
        .expect("Failed to compile example eBPF program");
}
