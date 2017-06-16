#! /bin/bash
# simple test run for the devsim layer
# mikew@lunarg.com 2017-06-02

set errexit
set nounset
set physical

export VK_INSTANCE_LAYERS="VK_LAYER_LUNARG_device_simulation"

cd $(dirname "${BASH_SOURCE[0]}")
VULKAN_TOOLS="$(pwd)"

cd "${VULKAN_TOOLS}/build/layersvt"
RELEASE_VK_LAYER_PATH="$(pwd)"

cd "${VULKAN_TOOLS}/dbuild/layersvt"
DEBUG_VK_LAYER_PATH="$(pwd)"

export VK_DEVSIM_FILENAME="tests/devsim_test1.json"

export VK_DEVSIM_DEBUG_ENABLE="1"
export VK_DEVSIM_EXIT_ON_ERROR="1"
#export VK_LOADER_DEBUG="all"

source "${HOME}/vksdk/setup-env.sh"
VULKANINFO="${VULKAN_SDK}/bin/vulkaninfo"
VKJSON_INFO="${VULKAN_SDK}/bin/vkjson_info"

cd "${VULKAN_TOOLS}"
build/tests/devsim_layer_test.sh
dbuild/tests/devsim_layer_test.sh

#export VK_LAYER_PATH="$RELEASE_VK_LAYER_PATH"
export VK_LAYER_PATH="$DEBUG_VK_LAYER_PATH"

#echo "vulkaninfo ================================================================================="
#$VULKANINFO

#echo "cube ======================================================================================="
#pushd ${VULKAN_SDK}/../examples/build > /dev/null
#./cube --c 10
#popd > /dev/null

#echo "vkjson_info ================================================================================"
${VKJSON_INFO}
rm device_simulation_layer_test_1.json

# vim: set sw=4 ts=8 et ic ai:
