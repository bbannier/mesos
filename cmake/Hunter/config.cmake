hunter_config(ZLIB VERSION ${HUNTER_ZLIB_VERSION} CMAKE_ARGS CMAKE_POSITION_INDEPENDENT_CODE=TRUE)# BUILD_SHARED_LIBS=ON)
hunter_config(gRPC VERSION ${HUNTER_gRPC_VERSION})
hunter_config(c-ares VERSION ${HUNTER_c-ares_VERSION} CMAKE_ARGS CMAKE_POSITION_INDEPENDENT_CODE=TRUE)
hunter_config(Protobuf VERSION ${HUNTER_Protobuf_VERSION} CMAKE_ARGS CMAKE_POSITION_INDEPENDENT_CODE=TRUE)
hunter_config(http-parser VERSION ${HUNTER_http-parser_VERSION} CMAKE_ARGS CMAKE_POSITION_INDEPENDENT_CODE=TRUE)
hunter_config(leveldb VERSION ${HUNTER_leveldb_VERSION} CMAKE_ARGS CMAKE_POSITION_INDEPENDENT_CODE=TRUE)
hunter_config(zookeeper VERSION ${HUNTER_zookeeper_VERSION} CMAKE_ARGS CMAKE_POSITION_INDEPENDENT_CODE=TRUE)
hunter_config(Libevent VERSION ${HUNTER_Libevent_VERSION} CMAKE_ARGS CMAKE_POSITION_INDEPENDENT_CODE=TRUE)


# hunter_default_version(concurrentqueue VERSION 7b69a8f)
