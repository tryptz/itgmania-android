list(APPEND SMDATA_GLOBAL_FILES_SRC
            "GameLoop.cpp"
            "global.cpp"
            "SongTreeImport.cpp"
            "SpecialFiles.cpp"
            "StepMania.cpp" # TODO: Refactor into separate main project.
            "${SM_GENERATED_SRC_DIR}/verstub.cpp")

list(APPEND SMDATA_GLOBAL_FILES_HPP
            "GameLoop.h"
            "global.h"
            "SongTreeImport.h"
            "PeriodicCaller.h"
            "ProductInfo.h" # TODO: Have this be auto-generated.
            "SpecialFiles.h"
            "StdString.h" # TODO: Remove the need for this file, transition to
                          # std::string.
            "StepMania.h" # TODO: Refactor into separate main project.
     )

if(ANDROID)
  # Shared JNI plumbing plus user-visible storage. Listed here rather than with
  # the sound driver because storage stands alone, and because AndroidJni holds
  # the single JNI_OnLoad this shared object is allowed to have.
  list(APPEND SMDATA_GLOBAL_FILES_SRC "AndroidJni.cpp" "AndroidStorage.cpp"
              "archutils/Android/CrashHandler_Android.cpp")
  list(APPEND SMDATA_GLOBAL_FILES_HPP "AndroidJni.h" "AndroidStorage.h")
endif()

source_group("Global Files"
             FILES
             ${SMDATA_GLOBAL_FILES_SRC}
             ${SMDATA_GLOBAL_FILES_HPP})
