new_http_archive(
    name = "boost",
    urls = ["file:3rdparty/boost-1.65.0.tar.gz"],
    strip_prefix = "boost-1.65.0",
    build_file_content = """
cc_library(
    name = "boost",
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
    name = "gtest",
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
    name = "gmock",
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
)"""
)

http_archive(
    name = "protobuf",
    urls = ["https://github.com/google/protobuf/archive/143851ed257b7c24e945396cb4acc0da697dff65.zip"],
    strip_prefix = "protobuf-143851ed257b7c24e945396cb4acc0da697dff65",
)

new_local_repository(
    name = "apr",
    path = "/",
    build_file_content = """
cc_library(
    name = "apr",
    hdrs = glob(["usr/include/apr-1/*.h"]),
    includes = ["usr/include/apr-1"],
    srcs = ["usr/lib64/libapr-1.so"],
    visibility = ["//visibility:public"],
)"""
)

new_local_repository(
    name = "svn",
    path = "/",
    build_file_content = """
cc_library(
    name = "svn",
    hdrs = glob(["usr/include/subversion-1/*.h"]),
    srcs = glob(["usr/lib64/libsvn*.so"]),
    includes = ["usr/include/subversion-1"],
    visibility = ["//visibility:public"],
)"""
)
