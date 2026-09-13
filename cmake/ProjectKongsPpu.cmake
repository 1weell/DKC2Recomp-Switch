# Keep the shared submodule pristine. The DKC2 build adds two narrow OAM
# callbacks to a build-local copy, with exact anchors that fail on drift.
set(_kongs_ppu_source "${SNESRECOMP_ROOT}/runner/src/snes/ppu.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_kongs_ppu_source}")
file(READ "${_kongs_ppu_source}" _kongs_ppu)
function(dkc2_kongs_patch before after)
    string(FIND "${_kongs_ppu}" "${before}" match)
    if(match EQUAL -1)
        message(FATAL_ERROR "Project Kongs PPU integration needs review: missing anchor ${before}")
    endif()
    string(LENGTH "${before}" anchor_length)
    math(EXPR after_anchor "${match} + ${anchor_length}")
    string(SUBSTRING "${_kongs_ppu}" ${after_anchor} -1 remainder)
    string(FIND "${remainder}" "${before}" duplicate)
    if(NOT duplicate EQUAL -1)
        message(FATAL_ERROR "Project Kongs PPU integration has an ambiguous anchor")
    endif()
    string(REPLACE "${before}" "${after}" _patched "${_kongs_ppu}")
    set(_kongs_ppu "${_patched}" PARENT_SCOPE)
endfunction()
dkc2_kongs_patch("#include \"ppu.h\""
    "#include \"ppu.h\"\n#include \"dkc2_kongs.h\"")
dkc2_kongs_patch("if(row < spriteHeight) {"
    "int kongVisible = Dkc2KongsOamVisible(line, index >> 1);\n    if(kongVisible > 0 || (kongVisible < 0 && row < spriteHeight)) {")
dkc2_kongs_patch("if(x + spriteSize > -left_extra) {"
    "if(kongVisible > 0 || x + spriteSize > -left_extra) {")
dkc2_kongs_patch("index = foundSprites[i - 1];"
    "index = foundSprites[i - 1];\n    if (Dkc2KongsRenderOam(ppu, line, index >> 1)) { tilesFound++; continue; }")
set(_kongs_ppu_output "${CMAKE_CURRENT_BINARY_DIR}/dkc2_ppu.c")
file(WRITE "${_kongs_ppu_output}.in" "${_kongs_ppu}")
configure_file("${_kongs_ppu_output}.in" "${_kongs_ppu_output}" COPYONLY)
set_source_files_properties("${_kongs_ppu_output}" PROPERTIES
    INCLUDE_DIRECTORIES "${SNESRECOMP_ROOT}/runner/src/snes;${CMAKE_CURRENT_SOURCE_DIR}/runner")
list(REMOVE_ITEM SNESRECOMP_RUNNER_SOURCES "${_kongs_ppu_source}")
list(APPEND SNESRECOMP_RUNNER_SOURCES "${_kongs_ppu_output}"
    "${CMAKE_CURRENT_SOURCE_DIR}/runner/dkc2_kongs.c")
