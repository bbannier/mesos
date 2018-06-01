load('@glog//:bazel/glog.bzl', 'glog_library')

glog_library(with_gflags=0)

cc_library(
    name = "all",
    hdrs = glob(["include/stout/**/*.hpp"]),
    includes = glob(["include/stout"]),
    strip_include_prefix = "include",
    deps = [
        "@picojson//:picojson",
    ],
    linkopts = ["-lrt", "-ldl"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "tests",
    deps = [
        ":all",
        ":cc_protobuf_tests_",
        "@gtest//:all",
        "@gmock//:all",
        "@glog//:glog",
        "@boost//:all",
        "@apr//:all",
        "@svn//:all",
        "@com_google_protobuf//:protobuf",
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
