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
        "@com_google_protobuf//:protobuf",
    ],
    defines = [
        'BUILD_DIR=\\".\\"',
    ],
    includes = ["src"],

    # srcs = glob([
    #   "src/tests/main.cpp",
    #   "src/tests/socket_tests.cpp",
    # ]),

    # srcs = glob(
    #     [
    #       "src/tests/*.*pp",
    #       "src/*.hpp",
    #     ],
    #     exclude = [
    #         "src/tests/grprc_tests.cpp", # FIXME(bbannier)
    #         "src/tests/benchmarks.cpp",
    # ]),

    srcs = [
      "src/tests/after_tests.cpp",
      "src/tests/collect_tests.cpp",
      "src/tests/count_down_latch_tests.cpp",
      "src/tests/decoder_tests.cpp",
      "src/tests/encoder_tests.cpp",
      "src/tests/future_tests.cpp",
      # "src/tests/grpc_tests.cpp", # FIXME(bbannier)
      # "src/tests/http_tests.cpp", # FIXME(bbannier)
      "src/tests/io_tests.cpp",
      "src/tests/jwt_tests.cpp",
      "src/tests/limiter_tests.cpp",
      "src/tests/loop_tests.cpp",
      "src/tests/main.cpp",
      "src/tests/metrics_tests.cpp",
      "src/tests/mutex_tests.cpp",
      "src/tests/owned_tests.cpp",
      # "src/tests/process_tests.cpp", # FIXME(bbannier)
      "src/tests/profiler_tests.cpp",
      "src/tests/queue_tests.cpp",
      "src/tests/reap_tests.cpp",
      "src/tests/rwlock_tests.cpp",
      "src/tests/sequence_tests.cpp",
      "src/tests/shared_tests.cpp",
      "src/tests/socket_tests.cpp",
      # "src/tests/ssl_tests.cpp", # FIXME(bbannier)
      "src/tests/state_machine_tests.cpp",
      "src/tests/statistics_tests.cpp",
      "src/tests/subprocess_tests.cpp",
      "src/tests/system_tests.cpp",
      "src/tests/time_tests.cpp",
      "src/tests/timeseries_tests.cpp",
    ]

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
    name = "test_linkee",
    srcs = ["src/tests/test_linkee.cpp"],
    includes = ["src"],
    deps = [
        ":process",
        # "@gmock//:gmock",
        # "@gtest//:gtest",
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
    tests = [
        ":process_tests",
        ":ssl_client",
    ],
)
