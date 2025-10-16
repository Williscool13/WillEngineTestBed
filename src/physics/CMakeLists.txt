add_library(physics-lib STATIC
        layer_interface.cpp
        layer_interface.h
        contact_listener.cpp
        contact_listener.h
        body_activation_listener.cpp
        body_activation_listener.h
        physics_job_system.cpp
        physics_job_system.h
        physics_constants.cpp
        physics_constants.h
)

target_link_libraries(physics-lib PUBLIC
        engine-common
        crash-handling
        Jolt
        enkiTS
)