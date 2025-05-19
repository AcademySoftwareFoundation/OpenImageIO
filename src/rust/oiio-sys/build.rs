// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

use anyhow::Result;

const NAMES: &[&str] = &["typedesc"];

fn main() -> Result<()> {
    // Skip linking on docs.rs: https://docs.rs/about/builds#detecting-docsrs
    let building_docs = std::env::var("DOCS_RS").is_ok();
    if building_docs {
        println!("cargo:rustc-cfg=docsrs");
        return Ok(());
    }

    let mut include_paths = Vec::new();

    let pkgconfig = pkg_config::probe_library("OpenImageIO")?;
    include_paths.extend(pkgconfig.include_paths);

    let std_version = std::env::var("OIIO_CXX_STANDARD").unwrap_or("c++17".to_string());

    cxx_build::bridges(NAMES.iter().map(|s| format!("src/{}.rs", s)))
        .files(NAMES.iter().map(|s| format!("src/{}.cpp", s)))
        .std(&std_version)
        .includes(&include_paths)
        .compile("oiio-sys");

    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=src/lib.rs");

    for name in NAMES {
        println!("cargo:rerun-if-changed=src/{}.rs", name);
        println!("cargo:rerun-if-changed=src/{}.cpp", name);
        println!("cargo:rerun-if-changed=include/{}.h", name);
    }

    for link_path in pkgconfig.link_paths {
        println!("cargo:rustc-link-search=native={}", link_path.display());
    }

    for lib in pkgconfig.libs {
        println!("cargo:rustc-link-lib={}", lib);
    }

    Ok(())
}
