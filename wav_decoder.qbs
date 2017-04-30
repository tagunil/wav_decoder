import qbs

Project {
    minimumQbsVersion: "1.7"

    CppApplication {
        consoleApplication: true

        cpp.warningLevel: "all"
        cpp.treatWarningsAsErrors: true

        cpp.cxxLanguageVersion: "c++14"

        cpp.defines: "_REENTRANT"

        // Workaround for Qt Creator code model issue
        cpp.systemIncludePaths: cpp.compilerIncludePaths

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
