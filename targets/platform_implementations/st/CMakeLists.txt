add_subdirectory(${E_FOC_TARGET_BOARD})

# A board without a peripheral map yet sets E_FOC_ST_STUBBED_PERIPHERALS and runs on stubs
if(E_FOC_ST_STUBBED_PERIPHERALS)
    add_subdirectory(stubs)
    set(E_FOC_PLATFORM_IMPLEMENTATION_DIR stubs PARENT_SCOPE)
else()
    add_subdirectory(implementation)
endif()

# Uses the infra mem file but the e-foc custom sections.ld (adds .error_handling)
function(e_foc_st_target_linker_scripts target)
    set(memLd "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../infra/hal/st/st/ldscripts/mem_${TARGET_MCU}.ld")
    set(sectionsLd "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ldscripts/sections.ld")
    target_link_options(${target} PUBLIC "LINKER:-T${memLd}")
    target_link_options(${target} PUBLIC "LINKER:-T${sectionsLd}")
    set_property(TARGET ${target} APPEND PROPERTY INTERFACE_LINK_DEPENDS "${memLd}" "${sectionsLd}")
endfunction()
