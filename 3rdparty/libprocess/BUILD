cc_library(
    name = "process",
    hdrs = glob(["include/process/**/*.hpp"]),
    strip_include_prefix = "include",
    srcs = glob(
        ["src/**/*.*pp"],
        exclude = [
            "src/grpc.cpp", # FIXME(bbannier)
            "src/libev.cpp",
            "src/libev_poll.cpp",
            "src/subprocess_windows.cpp",
            "src/tests/**",
            ]
    ),
    deps = [
        "//3rdparty/stout",
        "@boost//:boost",
        "@event//:event",
        "@glog//:glog",
        "@http_parser//:http_parser",
        "@openssl//:openssl",
    ],
    defines = ["USE_SSL_SOCKET=1"],
)

cc_test(
    name = "process_tests",
    shard_count=10,
    deps = [
        ":process",
        "@gmock//:gmock",
        "@gtest//:gtest",
    ],
    data = [
        ":ssl_client",
        ":test-linkee",
    ],
    defines = [
        'BUILD_DIR=\\".\\"',
    ],
    includes = ["src"],

    srcs = glob(
        [
          "src/tests/*.*pp",
          "src/*.hpp",
        ],
        exclude = [
            "src/tests/grpc_tests.cpp", # FIXME(bbannier)
            "src/tests/benchmarks.cpp",
            "src/tests/ssl_client.cpp",
            "src/tests/test_linkee.cpp",
            "src/tests/test_echo.cpp",
    ]),
)

cc_binary(
    name = "benchmark",
    srcs = ["src/tests/benchmarks.cpp"],
    deps = [
        ":process",
        ":cc_benchmarks_",
        "@gmock//:gmock",
        "@gtest//:gtest",
    ],
    testonly = 1,
)
proto_library(
    name = "benchmarks",
    srcs = ["src/tests/benchmarks.proto"],
    testonly = 1,
)
cc_proto_library(
    name = "cc_benchmarks",
    deps = [":benchmarks"],
    testonly = 1,
)
cc_library(
    name = "cc_benchmarks_",
    deps = [":cc_benchmarks"],
    includes = ["src/tests"],
    testonly = 1,
)

cc_binary(
    name = "test-linkee",
    srcs = ["src/tests/test_linkee.cpp"],
    includes = ["src"],
    deps = [
        ":process",
    ],
    testonly = 1,
)

cc_binary(
    name = "test_echo",
    srcs = ["src/tests/test_echo.cpp"],
    deps = [
        ":process",
    ],
    testonly = 1,
)

cc_test(
    name = "ssl_client",
    srcs = ["src/tests/ssl_client.cpp"],
    includes = ["src"],
    deps = [
        ":process",
        "@gmock//:gmock",
        "@gtest//:gtest",
    ],
    testonly = 1,
)

test_suite(
    name = "tests",
)
