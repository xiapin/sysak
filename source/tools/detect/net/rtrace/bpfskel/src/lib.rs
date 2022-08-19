
// see: https://github.com/mikemorris/hello_macro/blob/master/hello_macro_derive/src/lib.rs
use proc_macro::TokenStream;
use syn::{self, token::Token, parse_macro_input, ItemStruct};
use quote::quote;

#[proc_macro_derive(BpfSkelMacro)]
pub fn bpf_skel_derive(input: TokenStream) -> TokenStream {
    let ast = syn::parse(input).unwrap();
    impl_bpf_skel_derive(&ast)
}

// #[proc_macro_attribute]
// pub fn bpf_skel_add_field(_attr: TokenStream, input: TokenStream) -> TokenStream {
//     let itemstruct = parse_macro_input!(input as ItemStruct);
//     utils::macros::do_bpf_skel_add_field(&itemstruct)
// }

fn impl_bpf_skel_derive(ast: &syn::DeriveInput) -> TokenStream {
    let name = &ast.ident;
    // mark skelbuilder type
    let mut o_skelbuilder = None;

    // see: https://github.com/dtolnay/syn/issues/516
    let fields = match &ast.data {
        syn::Data::Struct(syn::DataStruct {
            fields: syn::Fields::Named(fields),
            ..
        }) => &fields.named,
        _ => panic!("expected named fields"),
    };

    for field in fields {
        if let Some(ident) = &field.ident {
            if ident.to_string().ends_with("skel") {
                o_skelbuilder = Some(&field.ty);
            }
        }
    }

    let skelbuilder = o_skelbuilder.unwrap();

    let gen = quote! {
        impl BpfSkel for #name {
            fn load(&mut self, debug: bool, btf: &Option<&str>, enables: Option<Vec<&str>>, disables: Option<Vec<&str>>) -> Result<()> {
                let btf_cstring;
                let mut btf_cstring_ptr = std::ptr::null();
                if let Some(btf) = btf {
                    btf_cstring = std::ffi::CString::new(btf.clone())?;
                    btf_cstring_ptr = btf_cstring.as_ptr();
                }
                let mut skel_builder = #skelbuilder::default();
                skel_builder.obj_builder.debug(debug);
                let mut open_opts = skel_builder.obj_builder.opts(std::ptr::null());
                open_skel = skel_builder.open_opts(open_opts)?;
                let mut open_skel = skel_builder.open()?;

                if let Some(xs) = enables {
                    for x in xs {
                        if let Some(y) = open_skel.obj.prog_mut(x) {
                            y.set_autoload(true);
                        } else {
                            bail!("failed to find program: {}", x)
                        }
                    }
                }

                if let Some(xs) = disables {
                    for x in xs {
                        if let Some(y) = open_skel.obj.prog_mut(x) {
                            y.set_autoload(false);
                        } else {
                            bail!("failed to find program: {}", x)
                        }
                    }
                }

                Ok(open_skel.load()?)
            }

            fn attach(&mut self, src: Option<&str>, dst: Option<&str>) -> Result<()> {
                if let Some(x) = src {
                    if let Some(y) = dst {

                    }
                }
                Ok(self.skel.attach()?)
            }
        }
    };

    gen.into()
}
