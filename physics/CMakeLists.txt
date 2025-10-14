add_executable(physics
        main.cpp
        physics.cpp
        physics.h
        layer_interface.cpp
        layer_interface.h
        contact_listener.cpp
        contact_listener.h
        body_activation_listener.cpp
        body_activation_listener.h
)

target_link_libraries(physics PRIVATE
        engine-common
        crash-handling
        Jolt
)