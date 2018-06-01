new_http_archive(
    name = "boost",
    urls = ["file:3rdparty/boost-1.65.0.tar.gz"],
    strip_prefix = "boost-1.65.0",
    build_file_content = """
cc_library(
    name = "all",
    hdrs = glob(["boost/**"]),
    includes = ["."],
    visibility = ["//visibility:public"],
)"""
)

new_http_archive(
    name = "gtest",
    urls = ["file:3rdparty/googletest-release-1.8.0.tar.gz"],
    strip_prefix = "googletest-release-1.8.0",
    build_file_content = """
genrule(
    name = "gtest_genrule",
    srcs = glob(["*", "**/*"]),
    outs = ["libgtest.a"],
    cmd = "cmake external/gtest && make gtest && cp googlemock/gtest/libgtest.a $@",
)

cc_library(
    name = "all",
    srcs = ["libgtest.a"],
    hdrs = glob(["googletest/include/**/*.h*"]),
    includes = ["googletest/include"],
    strip_include_prefix = "googletest/include",
    visibility = ["//visibility:public"],
)"""
)

new_http_archive(
    name = "gmock",
    urls = ["file:3rdparty/googletest-release-1.8.0.tar.gz"],
    strip_prefix = "googletest-release-1.8.0",
    build_file_content = """
genrule(
    name = "gmock_genrule",
    srcs = glob(["*", "**/*"]),
    outs = ["libgmock.a"],
    cmd = "cmake external/gmock && make gmock && cp googlemock/libgmock.a $@",
)

cc_library(
    name = "all",
    srcs = ["libgmock.a"],
    hdrs = glob(["googlemock/include/gmock/**/*.h*"]),
    includes = ["googlemock/include"],
    strip_include_prefix = "googlemock/include",
    visibility = ["//visibility:public"],
)"""
)

http_archive(
    name = "glog",
    url = "https://github.com/google/glog/archive/abce78806c8a93d99cf63a5a44ff09873f46b56f.zip",
    strip_prefix = "glog-abce78806c8a93d99cf63a5a44ff09873f46b56f",
)

# Needs to be declared for glog.
http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "6e16c8bc91b1310a44f3965e616383dbda48f83e8c1eaa2370a215057b00cabe",
    strip_prefix = "gflags-77592648e3f3be87d6c7123eb81cbad75f9aef5a",
    urls = [
        "https://mirror.bazel.build/github.com/gflags/gflags/archive/77592648e3f3be87d6c7123eb81cbad75f9aef5a.tar.gz",
        "https://github.com/gflags/gflags/archive/77592648e3f3be87d6c7123eb81cbad75f9aef5a.tar.gz",
    ],
)

new_http_archive(
    name = "picojson",
    urls = ["https://github.com/kazuho/picojson/archive/34b04e71bbd2ee6349f853ad83628a07ab2498f3.zip"],
    strip_prefix = "picojson-34b04e71bbd2ee6349f853ad83628a07ab2498f3",
    build_file_content = """
cc_library(
  name = "picojson",
  hdrs = ["picojson.h"],
  includes = ["."],
  visibility = ["//visibility:public"],
  defines = ["PICOJSON_USE_INT64", "__STDC_FORMAT_MACROS"]
)"""
)

http_archive(
    name = "com_google_protobuf",
    urls = ["https://github.com/google/protobuf/archive/143851ed257b7c24e945396cb4acc0da697dff65.zip"],
    strip_prefix = "protobuf-143851ed257b7c24e945396cb4acc0da697dff65",
)

new_local_repository(
    name = "apr",
    path = "/usr",
    build_file_content = """
cc_library(
    name = "all",
    hdrs = glob(["include/apr-1/*.h"]),
    includes = ["include/apr-1"],
    srcs = ["lib64/libapr-1.so"],
    visibility = ["//visibility:public"],
)"""
)

new_local_repository(
    name = "svn",
    path = "/usr",
    build_file_content = """
cc_library(
    name = "all",
    hdrs = glob(["include/subversion-1/*.h"]),
    srcs = glob(["lib64/libsvn*.so"]),
    includes = ["include/subversion-1"],
    visibility = ["//visibility:public"],
)"""
)

new_local_repository(
    name = "openssl",
    path = "/usr",
    build_file_content = """
cc_library(
    name = "openssl",
    hdrs = glob(["include/openssl/*.h"]),
    srcs = ["lib64/libssl.so", "lib64/libcrypto.so"],
    includes = ["include/openssl"],
    visibility = ["//visibility:public"],
)"""
)

new_local_repository(
    name = "event",
    path = "/usr",
    build_file_content = """
cc_library(
    name = "all",
    srcs = [
      "lib64/libevent.so",
      "lib64/libevent_pthreads.so",
      "lib64/libevent_openssl.so"],
    linkopts = ["-lz"],
    visibility = ["//visibility:public"],
)"""
)


new_http_archive(
    name = "http_parser",
    urls = ["https://github.com/nodejs/http-parser/archive/5b76466c6b9063e2c5982423962a16f7319c81f8.zip"],
    strip_prefix = "http-parser-5b76466c6b9063e2c5982423962a16f7319c81f8",
    build_file_content = """
cc_library(
    name = "http_parser",
    srcs = ["http_parser.c", "http_parser.h"],
    hdrs = ["http_parser.h"],
    includes = ["."],
    visibility = ["//visibility:public"],
)"""
)
