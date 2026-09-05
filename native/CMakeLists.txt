cmake_minimum_required(VERSION 3.20)

project(VTSFloat_Meow LANGUAGES CXX RC)

set(SPOUT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Spout2")
set(SPOUT_GL "${SPOUT_ROOT}/SPOUTSDK/SpoutGL")
set(SPOUT_DX "${SPOUT_ROOT}/SPOUTSDK/SpoutDirectX/SpoutDX")

set(SPOUT_DX11_SOURCES
    "${SPOUT_DX}/SpoutDX.cpp"
    "${SPOUT_GL}/SpoutCopy.cpp"
    "${SPOUT_GL}/SpoutDirectX.cpp"
    "${SPOUT_GL}/SpoutFrameCount.cpp"
    "${SPOUT_GL}/SpoutSenderNames.cpp"
    "${SPOUT_GL}/SpoutSharedMemory.cpp"
    "${SPOUT_GL}/SpoutUtils.cpp"
)

add_executable(vts_overlay_dx11
    overlay_dx11.cpp
    ${SPOUT_DX11_SOURCES}
)

add_executable(vts_spout_bridge WIN32
    spout_bridge.cpp
    ${SPOUT_DX11_SOURCES}
)

add_executable(vts_spout_test_sender
    spout_test_sender.cpp
    ${SPOUT_DX11_SOURCES}
)

add_executable(vts_overlay_layered WIN32
    overlay_layered.cpp
    localization.cpp
    overlay_resources.rc
    ${SPOUT_DX11_SOURCES}
)

add_executable(vts_localization_audit
    localization_layout_audit.cpp
    localization.cpp
)

option(VTSFLOAT_LANGUAGE_PREVIEW "Build the isolated multilingual preview" ON)
if(VTSFLOAT_LANGUAGE_PREVIEW)
    target_compile_definitions(vts_overlay_layered PRIVATE VTSFLOAT_LANGUAGE_PREVIEW=1)
endif()

foreach(target vts_overlay_dx11 vts_spout_bridge vts_spout_test_sender vts_overlay_layered vts_localization_audit)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_definitions(${target} PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
    )
    target_include_directories(${target} PRIVATE
        "${SPOUT_DX}"
        "${SPOUT_GL}"
    )
endforeach()

if(MSVC)
    target_compile_options(vts_overlay_dx11 PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(vts_spout_bridge PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(vts_spout_test_sender PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(vts_overlay_layered PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(vts_localization_audit PRIVATE /W3 /EHsc /utf-8)
endif()

set(SPOUT_DX11_LIBRARIES
    d3d11
    d3dcompiler
    dcomp
    dxgi
    dxguid
    psapi
    version
    winmm
    opengl32
    kernel32
    user32
    gdi32
    winspool
    comdlg32
    comctl32
    advapi32
    shell32
    ole32
    oleaut32
    uuid
    odbc32
    odbccp32
)

target_link_libraries(vts_overlay_dx11 PRIVATE ${SPOUT_DX11_LIBRARIES})
target_link_libraries(vts_spout_bridge PRIVATE ${SPOUT_DX11_LIBRARIES})
target_link_libraries(vts_spout_test_sender PRIVATE ${SPOUT_DX11_LIBRARIES})
target_link_libraries(vts_overlay_layered PRIVATE ${SPOUT_DX11_LIBRARIES} avrt winmm pdh winhttp gdiplus shlwapi iphlpapi dwmapi uxtheme)
target_link_libraries(vts_localization_audit PRIVATE user32 gdi32)

set_target_properties(vts_overlay_dx11 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)

set_target_properties(vts_spout_bridge PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)

set_target_properties(vts_spout_test_sender PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)

if(VTSFLOAT_LANGUAGE_PREVIEW)
    set(VTSFLOAT_LAYERED_OUTPUT_NAME "VTSFloat_Meow_LanguagePreview")
else()
    set(VTSFLOAT_LAYERED_OUTPUT_NAME "VTSFloat_Meow")
endif()

set_target_properties(vts_overlay_layered PROPERTIES
    OUTPUT_NAME "${VTSFLOAT_LAYERED_OUTPUT_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/.."
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/.."
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/.."
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
