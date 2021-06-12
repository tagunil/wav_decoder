import qbs

Project {
    minimumQbsVersion: "1.7"

    CppApplication {
        consoleApplication: true

        cpp.warningLevel: "all"
        cpp.treatWarningsAsErrors: true

        cpp.cxxLanguageVersion: "c++14"

        cpp.defines: [
            "_REENTRANT",
            "HAS_IEEE_FLOAT"
        ]

        cpp.dynamicLibraries: [
            "pulse-simple",
            "pulse"
        ]

        files: [
            "main.cpp",
            "wavreader.cpp",
            "wavreader.h",
        ]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }
}
