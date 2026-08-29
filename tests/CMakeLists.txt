add_executable(atlas_scheduler_regression
    atlas_scheduler_regression.cpp
)

target_link_libraries(atlas_scheduler_regression PRIVATE eoe_core)

add_test(NAME atlas_scheduler_regression COMMAND atlas_scheduler_regression)
