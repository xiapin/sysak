use proc_macro::TokenStream;
use quote::{format_ident, quote, ToTokens};
use syn::{
    parse_macro_input, punctuated::Punctuated, spanned::Spanned, token::Comma, Data, DataEnum,
    DataStruct, DeriveInput, Field, Fields, FieldsNamed, Ident, Meta, MetaList, NestedMeta, Path,
    Result, Variant,
};

macro_rules! fail {
    ($msg:literal) => {
        syn::Error::new(proc_macro2::Span::call_site(), $msg)
            .to_compile_error()
            .into()
    };
    ($tkns:ident, $msg:literal) => {
        syn::Error::new_spanned($tkns, $msg)
            .to_compile_error()
            .into()
    };
}

/// SkelBuilder: generate builder for bpf skel
/// your original structure should have below fields:
///  1. MUST HAVE: pub skel: [NAME]Skel<'a>,
///  2. OPTIONAL : rx: crossbeam_channel::Receiver<(usize, Vec<u8>)>,
#[proc_macro_derive(SkelBuilder)]
pub fn skelbuilder_derive(input: TokenStream) -> TokenStream {
    let syn = parse_macro_input!(input as DeriveInput);
    let gen = impl_skelbuilder(&syn);
    gen.into()
}

fn impl_skelbuilder(syn: &DeriveInput) -> proc_macro2::TokenStream {
    let (struct_ident, fields) = match syn.data {
        Data::Struct(ref datastruct) => (&syn.ident, &datastruct.fields),
        _ => {
            return syn::Error::new(syn.span(), "expected struct type")
                .into_compile_error()
                .into();
        }
    };

    // skel name default is struct name
    let mut skel_name = struct_ident.to_string();

    let named_fields = if let Data::Struct(DataStruct {
        fields: Fields::Named(FieldsNamed { ref named, .. }),
        ..
    }) = syn.data
    {
        named
    } else {
        return fail!("derive builder only supports structs with named fields");
    };

    let builder_name = format_ident!("{}Builder", skel_name.to_string());

    let build_method_token = impl_skelbuiler_build(struct_ident, &builder_name);
    let builder_token = impl_skelbuilder_builder(struct_ident, &skel_name, named_fields);

    quote! {

        static GLOBAL_TX: Lazy<Mutex<Option<crossbeam_channel::Sender<(usize, Vec<u8>)>>>> =
            Lazy::new(|| Mutex::new(None));

        fn handle_event(_cpu: i32, data: &[u8]) {
            let event = Vec::from(data);
            GLOBAL_TX
                .lock()
                .unwrap()
                .as_ref()
                .unwrap()
                .send((_cpu as usize, event))
                .unwrap();
        }

        fn handle_lost_events(cpu: i32, count: u64) {
            eprintln!("Lost {} events on CPU {}", count, cpu);
        }

        #builder_token
        #build_method_token
    }
}

fn impl_skelbuiler_build(
    struct_ident: &syn::Ident,
    builder_name: &Ident,
) -> proc_macro2::TokenStream {
    quote! {
        impl<'a> #struct_ident <'a>{
            pub fn builder() -> #builder_name <'a> {
                #builder_name {
                    openskel: None,
                    skel: None,
                    rx: None,
                }
            }
        }
    }
}

fn impl_skelbuilder_builder(
    struct_ident: &Ident,
    skel_name: &String,
    named_fields: &Punctuated<Field, Comma>,
) -> proc_macro2::TokenStream {
    let builder_name = format_ident!("{}Builder", skel_name.to_string());
    let openskel_name = format_ident!("Open{}Skel", skel_name);
    let skelskel_name = format_ident!("{}Skel", skel_name);

    let open_method = impl_skelbuilder_builder_methods_open(skel_name);
    let load_method = impl_skelbuilder_builder_methods_load();
    let open_perf_method = impl_skelbuilder_builder_methods_open_perf();
    let attach_method = impl_skelbuilder_builder_methods_attach();
    let build_method = impl_skelbuilder_builder_methods_build(struct_ident, named_fields);

    quote! {
        pub struct #builder_name<'a> {
            pub openskel: Option<#openskel_name<'a>>,
            pub skel: Option<#skelskel_name<'a>>,
            rx: Option<crossbeam_channel::Receiver<(usize, Vec<u8>)>>,
        }

        impl <'a> #builder_name<'a> {

            #open_method
            #load_method
            #open_perf_method
            #attach_method
            #build_method

        }
    }
}

