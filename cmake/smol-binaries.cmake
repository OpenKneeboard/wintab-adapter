# These flags are standard for `Release` binaries, but we want to distribute
# 'RelWithDebInfo'

add_link_options(
  "$<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO>"
  # COMDAT folding; drops off another 1MB
  "$<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>"
  # Remove unused functions and data, in particular, WinRT types
  "$<$<NOT:$<CONFIG:Debug>>:/OPT:REF>"
)