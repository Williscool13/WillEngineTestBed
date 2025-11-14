add_library(hot-reload-asset-load STATIC
        asset_load_types.cpp
        asset_load_types.h
        asset_loading_thread.cpp
        asset_loading_thread.h
)

target_link_libraries(hot-reload-asset-load PUBLIC
        core-lib
        render-lib
)