load('@glog//:bazel/glog.bzl', 'glog_library')

glog_library(with_gflags=0)

cc_library(
    name = "stout",
    hdrs = glob(["include/stout/**/*.hpp"]),
    includes = glob(["include/stout"]),
    strip_include_prefix = "include",
    deps = [
        "@picojson//:picojson",
        "@glog//:glog",
        "@boost//:boost",
        "@apr//:apr",
        "@svn//:svn",
        "@com_google_protobuf//:protobuf",
    ],
    copts = ["-Wno-unused-local-typedefs"],
    linkopts = ["-lrt", "-ldl"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "test",
    deps = [
        ":stout",
        ":cc_protobuf_tests_",
        "@gtest//:gtest",
        "@gmock//:gmock",
    ],
    srcs = glob(["tests/**/*.cpp"]),
)

proto_library(
      name = "protobuf_tests",
      srcs = ["tests/protobuf_tests.proto"],
      testonly = 1,
)
cc_proto_library(
      name = "cc_protobuf_tests",
      deps = [":protobuf_tests"],
      testonly = 1,
)
cc_library(
      name = "cc_protobuf_tests_",
      deps = [":cc_protobuf_tests"],
      includes = ["tests"],
      testonly = 1,
)
