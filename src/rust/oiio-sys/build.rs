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
    let mut lib_paths = Vec::new();
    let mut libs = Vec::new();

    let oiio_include_dir = std::env::var("OIIO_INCLUDE_DIR").map(|v| std::path::PathBuf::from(v));
    let oiio_lib_dir = std::env::var("OIIO_LIBRARY_DIR").map(|v| std::path::PathBuf::from(v));

    if oiio_include_dir.is_ok() && oiio_lib_dir.is_ok() {
        let oiio_include_dir = oiio_include_dir.unwrap();
        let oiio_lib_dir = oiio_lib_dir.unwrap();
        let mut paths = glob::glob(
            oiio_lib_dir
                .join("libOpenImageIO_d.*")
                .to_string_lossy()
                .as_ref(),
        )
        .expect("Could not open the OpenImageIO directory.");

        if paths.next().is_some() {
            libs.extend_from_slice(&["OpenImageIO_d".into(), "OpenImageIO_Util_d".into()]);
        } else {
            libs.extend_from_slice(&["OpenImageIO".into(), "OpenImageIO_Util".into()]);
        }

        include_paths.push(oiio_include_dir);
        lib_paths.push(oiio_lib_dir);
    } else {
        let pkgconfig = pkg_config::probe_library("OpenImageIO")?;
        include_paths.extend(pkgconfig.include_paths);
        lib_paths.extend(pkgconfig.link_paths);
        libs.extend(pkgconfig.libs);
    }

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

    for link_path in lib_paths {
        println!("cargo:rustc-link-search=native={}", link_path.display());
    }

    for lib in libs {
        println!("cargo:rustc-link-lib={}", lib);
    }

    Ok(())
}
