add_executable(standalone_compiler
	standalone_compiler.cc
	igc_interface.cc
)

target_include_directories(standalone_compiler PRIVATE
	${IGC_INCLUDE_DIRS}
	${IGDGMM_INCLUDE_DIRS}
	"${CMAKE_SOURCE_DIR}/src/")

target_link_libraries(standalone_compiler PRIVATE ${IGC_LIBRARIES} -ldl)
