build_lib_example(
    NAME inc-example
    SOURCE_FILES inc-example.cc
    LIBRARIES_TO_LINK ${libinc}
                      ${libinternet}
                      ${libpoint-to-point}
)


build_lib_example(
    NAME inc-topology-2hosts
    SOURCE_FILES inc-topology-2hosts.cc
    LIBRARIES_TO_LINK ${libinc}
                      ${libinternet}
                      ${libpoint-to-point}
)


build_lib_example(
    NAME inc-topology-tree
    SOURCE_FILES inc-topology-tree.cc
    LIBRARIES_TO_LINK ${libinc}
                      ${libinternet}
                      ${libpoint-to-point}
)


build_lib_example(
    NAME ring-allreduce-tcp-example
    SOURCE_FILES ring-allreduce-tcp-example.cc
    LIBRARIES_TO_LINK ${libinc}
                      ${libinternet}
                      ${libpoint-to-point}
                      ${libapplications}
)



build_lib_example(
    NAME inc-topology-tree-8hosts
    SOURCE_FILES inc-topology-tree-8hosts.cc
    LIBRARIES_TO_LINK ${libinc}
                      ${libinternet}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME inc-topology-tree-16hosts
    SOURCE_FILES inc-topology-tree-16hosts.cc
    LIBRARIES_TO_LINK ${libinc}
                      ${libinternet}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME inc-topology-tree-32hosts
    SOURCE_FILES inc-topology-tree-32hosts.cc
    LIBRARIES_TO_LINK ${libinc}
                      ${libinternet}
                      ${libpoint-to-point}
)