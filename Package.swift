// swift-tools-version: 5.8

import PackageDescription

let package = Package(
    name: "spdlog",
    platforms: [
        .iOS(.v12),
        .macOS(.v10_14),
    ],
    products: [
        .library(
            name: "spdlog",
            targets: ["spdlog"]),
    ],
    dependencies: [
    ],
    targets: [
        .target(
            name: "spdlog",
            dependencies: [],
            path: ".",
            exclude: [
              "bench",
              "cmake",
              "example",
              "logos",
              "tests",
              "scripts",
            ],
            sources: [
              "src",
              "include",
            ],
            publicHeadersPath: "include",
            cxxSettings: [
                .headerSearchPath("spdlog"),
                .define("SPDLOG_COMPILED_LIB"),
            ],
            linkerSettings: []
        ),
    ],
    cxxLanguageStandard: .cxx11
)
