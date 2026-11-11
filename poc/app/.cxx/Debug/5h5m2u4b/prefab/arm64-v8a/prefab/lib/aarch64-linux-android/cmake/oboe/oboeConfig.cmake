if(NOT TARGET oboe::oboe)
add_library(oboe::oboe SHARED IMPORTED)
set_target_properties(oboe::oboe PROPERTIES
    IMPORTED_LOCATION "C:/Users/valdi/.gradle/caches/8.9/transforms/4e048f399ed3b39dba6874b492b60712/transformed/oboe-1.9.0/prefab/modules/oboe/libs/android.arm64-v8a/liboboe.so"
    INTERFACE_INCLUDE_DIRECTORIES "C:/Users/valdi/.gradle/caches/8.9/transforms/4e048f399ed3b39dba6874b492b60712/transformed/oboe-1.9.0/prefab/modules/oboe/include"
    INTERFACE_LINK_LIBRARIES ""
)
endif()

