#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <iostream>
#include <string>
#include <webgpu/webgpu_cpp.h>

int main(int argc, char **argv) {
  DawnProcTable backendProcs = dawn::native::GetProcs();
  dawnProcSetProcs(&backendProcs);

  wgpu::InstanceDescriptor desc = {};
  desc.nextInChain = nullptr;

  wgpu::Instance instance = wgpu::CreateInstance(&desc);
  if (!instance) {
    std::cerr << "Could not initialize WebGPU!" << std::endl;
    return 1;
  }

  // wgpu::RequestAdapterOptions adapterOpts = {};
  // adapterOpts.nextInChain = nullptr;
  // wgpu::Adapter adapter = instance.RequestAdapter(
  //     &adapterOpts, RequestAdapterCallback callback, void *userdata);

  return 0;
}
