set(SOURCES
    BencodeDecoder.cpp
)

serenity_lib(LibTorrent torrent)
target_link_libraries(LibTorrent PRIVATE LibCore)
