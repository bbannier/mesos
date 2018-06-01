new_http_archive(
    name = "boost",
    urls = ["file:3rdparty/boost-1.53.0.tar.gz"],
    strip_prefix = "boost-1.53.0",
    build_file_content =
"""
cc_library(
    name = "headers",
    hdrs = glob(["boost/**"]),
    includes = ["."],
    visibility = ["//visibility:public"],
)
"""
)

new_http_archive(
    name = "gtest",
    urls = ["file:3rdparty/googletest-release-1.8.0.tar.gz"],
    strip_prefix = "googletest-release-1.8.0",
    build_file_content =
"""
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
)
""",
)

new_http_archive(
    name = "gmock",
    urls = ["file:3rdparty/googletest-release-1.8.0.tar.gz"],
    strip_prefix = "googletest-release-1.8.0",
    build_file_content =
"""
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
)
""",
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
    build_file_content =
"""
cc_library(
  name = "headers",
  hdrs = ["picojson.h"],
  includes = ["."],
  visibility = ["//visibility:public"],
)
"""
)
