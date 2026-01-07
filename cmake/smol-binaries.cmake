# These flags are standard for `Release` binaries, but... no one should use that.
# Enable them for everything except 'Debug' so that we get them in
# `RelWithDebInfo` too

add_link_options(
  "$<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO>"
  # COMDAT folding; drops off another 1MB
  "$<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>"
  # Remove unused functions and data, in particular, WinRT types
  "$<$<NOT:$<CONFIG:Debug>>:/OPT:REF>"
)