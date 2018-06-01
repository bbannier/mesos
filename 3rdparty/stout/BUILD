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
        "@boost//:boost",
        "@picojson//:picojson",
        "@apr//:apr",
        "@svn//:svn",
        "@protobuf//:protobuf",
    ],
    srcs = [
      "tests/adaptor_tests.cpp",
      "tests/bits_tests.cpp",
      "tests/cpp17_tests.cpp",
      # "tests/dynamiclibrary_tests.cpp", # FIXME(bbannier)
      "tests/base64_tests.cpp",
      "tests/error_tests.cpp",
      "tests/interval_tests.cpp",
      # "tests/os_tests.cpp", # FIXME(bbannier)
      "tests/path_tests.cpp",
      "tests/strings_tests.cpp",
      "tests/os/signals_tests.cpp",
      "tests/os/copyfile_tests.cpp",
      "tests/os/filesystem_tests.cpp",
      "tests/os/rmdir_tests.cpp",
      "tests/os/socket_tests.cpp",
      "tests/os/strerror_tests.cpp",
      "tests/os/env_tests.cpp",
      # "tests/os/process_tests.cpp", # FIXME(bbannier)
      "tests/os/sendfile_tests.cpp",
      "tests/os/systems_tests.cpp",
      "tests/hashmap_tests.cpp",
      "tests/recordio_tests.cpp",
      "tests/svn_tests.cpp",
      "tests/hashset_tests.cpp",
      # "tests/flags_tests.cpp", # FIXME(bbannier)
      "tests/main.cpp",
      "tests/result_tests.cpp",
      "tests/none_tests.cpp",
      "tests/duration_tests.cpp",
      "tests/bytes_tests.cpp",
      "tests/ip_tests.cpp",
      "tests/variant_tests.cpp",
      "tests/version_tests.cpp",
      "tests/boundedhashmap_tests.cpp",
      "tests/uri_tests.cpp",
      "tests/uuid_tests.cpp",
      # "tests/json_tests.cpp", # FIXME(bbannier)
      "tests/linkedhashmap_tests.cpp",
      "tests/jsonify_tests.cpp",
      "tests/mac_tests.cpp",
      "tests/some_tests.cpp",
      # "tests/subcommand_tests.cpp", # FIXME(bbannier)
      "tests/cache_tests.cpp",
      "tests/multimap_tests.cpp",
      "tests/option_tests.cpp",
      "tests/lambda_tests.cpp",
      "tests/try_tests.cpp",
      "tests/proc_tests.cpp",
      # "tests/protobuf_tests.cpp", # FIXME(bbannier)
      # "tests/protobuf_tests.proto", # FIXME(bbannier)
      "tests/gzip_tests.cpp",
      "tests/numify_tests.cpp",]
)
