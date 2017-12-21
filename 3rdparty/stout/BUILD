cc_library(
    name = "stout",
    includes = glob(["include/stout/**/*.hpp"]),
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
