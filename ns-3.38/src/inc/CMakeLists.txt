check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/inc-examples-test-suite.cc
        )
endif()

build_lib(
    LIBNAME inc
    SOURCE_FILES model/inc.cc
                 model/inc-header.cc
                 model/inc-switch.cc
                 model/inc-stack.cc
                 model/ring-header.cc
                 model/ring-application.cc
                 helper/inc-helper.cc
    HEADER_FILES model/inc.h
                 model/inc-header.h
                 model/inc-switch.h
                 model/inc-stack.h
                 model/ring-header.h
                 model/ring-application.h
                 helper/inc-helper.h
    LIBRARIES_TO_LINK ${libcore}
                      ${libnetwork}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
    TEST_SOURCES test/inc-test-suite.cc
                 ${examples_as_tests_sources}
)
