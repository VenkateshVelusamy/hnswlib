fn main() -> miette::Result<()> {
    tonic_build::compile_protos("proto/helloworld/helloworld.proto").unwrap();
    let path = std::path::PathBuf::from("src");
    let mut b = autocxx_build::Builder::new("src/hnswrustindex.rs", [&path]).build()?;
    b.flag_if_supported("-std=c++14").compile("hnswrustindex");

    Ok(())
}
