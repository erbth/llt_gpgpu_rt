set(THIRD_PARTY_HWINFO_SRCS
	"${CMAKE_SOURCE_DIR}/src/third_party/helpers/hw_info.cpp"
	"${CMAKE_SOURCE_DIR}/src/third_party/hw_info/gen9/hw_info_gen9.cpp"
	"${CMAKE_SOURCE_DIR}/src/third_party/hw_info/gen9/hw_info_cfl.cpp"
	"${CMAKE_SOURCE_DIR}/src/third_party/hw_info/gen9/hw_info_glk.cpp"
)

add_executable(gpu_issue_test
	gpu_issue_test.cc
	drm_interface.cc
	device_registry.cc
	compiler/igc_interface.cc
	kernel.cc
	${THIRD_PARTY_HWINFO_SRCS}
)

target_include_directories(gpu_issue_test PRIVATE
	${LIBDRM_INCLUDE_DIRS}
	${IGDGMM_INCLUDE_DIRS}
	${IGC_INCLUDE_DIRS}
	"${CMAKE_SOURCE_DIR}/src")

target_link_libraries(gpu_issue_test PRIVATE
	${LIBDRM_LIBRARIES}
	${IGC_LIBRARIES}
	-ldl)