fn impl_skelbuilder_builder_methods_open(skel_name: &String) -> proc_macro2::TokenStream {
    let skel_builder_ident = format_ident!("{}SkelBuilder", skel_name);
    quote! {
        pub fn open(&mut self, debug: bool, btf: &Option<String>) -> &mut Self {
            if let Some(openskel) = &self.openskel {
                panic!("don't try to open skeleton object twice")
            }
            let btf_cstring;
            let mut btf_cstring_ptr = std::ptr::null();
            if let Some(x) = btf {
                btf_cstring = std::ffi::CString::new(x.clone())
                    .expect(&format!("failed to create CString from :{}", x));
                btf_cstring_ptr = btf_cstring.as_ptr();
            }

            let mut skel_builder = #skel_builder_ident::default();
            skel_builder.obj_builder.debug(debug);
            let mut open_opts = skel_builder.obj_builder.opts(std::ptr::null());
            open_opts.btf_custom_path = btf_cstring_ptr;
            self.openskel = Some(
                skel_builder
                    .open_opts(open_opts)
                    .expect("failed to open target skeleton object"),
            );
            log::debug!(
                "open skeleton object sucessfully, btf: {:?}, debug: {}",
                btf,
                debug
            );
            self
        }
    }
}

fn impl_skelbuilder_builder_methods_load() -> proc_macro2::TokenStream {
    quote! {
        // load with some bpf program disabled or enabled
        pub fn load_enabled(&mut self, enabled: Vec<(&str, bool)>) -> &mut Self {
            let mut has_enabled = false;
            let mut has_disabled = false;
            for enable in &enabled {
                if enable.1 {
                    has_enabled = true;
                } else {
                    has_disabled = true;
                }
            }

            if let Some(openskel) = &mut self.openskel {
                for x in &enabled {
                    if has_enabled && !has_disabled {
                        log::debug!("disable autoload of all bpf program");
                        for x in openskel.obj.progs_iter_mut() {
                            x.set_autoload(false).expect("failed to set autoload");
                        }
                    }

                    if let Some(y) = openskel.obj.prog_mut(x.0) {
                        log::debug!("enabled({}) autoload of {} bpf program", x.1, x.0);
                        y.set_autoload(x.1).expect("failed to set autoload");
                        continue;
                    }
                    log::error!("failed to find bpf program: {}", x.0);
                }
            }
            self.load()
        }

        // disabled autoload of all bpf program
        pub fn load_nothing(&mut self) -> &mut Self {
            log::debug!("disable autoload of all bpf program");
            if let Some(openskel) = &mut self.openskel {
                for x in openskel.obj.progs_iter_mut() {
                    x.set_autoload(false).expect("failed to set autoload");
                }
            }
            self.load()
        }

        // enabled autoload of all bpf program
        pub fn load(&mut self) -> &mut Self {
            if let Some(mut openskel) = self.openskel.take() {
                log::debug!("start loading bpf program");
                self.skel = Some(openskel.load().expect("failed to load bpf program"));
                return self;
            }
            panic!("open skeleton object first")
        }
    }
}

fn impl_skelbuilder_builder_methods_attach() -> proc_macro2::TokenStream {
    quote! {
        pub fn attach(&mut self) -> &mut Self {
            if let Some(skel) = &mut self.skel {
                log::debug!("start attaching bpf program");
                skel.attach().expect("failed to attach bpf program");
                return self;
            }
            panic!("Before attach, you should open skeleton object and load bpf program first")
        }
    }
}

fn impl_skelbuilder_builder_methods_open_perf() -> proc_macro2::TokenStream {
    quote! {
        // open perf buffer and inital an perf poll thread with 200ms timeout
        pub fn open_perf(&mut self) -> &mut Self {
            if self.skel.is_none() {
                panic!("Before open perf buffer, you should load bpf program first")
            }

            let mut tmp_rx = None;
            if let Some(skel) = &mut self.skel {
                let (tx, rx) = crossbeam_channel::unbounded();
                *GLOBAL_TX.lock().unwrap() = Some(tx);
                tmp_rx = Some(rx);
                let perf = utils::perf::PerfBufferBuilder::new(skel.maps_mut().perf_map())
                    .sample_cb(handle_event)
                    .lost_cb(handle_lost_events)
                    .build()
                    .expect("failed to open perf buffer");

                log::debug!("start perf thread");
                std::thread::spawn(move || loop {
                    perf.poll(std::time::Duration::from_millis(200)).unwrap();
                });
            }

            self.rx = tmp_rx;
            self
        }
    }
}

// implement builder methods: build
fn impl_skelbuilder_builder_methods_build(
    struct_ident: &Ident,
    named_fields: &Punctuated<Field, Comma>,
) -> proc_macro2::TokenStream {
    let init_fields = named_fields.iter().map(|field| {
        let name = &field.ident;

        let mut skip = false;
        if let Some(ident) = &field.ident {
            let field_name = ident.to_string();
            if field_name.contains("skel") || field_name.contains("rx") {
                skip = true;
            }
        }

        if !skip {
            quote! { #name: Default::default(), }
        } else {
            quote! {}
        }
    });
    quote! {
        pub fn build(&mut self) -> #struct_ident<'a> {
            #struct_ident {
                skel: self.skel.take().unwrap(),
                rx: self.rx.take(),
                #(#init_fields)*
            }
        }
    }
}
