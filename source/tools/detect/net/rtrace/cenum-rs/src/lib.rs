use proc_macro::TokenStream;
use quote::quote;
use syn::{
    parse_macro_input, punctuated::Punctuated, spanned::Spanned, Data, DataEnum, DeriveInput, Meta,
    MetaList, NestedMeta, Path, Result, Variant,
};

// Generates the TryFrom, From and Display impl
#[proc_macro_derive(CEnum, attributes(cenum))]
pub fn cenum_derive(input: TokenStream) -> TokenStream {
    let syn = parse_macro_input!(input as DeriveInput);
    let gen = impl_cenum(&syn);
    gen.into()
}

fn impl_cenum(syn: &DeriveInput) -> proc_macro2::TokenStream {
    // default enum type is u32
    let default_type_token = "u32".parse().unwrap();
    let mut enum_type_token = &default_type_token;
    if let Some(attr) = syn.attrs.first() {
        enum_type_token = &attr.tokens;
    }

    let (enum_ident, variants) = match syn.data {
        Data::Enum(ref dataenum) => (&syn.ident, &dataenum.variants),
        _ => {
            return syn::Error::new(syn.span(), "expected enum type")
                .into_compile_error()
                .into();
        }
    };

    let mut tryfrom_rust2c_token = Vec::new();
    let mut tryfrom_c2rust_token = Vec::new();
    let mut from_rust2c_token = Vec::new();
    let mut from_c2rust_token = Vec::new();
    let mut display_token = Vec::new();

    for variant in variants {
        let ident = &variant.ident;
        match cenum_attr_value_display(variant) {
            Ok((Some(value_token), Some(display_str))) => {
                tryfrom_rust2c_token.push(quote! {
                    #enum_ident::#ident  => Ok(#value_token),
                });

                from_rust2c_token.push(quote! {
                    #enum_ident::#ident  => #value_token,
                });

                tryfrom_c2rust_token.push(quote! {
                    #value_token=> Ok( #enum_ident::#ident),
                });

                from_c2rust_token.push(quote! {
                    #value_token=> #enum_ident::#ident,
                });

                display_token.push(quote! {
                    #enum_ident::#ident => #display_str,
                });
            }
            _ => {
                return syn::Error::new(syn.span(), "expected value or display attribute")
                    .into_compile_error()
                    .into();
            }
        }
    }

    let output = quote! {
        impl std::convert::TryFrom<#enum_type_token> for #enum_ident {
            type Error = String;
            fn try_from(val: #enum_type_token) -> Result<Self, Self::Error> {
                match val {
                    #(#tryfrom_c2rust_token)*
                    _ => Err(format!("convert {} failurely", val))
                }
            }
        }

        // impl std::convert::From<#enum_type_token> for #enum_ident {
        //     fn from(val: #enum_type_token) -> Self {
        //         match val {
        //             #(#from_c2rust_token)*
        //             _ => panic!("failed to convert")
        //         }
        //     }
        // }

        impl std::convert::TryFrom<#enum_ident> for #enum_type_token {
            type Error = &'static str;
            fn try_from(ident: #enum_ident) -> Result<Self, Self::Error> {
                match ident {
                    #(#tryfrom_rust2c_token)*
                    _ => Err("failed to convert")
                }
            }
        }

        // impl std::convert::From<#enum_ident> for #enum_type_token {
        //     fn from(ident: #enum_ident) -> Self {
        //         match ident {
        //             #(#from_rust2c_token)*
        //             _ => panic!("failed to convert")
        //         }
        //     }
        // }

        impl std::fmt::Display for #enum_ident {
            fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
                let out = match self {
                    #(#display_token)*
                };
                write!(f, "{}", out)
            }
        }
    };
    output
}

// Punctuated<NestedMeta, Token![,]>
fn cenum_attr_value_display(
    variant: &Variant,
) -> Result<(Option<proc_macro2::TokenStream>, Option<String>)> {
    let mut value_token = None;
    let mut display_str = None;
    let attr = variant.attrs.first().unwrap().parse_meta()?;

    if let Meta::List(MetaList {
        ref path,
        ref nested,
        ..
    }) = attr
    {
        if let Some(p) = path.segments.first() {
            if p.ident == "cenum" {
                for nest in nested {
                    if let NestedMeta::Meta(syn::Meta::NameValue(kv)) = nest {
                        if kv.path.is_ident("value") {
                            if let syn::Lit::Str(ref ident_str) = kv.lit {
                                value_token = Some(ident_str.value().parse().unwrap());
                            }
                        } else if kv.path.is_ident("display") {
                            if let syn::Lit::Str(ref ident_str) = kv.lit {
                                display_str = Some(ident_str.value());
                            }
                        }
                    }
                }
            }
        }
    }
    Ok((value_token, display_str))
}

