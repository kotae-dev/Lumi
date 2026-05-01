cmake_minimum_required(VERSION 3.25)
project(Lumi VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Build the C++ core DLL
add_subdirectory(core)

# Trigger dotnet build as a post-build step
find_program(DOTNET_EXEC dotnet)
if(DOTNET_EXEC)
    add_custom_target(LumiUI ALL
        COMMAND ${DOTNET_EXEC} publish ${CMAKE_SOURCE_DIR}/ui/Lumi.csproj
                -c Release
                -r win-x64
                --self-contained false
                -o ${CMAKE_BINARY_DIR}/bin/
        DEPENDS lumi_core
        COMMENT "Building Lumi WPF UI..."
    )
else()
    message(WARNING "dotnet not found — skipping WPF UI build")
endif()
