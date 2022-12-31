add_executable(gpu_issue_test
	gpu_issue_test.cc
	drm_interface.cc
	device_registry.cc
)

target_include_directories(gpu_issue_test PRIVATE
	${LIBDRM_INCLUDE_DIRS}
	${IGDGMM_INCLUDE_DIRS}
	"${CMAKE_SOURCE_DIR}/src")

target_link_libraries(gpu_issue_test PRIVATE ${LIBDRM_LIBRARIES})

add_subdirectory(compiler)
