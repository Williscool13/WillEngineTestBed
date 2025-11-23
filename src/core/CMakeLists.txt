add_library(core-lib STATIC
        data-structures/free_list.cpp
        data-structures/free_list.h
        data-structures/handle_allocator.cpp
        data-structures/handle_allocator.h
        data-structures/ring_buffer.cpp
        data-structures/ring_buffer.h
        types/transform.cpp
        types/transform.h
        time.cpp
        time.h
)

target_link_libraries(core-lib PUBLIC
        engine-common
)