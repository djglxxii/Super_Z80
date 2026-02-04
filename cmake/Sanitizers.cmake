function(superz80_enable_sanitizers target)
  if (MSVC)
    message(WARNING "Sanitizers requested but not configured for MSVC")
    return()
  endif()

  target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(${target} PRIVATE -fsanitize=address,undefined)
endfunction()
