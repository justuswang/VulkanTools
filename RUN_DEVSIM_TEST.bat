REM devsim layer test for Windows

set VK_LAYER_PATH=%CD%\BUILD_WIN64\layersvt\Debug
set VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_device_simulation
set VK_DEVSIM_FILENAME=%CD%\tests\devsim_test1.json

set VK_DEVSIM_DEBUG_ENABLE=1
set VK_DEVSIM_EXIT_ON_ERROR=1
REM set VK_LOADER_DEBUG=all

vkjson_info.exe

pause

REM vim: set sw=4 ts=8 et ic ai:
