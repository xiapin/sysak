use rust_embed::RustEmbed;

#[cfg(feature = "embed-rtrace")]
#[derive(RustEmbed)]
#[folder = "resources"]
pub struct Asset;

#[cfg(not(feature = "embed-rtrace"))]
#[derive(RustEmbed)]
#[folder = "resources"]
#[include = "*.db"]
pub struct Asset;
