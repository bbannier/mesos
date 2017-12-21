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

new_http_archive(
    name = "glog",
    urls = ["file:3rdparty/glog-0.3.3.tar.gz"],
    strip_prefix = "glog-0.3.3",
    build_file_content =
"""
genrule(
    name = "glog_genrule",
    srcs = glob(["*", "**/*"]),
    outs = ["libglog.a"],
    cmd = "external/glog/configure GTEST_CONFIG=false --prefix=$$PWD/PREFIX && make install && cp .libs/libglog.a libglog.a",
)

cc_library(
    name = "glog",
    srcs = ["libglog.a"],
    hdrs = glob(["PREFIX/include/**"]),
    includes = ["PREFIX/include"],
    strip_include_prefix = "PREFIX/include",
    visibility = ["//visibility:public"],
)
"""
)
