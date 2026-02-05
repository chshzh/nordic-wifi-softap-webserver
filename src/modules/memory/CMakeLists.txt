# Heap diagnostics module
if(CONFIG_APP_HEAP_MONITOR)
  target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/heap_monitor.c
  )
endif()

