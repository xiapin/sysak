extern crate proc_macro;
use proc_macro::TokenStream;
use quote::quote;
use syn;

use anyhow::Result;

macro_rules! define_perf_event_channel {
    () => {
        static GLOBAL_TX: Lazy<Mutex<Option<crossbeam_channel::Sender<(usize, Vec<u8>)>>>> =
            Lazy::new(|| Mutex::new(None));

        pub fn handle_event(_cpu: i32, data: &[u8]) {
            let event = Vec::from(data);
            GLOBAL_TX
                .lock()
                .unwrap()
                .as_ref()
                .unwrap()
                .send((_cpu as usize, event))
                .unwrap();
        }

        pub fn handle_lost_events(cpu: i32, count: u64) {
            eprintln!("Lost {} events on CPU {}", count, cpu);
        }
    };
}

pub (crate) use define_perf_event_channel;

// pub fn impl_perf_event_channel() {
//     let gen = quote! {
//         static GLOBAL_TX: Lazy<Mutex<Option<crossbeam_channel::Sender<(usize, Vec<u8>)>>>> =
//     Lazy::new(|| Mutex::new(None));

//         pub fn handle_event(_cpu: i32, data: &[u8]) {
//             let event = Vec::from(data);
//             GLOBAL_TX
//                 .lock()
//                 .unwrap()
//                 .as_ref()
//                 .unwrap()
//                 .send((_cpu as usize, event))
//                 .unwrap();
//         }

//         pub fn handle_lost_events(cpu: i32, count: u64) {
//             eprintln!("Lost {} events on CPU {}", count, cpu);
//         }
//     };

//     gen.into()
// }

pub trait BpfSkel {
    fn open(&mut self, debug: bool, btf: &Option<&str>);
    fn load(&mut self, enables: Option<Vec<&str>>, disables: Option<Vec<&str>>) -> Result<()>;
    fn attach(&mut self, enables: Option<Vec<&str>>, disables: Option<Vec<&str>>) -> Result<()>;
}

// fn open_load_skel<'a>(debug: bool, btf: &Option<String>) -> Result<LatencySkel<'a>> {
//     let btf_cstring;
//     let mut btf_cstring_ptr = std::ptr::null();
//     if let Some(btf) = btf {
//         btf_cstring = CString::new(btf.clone())?;
//         btf_cstring_ptr = btf_cstring.as_ptr();
//     }

//     let mut skel_builder = LatencySkelBuilder::default();
//     skel_builder.obj_builder.debug(debug);
//     let mut open_opts = skel_builder.obj_builder.opts(std::ptr::null());
//     open_opts.btf_custom_path = btf_cstring_ptr;
//     let mut open_skel = skel_builder.open_opts(open_opts)?;
//     Ok(open_skel.load()?)
// }

pub fn bpf_skel_derive(input: TokenStream) -> TokenStream {
    let ast = syn::parse(input).unwrap();
    impl_bpf_skel_derive(&ast)
}

fn impl_bpf_skel_derive(ast: &syn::DeriveInput) -> TokenStream {
    let name = &ast.ident;

    let gen = quote! {
        impl BpfSkel for #name {
            fn open(&mut self, debug: bool, btf: &Option<&str>) -> Result<()>{
                let btf_cstring;
                let mut btf_cstring_ptr = std::ptr::null();
                if let Some(btf) = btf {
                    btf_cstring = CString::new(btf.clone())?;
                    btf_cstring_ptr = btf_cstring.as_ptr();
                }

                self.skel_builder.obj_builder.debug(debug);
                let mut open_opts =self.skel_builder.obj_builder.opts(std::ptr::null());
                self.open_skel = sself.kel_builder.open_opts(open_opts)?;
                Ok(())
            }

            fn load(&mut self, enables: Option<Vec<&str>>, disables: Option<Vec<&str>>) -> Result<()> {
                Ok(self.open_skel.load()?)
            }

            fn attach(&mut self, enables: Option<Vec<&str>>, disables: Option<Vec<&str>>) -> Result<()> {
                Ok(self.skel.attach()?)
            }
        }
    };

    gen.into()
}
