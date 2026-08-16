# Invoked via add_custom_target() (CMakeLists.txt), not at configure time --
# local time, "yymmdd-hhmmss" to match the timestamp format already used
# elsewhere in the app (screenshot/measurement default filenames).
string(TIMESTAMP ANTSCOPEZ_BUILD_TIMESTAMP "%y%m%d-%H%M%S")
configure_file("${IN_FILE}" "${OUT_FILE}" @ONLY)
