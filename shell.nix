{ pkgs ? (import <nixpkgs> {}) }:
with pkgs;
mkShell {
  buildInputs = [
    # put packages here.
    pkg-config
    ninja
    glslang # or shaderc
    shaderc
    shader-slang
    vulkan-headers
    vulkan-loader
    vulkan-validation-layers # maybe?
    glfw
    libxkbcommon
    wayland
    tinyobjloader
    clang-tools
    tinygltf
    glm
    cmake
    freetype
    vulkan-tools        # vulkaninfo
    renderdoc           # Graphics debugger
    tracy               # Graphics profiler
    vulkan-tools-lunarg # vkconfig
    ktx-tools
    vulkan-validation-layers
    stb
  ];

  shellHook = ''
    export CMAKE_PREFIX_PATH=${pkgs.ktx-tools}:$CMAKE_PREFIX_PATH
  '';

  # If it doesn’t get picked up through nix magic
  VULKAN_SDK = "${vulkan-validation-layers}/share/vulkan/explicit_layer.d";
  VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";

}
