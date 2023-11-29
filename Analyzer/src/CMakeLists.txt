llvm_map_components_to_libnames(llvm_libs bitwriter core ipo irreader instcombine instrumentation target linker analysis scalaropts support )
file (GLOB SOURCES
   *.cpp
)
add_executable(distance-analyzer ${SOURCES})

target_link_libraries(distance-analyzer ${SVF_LIB} ${llvm_libs})
target_link_libraries(distance-analyzer ${Z3_LIBRARIES})
target_link_libraries(distance-analyzer ${JSONCPP_LIB})
set_target_properties( distance-analyzer PROPERTIES
                       RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )