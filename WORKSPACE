# git_repository(
#     name = "com_google_glog",
#     remote = "https://github.com/google/glog.git",
#     commit = "0a9f71036f5617ecd3549abff78a8d9a09c7e56e",
# )

# git_repository(
#     name = "com_github_gflags_gflags",
#     remote = "https://github.com/gflags/gflags.git",
#     commit = "77592648e3f3be87d6c7123eb81cbad75f9aef5a",
# )

new_http_archive(
    name = "boost",
    urls = ["file:3rdparty/boost-1.53.0.tar.gz"],
    strip_prefix = "boost-1.53.0",
    build_file_content =
"""
cc_library(
    name = "headers",
    includes = ["boost/version.hpp"],
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
    includes = glob(["googletest/include/**/*.h*"]),
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
    includes = glob(["googlemock/include/gmock/**/*.h*"]),
    strip_include_prefix = "googlemock/include",
    visibility = ["//visibility:public"],
)
""",
)

new_http_archive(
    name = "glog",
    urls = ["file:3rdparty/glog-0.3.3.tar.gz"],
    build_file_content =
"""
genrule(
    name = "glog_genrule",
    srcs = glob(["**/*"]),
    outs = ["libglog.a"],
    cmd = "echo $$PWD &&  $(location libglog.a)/configure && make libglog.a && cp .libs/libglog.a $@",
)

cc_library(
    name = "glog",
    srcs = ["libglog.a"],
    includes = glob(["src/glog/**/*.h*"]),
    strip_include_prefix = "src",
    visibility = ["//visibility:public"],
)
"""
)
