if(NOT DEFINED E_FOC_MOTOR_BOARD)
    message(FATAL_ERROR
        "E_FOC_MOTOR_BOARD is not set. "
        "Specify the motor board in your CMake preset (e.g. \"E_FOC_MOTOR_BOARD\": \"FRDM-MC-LVPMSM\")."
    )
endif()

add_subdirectory(${E_FOC_MOTOR_BOARD})
