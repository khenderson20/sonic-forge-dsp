# =============================================================================
# CPM.cmake — C++ Package Manager bootstrap
#
# Downloads CPM.cmake at configure time if not already cached.
#
# Cache behaviour (checked in order):
#   1. CMake variable CPM_SOURCE_CACHE  (-DCPM_SOURCE_CACHE=<path>)
#   2. Environment variable CPM_SOURCE_CACHE
#   3. Falls back to <binary-dir>/cmake/ (re-downloaded every clean build)
#
# In CI, set CPM_SOURCE_CACHE to a persistent path and cache it between runs
# to avoid redundant downloads (see .github/workflows/ci.yml).
# =============================================================================

set(CPM_DOWNLOAD_VERSION 0.40.2)
# SHA256 of CPM_0.40.2.cmake — update this when bumping CPM_DOWNLOAD_VERSION.
set(_cpm_expected_hash
    "c8cdc32c03816538ce22781ed72964dc864b2a34a310d3b7104812a5ca2d835d")

if(CPM_SOURCE_CACHE)
    set(_cpm_download_location
        "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
    set(_cpm_download_location
        "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
    set(_cpm_download_location
        "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()


# ── Tilde expansion ───────────────────────────────────────────────────────────
# cmake file() operations do not expand the shell shorthand "~" to the home
# directory — that expansion is performed only by the shell.  When
# CPM_SOURCE_CACHE is set via a YAML env block (e.g. "~/.cache/CPM") the
# literal tilde is passed to cmake and must be resolved here.
if(_cpm_download_location MATCHES "^~[/\\]?")
    if(DEFINED ENV{HOME})
        set(_cpm_home "$ENV{HOME}")
    elseif(DEFINED ENV{USERPROFILE})
        file(TO_CMAKE_PATH "$ENV{USERPROFILE}" _cpm_home)
    else()
        message(WARNING "CPM bootstrap: cannot expand '~' — neither HOME nor "
            "USERPROFILE is set.  Set CPM_SOURCE_CACHE to an absolute path.")
        set(_cpm_home "~")
    endif()
    string(REGEX REPLACE "^~" "${_cpm_home}" _cpm_download_location
        "${_cpm_download_location}")
    unset(_cpm_home)
endif()

if(NOT EXISTS "${_cpm_download_location}")
    message(STATUS
        "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION} "
        "-> ${_cpm_download_location}")
    file(
        DOWNLOAD
        "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
        "${_cpm_download_location}"
        EXPECTED_HASH "SHA256=${_cpm_expected_hash}"
    )
endif()

include("${_cpm_download_location}")
