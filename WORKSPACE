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
    urls = ["file:3rdparty/glog-0.3.3_patched.tar.gz"],
    strip_prefix = "glog-0.3.3",
    build_file_content =
"""
genrule(
    name = "glog_genrule",
    srcs = glob(["*", "**/*"]),
    outs = [
      "libglog.a",
      "src/glog/logging.h",
      "src/glog/raw_logging.h",
      "src/glog/stl_logging.h",
      "src/glog/vlog_is_on.h",
    ],
    tools = ["configure"],
    cmd = "$(location :configure) GTEST_CONFIG=no" +
      "&& make install" +
      "&& cp -v .libs/libglog.a $(location libglog.a)" +
      "&& cp -v src/glog/logging.h $(location src/glog/logging.h)" +
      "&& cp -v src/glog/raw_logging.h $(location src/glog/raw_logging.h)" +
      "&& cp -v src/glog/stl_logging.h $(location src/glog/stl_logging.h)" +
      "&& cp -v src/glog/vlog_is_on.h $(location src/glog/vlog_is_on.h)",
)

cc_library(
    name = "glog",
    srcs = ["libglog.a"],
    hdrs = ["src/glog/logging.h"],
    includes = ["src/glog"],
    strip_include_prefix = "src",
    visibility = ["//visibility:public"],
)
"""
)
