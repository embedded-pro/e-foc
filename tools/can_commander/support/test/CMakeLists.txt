add_executable(e_foc.tool.can_commander.support_test)
emil_build_for(e_foc.tool.can_commander.support_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.can_commander.support_test)

target_link_libraries(e_foc.tool.can_commander.support_test PUBLIC
    gmock_main
    e_foc.tool.can_commander.support
)

target_sources(e_foc.tool.can_commander.support_test PRIVATE
    TestIssueLog.cpp
    TestIssueReport.cpp
    TestQtEventDispatcher.cpp
    TestReleaseFeed.cpp
    TestReleaseVersion.cpp
    TestUpdateChecker.cpp
    TestUpdateDownloader.cpp
    TestUpdateInstallation.cpp
    TestUpdateSession.cpp
)
