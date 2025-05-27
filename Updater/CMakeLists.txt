# project(PotatoUpdater)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-D_HAS_EXCEPTIONS=0)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 COMPONENTS Core Network Widgets REQUIRED)

# Core updater library with new architecture
add_library(UpdaterCore
    # Interfaces
    include/Updater/Interfaces/IDownloader.hpp
    include/Updater/Interfaces/IFileManager.hpp
    include/Updater/Interfaces/IBackupManager.hpp
    include/Updater/Interfaces/IProcessManager.hpp
    include/Updater/Interfaces/IUpdateVerifier.hpp
    include/Updater/Interfaces/IProgressReporter.hpp
    include/Updater/Interfaces/IPlatformManager.hpp
    
    # Models
    include/Updater/Models/UpdateInfo.hpp
    include/Updater/Models/DownloadProgress.hpp
    include/Updater/Models/UpdateConfiguration.hpp
    include/Updater/Models/UpdateResult.hpp
    include/Updater/Models/ProgressInfo.hpp
    include/Updater/Models/UpdateError.hpp
    
    # Core components
    src/Core/UpdateOrchestrator.cpp
    include/Updater/Core/UpdateOrchestrator.hpp
    src/Core/UpdateStateMachine.cpp
    include/Updater/Core/UpdateStateMachine.hpp
    src/Core/ErrorHandler.cpp
    include/Updater/Core/ErrorHandler.hpp
    
    # Services
    src/Services/GitHubDownloader.cpp
    include/Updater/Services/GitHubDownloader.hpp
    src/Services/FileManager.cpp
    include/Updater/Services/FileManager.hpp
    src/Services/BackupManager.cpp
    include/Updater/Services/BackupManager.hpp
    src/Services/CryptoVerifier.cpp
    include/Updater/Services/CryptoVerifier.hpp
    src/Services/ProgressReporter.cpp
    include/Updater/Services/ProgressReporter.hpp    
    # Platform-specific implementations
    src/Platform/Win32ProcessManager.cpp
    include/Updater/Platform/Win32ProcessManager.hpp
)

# Platform-specific sources
if(WIN32)
    target_sources(UpdaterCore PRIVATE
        src/Platform/win32/Win32PlatformManager.cpp
        include/Updater/Platform/win32/Win32PlatformManager.hpp
    )
else()
    target_sources(UpdaterCore PRIVATE
        src/Platform/linux/LinuxProcessManager.cpp
        include/Updater/Platform/linux/LinuxProcessManager.hpp
        src/Platform/linux/LinuxPlatformManager.cpp
        include/Updater/Platform/linux/LinuxPlatformManager.hpp
    )
endif()

target_link_libraries(UpdaterCore PRIVATE Core Qt::Network Qt::Core)
if(WIN32)
    target_link_libraries(UpdaterCore PRIVATE Win32)
endif()

target_include_directories(UpdaterCore PUBLIC include)
set_target_properties(UpdaterCore PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED true)

# Legacy updater library for backward compatibility
add_library(Updater 
    src/Updater.cpp 
    include/Updater/Updater.hpp
    src/ModernUpdater.cpp
    include/Updater/ModernUpdater.hpp
    src/UpdaterFactory.cpp
    include/Updater/UpdaterFactory.hpp
)
target_link_libraries(Updater PRIVATE UpdaterCore Gui Core Qt::Network Qt::Widgets)
if(WIN32)
    target_link_libraries(Updater PRIVATE Win32)
endif()
target_include_directories(Updater PUBLIC include)
set_target_properties(Updater PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED true)

add_executable(PotatoUpdater src/PotatoUpdater.cpp ${PROJECT_SOURCE_DIR}/Resources/PotatoUpdater.qrc ${VersionFilesOutputVariable})

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set_property(TARGET PotatoUpdater PROPERTY WIN32_EXECUTABLE true)
endif()

include(CompilerFlags)
SetCompilerFlags(PotatoUpdater)

target_link_libraries(PotatoUpdater PRIVATE Core Gui Updater Qt::Core Qt::Widgets Win32)
set_target_properties(PotatoUpdater PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED true)
set_target_properties(PotatoUpdater
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_command(
    TARGET PotatoUpdater
    POST_BUILD
    COMMAND "mt.exe" -manifest \"${CMAKE_CURRENT_SOURCE_DIR}/PotatoUpdater.manifest\" -outputresource:$<TARGET_FILE_DIR:PotatoUpdater>/PotatoUpdater.exe\;\#1
    COMMENT "Adding manifest..."
)

include(Packaging)
WinDeployQt(PotatoUpdater)
