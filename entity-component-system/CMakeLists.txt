add_executable(entity-component-system
        main.cpp
        entity_physics_system.cpp
        entity_physics_system.h
        types.h
        entity_bounce_system.cpp
        entity_bounce_system.h
        entity_render_system.cpp
        entity_render_system.h
        game_object.cpp
        game_object.h
)

target_link_libraries(entity-component-system PRIVATE
        engine-common
        crash-handling
)