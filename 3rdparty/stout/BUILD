load('@glog//:bazel/glog.bzl', 'glog_library')

glog_library(with_gflags=0)

cc_library(
    name = "stout",
    hdrs = glob(["include/stout/**/*.hpp"]),
    includes = glob(["include/stout"]),
    strip_include_prefix = "include",
)

cc_test(
    name = "stout_tests",
    deps = [
        ":stout",
        "@gtest//:gtest",
        "@gmock//:gmock",
        "@glog//:glog",
        "@boost//:headers",
        "@picojson//:headers",
    ],
    srcs = glob(["tests/**/*.cpp"])
)

cc_test(
    name = "bazel_test",
    srcs = ["tests/bazel_test.cpp"],
    deps = [
        "@gtest//:gtest",
        "@gmock//:gmock",
        "@boost//:headers",
        "@glog//:glog",
    ],
)
